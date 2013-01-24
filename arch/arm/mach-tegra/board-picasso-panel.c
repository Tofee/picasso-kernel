/*
 * arch/arm/mach-tegra/board-picasso-panel.c
 *
 * Copyright (c) 2010-2011, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/resource.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>
#include <linux/earlysuspend.h>
#include <linux/pwm_backlight.h>
#include <linux/nvhost.h>
#include <linux/mm.h>
#include <linux/nvmap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/dc.h>
#include <mach/fb.h>

#include "tegra2_host1x_devices.h"
#include "devices.h"
#include "gpio-names.h"
#include "board.h"

#define picasso_pnl_pwr_enb	TEGRA_GPIO_PC6
#define picasso_bl_enb		TEGRA_GPIO_PD4
#define picasso_lvds_shutdown	TEGRA_GPIO_PB2
#if defined(CONFIG_TEGRA_HDMI)
#define picasso_hdmi_hpd	TEGRA_GPIO_PN7
#endif

#if defined(CONFIG_TEGRA_HDMI)
static struct regulator *picasso_hdmi_reg = NULL;
static struct regulator *picasso_hdmi_pll = NULL;
#endif

static int picasso_backlight_init(struct device *dev) {
	int ret;

	ret = gpio_request(picasso_bl_enb, "backlight_enb");
	if (ret < 0)
		return ret;

	ret = gpio_direction_output(picasso_bl_enb, 1);
	if (ret < 0)
		gpio_free(picasso_bl_enb);
	else
		tegra_gpio_enable(picasso_bl_enb);

	return ret;
};

static void picasso_backlight_exit(struct device *dev) {
	gpio_set_value(picasso_bl_enb, 0);
	gpio_free(picasso_bl_enb);
	tegra_gpio_disable(picasso_bl_enb);
}

static int picasso_backlight_notify(struct device *unused, int brightness)
{
	static int ori_brightness = 0;

	if (ori_brightness != !!brightness) {
		if (!ori_brightness)
			msleep(200);
		gpio_set_value(picasso_bl_enb, !!brightness);
	}

	ori_brightness = !!brightness;
	return brightness;
}

static int picasso_disp1_check_fb(struct device *dev, struct fb_info *info);

static struct platform_pwm_backlight_data picasso_backlight_data = {
	.pwm_id		= 2,
	.max_brightness	= 255,
	.dft_brightness	= 224,
	.pwm_period_ns	= 41667,
	.init		= picasso_backlight_init,
	.exit		= picasso_backlight_exit,
	.notify		= picasso_backlight_notify,
	/* Only toggle backlight on fb blank notifications for disp1 */
	.check_fb   = picasso_disp1_check_fb,
};

static struct platform_device picasso_backlight_device = {
	.name	= "pwm-backlight",
	.id	= -1,
	.dev	= {
		.platform_data = &picasso_backlight_data,
	},
};

static int picasso_panel_enable(void)
{
	struct regulator *reg = regulator_get(NULL, "vdd_ldo4");

	if (!reg) {
		regulator_enable(reg);
		regulator_put(reg);
	}
	gpio_set_value(picasso_pnl_pwr_enb, 1);
	gpio_set_value(picasso_lvds_shutdown, 1);
	return 0;
}

static int picasso_panel_disable(void)
{
	gpio_set_value(picasso_lvds_shutdown, 0);
	gpio_set_value(picasso_pnl_pwr_enb, 0);
	return 0;
}

#if defined(CONFIG_TEGRA_HDMI)
static int picasso_hdmi_enable(void)
{
	if (!picasso_hdmi_reg) {
		picasso_hdmi_reg = regulator_get(NULL, "avdd_hdmi"); /* LD07 */
		if (IS_ERR_OR_NULL(picasso_hdmi_reg)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi\n");
			picasso_hdmi_reg = NULL;
			return PTR_ERR(picasso_hdmi_reg);
		}
	}
	regulator_enable(picasso_hdmi_reg);

	if (!picasso_hdmi_pll) {
		picasso_hdmi_pll = regulator_get(NULL, "avdd_hdmi_pll"); /* LD08 */
		if (IS_ERR_OR_NULL(picasso_hdmi_pll)) {
			pr_err("hdmi: couldn't get regulator avdd_hdmi_pll\n");
			picasso_hdmi_pll = NULL;
			regulator_disable(picasso_hdmi_reg);
			picasso_hdmi_reg = NULL;
			return PTR_ERR(picasso_hdmi_pll);
		}
	}
	regulator_enable(picasso_hdmi_pll);
	return 0;
}

static int picasso_hdmi_disable(void)
{
	regulator_disable(picasso_hdmi_reg);
	regulator_disable(picasso_hdmi_pll);
	return 0;
}
#endif

static struct resource picasso_disp1_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_GENERAL,
		.end	= INT_DISPLAY_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY_BASE,
		.end	= TEGRA_DISPLAY_BASE + TEGRA_DISPLAY_SIZE-1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
};

#if defined(CONFIG_TEGRA_HDMI)
static struct resource picasso_disp2_resources[] = {
	{
		.name	= "irq",
		.start	= INT_DISPLAY_B_GENERAL,
		.end	= INT_DISPLAY_B_GENERAL,
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "regs",
		.start	= TEGRA_DISPLAY2_BASE,
		.end	= TEGRA_DISPLAY2_BASE + TEGRA_DISPLAY2_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "fbmem",
		.flags	= IORESOURCE_MEM,
	},
	{
		.name	= "hdmi_regs",
		.start	= TEGRA_HDMI_BASE,
		.end	= TEGRA_HDMI_BASE + TEGRA_HDMI_SIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
};
#endif

static struct tegra_dc_mode picasso_panel_modes[] = {
	{
#if defined(CONFIG_MACH_PICASSO)
		.pclk = 70500000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 58,
		.v_sync_width = 4,
		.h_back_porch = 58,
		.v_back_porch = 4,
		.h_active = 1280,
		.v_active = 800,
		.h_front_porch = 58,
		.v_front_porch = 4,
#endif
#if defined(CONFIG_MACH_VANGOGH)
		.pclk = 54000000,
		.h_ref_to_sync = 11,
		.v_ref_to_sync = 1,
		.h_sync_width = 20,
		.v_sync_width = 5,
		.h_back_porch = 150,
		.v_back_porch = 5,
		.h_active = 1024,
		.v_active = 600,
		.h_front_porch = 150,
		.v_front_porch = 15,
#endif
	},
};

static struct tegra_fb_data picasso_fb_data = {
	.win		= 0,
#if defined(CONFIG_MACH_PICASSO)
	.xres		= 1280,
	.yres		= 800,
#endif
#if defined(CONFIG_MACH_VANGOGH)
	.xres		= 1024,
	.yres		= 600,
#endif
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};

#if defined(CONFIG_TEGRA_HDMI)
static struct tegra_fb_data picasso_hdmi_fb_data = {
	.win		= 0,
	.xres		= 1280,
	.yres		= 800,
	.bits_per_pixel	= 32,
	.flags		= TEGRA_FB_FLIP_ON_PROBE,
};
#endif

static struct tegra_dc_out picasso_disp1_out = {
	.type		= TEGRA_DC_OUT_RGB,

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,
	.depth		= 18,
	.dither		= TEGRA_DC_ORDERED_DITHER,
	.parent_clk     = "pll_c",

	.modes	 	= picasso_panel_modes,
	.n_modes 	= ARRAY_SIZE(picasso_panel_modes),

	.enable		= picasso_panel_enable,
	.disable	= picasso_panel_disable,
};

#if defined(CONFIG_TEGRA_HDMI)
static struct tegra_dc_out picasso_disp2_out = {
	.type		= TEGRA_DC_OUT_HDMI,
	.flags		= TEGRA_DC_OUT_HOTPLUG_HIGH,

	.dcc_bus	= 1,
	.hotplug_gpio	= picasso_hdmi_hpd,

	.max_pixclock	= KHZ2PICOS(148500),

	.align		= TEGRA_DC_ALIGN_MSB,
	.order		= TEGRA_DC_ORDER_RED_BLUE,

	.enable		= picasso_hdmi_enable,
	.disable	= picasso_hdmi_disable,
};
#endif

static struct tegra_dc_platform_data picasso_disp1_pdata = {
	.flags		= TEGRA_DC_FLAG_ENABLED,
	.default_out	= &picasso_disp1_out,
	.fb		= &picasso_fb_data,
};

#if defined(CONFIG_TEGRA_HDMI)
static struct tegra_dc_platform_data picasso_disp2_pdata = {
	.flags		= 0,
	.default_out	= &picasso_disp2_out,
	.fb		= &picasso_hdmi_fb_data,
};
#endif

static struct nvhost_device picasso_disp1_device = {
	.name		= "tegradc",
	.id		= 0,
	.resource	= picasso_disp1_resources,
	.num_resources	= ARRAY_SIZE(picasso_disp1_resources),
	.dev = {
		.platform_data = &picasso_disp1_pdata,
	},
};

static int picasso_disp1_check_fb(struct device *dev, struct fb_info *info)
{
	return info->device == &picasso_disp1_device.dev;
}

#if defined(CONFIG_TEGRA_HDMI)
static struct nvhost_device picasso_disp2_device = {
	.name		= "tegradc",
	.id		= 1,
	.resource	= picasso_disp2_resources,
	.num_resources	= ARRAY_SIZE(picasso_disp2_resources),
	.dev = {
		.platform_data = &picasso_disp2_pdata,
	},
};
#endif

static struct nvmap_platform_carveout picasso_carveouts[] = {
	[0] = NVMAP_HEAP_CARVEOUT_IRAM_INIT,
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data picasso_nvmap_data = {
	.carveouts	= picasso_carveouts,
	.nr_carveouts	= ARRAY_SIZE(picasso_carveouts),
};

static struct platform_device picasso_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &picasso_nvmap_data,
	},
};

static struct platform_device *picasso_gfx_devices[] __initdata = {
	&picasso_nvmap_device,
//#ifdef CONFIG_TEGRA_GRHOST
//	&tegra_grhost_device,
//#endif
	&tegra_pwfm2_device,
	&picasso_backlight_device,
};

#ifdef CONFIG_HAS_EARLYSUSPEND
/* put early_suspend/late_resume handlers here for the display in order
 * to keep the code out of the display driver, keeping it closer to upstream
 */
struct early_suspend picasso_panel_early_suspender;

static void picasso_panel_early_suspend(struct early_suspend *h)
{
	gpio_set_value(picasso_bl_enb, 0);
	msleep(210);
	/* power down LCD, add use a black screen for HDMI */
	if (num_registered_fb > 0)
		fb_blank(registered_fb[0], FB_BLANK_POWERDOWN);
	if (num_registered_fb > 1)
		fb_blank(registered_fb[1], FB_BLANK_NORMAL);
}

static void picasso_panel_late_resume(struct early_suspend *h)
{
	unsigned i;
	for (i = 0; i < num_registered_fb; i++)
		fb_blank(registered_fb[i], FB_BLANK_UNBLANK);
}
#endif

void picasso_clear_framebuffer(unsigned long base, unsigned long size)
{
	void __iomem *io;

	BUG_ON(PAGE_ALIGN((unsigned long)base) != (unsigned long)base);
	BUG_ON(PAGE_ALIGN(size) != size);

	io = ioremap(base, size);
	if (!io) {
		pr_err("%s: Failed to map framebuffer\n", __func__);
		return;
	}

	memset(io, 0, size);
	iounmap(io);
}

int __init picasso_panel_init(void)
{
	int err;
	struct resource __maybe_unused *res;

	gpio_request(picasso_pnl_pwr_enb, "pnl_pwr_enb");
	gpio_direction_output(picasso_pnl_pwr_enb, 1);
	tegra_gpio_enable(picasso_pnl_pwr_enb);

	gpio_request(picasso_lvds_shutdown, "lvds_shdn");
	gpio_direction_output(picasso_lvds_shutdown, 1);
	tegra_gpio_enable(picasso_lvds_shutdown);

#if defined(CONFIG_TEGRA_HDMI)
	tegra_gpio_enable(picasso_hdmi_hpd);
	gpio_request(picasso_hdmi_hpd, "hdmi_hpd");
	gpio_direction_input(picasso_hdmi_hpd);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	picasso_panel_early_suspender.suspend = picasso_panel_early_suspend;
	picasso_panel_early_suspender.resume = picasso_panel_late_resume;
	picasso_panel_early_suspender.level = EARLY_SUSPEND_LEVEL_DISABLE_FB;
	register_early_suspend(&picasso_panel_early_suspender);
#endif

	picasso_carveouts[1].base = tegra_carveout_start;
	picasso_carveouts[1].size = tegra_carveout_size;

#ifdef CONFIG_TEGRA_GRHOST
	err = tegra2_register_host1x_devices();
	if (err)
		return err;
#endif

	err = platform_add_devices(picasso_gfx_devices,
				   ARRAY_SIZE(picasso_gfx_devices));


	picasso_clear_framebuffer(tegra_fb_start, tegra_fb_size);

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	res = nvhost_get_resource_byname(&picasso_disp1_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb_start;
	res->end = tegra_fb_start + tegra_fb_size - 1;

#if defined(CONFIG_TEGRA_HDMI)
	res = nvhost_get_resource_byname(&picasso_disp2_device,
		IORESOURCE_MEM, "fbmem");
	res->start = tegra_fb2_start;
	res->end = tegra_fb2_start + tegra_fb2_size - 1;
#endif
#endif

	/* Copy the bootloader fb to the fb. */
//	tegra_move_framebuffer(tegra_fb_start, tegra_bootloader_fb_start,
//		min(tegra_fb_size, tegra_bootloader_fb_size));

#if defined(CONFIG_TEGRA_GRHOST) && defined(CONFIG_TEGRA_DC)
	if (!err)
		err = nvhost_device_register(&picasso_disp1_device);

#if defined(CONFIG_TEGRA_HDMI)
	if (!err)
		err = nvhost_device_register(&picasso_disp2_device);
#endif
#endif

	return err;
}
