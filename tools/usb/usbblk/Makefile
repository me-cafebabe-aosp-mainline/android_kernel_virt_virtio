# SPDX-License-Identifier: GPL-2.0
CC ?= gcc
PKG_CONFIG ?= pkg-config
LIBUSB_CFLAGS := $(shell $(PKG_CONFIG) --cflags libusb-1.0)
LIBUSB_LIBS := $(shell $(PKG_CONFIG) --libs libusb-1.0)
# -idirafter: use the system <linux/types.h>, find <linux/usb/f_blk.h>
# in the kernel tree unless the installed headers already have it
CFLAGS += -O2 -Wall -Wextra -idirafter ../../../include/uapi $(LIBUSB_CFLAGS)
LDLIBS += $(LIBUSB_LIBS)

all: usbblk

usbblk: usbblk.c ../../../include/uapi/linux/usb/f_blk.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)

clean:
	$(RM) usbblk

.PHONY: all clean
