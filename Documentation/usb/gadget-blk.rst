.. SPDX-License-Identifier: GPL-2.0

===================================
USB block device function (f_blk)
===================================

Overview
========

The ``blk`` function exposes a block device, ``/dev/usbblkN``, on the gadget
whose storage is provided by a program running on the USB host. It is the
reverse of the mass storage function: the gadget is the one using the disk.

The main use is tethered boot during board bring-up, for example on phones:
the device boots a kernel with a small initramfs and mounts its root file
system from an image that lives on the PC it is plugged into, without
touching on-board storage.

Requests are forwarded to the host over a bulk IN endpoint and completed by
the host over a bulk OUT endpoint. Data is bounced through preallocated
buffers, so the function works with any UDC. The protocol is described in
``include/uapi/linux/usb/f_blk.h``.

The block device belongs to the function instance. It survives the cable
being unplugged, the UDC being unbound and the host program being
restarted:

- While the host is away, new requests are held back. Requests that were
  sent to the host are sent again when the host attaches the same disk
  (identified by a UUID) again.
- Requests fail if the host does not complete them within ``timeout``
  seconds, or does not attach again within ``timeout`` seconds of a
  disconnect. From then on all I/O fails until the host attaches again.
- The disk is removed when the host detaches it, attaches a different disk,
  or when the function instance is removed from configfs.

Kernel configuration
====================

Enable ``CONFIG_USB_CONFIGFS_F_BLK``. The module is called ``usb_f_blk``.

Gadget side
===========

Create a gadget with the function as usual, see
Documentation/usb/gadget_configfs.rst::

  cd /sys/kernel/config/usb_gadget
  mkdir g1 && cd g1
  echo 0x1d6b > idVendor        # Linux Foundation
  echo 0x0104 > idProduct       # Multifunction Composite Gadget
  mkdir strings/0x409
  echo "$(cat /proc/sys/kernel/random/uuid)" > strings/0x409/serialnumber
  echo "USB block device" > strings/0x409/product
  mkdir configs/c.1
  mkdir functions/blk.0
  ln -s functions/blk.0 configs/c.1/

  # Optional: let Windows bind WinUSB without a driver package
  echo 1 > os_desc/use
  echo 0xcd > os_desc/b_vendor_code
  echo MSFT100 > os_desc/qw_sign
  ln -s configs/c.1 os_desc/
  mkdir functions/blk.0/os_desc/interface.blk/DeviceInterfaceGUID
  echo 1 > functions/blk.0/os_desc/interface.blk/DeviceInterfaceGUID/type
  echo "{4c2c7e7b-8b8f-4bd4-9b5f-6f0a5d8b0b1e}" > \
       functions/blk.0/os_desc/interface.blk/DeviceInterfaceGUID/data

  ls /sys/class/udc > UDC

The vendor and product IDs are not used to find the device, host programs
look for the interface (class 0xff, subclass 0x42, protocol 0x01), so any
IDs can be used. The vendor code of the OS descriptors must not be one of
the request codes used by the function (0x01 to 0x03).

Once the host has attached a disk, the ``disk`` attribute contains its
name::

  until [ -n "$(cat functions/blk.0/disk)" ]; do sleep 0.1; done
  mount /dev/$(cat functions/blk.0/disk) /mnt

Attributes
----------

=========== ==================================================================
timeout     Seconds to wait for the host to complete a request, or to attach
            again after a disconnect, before requests fail. 0 waits forever.
            Default 30.
queue_depth Maximum number of outstanding requests. Default 32.
max_xfer    Maximum size of a read or write request in bytes, a multiple of
            4096. Each of the qlen transmit buffers and one receive buffer
            have this size. Default 131072.
qlen        Number of requests that can be queued for transmission to the
            host at once. Default 8.
disk        Read only, name of the block device or empty.
=========== ==================================================================

``queue_depth``, ``max_xfer`` and ``qlen`` can only be changed while the
function is not linked to a configuration and no disk exists.

Booting from the device
-----------------------

The function is set up through configfs, so an initramfs is needed to
create the gadget, wait for the disk and switch to it::

  #!/bin/sh
  mount -t proc proc /proc
  mount -t sysfs sys /sys
  mount -t devtmpfs dev /dev
  mount -t configfs none /sys/kernel/config
  # ... create the gadget as above ...
  F=/sys/kernel/config/usb_gadget/g1/functions/blk.0
  until [ -n "$(cat $F/disk)" ]; do sleep 0.1; done
  mount /dev/$(cat $F/disk)p2 /newroot  # or whatever the layout is
  exec switch_root /newroot /sbin/init

The disk stays usable when the gadget is unbound and bound again, e.g. when
a USB role switch toggles, as long as the function instance exists.

Host side
=========

``tools/usb/usbblk`` contains a minimal reference implementation using
libusb::

  make -C tools/usb/usbblk
  sudo tools/usb/usbblk/usbblk disk.img

It serves one image file or block device, handles one request at a time and
reconnects when the gadget goes away. Requests are served from the image as
is; partition tables and file systems inside the image are seen by the
gadget.

The host program identifies the disk with a UUID, by default derived from
the image path. Restarting the program with the same image resumes I/O
without errors on the gadget, as long as it happens within the timeout.

Limitations
===========

- Data is copied through bounce buffers; throughput is bounded by USB and
  the host program rather than by the copy, but zero copy transfers are not
  implemented.
- One request queue per disk, one disk per function instance.
