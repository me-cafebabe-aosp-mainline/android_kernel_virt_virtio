// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>

#include <video/mipi_display.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct gx_rm67198_amoled {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct gx_rm67198_amoled *to_gx_rm67198_amoled(struct drm_panel *panel)
{
	return container_of(panel, struct gx_rm67198_amoled, panel);
}

static void gx_rm67198_amoled_reset(struct gx_rm67198_amoled *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int gx_rm67198_amoled_on(struct gx_rm67198_amoled *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x4b, 0x4c);
	mipi_dsi_dcs_write_seq(dsi, 0x49, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0xc7, 0x85);
	mipi_dsi_dcs_write_seq(dsi, 0xc8, 0x32);
	mipi_dsi_dcs_write_seq(dsi, 0xc9, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xca, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0xcb, 0x22);
	mipi_dsi_dcs_write_seq(dsi, 0xcc, 0x44);
	mipi_dsi_dcs_write_seq(dsi, 0xcd, 0x11);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x06, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0x08, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x09, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0x0a, 0xe6);
	mipi_dsi_dcs_write_seq(dsi, 0x0b, 0x88);
	mipi_dsi_dcs_write_seq(dsi, 0x20, 0x93);
	mipi_dsi_dcs_write_seq(dsi, 0x21, 0x93);
	mipi_dsi_dcs_write_seq(dsi, 0x24, 0x02);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_GAMMA_CURVE, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x28, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x2a, 0x05);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_LUT, 0x23);
	mipi_dsi_dcs_write_seq(dsi, 0x2f, 0x23);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0x23);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_COLUMNS, 0x23);
	mipi_dsi_dcs_write_seq(dsi, 0x37, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0x39, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x46, 0x1d);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x74, 0x2f);
	mipi_dsi_dcs_write_seq(dsi, 0x75, 0x19);
	mipi_dsi_dcs_write_seq(dsi, 0xad, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x60);
	mipi_dsi_dcs_write_seq(dsi, 0x00, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x01, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x02, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x03, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x04, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x05, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x06, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x07, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x09, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x0a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x0b, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x0c, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x0d, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x0e, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0x0f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x10, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x12, 0xcc);
	mipi_dsi_dcs_write_seq(dsi, 0x13, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x14, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x15, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x16, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x17, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x18, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x19, 0xc0);
	mipi_dsi_dcs_write_seq(dsi, 0x1b, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x1c, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x1d, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x1e, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x1f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x20, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x21, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x22, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x24, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x25, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_GAMMA_CURVE, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x27, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x28, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x29, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0x2a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x2b, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x2f, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_COLUMNS, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x32, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x33, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x34, 0x07);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_ADDRESS_MODE, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x39, 0x00);

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0x02);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x3b, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_3D_CONTROL, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x3f, 0x08);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x41, 0x83);
	mipi_dsi_dcs_write_seq(dsi, 0x4c, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x4d, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x4e, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x4f, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x50, 0x00);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0008);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x64);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x56, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x58, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x59, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x5a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x5b, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x5c, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x5d, 0x64);
	mipi_dsi_dcs_write_seq(dsi, 0x5f, 0xca);
	mipi_dsi_dcs_write_seq(dsi, 0x60, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x61, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x62, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x63, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x64, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x65, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x66, 0xbb);
	mipi_dsi_dcs_write_seq(dsi, 0x67, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x9b, 0x03);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_CONTINUE, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xaa, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xab, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0xac, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0xad, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xae, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0xaf, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb0, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb1, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb2, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb3, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb4, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb5, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb6, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb7, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb8, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xb9, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0xba, 0x0a);
	mipi_dsi_dcs_write_seq(dsi, 0xbb, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0xbc, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0xbd, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0xbe, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0xbf, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xc0, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0xc4, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x70);
	mipi_dsi_dcs_write_seq(dsi, 0x48, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x5a, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x5c, 0xf6);
	mipi_dsi_dcs_write_seq(dsi, 0x5d, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x7d, 0x35);
	mipi_dsi_dcs_write_seq(dsi, 0x86, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0xa7, 0x02);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_READ_PPS_CONTINUE, 0x2c);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0xa0);
	mipi_dsi_dcs_write_seq(dsi, 0x2b, 0x18);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x90);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_GAMMA_CURVE, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0x28, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x2a, 0x40);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_LUT, 0x60);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_PARTIAL_ROWS, 0x70);
	mipi_dsi_dcs_write_seq(dsi, 0x32, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x34, 0x90);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_ADDRESS_MODE, 0x98);
	mipi_dsi_dcs_write_seq(dsi, 0x38, 0xa0);

	ret = mipi_dsi_dcs_set_pixel_format(dsi, 0xc0);
	if (ret < 0) {
		dev_err(dev, "Failed to set pixel format: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_3D_CONTROL, 0xe0);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_VSYNC_TIMING, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0x42, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x43, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x44, 0x40);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_GET_SCANLINE, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x46, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x47, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x48, 0xc0);
	mipi_dsi_dcs_write_seq(dsi, 0x49, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x4a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x4b, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x4c, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x4d, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x4e, 0x80);
	mipi_dsi_dcs_write_seq(dsi, 0x4f, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x50, 0x00);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0003);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x52, 0x80);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x03);
	mipi_dsi_dcs_write_seq(dsi, 0x54, 0x00);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_POWER_SAVE, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x56, 0x8d);
	mipi_dsi_dcs_write_seq(dsi, 0x58, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x59, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x5a, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x5b, 0xbd);
	mipi_dsi_dcs_write_seq(dsi, 0x5c, 0x05);
	mipi_dsi_dcs_write_seq(dsi, 0x5d, 0x63);
	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_SET_CABC_MIN_BRIGHTNESS, 0x06);
	mipi_dsi_dcs_write_seq(dsi, 0x5f, 0x13);
	mipi_dsi_dcs_write_seq(dsi, 0x60, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x61, 0xcd);
	mipi_dsi_dcs_write_seq(dsi, 0x62, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x63, 0x91);
	mipi_dsi_dcs_write_seq(dsi, 0x64, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0x65, 0x60);
	mipi_dsi_dcs_write_seq(dsi, 0x66, 0x09);
	mipi_dsi_dcs_write_seq(dsi, 0x67, 0x38);
	mipi_dsi_dcs_write_seq(dsi, 0x68, 0x0a);
	mipi_dsi_dcs_write_seq(dsi, 0x69, 0x1a);
	mipi_dsi_dcs_write_seq(dsi, 0x6a, 0x0b);
	mipi_dsi_dcs_write_seq(dsi, 0x6b, 0x07);
	mipi_dsi_dcs_write_seq(dsi, 0x6c, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0x6d, 0xfe);
	mipi_dsi_dcs_write_seq(dsi, 0x6e, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0x6f, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x70, 0x0e);
	mipi_dsi_dcs_write_seq(dsi, 0x71, 0x0c);
	mipi_dsi_dcs_write_seq(dsi, 0x72, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x73, 0x96);
	mipi_dsi_dcs_write_seq(dsi, 0x74, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x75, 0xdc);
	mipi_dsi_dcs_write_seq(dsi, 0x76, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x77, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x78, 0x0f);
	mipi_dsi_dcs_write_seq(dsi, 0x79, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x7a, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x7b, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0x7c, 0x01);
	mipi_dsi_dcs_write_seq(dsi, 0x7d, 0x02);
	mipi_dsi_dcs_write_seq(dsi, 0x7e, 0x04);
	mipi_dsi_dcs_write_seq(dsi, 0x7f, 0x08);
	mipi_dsi_dcs_write_seq(dsi, 0x80, 0x10);
	mipi_dsi_dcs_write_seq(dsi, 0x81, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0x82, 0x30);
	mipi_dsi_dcs_write_seq(dsi, 0x83, 0x40);
	mipi_dsi_dcs_write_seq(dsi, 0x84, 0x50);
	mipi_dsi_dcs_write_seq(dsi, 0x85, 0x60);
	mipi_dsi_dcs_write_seq(dsi, 0x86, 0x70);
	mipi_dsi_dcs_write_seq(dsi, 0x87, 0x78);
	mipi_dsi_dcs_write_seq(dsi, 0x88, 0x88);
	mipi_dsi_dcs_write_seq(dsi, 0x89, 0x96);
	mipi_dsi_dcs_write_seq(dsi, 0x8a, 0xa3);
	mipi_dsi_dcs_write_seq(dsi, 0x8b, 0xaf);
	mipi_dsi_dcs_write_seq(dsi, 0x8c, 0xba);
	mipi_dsi_dcs_write_seq(dsi, 0x8d, 0xc4);
	mipi_dsi_dcs_write_seq(dsi, 0x8e, 0xce);
	mipi_dsi_dcs_write_seq(dsi, 0x8f, 0xd7);
	mipi_dsi_dcs_write_seq(dsi, 0x90, 0xe0);
	mipi_dsi_dcs_write_seq(dsi, 0x91, 0xe8);
	mipi_dsi_dcs_write_seq(dsi, 0x92, 0xf0);
	mipi_dsi_dcs_write_seq(dsi, 0x93, 0xf8);
	mipi_dsi_dcs_write_seq(dsi, 0x94, 0xff);
	mipi_dsi_dcs_write_seq(dsi, 0x99, 0x20);
	mipi_dsi_dcs_write_seq(dsi, 0xfe, 0x00);
	mipi_dsi_dcs_write_seq(dsi, 0xc2, 0x09);

	ret = mipi_dsi_dcs_set_tear_on(dsi, MIPI_DSI_DCS_TEAR_MODE_VBLANK);
	if (ret < 0) {
		dev_err(dev, "Failed to set tear on: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x28);

	ret = mipi_dsi_dcs_set_display_brightness(dsi, 0x0028);
	if (ret < 0) {
		dev_err(dev, "Failed to set display brightness: %d\n", ret);
		return ret;
	}

	mipi_dsi_dcs_write_seq(dsi, 0x11, 0x00);
	msleep(120);
	mipi_dsi_dcs_write_seq(dsi, 0x29, 0x00);
	msleep(58);

	return 0;
}

static int gx_rm67198_amoled_off(struct gx_rm67198_amoled *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;
	struct device *dev = &dsi->dev;
	int ret;

	dsi->mode_flags &= ~MIPI_DSI_MODE_LPM;

	ret = mipi_dsi_dcs_set_display_off(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to set display off: %d\n", ret);
		return ret;
	}
	usleep_range(16000, 17000);

	ret = mipi_dsi_dcs_enter_sleep_mode(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to enter sleep mode: %d\n", ret);
		return ret;
	}
	msleep(64);

	return 0;
}

static int gx_rm67198_amoled_prepare(struct drm_panel *panel)
{
	struct gx_rm67198_amoled *ctx = to_gx_rm67198_amoled(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	gx_rm67198_amoled_reset(ctx);

	ret = gx_rm67198_amoled_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int gx_rm67198_amoled_unprepare(struct drm_panel *panel)
{
	struct gx_rm67198_amoled *ctx = to_gx_rm67198_amoled(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = gx_rm67198_amoled_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode gx_rm67198_amoled_mode = {
	.clock = (1080 + 26 + 2 + 36) * (1920 + 8 + 4 + 4) * 60 / 1000,
	.hdisplay = 1080,
	.hsync_start = 1080 + 26,
	.hsync_end = 1080 + 26 + 2,
	.htotal = 1080 + 26 + 2 + 36,
	.vdisplay = 1920,
	.vsync_start = 1920 + 8,
	.vsync_end = 1920 + 8 + 4,
	.vtotal = 1920 + 8 + 4 + 4,
	.width_mm = 68,
	.height_mm = 121,
};

static int gx_rm67198_amoled_get_modes(struct drm_panel *panel,
				       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &gx_rm67198_amoled_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs gx_rm67198_amoled_panel_funcs = {
	.prepare = gx_rm67198_amoled_prepare,
	.unprepare = gx_rm67198_amoled_unprepare,
	.get_modes = gx_rm67198_amoled_get_modes,
};

static int gx_rm67198_amoled_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct gx_rm67198_amoled *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->reset_gpio = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(ctx->reset_gpio))
		return dev_err_probe(dev, PTR_ERR(ctx->reset_gpio),
				     "Failed to get reset-gpios\n");

	ctx->dsi = dsi;
	mipi_dsi_set_drvdata(dsi, ctx);

	dsi->lanes = 4;
	dsi->format = MIPI_DSI_FMT_RGB888;
	dsi->mode_flags = MIPI_DSI_MODE_VIDEO_BURST |
			  MIPI_DSI_MODE_NO_EOT_PACKET |
			  MIPI_DSI_CLOCK_NON_CONTINUOUS;

	drm_panel_init(&ctx->panel, dev, &gx_rm67198_amoled_panel_funcs,
		       DRM_MODE_CONNECTOR_DSI);

	drm_panel_add(&ctx->panel);

	ret = mipi_dsi_attach(dsi);
	if (ret < 0) {
		dev_err(dev, "Failed to attach to DSI host: %d\n", ret);
		drm_panel_remove(&ctx->panel);
		return ret;
	}

	return 0;
}

static void gx_rm67198_amoled_remove(struct mipi_dsi_device *dsi)
{
	struct gx_rm67198_amoled *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id gx_rm67198_amoled_of_match[] = {
	{ .compatible = "xiaomi,uter-gx_rm67198_amoled" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gx_rm67198_amoled_of_match);

static struct mipi_dsi_driver gx_rm67198_amoled_driver = {
	.probe = gx_rm67198_amoled_probe,
	.remove = gx_rm67198_amoled_remove,
	.driver = {
		.name = "panel-gx-rm67198-amoled",
		.of_match_table = gx_rm67198_amoled_of_match,
	},
};
module_mipi_dsi_driver(gx_rm67198_amoled_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for guoxian rm67198 amoled 1080p cmd mode dsi panel");
MODULE_LICENSE("GPL");
