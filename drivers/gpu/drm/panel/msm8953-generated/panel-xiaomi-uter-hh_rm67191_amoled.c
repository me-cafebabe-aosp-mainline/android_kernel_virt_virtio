// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2023 FIXME
// Generated with linux-mdss-dsi-panel-driver-generator from vendor device tree:
//   Copyright (c) 2013, The Linux Foundation. All rights reserved. (FIXME)

#include <linux/delay.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>

struct hh_rm67191_amoled {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	struct regulator_bulk_data supplies[2];
	struct gpio_desc *reset_gpio;
	bool prepared;
};

static inline
struct hh_rm67191_amoled *to_hh_rm67191_amoled(struct drm_panel *panel)
{
	return container_of(panel, struct hh_rm67191_amoled, panel);
}

#define dsi_generic_write_seq(dsi, seq...) do {				\
		static const u8 d[] = { seq };				\
		int ret;						\
		ret = mipi_dsi_generic_write(dsi, d, ARRAY_SIZE(d));	\
		if (ret < 0)						\
			return ret;					\
	} while (0)

static void hh_rm67191_amoled_reset(struct hh_rm67191_amoled *ctx)
{
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	msleep(20);
	gpiod_set_value_cansleep(ctx->reset_gpio, 0);
	usleep_range(10000, 11000);
}

static int hh_rm67191_amoled_on(struct hh_rm67191_amoled *ctx)
{
	struct mipi_dsi_device *dsi = ctx->dsi;

	dsi->mode_flags |= MIPI_DSI_MODE_LPM;

	dsi_generic_write_seq(dsi, 0xfe, 0x0b);
	dsi_generic_write_seq(dsi, 0x28, 0x40);
	dsi_generic_write_seq(dsi, 0x29, 0x4f);
	dsi_generic_write_seq(dsi, 0xfe, 0x0e);
	dsi_generic_write_seq(dsi, 0x4b, 0x00);
	dsi_generic_write_seq(dsi, 0x4c, 0x0f);
	dsi_generic_write_seq(dsi, 0x4d, 0x20);
	dsi_generic_write_seq(dsi, 0x4e, 0x40);
	dsi_generic_write_seq(dsi, 0x4f, 0x60);
	dsi_generic_write_seq(dsi, 0x50, 0xa0);
	dsi_generic_write_seq(dsi, 0x51, 0xc0);
	dsi_generic_write_seq(dsi, 0x52, 0xe0);
	dsi_generic_write_seq(dsi, 0x53, 0xff);
	dsi_generic_write_seq(dsi, 0xfe, 0x0d);
	dsi_generic_write_seq(dsi, 0x18, 0x08);
	dsi_generic_write_seq(dsi, 0x42, 0x00);
	dsi_generic_write_seq(dsi, 0x08, 0x41);
	dsi_generic_write_seq(dsi, 0x46, 0x02);
	dsi_generic_write_seq(dsi, 0x72, 0x09);
	dsi_generic_write_seq(dsi, 0xfe, 0x0a);
	dsi_generic_write_seq(dsi, 0x24, 0x17);
	dsi_generic_write_seq(dsi, 0x04, 0x07);
	dsi_generic_write_seq(dsi, 0x1a, 0x0c);
	dsi_generic_write_seq(dsi, 0x0f, 0x44);
	dsi_generic_write_seq(dsi, 0xfe, 0x04);
	dsi_generic_write_seq(dsi, 0x00, 0x0c);
	dsi_generic_write_seq(dsi, 0x05, 0x08);
	dsi_generic_write_seq(dsi, 0x06, 0x08);
	dsi_generic_write_seq(dsi, 0x08, 0x08);
	dsi_generic_write_seq(dsi, 0x09, 0x08);
	dsi_generic_write_seq(dsi, 0x0a, 0xe6);
	dsi_generic_write_seq(dsi, 0x0b, 0x8c);
	dsi_generic_write_seq(dsi, 0x1a, 0x12);
	dsi_generic_write_seq(dsi, 0x1e, 0xe0);
	dsi_generic_write_seq(dsi, 0x29, 0x93);
	dsi_generic_write_seq(dsi, 0x2a, 0x93);
	dsi_generic_write_seq(dsi, 0x2f, 0x02);
	dsi_generic_write_seq(dsi, 0x31, 0x02);
	dsi_generic_write_seq(dsi, 0x33, 0x05);
	dsi_generic_write_seq(dsi, 0x37, 0x2d);
	dsi_generic_write_seq(dsi, 0x38, 0x2d);
	dsi_generic_write_seq(dsi, 0x3a, 0x1e);
	dsi_generic_write_seq(dsi, 0x3b, 0x1e);
	dsi_generic_write_seq(dsi, 0x3d, 0x27);
	dsi_generic_write_seq(dsi, 0x3f, 0x80);
	dsi_generic_write_seq(dsi, 0x40, 0x40);
	dsi_generic_write_seq(dsi, 0x41, 0xe0);
	dsi_generic_write_seq(dsi, 0x4f, 0x2f);
	dsi_generic_write_seq(dsi, 0x50, 0x1e);
	dsi_generic_write_seq(dsi, 0xfe, 0x06);
	dsi_generic_write_seq(dsi, 0x00, 0xcc);
	dsi_generic_write_seq(dsi, 0x05, 0x05);
	dsi_generic_write_seq(dsi, 0x07, 0xa2);
	dsi_generic_write_seq(dsi, 0x08, 0xcc);
	dsi_generic_write_seq(dsi, 0x0d, 0x03);
	dsi_generic_write_seq(dsi, 0x0f, 0xa2);
	dsi_generic_write_seq(dsi, 0x32, 0xcc);
	dsi_generic_write_seq(dsi, 0x37, 0x05);
	dsi_generic_write_seq(dsi, 0x39, 0x83);
	dsi_generic_write_seq(dsi, 0x3a, 0xcc);
	dsi_generic_write_seq(dsi, 0x41, 0x04);
	dsi_generic_write_seq(dsi, 0x43, 0x83);
	dsi_generic_write_seq(dsi, 0x44, 0xcc);
	dsi_generic_write_seq(dsi, 0x49, 0x05);
	dsi_generic_write_seq(dsi, 0x4b, 0xa2);
	dsi_generic_write_seq(dsi, 0x4c, 0xcc);
	dsi_generic_write_seq(dsi, 0x51, 0x03);
	dsi_generic_write_seq(dsi, 0x53, 0xa2);
	dsi_generic_write_seq(dsi, 0x75, 0xcc);
	dsi_generic_write_seq(dsi, 0x7a, 0x03);
	dsi_generic_write_seq(dsi, 0x7c, 0x83);
	dsi_generic_write_seq(dsi, 0x7d, 0xcc);
	dsi_generic_write_seq(dsi, 0x82, 0x02);
	dsi_generic_write_seq(dsi, 0x84, 0x83);
	dsi_generic_write_seq(dsi, 0x85, 0xec);
	dsi_generic_write_seq(dsi, 0x86, 0x0f);
	dsi_generic_write_seq(dsi, 0x87, 0xff);
	dsi_generic_write_seq(dsi, 0x88, 0x00);
	dsi_generic_write_seq(dsi, 0x8a, 0x02);
	dsi_generic_write_seq(dsi, 0x8c, 0xa2);
	dsi_generic_write_seq(dsi, 0x8d, 0xea);
	dsi_generic_write_seq(dsi, 0x8e, 0x01);
	dsi_generic_write_seq(dsi, 0x8f, 0xe8);
	dsi_generic_write_seq(dsi, 0xfe, 0x06);
	dsi_generic_write_seq(dsi, 0x90, 0x0a);
	dsi_generic_write_seq(dsi, 0x92, 0x06);
	dsi_generic_write_seq(dsi, 0x93, 0xa0);
	dsi_generic_write_seq(dsi, 0x94, 0xa8);
	dsi_generic_write_seq(dsi, 0x95, 0xec);
	dsi_generic_write_seq(dsi, 0x96, 0x0f);
	dsi_generic_write_seq(dsi, 0x97, 0xff);
	dsi_generic_write_seq(dsi, 0x98, 0x00);
	dsi_generic_write_seq(dsi, 0x9a, 0x02);
	dsi_generic_write_seq(dsi, 0x9c, 0xa2);
	dsi_generic_write_seq(dsi, 0xac, 0x04);
	dsi_generic_write_seq(dsi, 0xfe, 0x06);
	dsi_generic_write_seq(dsi, 0xb1, 0x12);
	dsi_generic_write_seq(dsi, 0xb2, 0x17);
	dsi_generic_write_seq(dsi, 0xb3, 0x17);
	dsi_generic_write_seq(dsi, 0xb4, 0x17);
	dsi_generic_write_seq(dsi, 0xb5, 0x17);
	dsi_generic_write_seq(dsi, 0xb6, 0x11);
	dsi_generic_write_seq(dsi, 0xb7, 0x08);
	dsi_generic_write_seq(dsi, 0xb8, 0x09);
	dsi_generic_write_seq(dsi, 0xb9, 0x06);
	dsi_generic_write_seq(dsi, 0xba, 0x07);
	dsi_generic_write_seq(dsi, 0xbb, 0x17);
	dsi_generic_write_seq(dsi, 0xbc, 0x17);
	dsi_generic_write_seq(dsi, 0xbd, 0x17);
	dsi_generic_write_seq(dsi, 0xbe, 0x17);
	dsi_generic_write_seq(dsi, 0xbf, 0x17);
	dsi_generic_write_seq(dsi, 0xc0, 0x17);
	dsi_generic_write_seq(dsi, 0xc1, 0x17);
	dsi_generic_write_seq(dsi, 0xc2, 0x17);
	dsi_generic_write_seq(dsi, 0xc3, 0x17);
	dsi_generic_write_seq(dsi, 0xc4, 0x0f);
	dsi_generic_write_seq(dsi, 0xc5, 0x0e);
	dsi_generic_write_seq(dsi, 0xc6, 0x00);
	dsi_generic_write_seq(dsi, 0xc7, 0x01);
	dsi_generic_write_seq(dsi, 0xc8, 0x10);
	dsi_generic_write_seq(dsi, 0xfe, 0x06);
	dsi_generic_write_seq(dsi, 0x95, 0xec);
	dsi_generic_write_seq(dsi, 0x8d, 0xee);
	dsi_generic_write_seq(dsi, 0x44, 0xec);
	dsi_generic_write_seq(dsi, 0x4c, 0xec);
	dsi_generic_write_seq(dsi, 0x32, 0xec);
	dsi_generic_write_seq(dsi, 0x3a, 0xec);
	dsi_generic_write_seq(dsi, 0x7d, 0xec);
	dsi_generic_write_seq(dsi, 0x75, 0xec);
	dsi_generic_write_seq(dsi, 0x00, 0xec);
	dsi_generic_write_seq(dsi, 0x08, 0xec);
	dsi_generic_write_seq(dsi, 0x85, 0xec);
	dsi_generic_write_seq(dsi, 0xa6, 0x21);
	dsi_generic_write_seq(dsi, 0xa7, 0x05);
	dsi_generic_write_seq(dsi, 0xa9, 0x06);
	dsi_generic_write_seq(dsi, 0x82, 0x06);
	dsi_generic_write_seq(dsi, 0x41, 0x06);
	dsi_generic_write_seq(dsi, 0x7a, 0x07);
	dsi_generic_write_seq(dsi, 0x37, 0x07);
	dsi_generic_write_seq(dsi, 0x05, 0x06);
	dsi_generic_write_seq(dsi, 0x49, 0x06);
	dsi_generic_write_seq(dsi, 0x0d, 0x04);
	dsi_generic_write_seq(dsi, 0x51, 0x04);
	dsi_generic_write_seq(dsi, 0xfe, 0x00);
	dsi_generic_write_seq(dsi, 0xc2, 0x08);
	dsi_generic_write_seq(dsi, 0x35, 0x00);
	dsi_generic_write_seq(dsi, 0x44, 0x03, 0x80);
	dsi_generic_write_seq(dsi, 0x51, 0x28);
	dsi_generic_write_seq(dsi, 0x11, 0x00);
	msleep(120);
	dsi_generic_write_seq(dsi, 0x29, 0x00);
	msleep(120);

	return 0;
}

static int hh_rm67191_amoled_off(struct hh_rm67191_amoled *ctx)
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

static int hh_rm67191_amoled_prepare(struct drm_panel *panel)
{
	struct hh_rm67191_amoled *ctx = to_hh_rm67191_amoled(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (ctx->prepared)
		return 0;

	ret = regulator_bulk_enable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
	if (ret < 0) {
		dev_err(dev, "Failed to enable regulators: %d\n", ret);
		return ret;
	}

	hh_rm67191_amoled_reset(ctx);

	ret = hh_rm67191_amoled_on(ctx);
	if (ret < 0) {
		dev_err(dev, "Failed to initialize panel: %d\n", ret);
		gpiod_set_value_cansleep(ctx->reset_gpio, 1);
		regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);
		return ret;
	}

	ctx->prepared = true;
	return 0;
}

static int hh_rm67191_amoled_unprepare(struct drm_panel *panel)
{
	struct hh_rm67191_amoled *ctx = to_hh_rm67191_amoled(panel);
	struct device *dev = &ctx->dsi->dev;
	int ret;

	if (!ctx->prepared)
		return 0;

	ret = hh_rm67191_amoled_off(ctx);
	if (ret < 0)
		dev_err(dev, "Failed to un-initialize panel: %d\n", ret);

	gpiod_set_value_cansleep(ctx->reset_gpio, 1);
	regulator_bulk_disable(ARRAY_SIZE(ctx->supplies), ctx->supplies);

	ctx->prepared = false;
	return 0;
}

static const struct drm_display_mode hh_rm67191_amoled_mode = {
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

static int hh_rm67191_amoled_get_modes(struct drm_panel *panel,
				       struct drm_connector *connector)
{
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, &hh_rm67191_amoled_mode);
	if (!mode)
		return -ENOMEM;

	drm_mode_set_name(mode);

	mode->type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED;
	connector->display_info.width_mm = mode->width_mm;
	connector->display_info.height_mm = mode->height_mm;
	drm_mode_probed_add(connector, mode);

	return 1;
}

static const struct drm_panel_funcs hh_rm67191_amoled_panel_funcs = {
	.prepare = hh_rm67191_amoled_prepare,
	.unprepare = hh_rm67191_amoled_unprepare,
	.get_modes = hh_rm67191_amoled_get_modes,
};

static int hh_rm67191_amoled_probe(struct mipi_dsi_device *dsi)
{
	struct device *dev = &dsi->dev;
	struct hh_rm67191_amoled *ctx;
	int ret;

	ctx = devm_kzalloc(dev, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->supplies[0].supply = "vsn";
	ctx->supplies[1].supply = "vsp";
	ret = devm_regulator_bulk_get(dev, ARRAY_SIZE(ctx->supplies),
				      ctx->supplies);
	if (ret < 0)
		return dev_err_probe(dev, ret, "Failed to get regulators\n");

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

	drm_panel_init(&ctx->panel, dev, &hh_rm67191_amoled_panel_funcs,
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

static void hh_rm67191_amoled_remove(struct mipi_dsi_device *dsi)
{
	struct hh_rm67191_amoled *ctx = mipi_dsi_get_drvdata(dsi);
	int ret;

	ret = mipi_dsi_detach(dsi);
	if (ret < 0)
		dev_err(&dsi->dev, "Failed to detach from DSI host: %d\n", ret);

	drm_panel_remove(&ctx->panel);
}

static const struct of_device_id hh_rm67191_amoled_of_match[] = {
	{ .compatible = "xiaomi,uter-hh_rm67191_amoled" }, // FIXME
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, hh_rm67191_amoled_of_match);

static struct mipi_dsi_driver hh_rm67191_amoled_driver = {
	.probe = hh_rm67191_amoled_probe,
	.remove = hh_rm67191_amoled_remove,
	.driver = {
		.name = "panel-hh-rm67191-amoled",
		.of_match_table = hh_rm67191_amoled_of_match,
	},
};
module_mipi_dsi_driver(hh_rm67191_amoled_driver);

MODULE_AUTHOR("linux-mdss-dsi-panel-driver-generator <fix@me>"); // FIXME
MODULE_DESCRIPTION("DRM driver for hehui rm67191 amoled 1080p cmd mode dsi panel");
MODULE_LICENSE("GPL v2");
