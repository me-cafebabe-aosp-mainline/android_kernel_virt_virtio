// SPDX-License-Identifier: GPL-2.0
/*
 * f_blk.c - USB block device function
 *
 * Exposes a block device (/dev/usbblkN) on the gadget whose storage is
 * served by a program on the USB host. This makes it possible to run a
 * system from storage that lives on the host, e.g. for tethered boot
 * during board bring-up.
 *
 * Block requests are sent to the host over a bulk IN endpoint and the host
 * completes them over a bulk OUT endpoint, see include/uapi/linux/usb/f_blk.h
 * for the protocol. Data is bounced through preallocated buffers so any UDC
 * can be used.
 *
 * The block device is owned by the function instance, so it survives
 * the gadget being disconnected or unbound from the UDC. While the host is
 * gone requests are held back and are resent once the host attaches the
 * same disk again; they fail if the host stays away for longer than the
 * configured timeout.
 */

#include <linux/blk-mq.h>
#include <linux/blkdev.h>
#include <linux/idr.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/usb/composite.h>
#include <linux/usb/f_blk.h>
#include <linux/usb/func_utils.h>
#include <linux/wait.h>
#include <linux/workqueue.h>

#include "configfs.h"
#include "u_blk.h"

#define FBLK_DEFAULT_TIMEOUT		30	/* seconds */
#define FBLK_DEFAULT_QUEUE_DEPTH	32
#define FBLK_MAX_QUEUE_DEPTH		1024
#define FBLK_DEFAULT_MAX_XFER		SZ_128K
#define FBLK_MIN_MAX_XFER		SZ_4K
#define FBLK_MAX_MAX_XFER		SZ_4M
#define FBLK_DEFAULT_QLEN		8
#define FBLK_MAX_QLEN			64

/* receive buffer for response headers, a multiple of any bulk maxpacket */
#define FBLK_RX_HDR_LEN			1024

/* time to wait for dequeued USB requests to be given back */
#define FBLK_FLUSH_TIMEOUT		HZ

static DEFINE_IDA(fblk_index_ida);

enum fblk_cmd_state {
	FBLK_CMD_IDLE,		/* owned by the block layer */
	FBLK_CMD_INFLIGHT,	/* sent to the host, waiting for a response */
	FBLK_CMD_CLAIMED,	/* being completed */
};

struct fblk_cmd {
	atomic_t		state;
	u16			seq;
	blk_status_t		status;
};

struct f_blk;

/* a request message on its way to the host */
struct fblk_slot {
	struct list_head	list;
	struct f_blk		*fb;
	struct usb_request	*hdr_req;
	struct usb_request	*data_req;
	bool			has_data;
	unsigned int		pending;	/* queued transfers */
};

/* state shared by the block device and the bound USB function */
struct fblk_dev {
	struct f_blk_opts	*opts;

	/*
	 * Protects the fields below up to the mutex. Taken from USB
	 * completion handlers and from the block layer.
	 */
	spinlock_t		lock;
	struct f_blk		*fb;		/* function with enabled eps */
	bool			online;		/* host attached, send requests */
	bool			dead;		/* fail requests */
	bool			has_disk;	/* tag set is valid */
	bool			attach_failed;
	unsigned int		busy_slots;
	struct request		*rx_rq;		/* request receiving data */
	bool			rx_data;	/* data stage queued */
	struct usb_blk_attach	attach_buf;

	wait_queue_head_t	idle_wq;
	struct work_struct	attach_work;
	struct work_struct	detach_work;
	struct delayed_work	dead_work;

	/* serializes disk creation and removal */
	struct mutex		mutex;
	struct blk_mq_tag_set	tag_set;
	struct gendisk		*disk;
	int			index;
	struct usb_blk_attach	cur;
	unsigned int		max_xfer;
	unsigned int		queue_depth;
	unsigned int		timeout;	/* seconds, READ_ONCE() */
};

struct f_blk {
	struct usb_function	func;
	struct fblk_dev		*dev;

	struct usb_ep		*in_ep;
	struct usb_ep		*out_ep;
	u8			intf;

	unsigned int		max_xfer;
	unsigned int		nr_slots;
	struct fblk_slot	*slots;
	struct list_head	free_slots;	/* under dev->lock */
	/* keeps the transfers of a message together on the IN endpoint */
	spinlock_t		tx_lock;

	struct usb_request	*rx_hdr_req;
	struct usb_request	*rx_data_req;
};

static inline struct f_blk *func_to_fblk(struct usb_function *f)
{
	return container_of(f, struct f_blk, func);
}

/*-------------------------------------------------------------------------*/

static struct usb_interface_descriptor fblk_intf = {
	.bLength		= sizeof(fblk_intf),
	.bDescriptorType	= USB_DT_INTERFACE,
	.bNumEndpoints		= 2,
	.bInterfaceClass	= USB_CLASS_VENDOR_SPEC,
	.bInterfaceSubClass	= USB_BLK_SUBCLASS,
	.bInterfaceProtocol	= USB_BLK_PROTOCOL,
	/* .bInterfaceNumber and .iInterface = DYNAMIC */
};

static struct usb_endpoint_descriptor fblk_fs_in_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_IN,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
};

static struct usb_endpoint_descriptor fblk_fs_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bEndpointAddress	= USB_DIR_OUT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
};

static struct usb_descriptor_header *fblk_fs_descs[] = {
	(struct usb_descriptor_header *)&fblk_intf,
	(struct usb_descriptor_header *)&fblk_fs_in_desc,
	(struct usb_descriptor_header *)&fblk_fs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor fblk_hs_in_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(512),
};

static struct usb_endpoint_descriptor fblk_hs_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(512),
};

static struct usb_descriptor_header *fblk_hs_descs[] = {
	(struct usb_descriptor_header *)&fblk_intf,
	(struct usb_descriptor_header *)&fblk_hs_in_desc,
	(struct usb_descriptor_header *)&fblk_hs_out_desc,
	NULL,
};

static struct usb_endpoint_descriptor fblk_ss_in_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor fblk_ss_in_comp_desc = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst		= 15,
};

static struct usb_endpoint_descriptor fblk_ss_out_desc = {
	.bLength		= USB_DT_ENDPOINT_SIZE,
	.bDescriptorType	= USB_DT_ENDPOINT,
	.bmAttributes		= USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize		= cpu_to_le16(1024),
};

static struct usb_ss_ep_comp_descriptor fblk_ss_out_comp_desc = {
	.bLength		= USB_DT_SS_EP_COMP_SIZE,
	.bDescriptorType	= USB_DT_SS_ENDPOINT_COMP,
	.bMaxBurst		= 15,
};

static struct usb_descriptor_header *fblk_ss_descs[] = {
	(struct usb_descriptor_header *)&fblk_intf,
	(struct usb_descriptor_header *)&fblk_ss_in_desc,
	(struct usb_descriptor_header *)&fblk_ss_in_comp_desc,
	(struct usb_descriptor_header *)&fblk_ss_out_desc,
	(struct usb_descriptor_header *)&fblk_ss_out_comp_desc,
	NULL,
};

static struct usb_string fblk_string_defs[] = {
	[0].s = "USB Block Device",
	{ }
};

static struct usb_gadget_strings fblk_string_table = {
	.language	= 0x0409,	/* en-us */
	.strings	= fblk_string_defs,
};

static struct usb_gadget_strings *fblk_strings[] = {
	&fblk_string_table,
	NULL,
};

/*-------------------------------------------------------------------------*/
/* Request bookkeeping */

static inline struct request_queue *fblk_queue(struct fblk_dev *dev)
{
	return dev->disk->queue;
}

static void fblk_end_request(struct request *rq, blk_status_t status)
{
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);

	cmd->status = status;
	blk_mq_complete_request(rq);
}

/*
 * Look up the in-flight request a response cookie refers to and take
 * ownership of it. Called with dev->lock held.
 */
static struct request *fblk_claim(struct fblk_dev *dev, u32 cookie)
{
	unsigned int tag = cookie & 0xffff;
	struct fblk_cmd *cmd;
	struct request *rq;

	lockdep_assert_held(&dev->lock);

	if (!dev->has_disk || tag >= dev->tag_set.queue_depth)
		return NULL;

	rq = blk_mq_tag_to_rq(dev->tag_set.tags[0], tag);
	if (!rq)
		return NULL;

	cmd = blk_mq_rq_to_pdu(rq);
	if (atomic_cmpxchg(&cmd->state, FBLK_CMD_INFLIGHT,
			   FBLK_CMD_CLAIMED) != FBLK_CMD_INFLIGHT)
		return NULL;

	if (cmd->seq != cookie >> 16) {
		atomic_set(&cmd->state, FBLK_CMD_INFLIGHT);
		return NULL;
	}

	return rq;
}

static void fblk_copy_data(struct request *rq, void *buf, bool to_rq)
{
	struct req_iterator iter;
	struct bio_vec bv;

	rq_for_each_segment(bv, rq, iter) {
		if (to_rq)
			memcpy_to_bvec(&bv, buf);
		else
			memcpy_from_bvec(buf, &bv);
		buf += bv.bv_len;
	}
}

static blk_status_t fblk_errno_to_status(u8 status)
{
	switch (status) {
	case USB_BLK_S_OK:
		return BLK_STS_OK;
	case USB_BLK_S_UNSUPP:
		return BLK_STS_NOTSUPP;
	case USB_BLK_S_NOSPC:
		return BLK_STS_NOSPC;
	default:
		return BLK_STS_IOERR;
	}
}

/*
 * The host went away or stopped responding: hold back new requests and
 * fail everything if it does not come back in time. Called with dev->lock
 * held, may be called from atomic context.
 */
static void fblk_link_down_locked(struct fblk_dev *dev)
{
	unsigned int timeout = READ_ONCE(dev->timeout);

	lockdep_assert_held(&dev->lock);

	dev->online = false;
	if (dev->has_disk && !dev->dead && timeout)
		queue_delayed_work(system_dfl_wq, &dev->dead_work, timeout * HZ);
}

static void fblk_put_slot_locked(struct fblk_dev *dev, struct fblk_slot *slot)
{
	lockdep_assert_held(&dev->lock);

	list_add_tail(&slot->list, &slot->fb->free_slots);
	dev->busy_slots--;
	/* the disk can not go away while a slot is busy */
	if (dev->online)
		blk_mq_start_stopped_hw_queues(fblk_queue(dev), true);
	else
		wake_up(&dev->idle_wq);
}

/* @count transfers of @slot are done, free it once all are */
static void fblk_tx_done(struct fblk_slot *slot, unsigned int count)
{
	struct fblk_dev *dev = slot->fb->dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	slot->pending -= count;
	if (!slot->pending)
		fblk_put_slot_locked(dev, slot);
	spin_unlock_irqrestore(&dev->lock, flags);
}

/*-------------------------------------------------------------------------*/
/* Block device */

static blk_status_t fblk_queue_rq(struct blk_mq_hw_ctx *hctx,
				  const struct blk_mq_queue_data *bd)
{
	struct fblk_dev *dev = hctx->queue->queuedata;
	struct request *rq = bd->rq;
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);
	struct usb_blk_request *hdr;
	struct fblk_slot *slot;
	unsigned int len = blk_rq_bytes(rq);
	unsigned long flags;
	struct f_blk *fb;
	u16 rq_flags = 0;
	bool has_data;
	u8 op;

	switch (req_op(rq)) {
	case REQ_OP_READ:
		op = USB_BLK_OP_READ;
		break;
	case REQ_OP_WRITE:
		op = USB_BLK_OP_WRITE;
		if (rq->cmd_flags & REQ_FUA)
			rq_flags |= USB_BLK_RQ_FUA;
		break;
	case REQ_OP_FLUSH:
		op = USB_BLK_OP_FLUSH;
		break;
	case REQ_OP_DISCARD:
		op = USB_BLK_OP_DISCARD;
		break;
	case REQ_OP_WRITE_ZEROES:
		op = USB_BLK_OP_WRITE_ZEROES;
		if (rq->cmd_flags & REQ_NOUNMAP)
			rq_flags |= USB_BLK_RQ_NOUNMAP;
		break;
	default:
		return BLK_STS_NOTSUPP;
	}

	if ((op == USB_BLK_OP_READ || op == USB_BLK_OP_WRITE) &&
	    len > dev->max_xfer)
		return BLK_STS_IOERR;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->dead) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return BLK_STS_IOERR;
	}
	fb = dev->fb;
	if (!dev->online || !fb || list_empty(&fb->free_slots)) {
		/* restarted when a slot is freed or the host attaches */
		blk_mq_stop_hw_queue(hctx);
		spin_unlock_irqrestore(&dev->lock, flags);
		return BLK_STS_DEV_RESOURCE;
	}
	slot = list_first_entry(&fb->free_slots, struct fblk_slot, list);
	list_del(&slot->list);
	dev->busy_slots++;
	spin_unlock_irqrestore(&dev->lock, flags);

	blk_mq_start_request(rq);

	cmd->seq++;
	hdr = slot->hdr_req->buf;
	hdr->magic = cpu_to_le32(USB_BLK_REQUEST_MAGIC);
	hdr->cookie = cpu_to_le32(rq->tag | (u32)cmd->seq << 16);
	hdr->op = op;
	hdr->reserved = 0;
	hdr->flags = cpu_to_le16(rq_flags);
	hdr->len = cpu_to_le32(len);
	hdr->sector = cpu_to_le64(blk_rq_pos(rq));

	/* the slot may be reused as soon as its last transfer completes */
	has_data = op == USB_BLK_OP_WRITE && len;
	slot->has_data = has_data;
	if (has_data) {
		fblk_copy_data(rq, slot->data_req->buf, false);
		slot->data_req->length = len;
	}

	spin_lock_irqsave(&dev->lock, flags);
	if (!dev->online || dev->fb != fb) {
		fblk_put_slot_locked(dev, slot);
		blk_mq_stop_hw_queue(hctx);
		spin_unlock_irqrestore(&dev->lock, flags);
		return BLK_STS_DEV_RESOURCE;
	}
	slot->pending = has_data ? 2 : 1;
	atomic_set(&cmd->state, FBLK_CMD_INFLIGHT);
	spin_unlock_irqrestore(&dev->lock, flags);

	/*
	 * Queue outside the lock, some UDCs complete requests synchronously.
	 * If the endpoint is going away the request stays in flight and is
	 * resent or fails like any other request the host did not complete.
	 */
	spin_lock_irqsave(&fb->tx_lock, flags);
	if (usb_ep_queue(fb->in_ep, slot->hdr_req, GFP_ATOMIC)) {
		spin_unlock_irqrestore(&fb->tx_lock, flags);
		fblk_tx_done(slot, has_data ? 2 : 1);
	} else if (has_data &&
		   usb_ep_queue(fb->in_ep, slot->data_req, GFP_ATOMIC)) {
		spin_unlock_irqrestore(&fb->tx_lock, flags);
		fblk_tx_done(slot, 1);
	} else {
		spin_unlock_irqrestore(&fb->tx_lock, flags);
	}

	return BLK_STS_OK;
}

static void fblk_complete_rq(struct request *rq)
{
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);

	blk_mq_end_request(rq, cmd->status);
}

static enum blk_eh_timer_return fblk_timeout(struct request *rq)
{
	struct fblk_dev *dev = rq->q->queuedata;
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);
	unsigned long flags;

	if (!READ_ONCE(dev->timeout))
		return BLK_EH_RESET_TIMER;

	if (atomic_cmpxchg(&cmd->state, FBLK_CMD_INFLIGHT,
			   FBLK_CMD_CLAIMED) != FBLK_CMD_INFLIGHT) {
		/*
		 * Stuck receiving the response data: the host must attach
		 * again, which aborts the transfer, or the request fails.
		 */
		spin_lock_irqsave(&dev->lock, flags);
		if (dev->rx_rq == rq)
			fblk_link_down_locked(dev);
		spin_unlock_irqrestore(&dev->lock, flags);
		return BLK_EH_RESET_TIMER;
	}

	/*
	 * The host is not responding. Stop sending requests until it
	 * attaches again, the remaining ones fail if it does not.
	 */
	spin_lock_irqsave(&dev->lock, flags);
	if (dev->online)
		pr_warn_ratelimited("%s: host not responding\n",
				    dev->disk->disk_name);
	fblk_link_down_locked(dev);
	spin_unlock_irqrestore(&dev->lock, flags);

	fblk_end_request(rq, BLK_STS_TIMEOUT);
	return BLK_EH_DONE;
}

static int fblk_init_request(struct blk_mq_tag_set *set, struct request *rq,
			     unsigned int hctx_idx, int numa_node)
{
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);

	atomic_set(&cmd->state, FBLK_CMD_IDLE);
	return 0;
}

static const struct blk_mq_ops fblk_mq_ops = {
	.queue_rq	= fblk_queue_rq,
	.complete	= fblk_complete_rq,
	.timeout	= fblk_timeout,
	.init_request	= fblk_init_request,
};

static const struct block_device_operations fblk_fops = {
	.owner		= THIS_MODULE,
};

static bool fblk_requeue_inflight(struct request *rq, void *data)
{
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);

	if (atomic_cmpxchg(&cmd->state, FBLK_CMD_INFLIGHT,
			   FBLK_CMD_IDLE) == FBLK_CMD_INFLIGHT)
		blk_mq_requeue_request(rq, false);
	return true;
}

static bool fblk_fail_inflight(struct request *rq, void *data)
{
	struct fblk_cmd *cmd = blk_mq_rq_to_pdu(rq);

	if (atomic_cmpxchg(&cmd->state, FBLK_CMD_INFLIGHT,
			   FBLK_CMD_CLAIMED) == FBLK_CMD_INFLIGHT)
		fblk_end_request(rq, BLK_STS_IOERR);
	return true;
}

/* fail all requests, now and until the host attaches again */
static void fblk_dead_work(struct work_struct *work)
{
	struct fblk_dev *dev = container_of(to_delayed_work(work),
					    struct fblk_dev, dead_work);
	bool abort_rx;
	struct f_blk *fb;

	spin_lock_irq(&dev->lock);
	if (dev->online || !dev->has_disk) {
		spin_unlock_irq(&dev->lock);
		return;
	}
	dev->dead = true;
	fb = dev->fb;
	abort_rx = dev->rx_data;
	spin_unlock_irq(&dev->lock);

	pr_warn("%s: host did not attach in time, failing I/O\n",
		dev->disk->disk_name);

	/* fb stays bound while we run, see fblk_unbind() */
	if (fb && abort_rx) {
		usb_ep_dequeue(fb->out_ep, fb->rx_data_req);
		wait_event_timeout(dev->idle_wq, !READ_ONCE(dev->rx_data),
				   FBLK_FLUSH_TIMEOUT);
	}

	blk_mq_tagset_busy_iter(&dev->tag_set, fblk_fail_inflight, NULL);
	blk_mq_start_stopped_hw_queues(fblk_queue(dev), true);
}

/*-------------------------------------------------------------------------*/
/* Attach and detach, process context with dev->mutex held */

static bool fblk_usb_idle(struct fblk_dev *dev)
{
	bool idle;

	spin_lock_irq(&dev->lock);
	idle = !dev->busy_slots && !dev->rx_data;
	spin_unlock_irq(&dev->lock);

	return idle;
}

/*
 * Stop sending requests and make sure the host can not complete any
 * request we are about to resend or fail: drop the messages that were not
 * picked up by the host yet and abort a response data stage.
 */
static void fblk_offline(struct fblk_dev *dev)
{
	struct f_blk *fb;
	unsigned int i;

	lockdep_assert_held(&dev->mutex);

	spin_lock_irq(&dev->lock);
	/* fail I/O in time even if the host never manages to attach */
	fblk_link_down_locked(dev);
	fb = dev->fb;
	spin_unlock_irq(&dev->lock);

	/*
	 * fb can not be unbound while we hold dev->mutex. If its endpoints
	 * get disabled meanwhile, all requests are given back anyway.
	 */
	if (fb) {
		for (i = 0; i < fb->nr_slots; i++) {
			struct fblk_slot *slot = &fb->slots[i];

			if (!READ_ONCE(slot->pending))
				continue;
			if (slot->has_data)
				usb_ep_dequeue(fb->in_ep, slot->data_req);
			usb_ep_dequeue(fb->in_ep, slot->hdr_req);
		}
		if (READ_ONCE(dev->rx_data))
			usb_ep_dequeue(fb->out_ep, fb->rx_data_req);
	}

	wait_event_timeout(dev->idle_wq, fblk_usb_idle(dev),
			   FBLK_FLUSH_TIMEOUT);
}

static void fblk_online(struct fblk_dev *dev)
{
	struct request_queue *q = fblk_queue(dev);

	lockdep_assert_held(&dev->mutex);

	cancel_delayed_work_sync(&dev->dead_work);

	/* everything still in flight was sent to the previous host session */
	blk_mq_tagset_busy_iter(&dev->tag_set, fblk_requeue_inflight, NULL);

	spin_lock_irq(&dev->lock);
	dev->dead = false;
	dev->attach_failed = false;
	if (dev->fb)
		dev->online = true;
	else
		fblk_link_down_locked(dev);
	spin_unlock_irq(&dev->lock);

	blk_mq_kick_requeue_list(q);
	blk_mq_start_stopped_hw_queues(q, true);
}

static void fblk_remove_disk(struct fblk_dev *dev)
{
	struct gendisk *disk = dev->disk;

	lockdep_assert_held(&dev->mutex);

	if (!disk)
		return;

	spin_lock_irq(&dev->lock);
	dev->dead = true;
	spin_unlock_irq(&dev->lock);

	fblk_offline(dev);
	cancel_delayed_work_sync(&dev->dead_work);

	blk_mq_tagset_busy_iter(&dev->tag_set, fblk_fail_inflight, NULL);
	blk_mq_start_stopped_hw_queues(disk->queue, true);
	blk_mq_kick_requeue_list(disk->queue);

	pr_info("%s: removed\n", disk->disk_name);
	del_gendisk(disk);

	spin_lock_irq(&dev->lock);
	dev->has_disk = false;
	dev->dead = false;
	spin_unlock_irq(&dev->lock);

	put_disk(disk);
	blk_mq_free_tag_set(&dev->tag_set);
	ida_free(&fblk_index_ida, dev->index);
	dev->disk = NULL;
}

static int fblk_check_attach(const struct usb_blk_attach *a)
{
	u32 lbs = le32_to_cpu(a->logical_block_size);
	u32 pbs = le32_to_cpu(a->physical_block_size);
	u32 flags = le32_to_cpu(a->flags);

	if (le16_to_cpu(a->version) != USB_BLK_VERSION)
		return -EPROTO;
	if (blk_validate_block_size(lbs) || lbs > PAGE_SIZE)
		return -EINVAL;
	if (pbs && (!is_power_of_2(pbs) || pbs < lbs))
		return -EINVAL;
	if ((flags & USB_BLK_F_FUA) && !(flags & USB_BLK_F_FLUSH))
		return -EINVAL;
	if (le64_to_cpu(a->nr_sectors) < lbs >> SECTOR_SHIFT)
		return -EINVAL;

	return 0;
}

/* does @a describe the disk we already have? */
static bool fblk_same_disk(const struct usb_blk_attach *cur,
			   const struct usb_blk_attach *a)
{
	struct usb_blk_attach x = *cur, y = *a;

	/* capacity and the read only flag can change */
	x.nr_sectors = 0;
	y.nr_sectors = 0;
	x.flags &= ~cpu_to_le32(USB_BLK_F_RO);
	y.flags &= ~cpu_to_le32(USB_BLK_F_RO);

	return !memcmp(&x, &y, sizeof(x));
}

static sector_t fblk_capacity(const struct usb_blk_attach *a)
{
	u32 lbs = le32_to_cpu(a->logical_block_size);

	return round_down(le64_to_cpu(a->nr_sectors), lbs >> SECTOR_SHIFT);
}

static int fblk_create_disk(struct fblk_dev *dev,
			    const struct usb_blk_attach *a)
{
	u32 flags = le32_to_cpu(a->flags);
	u32 lbs = le32_to_cpu(a->logical_block_size);
	u32 pbs = le32_to_cpu(a->physical_block_size);
	u32 max_discard = le32_to_cpu(a->max_discard_sectors);
	u32 max_wzeroes = le32_to_cpu(a->max_write_zeroes_sectors);
	struct f_blk_opts *opts = dev->opts;
	struct queue_limits lim = {
		.logical_block_size	= lbs,
		.physical_block_size	= pbs ?: lbs,
		.max_segments		= USHRT_MAX,
		.max_segment_size	= UINT_MAX,
	};
	struct gendisk *disk;
	unsigned int timeout;
	int ret;

	lockdep_assert_held(&dev->mutex);

	mutex_lock(&opts->lock);
	dev->max_xfer = opts->max_xfer;
	dev->queue_depth = opts->queue_depth;
	mutex_unlock(&opts->lock);

	lim.max_hw_sectors = dev->max_xfer >> SECTOR_SHIFT;
	if (flags & USB_BLK_F_FLUSH)
		lim.features |= BLK_FEAT_WRITE_CACHE;
	if (flags & USB_BLK_F_FUA)
		lim.features |= BLK_FEAT_FUA;
	if (flags & USB_BLK_F_ROTATIONAL)
		lim.features |= BLK_FEAT_ROTATIONAL;
	if (flags & USB_BLK_F_DISCARD) {
		lim.max_hw_discard_sectors = UINT_MAX >> SECTOR_SHIFT;
		if (max_discard)
			lim.max_hw_discard_sectors =
				min(max_discard, lim.max_hw_discard_sectors);
		lim.discard_granularity = le32_to_cpu(a->discard_granularity);
	}
	if (flags & USB_BLK_F_WRITE_ZEROES) {
		lim.max_write_zeroes_sectors = UINT_MAX >> SECTOR_SHIFT;
		if (max_wzeroes)
			lim.max_write_zeroes_sectors =
				min(max_wzeroes, lim.max_write_zeroes_sectors);
	}

	timeout = READ_ONCE(dev->timeout);
	memset(&dev->tag_set, 0, sizeof(dev->tag_set));
	dev->tag_set.ops = &fblk_mq_ops;
	dev->tag_set.nr_hw_queues = 1;
	dev->tag_set.queue_depth = dev->queue_depth;
	dev->tag_set.numa_node = NUMA_NO_NODE;
	dev->tag_set.cmd_size = sizeof(struct fblk_cmd);
	dev->tag_set.timeout = (timeout ?: FBLK_DEFAULT_TIMEOUT) * HZ;
	dev->tag_set.driver_data = dev;

	ret = blk_mq_alloc_tag_set(&dev->tag_set);
	if (ret)
		return ret;

	disk = blk_mq_alloc_disk(&dev->tag_set, &lim, dev);
	if (IS_ERR(disk)) {
		ret = PTR_ERR(disk);
		goto err_free_tag_set;
	}

	ret = ida_alloc(&fblk_index_ida, GFP_KERNEL);
	if (ret < 0)
		goto err_put_disk;
	dev->index = ret;

	disk->fops = &fblk_fops;
	disk->private_data = dev;
	snprintf(disk->disk_name, DISK_NAME_LEN, "usbblk%d", dev->index);
	set_capacity(disk, fblk_capacity(a));
	set_disk_ro(disk, flags & USB_BLK_F_RO);

	dev->disk = disk;
	dev->cur = *a;

	spin_lock_irq(&dev->lock);
	dev->has_disk = true;
	spin_unlock_irq(&dev->lock);

	return 0;

err_put_disk:
	put_disk(disk);
err_free_tag_set:
	blk_mq_free_tag_set(&dev->tag_set);
	return ret;
}

static void fblk_attach_work(struct work_struct *work)
{
	struct fblk_dev *dev = container_of(work, struct fblk_dev,
					    attach_work);
	struct usb_blk_attach a;
	bool new_disk = false;
	int ret;

	mutex_lock(&dev->mutex);

	spin_lock_irq(&dev->lock);
	a = dev->attach_buf;
	spin_unlock_irq(&dev->lock);

	ret = fblk_check_attach(&a);
	if (ret) {
		pr_err("usb_f_blk: invalid attach request: %d\n", ret);
		goto fail;
	}

	if (dev->disk) {
		/* a new session, the previous one is gone */
		fblk_offline(dev);
		if (!fblk_same_disk(&dev->cur, &a))
			fblk_remove_disk(dev);
	}

	if (!dev->disk) {
		ret = fblk_create_disk(dev, &a);
		if (ret) {
			pr_err("usb_f_blk: failed to create disk: %d\n", ret);
			goto fail;
		}
		new_disk = true;
	} else {
		dev->cur = a;
		set_disk_ro(dev->disk, le32_to_cpu(a.flags) & USB_BLK_F_RO);
		set_capacity_and_notify(dev->disk, fblk_capacity(&a));
	}

	fblk_online(dev);

	if (new_disk) {
		ret = add_disk(dev->disk);
		if (ret) {
			pr_err("usb_f_blk: failed to add disk: %d\n", ret);
			goto fail_remove;
		}
		pr_info("%s: attached, %llu sectors\n", dev->disk->disk_name,
			(unsigned long long)get_capacity(dev->disk));
	} else {
		pr_info("%s: reattached\n", dev->disk->disk_name);
	}

	mutex_unlock(&dev->mutex);
	return;

fail_remove:
	/* not added, tear down by hand */
	spin_lock_irq(&dev->lock);
	dev->online = false;
	dev->has_disk = false;
	spin_unlock_irq(&dev->lock);
	cancel_delayed_work_sync(&dev->dead_work);
	put_disk(dev->disk);
	blk_mq_free_tag_set(&dev->tag_set);
	ida_free(&fblk_index_ida, dev->index);
	dev->disk = NULL;
fail:
	spin_lock_irq(&dev->lock);
	dev->attach_failed = true;
	spin_unlock_irq(&dev->lock);
	mutex_unlock(&dev->mutex);
}

static void fblk_detach_work(struct work_struct *work)
{
	struct fblk_dev *dev = container_of(work, struct fblk_dev,
					    detach_work);

	mutex_lock(&dev->mutex);
	fblk_remove_disk(dev);
	spin_lock_irq(&dev->lock);
	dev->attach_failed = false;
	spin_unlock_irq(&dev->lock);
	mutex_unlock(&dev->mutex);
}

/*-------------------------------------------------------------------------*/
/* USB data path */

static void fblk_tx_complete(struct usb_ep *ep, struct usb_request *req)
{
	fblk_tx_done(req->context, 1);
}

static void fblk_queue_rx_hdr(struct f_blk *fb)
{
	struct fblk_dev *dev = fb->dev;
	unsigned long flags;
	int ret = 0;

	bool enabled;

	spin_lock_irqsave(&dev->lock, flags);
	enabled = dev->fb == fb;
	spin_unlock_irqrestore(&dev->lock, flags);
	if (!enabled)
		return;

	fb->rx_hdr_req->length = FBLK_RX_HDR_LEN;
	ret = usb_ep_queue(fb->out_ep, fb->rx_hdr_req, GFP_ATOMIC);
	if (ret && ret != -ESHUTDOWN)
		pr_err("usb_f_blk: failed to queue OUT request: %d\n", ret);
}

static void fblk_rx_hdr_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_blk *fb = req->context;
	struct fblk_dev *dev = fb->dev;
	struct usb_blk_response *resp = req->buf;
	struct request *rq;
	unsigned long flags;
	u32 len;
	int ret;

	if (req->status == -ESHUTDOWN)
		return;
	if (req->status)
		goto requeue;

	/* headers always end with a short packet, anything else is noise */
	if (req->actual != sizeof(*resp) ||
	    le32_to_cpu(resp->magic) != USB_BLK_RESPONSE_MAGIC)
		goto requeue;

	len = le32_to_cpu(resp->len);

	spin_lock_irqsave(&dev->lock, flags);
	rq = fblk_claim(dev, le32_to_cpu(resp->cookie));

	if (!len) {
		spin_unlock_irqrestore(&dev->lock, flags);
		if (rq)
			fblk_end_request(rq, fblk_errno_to_status(resp->status));
		goto requeue;
	}

	if (len > fb->max_xfer || len % SECTOR_SIZE) {
		spin_unlock_irqrestore(&dev->lock, flags);
		pr_err_ratelimited("usb_f_blk: bad response length %u\n", len);
		if (rq)
			fblk_end_request(rq, BLK_STS_IOERR);
		goto requeue;
	}

	if (rq && (req_op(rq) != REQ_OP_READ ||
		   resp->status != USB_BLK_S_OK || len != blk_rq_bytes(rq))) {
		pr_err_ratelimited("usb_f_blk: unexpected response data\n");
		fblk_end_request(rq, BLK_STS_IOERR);
		rq = NULL;
	}

	/* receive the data, or drain it if the request is gone */
	dev->rx_rq = rq;
	dev->rx_data = true;
	spin_unlock_irqrestore(&dev->lock, flags);

	fb->rx_data_req->length = len;
	ret = usb_ep_queue(fb->out_ep, fb->rx_data_req, GFP_ATOMIC);
	if (ret) {
		/* endpoint going away, the request gets resent or fails */
		spin_lock_irqsave(&dev->lock, flags);
		dev->rx_rq = NULL;
		dev->rx_data = false;
		spin_unlock_irqrestore(&dev->lock, flags);
		if (rq)
			atomic_set(&((struct fblk_cmd *)blk_mq_rq_to_pdu(rq))->state,
				   FBLK_CMD_INFLIGHT);
		wake_up(&dev->idle_wq);
	}
	return;

requeue:
	fblk_queue_rx_hdr(fb);
}

static void fblk_rx_data_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct f_blk *fb = req->context;
	struct fblk_dev *dev = fb->dev;
	unsigned long flags;
	struct request *rq;

	spin_lock_irqsave(&dev->lock, flags);
	rq = dev->rx_rq;
	dev->rx_rq = NULL;
	dev->rx_data = false;
	spin_unlock_irqrestore(&dev->lock, flags);
	wake_up(&dev->idle_wq);

	if (req->status) {
		/* aborted, the request gets resent or fails later */
		if (rq)
			atomic_set(&((struct fblk_cmd *)blk_mq_rq_to_pdu(rq))->state,
				   FBLK_CMD_INFLIGHT);
		if (req->status == -ESHUTDOWN)
			return;
		goto requeue;
	}

	if (rq) {
		if (req->actual == req->length) {
			fblk_copy_data(rq, req->buf, true);
			fblk_end_request(rq, BLK_STS_OK);
		} else {
			fblk_end_request(rq, BLK_STS_IOERR);
		}
	}

requeue:
	fblk_queue_rx_hdr(fb);
}

/*-------------------------------------------------------------------------*/
/* Control requests */

static void fblk_attach_complete(struct usb_ep *ep, struct usb_request *req)
{
	struct fblk_dev *dev = req->context;

	if (req->status || req->actual != sizeof(struct usb_blk_attach))
		return;

	spin_lock(&dev->lock);
	memcpy(&dev->attach_buf, req->buf, sizeof(dev->attach_buf));
	spin_unlock(&dev->lock);

	queue_work(system_dfl_wq, &dev->attach_work);
}

static u32 fblk_state(struct fblk_dev *dev)
{
	if (dev->online)
		return USB_BLK_STATE_ATTACHED;
	if (dev->attach_failed)
		return USB_BLK_STATE_FAILED;
	if (dev->has_disk)
		return USB_BLK_STATE_OFFLINE;
	return USB_BLK_STATE_DETACHED;
}

static int fblk_setup(struct usb_function *f,
		      const struct usb_ctrlrequest *ctrl)
{
	struct f_blk *fb = func_to_fblk(f);
	struct fblk_dev *dev = fb->dev;
	struct usb_composite_dev *cdev = f->config->cdev;
	struct usb_request *req = cdev->req;
	u16 w_index = le16_to_cpu(ctrl->wIndex);
	u16 w_length = le16_to_cpu(ctrl->wLength);
	struct usb_blk_info *info;
	int value;
	int ret;

	if ((ctrl->bRequestType & (USB_TYPE_MASK | USB_RECIP_MASK)) !=
	    (USB_TYPE_VENDOR | USB_RECIP_INTERFACE) ||
	    (w_index & 0xff) != fb->intf)
		return -EOPNOTSUPP;

	switch (ctrl->bRequest) {
	case USB_BLK_REQ_GET_INFO:
		if (!(ctrl->bRequestType & USB_DIR_IN))
			return -EINVAL;
		info = req->buf;
		memset(info, 0, sizeof(*info));
		info->version = cpu_to_le16(USB_BLK_VERSION);
		info->max_xfer = cpu_to_le32(fb->max_xfer);
		spin_lock(&dev->lock);
		info->queue_depth = cpu_to_le16(dev->has_disk ?
						dev->queue_depth :
						dev->opts->queue_depth);
		info->state = cpu_to_le32(fblk_state(dev));
		spin_unlock(&dev->lock);
		value = min_t(u16, w_length, sizeof(*info));
		break;

	case USB_BLK_REQ_ATTACH:
		if ((ctrl->bRequestType & USB_DIR_IN) ||
		    w_length != sizeof(struct usb_blk_attach))
			return -EINVAL;
		value = w_length;
		req->context = dev;
		req->complete = fblk_attach_complete;
		break;

	case USB_BLK_REQ_DETACH:
		if ((ctrl->bRequestType & USB_DIR_IN) || w_length)
			return -EINVAL;
		queue_work(system_dfl_wq, &dev->detach_work);
		value = 0;
		break;

	default:
		return -EOPNOTSUPP;
	}

	req->length = value;
	req->zero = value < w_length;
	ret = usb_ep_queue(cdev->gadget->ep0, req, GFP_ATOMIC);
	if (ret < 0)
		return ret;

	return value;
}

/*-------------------------------------------------------------------------*/
/* Function */

static void fblk_disable(struct usb_function *f)
{
	struct f_blk *fb = func_to_fblk(f);
	struct fblk_dev *dev = fb->dev;
	unsigned long flags;

	spin_lock_irqsave(&dev->lock, flags);
	if (dev->fb != fb) {
		spin_unlock_irqrestore(&dev->lock, flags);
		return;
	}
	if (dev->online)
		pr_info("%s: host disconnected\n", dev->disk->disk_name);
	fblk_link_down_locked(dev);
	/* keeps completion handlers from queueing requests again */
	dev->fb = NULL;
	spin_unlock_irqrestore(&dev->lock, flags);

	/* gives back all requests */
	usb_ep_disable(fb->in_ep);
	usb_ep_disable(fb->out_ep);
	wake_up(&dev->idle_wq);
}

static int fblk_set_alt(struct usb_function *f, unsigned int intf,
			unsigned int alt)
{
	struct f_blk *fb = func_to_fblk(f);
	struct fblk_dev *dev = fb->dev;
	struct usb_composite_dev *cdev = f->config->cdev;
	unsigned long flags;
	int ret;

	if (alt)
		return -EINVAL;

	fblk_disable(f);

	ret = config_ep_by_speed(cdev->gadget, f, fb->in_ep);
	if (ret)
		return ret;
	ret = config_ep_by_speed(cdev->gadget, f, fb->out_ep);
	if (ret)
		return ret;

	ret = usb_ep_enable(fb->in_ep);
	if (ret)
		return ret;
	ret = usb_ep_enable(fb->out_ep);
	if (ret) {
		usb_ep_disable(fb->in_ep);
		return ret;
	}

	/* requests flow once the host attaches */
	spin_lock_irqsave(&dev->lock, flags);
	dev->fb = fb;
	spin_unlock_irqrestore(&dev->lock, flags);

	fblk_queue_rx_hdr(fb);

	return 0;
}

static struct usb_request *fblk_alloc_req(struct usb_ep *ep, size_t len,
					  void (*complete)(struct usb_ep *,
							   struct usb_request *),
					  void *context)
{
	struct usb_request *req;

	req = usb_ep_alloc_request(ep, GFP_KERNEL);
	if (!req)
		return NULL;

	req->buf = kmalloc(len, GFP_KERNEL);
	if (!req->buf) {
		usb_ep_free_request(ep, req);
		return NULL;
	}
	req->length = len;
	req->complete = complete;
	req->context = context;

	return req;
}

static void fblk_free_req(struct usb_ep *ep, struct usb_request *req)
{
	if (req)
		free_ep_req(ep, req);
}

static void fblk_free_requests(struct f_blk *fb)
{
	unsigned int i;

	if (fb->slots) {
		for (i = 0; i < fb->nr_slots; i++) {
			fblk_free_req(fb->in_ep, fb->slots[i].hdr_req);
			fblk_free_req(fb->in_ep, fb->slots[i].data_req);
		}
		kfree(fb->slots);
		fb->slots = NULL;
	}
	fblk_free_req(fb->out_ep, fb->rx_hdr_req);
	fb->rx_hdr_req = NULL;
	fblk_free_req(fb->out_ep, fb->rx_data_req);
	fb->rx_data_req = NULL;
}

static int fblk_alloc_requests(struct f_blk *fb)
{
	unsigned int i;

	INIT_LIST_HEAD(&fb->free_slots);
	spin_lock_init(&fb->tx_lock);

	fb->slots = kcalloc(fb->nr_slots, sizeof(*fb->slots), GFP_KERNEL);
	if (!fb->slots)
		return -ENOMEM;

	for (i = 0; i < fb->nr_slots; i++) {
		struct fblk_slot *slot = &fb->slots[i];

		slot->fb = fb;
		slot->hdr_req = fblk_alloc_req(fb->in_ep,
					       sizeof(struct usb_blk_request),
					       fblk_tx_complete, slot);
		slot->data_req = fblk_alloc_req(fb->in_ep, fb->max_xfer,
						fblk_tx_complete, slot);
		if (!slot->hdr_req || !slot->data_req)
			return -ENOMEM;
		/* data is always a multiple of maxpacket, terminate it */
		slot->data_req->zero = 1;
		list_add_tail(&slot->list, &fb->free_slots);
	}

	fb->rx_hdr_req = fblk_alloc_req(fb->out_ep, FBLK_RX_HDR_LEN,
					fblk_rx_hdr_complete, fb);
	fb->rx_data_req = fblk_alloc_req(fb->out_ep, fb->max_xfer,
					 fblk_rx_data_complete, fb);
	if (!fb->rx_hdr_req || !fb->rx_data_req)
		return -ENOMEM;

	return 0;
}

static int fblk_bind(struct usb_configuration *c, struct usb_function *f)
{
	struct usb_composite_dev *cdev = c->cdev;
	struct f_blk *fb = func_to_fblk(f);
	struct f_blk_opts *opts = container_of(f->fi, struct f_blk_opts,
					       func_inst);
	struct usb_string *us;
	int ret;

	us = usb_gstrings_attach(cdev, fblk_strings,
				 ARRAY_SIZE(fblk_string_defs));
	if (IS_ERR(us))
		return PTR_ERR(us);
	fblk_intf.iInterface = us[0].id;

	ret = usb_interface_id(c, f);
	if (ret < 0)
		return ret;
	fb->intf = ret;
	fblk_intf.bInterfaceNumber = ret;

	fb->in_ep = usb_ep_autoconfig(cdev->gadget, &fblk_fs_in_desc);
	if (!fb->in_ep)
		goto autoconf_fail;
	fb->out_ep = usb_ep_autoconfig(cdev->gadget, &fblk_fs_out_desc);
	if (!fb->out_ep)
		goto autoconf_fail;

	fblk_hs_in_desc.bEndpointAddress = fblk_fs_in_desc.bEndpointAddress;
	fblk_hs_out_desc.bEndpointAddress = fblk_fs_out_desc.bEndpointAddress;
	fblk_ss_in_desc.bEndpointAddress = fblk_fs_in_desc.bEndpointAddress;
	fblk_ss_out_desc.bEndpointAddress = fblk_fs_out_desc.bEndpointAddress;

	ret = usb_assign_descriptors(f, fblk_fs_descs, fblk_hs_descs,
				     fblk_ss_descs, fblk_ss_descs);
	if (ret)
		return ret;

	if (cdev->use_os_string) {
		f->os_desc_table = kzalloc_obj(*f->os_desc_table);
		if (!f->os_desc_table) {
			ret = -ENOMEM;
			goto fail;
		}
		f->os_desc_table[0].os_desc = &opts->os_desc;
		f->os_desc_table[0].if_id = fb->intf;
		f->os_desc_n = 1;
	}

	ret = fblk_alloc_requests(fb);
	if (ret)
		goto fail;

	DBG(cdev, "%s: IN/%s, OUT/%s\n", f->name, fb->in_ep->name,
	    fb->out_ep->name);
	return 0;

fail:
	fblk_free_requests(fb);
	kfree(f->os_desc_table);
	f->os_desc_table = NULL;
	f->os_desc_n = 0;
	usb_free_all_descriptors(f);
	return ret;

autoconf_fail:
	ERROR(cdev, "%s: can't autoconfigure on %s\n", f->name,
	      cdev->gadget->name);
	return -ENODEV;
}

static void fblk_unbind(struct usb_configuration *c, struct usb_function *f)
{
	struct f_blk *fb = func_to_fblk(f);
	struct fblk_dev *dev = fb->dev;

	fblk_disable(f);

	/* wait for work and fblk_queue_rq() that may still use fb */
	flush_work(&dev->dead_work.work);
	mutex_lock(&dev->mutex);
	if (dev->disk) {
		blk_mq_quiesce_queue(dev->disk->queue);
		blk_mq_unquiesce_queue(dev->disk->queue);
	}
	mutex_unlock(&dev->mutex);

	fblk_free_requests(fb);
	kfree(f->os_desc_table);
	f->os_desc_table = NULL;
	f->os_desc_n = 0;
	usb_free_all_descriptors(f);
}

static void fblk_free_func(struct usb_function *f)
{
	struct f_blk_opts *opts = container_of(f->fi, struct f_blk_opts,
					       func_inst);

	mutex_lock(&opts->lock);
	opts->refcnt--;
	mutex_unlock(&opts->lock);

	kfree(func_to_fblk(f));
}

static struct usb_function *fblk_alloc_func(struct usb_function_instance *fi)
{
	struct f_blk_opts *opts = container_of(fi, struct f_blk_opts,
					       func_inst);
	struct f_blk *fb;

	fb = kzalloc_obj(*fb);
	if (!fb)
		return ERR_PTR(-ENOMEM);

	mutex_lock(&opts->lock);
	fb->max_xfer = opts->max_xfer;
	fb->nr_slots = opts->qlen;
	opts->refcnt++;
	mutex_unlock(&opts->lock);

	fb->dev = opts->dev;
	fb->func.name = "blk";
	fb->func.bind = fblk_bind;
	fb->func.unbind = fblk_unbind;
	fb->func.set_alt = fblk_set_alt;
	fb->func.disable = fblk_disable;
	fb->func.setup = fblk_setup;
	fb->func.free_func = fblk_free_func;

	return &fb->func;
}

/*-------------------------------------------------------------------------*/
/* configfs */

static inline struct f_blk_opts *to_f_blk_opts(struct config_item *item)
{
	return container_of(to_config_group(item), struct f_blk_opts,
			    func_inst.group);
}

static void fblk_attr_release(struct config_item *item)
{
	struct f_blk_opts *opts = to_f_blk_opts(item);

	usb_put_function_instance(&opts->func_inst);
}

static const struct configfs_item_operations fblk_item_ops = {
	.release	= fblk_attr_release,
};

static ssize_t f_blk_opts_timeout_show(struct config_item *item, char *page)
{
	struct f_blk_opts *opts = to_f_blk_opts(item);
	ssize_t ret;

	mutex_lock(&opts->lock);
	ret = sysfs_emit(page, "%u\n", opts->timeout);
	mutex_unlock(&opts->lock);

	return ret;
}

static ssize_t f_blk_opts_timeout_store(struct config_item *item,
					const char *page, size_t len)
{
	struct f_blk_opts *opts = to_f_blk_opts(item);
	struct fblk_dev *dev = opts->dev;
	unsigned int val;
	int ret;

	ret = kstrtouint(page, 0, &val);
	if (ret)
		return ret;
	if (val > INT_MAX / HZ)
		return -ERANGE;

	mutex_lock(&dev->mutex);
	mutex_lock(&opts->lock);
	opts->timeout = val;
	WRITE_ONCE(dev->timeout, val);
	mutex_unlock(&opts->lock);
	if (dev->disk)
		blk_queue_rq_timeout(dev->disk->queue,
				     (val ?: FBLK_DEFAULT_TIMEOUT) * HZ);
	mutex_unlock(&dev->mutex);

	return len;
}

CONFIGFS_ATTR(f_blk_opts_, timeout);

/* attributes that can not change while the function is in use */
#define F_BLK_OPT(name, min, max, align)				\
static ssize_t f_blk_opts_##name##_show(struct config_item *item,	\
					char *page)			\
{									\
	struct f_blk_opts *opts = to_f_blk_opts(item);			\
	ssize_t ret;							\
									\
	mutex_lock(&opts->lock);					\
	ret = sysfs_emit(page, "%u\n", opts->name);			\
	mutex_unlock(&opts->lock);					\
									\
	return ret;							\
}									\
									\
static ssize_t f_blk_opts_##name##_store(struct config_item *item,	\
					 const char *page, size_t len)	\
{									\
	struct f_blk_opts *opts = to_f_blk_opts(item);			\
	struct fblk_dev *dev = opts->dev;				\
	unsigned int val;						\
	int ret;							\
									\
	ret = kstrtouint(page, 0, &val);				\
	if (ret)							\
		return ret;						\
	if (val < (min) || val > (max) || val % (align))		\
		return -EINVAL;						\
									\
	mutex_lock(&dev->mutex);					\
	mutex_lock(&opts->lock);					\
	if (opts->refcnt || dev->disk) {				\
		ret = -EBUSY;						\
	} else {							\
		opts->name = val;					\
		ret = len;						\
	}								\
	mutex_unlock(&opts->lock);					\
	mutex_unlock(&dev->mutex);					\
									\
	return ret;							\
}									\
									\
CONFIGFS_ATTR(f_blk_opts_, name)

F_BLK_OPT(queue_depth, 1, FBLK_MAX_QUEUE_DEPTH, 1);
F_BLK_OPT(max_xfer, FBLK_MIN_MAX_XFER, FBLK_MAX_MAX_XFER, SZ_4K);
F_BLK_OPT(qlen, 1, FBLK_MAX_QLEN, 1);

#undef F_BLK_OPT

static ssize_t f_blk_opts_disk_show(struct config_item *item, char *page)
{
	struct f_blk_opts *opts = to_f_blk_opts(item);
	struct fblk_dev *dev = opts->dev;
	ssize_t ret;

	mutex_lock(&dev->mutex);
	ret = sysfs_emit(page, "%s\n", dev->disk ? dev->disk->disk_name : "");
	mutex_unlock(&dev->mutex);

	return ret;
}

CONFIGFS_ATTR_RO(f_blk_opts_, disk);

static struct configfs_attribute *fblk_attrs[] = {
	&f_blk_opts_attr_timeout,
	&f_blk_opts_attr_queue_depth,
	&f_blk_opts_attr_max_xfer,
	&f_blk_opts_attr_qlen,
	&f_blk_opts_attr_disk,
	NULL,
};

static const struct config_item_type fblk_func_type = {
	.ct_item_ops	= &fblk_item_ops,
	.ct_attrs	= fblk_attrs,
	.ct_owner	= THIS_MODULE,
};

static void fblk_free_inst(struct usb_function_instance *fi)
{
	struct f_blk_opts *opts = container_of(fi, struct f_blk_opts,
					       func_inst);
	struct fblk_dev *dev = opts->dev;

	cancel_work_sync(&dev->attach_work);
	cancel_work_sync(&dev->detach_work);

	mutex_lock(&dev->mutex);
	fblk_remove_disk(dev);
	mutex_unlock(&dev->mutex);
	cancel_delayed_work_sync(&dev->dead_work);

	kfree(opts->interf_group);
	kfree(dev);
	kfree(opts);
}

static struct usb_function_instance *fblk_alloc_inst(void)
{
	struct usb_os_desc *descs[1];
	struct config_group *group;
	struct f_blk_opts *opts;
	struct fblk_dev *dev;
	char *names[1];

	opts = kzalloc_obj(*opts);
	if (!opts)
		return ERR_PTR(-ENOMEM);

	dev = kzalloc_obj(*dev);
	if (!dev) {
		kfree(opts);
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&opts->lock);
	opts->func_inst.free_func_inst = fblk_free_inst;
	opts->timeout = FBLK_DEFAULT_TIMEOUT;
	opts->queue_depth = FBLK_DEFAULT_QUEUE_DEPTH;
	opts->max_xfer = FBLK_DEFAULT_MAX_XFER;
	opts->qlen = FBLK_DEFAULT_QLEN;
	opts->dev = dev;

	dev->opts = opts;
	dev->timeout = opts->timeout;
	spin_lock_init(&dev->lock);
	mutex_init(&dev->mutex);
	init_waitqueue_head(&dev->idle_wq);
	INIT_WORK(&dev->attach_work, fblk_attach_work);
	INIT_WORK(&dev->detach_work, fblk_detach_work);
	INIT_DELAYED_WORK(&dev->dead_work, fblk_dead_work);

	/* let Windows bind WinUSB without a driver package */
	memcpy(opts->ext_compat_id, "WINUSB", 6);
	opts->os_desc.ext_compat_id = opts->ext_compat_id;
	opts->os_desc.opts_mutex = &opts->lock;
	INIT_LIST_HEAD(&opts->os_desc.ext_prop);

	config_group_init_type_name(&opts->func_inst.group, "",
				    &fblk_func_type);

	descs[0] = &opts->os_desc;
	names[0] = "blk";
	group = usb_os_desc_prepare_interf_dir(&opts->func_inst.group, 1,
					       descs, names, THIS_MODULE);
	if (IS_ERR(group)) {
		kfree(dev);
		kfree(opts);
		return ERR_CAST(group);
	}
	opts->interf_group = group;

	return &opts->func_inst;
}

DECLARE_USB_FUNCTION_INIT(blk, fblk_alloc_inst, fblk_alloc_func);
MODULE_DESCRIPTION("USB block device function");
MODULE_LICENSE("GPL");
