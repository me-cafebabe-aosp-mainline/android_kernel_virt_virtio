// SPDX-License-Identifier: GPL-2.0
/*
 * usbblk - serve a disk image to a USB block device gadget (usb_f_blk)
 *
 * Minimal reference implementation of the host side of the protocol in
 * include/uapi/linux/usb/f_blk.h. Requests are handled one at a time.
 */

#define _GNU_SOURCE
#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/fs.h>
#include <linux/usb/f_blk.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libusb.h>

#define CTRL_TIMEOUT	1000	/* ms */
#define BULK_TIMEOUT	1000	/* ms */

struct gadget {
	libusb_device_handle	*handle;
	int			intf;
	unsigned char		ep_in;
	unsigned char		ep_out;
	unsigned int		max_xfer;
};

static const char *image;
static int img_fd = -1;
static uint64_t img_size;
static bool read_only;
static bool verbose;
static unsigned char uuid[16];
static volatile sig_atomic_t stop;

static void on_signal(int sig)
{
	(void)sig;
	stop = 1;
}

static void msleep(unsigned int ms)
{
	struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };

	nanosleep(&ts, NULL);
}

/* derive a stable disk identity from the image path */
static void make_uuid(const char *path)
{
	char buf[PATH_MAX];
	uint64_t h[2] = { 0xcbf29ce484222325ULL, 0x84222325cbf29ce4ULL };
	const char *p = realpath(path, buf) ? buf : path;
	int i;

	for (; *p; p++) {
		for (i = 0; i < 2; i++) {
			h[i] ^= (unsigned char)*p;
			h[i] *= 0x100000001b3ULL;
		}
	}
	memcpy(uuid, h, sizeof(uuid));
	uuid[6] = (uuid[6] & 0x0f) | 0x80;	/* custom (v8) */
	uuid[8] = (uuid[8] & 0x3f) | 0x80;
}

static int parse_uuid(const char *s)
{
	int i;

	for (i = 0; i < 16; i++) {
		while (*s == '-')
			s++;
		if (sscanf(s, "%2hhx", &uuid[i]) != 1)
			return -1;
		s += 2;
	}
	return *s ? -1 : 0;
}

static int find_gadget(libusb_context *ctx, struct gadget *g)
{
	libusb_device **list;
	ssize_t n, i;
	int ret = -1;

	n = libusb_get_device_list(ctx, &list);
	if (n < 0)
		return -1;

	for (i = 0; i < n && ret; i++) {
		struct libusb_config_descriptor *cfg;
		int j;

		if (libusb_get_active_config_descriptor(list[i], &cfg))
			continue;

		for (j = 0; j < cfg->bNumInterfaces && ret; j++) {
			const struct libusb_interface_descriptor *id =
				&cfg->interface[j].altsetting[0];
			int k;

			if (id->bInterfaceClass != LIBUSB_CLASS_VENDOR_SPEC ||
			    id->bInterfaceSubClass != USB_BLK_SUBCLASS ||
			    id->bInterfaceProtocol != USB_BLK_PROTOCOL ||
			    id->bNumEndpoints < 2)
				continue;

			g->intf = id->bInterfaceNumber;
			g->ep_in = 0;
			g->ep_out = 0;
			for (k = 0; k < id->bNumEndpoints; k++) {
				const struct libusb_endpoint_descriptor *ep =
					&id->endpoint[k];

				if ((ep->bmAttributes & 3) !=
				    LIBUSB_TRANSFER_TYPE_BULK)
					continue;
				if (ep->bEndpointAddress & LIBUSB_ENDPOINT_IN)
					g->ep_in = ep->bEndpointAddress;
				else
					g->ep_out = ep->bEndpointAddress;
			}
			if (!g->ep_in || !g->ep_out)
				continue;

			if (libusb_open(list[i], &g->handle))
				continue;
			libusb_set_auto_detach_kernel_driver(g->handle, 1);
			if (libusb_claim_interface(g->handle, g->intf)) {
				libusb_close(g->handle);
				g->handle = NULL;
				continue;
			}
			ret = 0;
		}
		libusb_free_config_descriptor(cfg);
	}

	libusb_free_device_list(list, 1);
	return ret;
}

static int get_info(struct gadget *g, struct usb_blk_info *info)
{
	int ret;

	ret = libusb_control_transfer(g->handle,
				      LIBUSB_ENDPOINT_IN |
				      LIBUSB_REQUEST_TYPE_VENDOR |
				      LIBUSB_RECIPIENT_INTERFACE,
				      USB_BLK_REQ_GET_INFO, 0, g->intf,
				      (unsigned char *)info, sizeof(*info),
				      CTRL_TIMEOUT);
	if (ret != sizeof(*info))
		return ret < 0 ? ret : -1;
	return 0;
}

static int attach(struct gadget *g)
{
	struct usb_blk_attach a = { 0 };
	struct usb_blk_info info;
	uint32_t flags = USB_BLK_F_FLUSH;
	int ret, i;

	if (read_only)
		flags |= USB_BLK_F_RO;
	else
		flags |= USB_BLK_F_DISCARD | USB_BLK_F_WRITE_ZEROES;

	a.version = htole16(USB_BLK_VERSION);
	a.flags = htole32(flags);
	a.nr_sectors = htole64(img_size / 512);
	a.logical_block_size = htole32(512);
	memcpy(a.uuid, uuid, sizeof(uuid));

	ret = libusb_control_transfer(g->handle,
				      LIBUSB_ENDPOINT_OUT |
				      LIBUSB_REQUEST_TYPE_VENDOR |
				      LIBUSB_RECIPIENT_INTERFACE,
				      USB_BLK_REQ_ATTACH, 0, g->intf,
				      (unsigned char *)&a, sizeof(a),
				      CTRL_TIMEOUT);
	if (ret != sizeof(a))
		return -1;

	for (i = 0; i < 50; i++) {
		if (get_info(g, &info))
			return -1;
		switch (le32toh(info.state)) {
		case USB_BLK_STATE_ATTACHED:
			return 0;
		case USB_BLK_STATE_FAILED:
			fprintf(stderr, "gadget rejected the disk\n");
			return -1;
		}
		msleep(100);
	}
	fprintf(stderr, "timed out attaching\n");
	return -1;
}

static int full_pwrite(const void *buf, size_t len, off_t off)
{
	while (len) {
		ssize_t n = pwrite(img_fd, buf, len, off);

		if (n < 0 && errno == EINTR)
			continue;
		if (n <= 0)
			return -1;
		buf = (const char *)buf + n;
		len -= n;
		off += n;
	}
	return 0;
}

static int full_pread(void *buf, size_t len, off_t off)
{
	while (len) {
		ssize_t n = pread(img_fd, buf, len, off);

		if (n < 0 && errno == EINTR)
			continue;
		if (n < 0)
			return -1;
		if (n == 0) {	/* past EOF of a file being shrunk */
			memset(buf, 0, len);
			return 0;
		}
		buf = (char *)buf + n;
		len -= n;
		off += n;
	}
	return 0;
}

static int punch(uint64_t off, uint64_t len, bool zero_only)
{
	struct stat st;
	uint64_t range[2] = { off, len };

	if (fstat(img_fd, &st))
		return -1;
	if (S_ISBLK(st.st_mode))
		return ioctl(img_fd, zero_only ? BLKZEROOUT : BLKDISCARD,
			     range);
	return fallocate(img_fd, zero_only ? FALLOC_FL_ZERO_RANGE :
			 FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE,
			 off, len);
}

/* returns USB_BLK_S_* */
static int handle(const struct usb_blk_request *rq, void *data)
{
	uint64_t off = le64toh(rq->sector) * 512;
	uint32_t len = le32toh(rq->len);

	if (rq->op != USB_BLK_OP_FLUSH &&
	    (off > img_size || len > img_size - off))
		return USB_BLK_S_IOERR;

	switch (rq->op) {
	case USB_BLK_OP_READ:
		return full_pread(data, len, off) ? USB_BLK_S_IOERR :
						    USB_BLK_S_OK;
	case USB_BLK_OP_WRITE:
		if (read_only)
			return USB_BLK_S_IOERR;
		if (full_pwrite(data, len, off))
			return errno == ENOSPC ? USB_BLK_S_NOSPC :
						 USB_BLK_S_IOERR;
		if ((le16toh(rq->flags) & USB_BLK_RQ_FUA) && fdatasync(img_fd))
			return USB_BLK_S_IOERR;
		return USB_BLK_S_OK;
	case USB_BLK_OP_FLUSH:
		return fdatasync(img_fd) ? USB_BLK_S_IOERR : USB_BLK_S_OK;
	case USB_BLK_OP_DISCARD:
		if (read_only)
			return USB_BLK_S_IOERR;
		return punch(off, len, false) ? USB_BLK_S_IOERR : USB_BLK_S_OK;
	case USB_BLK_OP_WRITE_ZEROES:
		if (read_only)
			return USB_BLK_S_IOERR;
		if (!punch(off, len, !!(le16toh(rq->flags) &
					USB_BLK_RQ_NOUNMAP)))
			return USB_BLK_S_OK;
		/* fall back to writing zeroes */
		memset(data, 0, 65536);
		while (len) {
			uint32_t n = len < 65536 ? len : 65536;

			if (full_pwrite(data, n, off))
				return USB_BLK_S_IOERR;
			off += n;
			len -= n;
		}
		return USB_BLK_S_OK;
	default:
		return USB_BLK_S_UNSUPP;
	}
}

static int bulk(struct gadget *g, unsigned char ep, void *buf, int len,
		int *actual)
{
	return libusb_bulk_transfer(g->handle, ep, buf, len, actual,
				    BULK_TIMEOUT);
}

/* serve requests until the gadget goes away */
static int serve(struct gadget *g, unsigned char *buf)
{
	struct usb_blk_request rq;
	struct usb_blk_response resp;
	struct usb_blk_info info;
	int actual, ret, status;
	uint32_t len;

	while (!stop) {
		ret = bulk(g, g->ep_in, buf, g->max_xfer, &actual);
		if (ret == LIBUSB_ERROR_TIMEOUT) {
			/* idle, make sure the gadget still has us attached */
			if (get_info(g, &info))
				return -1;
			if (le32toh(info.state) != USB_BLK_STATE_ATTACHED &&
			    attach(g))
				return -1;
			continue;
		}
		if (ret)
			return -1;

		memcpy(&rq, buf, sizeof(rq));
		if (actual != sizeof(rq) ||
		    le32toh(rq.magic) != USB_BLK_REQUEST_MAGIC) {
			if (verbose)
				fprintf(stderr, "skipping %d bytes\n", actual);
			continue;
		}
		len = le32toh(rq.len);

		if (rq.op == USB_BLK_OP_WRITE && len) {
			if (len > g->max_xfer)
				return -1;
			ret = bulk(g, g->ep_in, buf, g->max_xfer, &actual);
			if (ret || (uint32_t)actual != len)
				return -1;
		}

		if (rq.op == USB_BLK_OP_READ && len > g->max_xfer)
			status = USB_BLK_S_IOERR;
		else
			status = handle(&rq, buf);

		if (verbose)
			fprintf(stderr, "op %u sector %llu len %u -> %d\n",
				rq.op, (unsigned long long)le64toh(rq.sector),
				len, status);

		memset(&resp, 0, sizeof(resp));
		resp.magic = htole32(USB_BLK_RESPONSE_MAGIC);
		resp.cookie = rq.cookie;
		resp.status = status;
		if (rq.op == USB_BLK_OP_READ && status == USB_BLK_S_OK)
			resp.len = htole32(len);

		ret = bulk(g, g->ep_out, &resp, sizeof(resp), &actual);
		if (ret)
			return -1;
		if (resp.len) {
			ret = bulk(g, g->ep_out, buf, len, &actual);
			if (ret)
				return -1;
		}
	}

	return 0;
}

static void usage(const char *prog)
{
	fprintf(stderr,
		"usage: %s [-r] [-u uuid] [-v] image\n"
		"  -r       export read only\n"
		"  -u uuid  disk identity (default: derived from image path)\n"
		"  -v       log every request\n", prog);
	exit(1);
}

int main(int argc, char **argv)
{
	libusb_context *ctx;
	struct usb_blk_info info;
	struct stat st;
	unsigned char *buf = NULL;
	bool have_uuid = false;
	int opt;

	while ((opt = getopt(argc, argv, "ru:v")) != -1) {
		switch (opt) {
		case 'r':
			read_only = true;
			break;
		case 'u':
			if (parse_uuid(optarg))
				usage(argv[0]);
			have_uuid = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage(argv[0]);
		}
	}
	if (optind != argc - 1)
		usage(argv[0]);
	image = argv[optind];

	img_fd = open(image, read_only ? O_RDONLY : O_RDWR);
	if (img_fd < 0 || fstat(img_fd, &st)) {
		perror(image);
		return 1;
	}
	if (S_ISBLK(st.st_mode)) {
		if (ioctl(img_fd, BLKGETSIZE64, &img_size)) {
			perror(image);
			return 1;
		}
	} else {
		img_size = st.st_size;
	}
	if (img_size < 512) {
		fprintf(stderr, "%s: too small\n", image);
		return 1;
	}
	if (!have_uuid)
		make_uuid(image);

	signal(SIGINT, on_signal);
	signal(SIGTERM, on_signal);

	while (!stop) {
		struct gadget g = { 0 };

		/*
		 * A fresh context per attempt, so devices show up even when
		 * libusb gets no hotplug events (e.g. no udev daemon).
		 */
		if (libusb_init(&ctx)) {
			fprintf(stderr, "libusb_init failed\n");
			return 1;
		}
		if (find_gadget(ctx, &g)) {
			libusb_exit(ctx);
			msleep(500);
			continue;
		}

		if (get_info(&g, &info) ||
		    le16toh(info.version) != USB_BLK_VERSION) {
			fprintf(stderr, "unsupported gadget\n");
			goto close;
		}
		g.max_xfer = le32toh(info.max_xfer);
		free(buf);
		buf = malloc(g.max_xfer < 65536 ? 65536 : g.max_xfer);
		if (!buf)
			return 1;

		if (attach(&g))
			goto close;
		fprintf(stderr, "serving %s (%llu bytes)\n", image,
			(unsigned long long)img_size);

		serve(&g, buf);
		if (!stop)
			fprintf(stderr, "gadget lost, waiting\n");
close:
		libusb_release_interface(g.handle, g.intf);
		libusb_close(g.handle);
		libusb_exit(ctx);
		if (!stop)
			msleep(500);
	}

	free(buf);
	return 0;
}
