/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * USB block device gadget function protocol (usb_f_blk)
 *
 * The gadget exposes a block device (/dev/usbblkN) whose contents are
 * served by a program running on the USB host. The gadget forwards block
 * requests to the host over a bulk IN endpoint and receives completions
 * over a bulk OUT endpoint.
 *
 * Control requests (vendor, recipient interface, wIndex = interface number):
 *
 *   USB_BLK_REQ_GET_INFO  device-to-host, returns struct usb_blk_info
 *   USB_BLK_REQ_ATTACH    host-to-device, struct usb_blk_attach
 *   USB_BLK_REQ_DETACH    host-to-device, no data stage
 *
 * Attach and detach are processed asynchronously; the host polls
 * USB_BLK_REQ_GET_INFO to learn the outcome.
 *
 * Bulk messages. Every message is a header transfer, optionally followed
 * by a separate data transfer. Header transfers are always shorter than
 * the endpoint max packet size, data transfers are always a multiple of
 * 512 bytes.
 *
 *   bulk IN:  struct usb_blk_request [+ data for USB_BLK_OP_WRITE]
 *             The data transfer is terminated by a short or zero length
 *             packet.
 *   bulk OUT: struct usb_blk_response [+ data for USB_BLK_OP_READ when
 *             status is USB_BLK_S_OK]
 *             The data transfer is exactly usb_blk_response.len bytes long
 *             and must not be followed by a zero length packet.
 *
 * Responses may be sent in any order. A response is matched to its request
 * by the opaque cookie; responses with unknown or stale cookies are
 * discarded by the gadget (including any data that follows).
 *
 * All multi-byte fields are little endian.
 */

#ifndef _UAPI__LINUX_USB_F_BLK_H
#define _UAPI__LINUX_USB_F_BLK_H

#include <linux/types.h>

#define USB_BLK_VERSION			1

/* Interface descriptor: bInterfaceClass is USB_CLASS_VENDOR_SPEC */
#define USB_BLK_SUBCLASS		0x42
#define USB_BLK_PROTOCOL		0x01

/* Vendor control requests */
#define USB_BLK_REQ_GET_INFO		0x01
#define USB_BLK_REQ_ATTACH		0x02
#define USB_BLK_REQ_DETACH		0x03

/* usb_blk_info.state */
#define USB_BLK_STATE_DETACHED		0  /* no disk attached by the host */
#define USB_BLK_STATE_ATTACHED		1  /* disk attached, requests flow */
#define USB_BLK_STATE_OFFLINE		2  /* disk exists, host must attach */
#define USB_BLK_STATE_FAILED		3  /* last attach was rejected */

/**
 * struct usb_blk_info - gadget capabilities and state
 * @version: USB_BLK_VERSION
 * @queue_depth: maximum number of outstanding requests
 * @max_xfer: maximum data length of a single read or write request in bytes
 * @state: USB_BLK_STATE_*
 * @reserved: reserved, zero
 *
 * The gadget moves from USB_BLK_STATE_ATTACHED to USB_BLK_STATE_OFFLINE on
 * USB disconnect or when the host stops responding to requests. The host
 * should poll the state and attach again when it is not attached.
 */
struct usb_blk_info {
	__le16	version;
	__le16	queue_depth;
	__le32	max_xfer;
	__le32	state;
	__le32	reserved;
};

/* usb_blk_attach.flags */
#define USB_BLK_F_RO			(1 << 0)  /* read only */
#define USB_BLK_F_FLUSH			(1 << 1)  /* volatile write cache */
#define USB_BLK_F_FUA			(1 << 2)  /* USB_BLK_RQ_FUA writes */
#define USB_BLK_F_DISCARD		(1 << 3)  /* USB_BLK_OP_DISCARD */
#define USB_BLK_F_WRITE_ZEROES		(1 << 4)  /* USB_BLK_OP_WRITE_ZEROES */
#define USB_BLK_F_ROTATIONAL		(1 << 5)  /* rotational media */

/**
 * struct usb_blk_attach - describe the disk served by the host
 * @version: USB_BLK_VERSION
 * @reserved0: reserved, zero
 * @flags: USB_BLK_F_* flags
 * @nr_sectors: capacity in 512 byte sectors
 * @logical_block_size: logical block size in bytes, 512 to PAGE_SIZE
 * @physical_block_size: physical block size in bytes, 0 if unknown
 * @max_discard_sectors: discard length limit in sectors, 0 if unlimited
 * @discard_granularity: discard granularity in bytes, 0 if unknown
 * @max_write_zeroes_sectors: write zeroes length limit in sectors, 0 if
 *	unlimited
 * @reserved1: reserved, zero
 * @uuid: identity of the disk
 *
 * Attaching a disk with the same @uuid and block sizes as the current one
 * resumes it: requests that were outstanding are sent again and capacity
 * and the read only flag are updated. Attaching a different disk removes
 * the current one first.
 */
struct usb_blk_attach {
	__le16	version;
	__le16	reserved0;
	__le32	flags;
	__le64	nr_sectors;
	__le32	logical_block_size;
	__le32	physical_block_size;
	__le32	max_discard_sectors;
	__le32	discard_granularity;
	__le32	max_write_zeroes_sectors;
	__le32	reserved1;
	__u8	uuid[16];
};

#define USB_BLK_REQUEST_MAGIC		0x51524255	/* "UBRQ" */
#define USB_BLK_RESPONSE_MAGIC		0x53524255	/* "UBRS" */

/* usb_blk_request.op */
#define USB_BLK_OP_READ			0
#define USB_BLK_OP_WRITE		1
#define USB_BLK_OP_FLUSH		2
#define USB_BLK_OP_DISCARD		3
#define USB_BLK_OP_WRITE_ZEROES		4

/* usb_blk_request.flags */
#define USB_BLK_RQ_FUA			(1 << 0)  /* write: forced unit access */
#define USB_BLK_RQ_NOUNMAP		(1 << 1)  /* write zeroes: keep allocated */

/**
 * struct usb_blk_request - block request, gadget to host
 * @magic: USB_BLK_REQUEST_MAGIC
 * @cookie: opaque, echoed in the response
 * @op: USB_BLK_OP_*
 * @reserved: reserved, zero
 * @flags: USB_BLK_RQ_* flags
 * @len: length in bytes
 * @sector: offset in 512 byte sectors
 */
struct usb_blk_request {
	__le32	magic;
	__le32	cookie;
	__u8	op;
	__u8	reserved;
	__le16	flags;
	__le32	len;
	__le64	sector;
};

/* usb_blk_response.status */
#define USB_BLK_S_OK			0
#define USB_BLK_S_IOERR			1
#define USB_BLK_S_UNSUPP		2
#define USB_BLK_S_NOSPC			3

/**
 * struct usb_blk_response - request completion, host to gadget
 * @magic: USB_BLK_RESPONSE_MAGIC
 * @cookie: cookie of the request being completed
 * @status: USB_BLK_S_*
 * @reserved0: reserved, zero
 * @len: length of the data transfer that follows, zero if none
 * @reserved1: reserved, zero
 */
struct usb_blk_response {
	__le32	magic;
	__le32	cookie;
	__u8	status;
	__u8	reserved0[3];
	__le32	len;
	__le32	reserved1;
};

#endif /* _UAPI__LINUX_USB_F_BLK_H */
