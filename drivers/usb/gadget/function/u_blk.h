/* SPDX-License-Identifier: GPL-2.0 */
/*
 * u_blk.h - interface to the USB block device function
 */

#ifndef U_BLK_H
#define U_BLK_H

#include <linux/mutex.h>
#include <linux/usb/composite.h>

struct fblk_dev;

struct f_blk_opts {
	struct usb_function_instance	func_inst;
	struct fblk_dev			*dev;

	unsigned int			timeout;	/* seconds, 0 = never */
	unsigned int			queue_depth;
	unsigned int			max_xfer;
	unsigned int			qlen;

	struct usb_os_desc		os_desc;
	char				ext_compat_id[16];
	struct config_group		*interf_group;

	/*
	 * Protects the attributes above and refcnt, which counts the
	 * functions allocated from this instance.
	 */
	struct mutex			lock;
	int				refcnt;
};

#endif /* U_BLK_H */
