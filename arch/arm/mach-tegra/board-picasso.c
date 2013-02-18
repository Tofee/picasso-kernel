/*
 * arch/arm/mach-tegra/board-picasso.c
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/ctype.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/serial_8250.h>
#include <linux/i2c.h>
#include <linux/i2c/panjit_ts.h>
#include <linux/dma-mapping.h>
#include <linux/delay.h>
#include <linux/i2c-tegra.h>
#include <linux/gpio.h>
#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/mfd/acer_picasso_ec.h>
#include <linux/mfd/tps6586x.h>
#include <linux/memblock.h>
#include <linux/i2c/atmel_mxt_ts.h>
#include <linux/tegra_uart.h>
#include <linux/rfkill-gpio.h>

#include <sound/wm8903.h>

#include <mach/clk.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/pinmux.h>
#include <mach/iomap.h>
#include <mach/io.h>
#include <mach/i2s.h>
#include <mach/tegra_wm8903_pdata.h>
#include <mach/tegra_asoc_pdata.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/usb_phy.h>
#ifdef CONFIG_ROTATELOCK
#include <linux/switch.h>
#endif

#include "board.h"
#include "clock.h"
#include "board-picasso.h"
#include "devices.h"
#include "gpio-names.h"
#include "fuse.h"
#include "wakeups-t2.h"
#include "pm.h"

#if defined(CONFIG_ACER_VIBRATOR)
#include <../../../drivers/staging/android/timed_output.h>
#include <../../../drivers/staging/android/timed_gpio.h>
#endif

//extern void SysShutdown(void);

/* USB */
static struct tegra_usb_platform_data tegra_udc_pdata = {
	.port_otg = true,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode = TEGRA_USB_OPMODE_DEVICE,
	.u_data.dev = {
		.vbus_pmu_irq = 0,
		.vbus_gpio = -1,
		.charging_supported = false,
		.remote_wakeup_supported = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 0,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
		.xcvr_setup_offset = 0,
		.xcvr_use_fuses = 1,
	},
};

static struct tegra_usb_platform_data tegra_ehci1_utmi_pdata = {
	.port_otg = true,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = TEGRA_GPIO_PD0,
		.vbus_reg = NULL,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 9,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static void ulpi_link_platform_open(void)
{
	int reset_gpio = TEGRA_GPIO_PV1;

	gpio_request(reset_gpio, "ulpi_phy_reset");
	gpio_direction_output(reset_gpio, 0);
	msleep(5);
	gpio_direction_output(reset_gpio, 1);
}

static struct tegra_usb_phy_platform_ops ulpi_link_plat_ops = {
	.open = ulpi_link_platform_open,
};

static struct tegra_usb_platform_data tegra_ehci2_ulpi_link_pdata = {
	.port_otg = false,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_ULPI_LINK,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = -1,
		.vbus_reg = NULL,
		.hot_plug = false,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = true,
	},
	.u_cfg.ulpi = {
		.shadow_clk_delay = 10,
		.clock_out_delay = 1,
		.data_trimmer = 4,
		.stpdirnxt_trimmer = 4,
		.dir_trimmer = 4,
		.clk = "cdev2",
	},
	.ops = &ulpi_link_plat_ops,
};

static struct tegra_usb_platform_data tegra_ehci3_utmi_pdata = {
	.port_otg = false,
	.has_hostpc = false,
	.phy_intf = TEGRA_USB_PHY_INTF_UTMI,
	.op_mode	= TEGRA_USB_OPMODE_HOST,
	.u_data.host = {
		.vbus_gpio = TEGRA_GPIO_PD3,
		.vbus_reg = NULL,
		.hot_plug = true,
		.remote_wakeup_supported = false,
		.power_off_on_suspend = false,
	},
	.u_cfg.utmi = {
		.hssync_start_delay = 9,
		.elastic_limit = 16,
		.idle_wait_delay = 17,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_usb_otg_data tegra_otg_pdata = {
	.ehci_device = &tegra_ehci1_device,
	.ehci_pdata = &tegra_ehci1_utmi_pdata,
};

static void picasso_usb_init(void)
{
	/* OTG should be the first to be registered */
	tegra_otg_device.dev.platform_data = &tegra_otg_pdata;
	platform_device_register(&tegra_otg_device);

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	platform_device_register(&tegra_udc_device);

	tegra_ehci2_device.dev.platform_data = &tegra_ehci2_ulpi_link_pdata;
	platform_device_register(&tegra_ehci2_device);

	tegra_ehci3_device.dev.platform_data = &tegra_ehci3_utmi_pdata;
	platform_device_register(&tegra_ehci3_device);
}

#ifdef CONFIG_BCM4329_RFKILL

static struct rfkill_gpio_platform_data picasso_bt_rfkill_pdata[] = {
	{
		.name           = "bt_rfkill",
		.shutdown_gpio  = TEGRA_GPIO_PU0,
		.reset_gpio     = TEGRA_GPIO_INVALID,
		.type           = RFKILL_TYPE_BLUETOOTH,
	},
};

static struct platform_device picasso_bt_rfkill_device = {
	.name = "rfkill_gpio",
	.id             = -1,
	.dev = {
		.platform_data  = picasso_bt_rfkill_pdata,
	},
};

static void __init picasso_bt_rfkill(void)
{
	/*Add Clock Resource*/
	clk_add_alias("bcm4329_32k_clk", picasso_bt_rfkill_device.name, \
				"blink", NULL);
	return;
}

#else
static inline void picasso_bt_rfkill(void) { }
#endif

static struct resource ventana_bluesleep_resources[] = {
	[0] = {
		.name = "gpio_host_wake",
			.start  = TEGRA_GPIO_PU6,
			.end    = TEGRA_GPIO_PU6,
			.flags  = IORESOURCE_IO,
	},
	[1] = {
		.name = "gpio_ext_wake",
			.start  = TEGRA_GPIO_PU1,
			.end    = TEGRA_GPIO_PU1,
			.flags  = IORESOURCE_IO,
	},
	[2] = {
		.name = "host_wake",
			.start  = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PU6),
			.end    = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PU6),
			.flags  = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHEDGE,
	},
};

static struct platform_device ventana_bluesleep_device = {
	.name           = "bluesleep",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(ventana_bluesleep_resources),
	.resource       = ventana_bluesleep_resources,
};

static void __init picasso_setup_bluesleep(void)
{
	platform_device_register(&ventana_bluesleep_device);
	return;
}

/* CLOCKS */

static __initdata struct tegra_clk_init_table picasso_clk_init_table[] = {
	/* name			parent			rate		enabled */
	{ "blink",		"clk_32k",		32768,		false},
	{ "pll_p_out4",	"pll_p",		24000000,	true },
	{ "pwm",		"clk_32k",		32768,		false},
	{ "i2s1",		"pll_a_out0",	0,			false},
	{ "i2s2",		"pll_a_out0",	0,			false},
	{ "spdif_out",	"pll_a_out0",	0,			false},
	{ NULL,			NULL,			0,			0},
};

/* I2C */

static struct tegra_i2c_platform_data picasso_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.slave_addr = 0x00FC,
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data picasso_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate   = { 50000, 100000 },
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
	.slave_addr = 0x00FC,
};

static struct tegra_i2c_platform_data picasso_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.slave_addr = 0x00FC,
};

static struct tegra_i2c_platform_data picasso_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
	.is_dvc		= true,
};

static struct wm8903_platform_data picasso_wm8903_pdata = {
	.irq_active_low = 0,
	.micdet_cfg = 0,
	.micdet_delay = 100,
	.gpio_base = PICASSO_WM8903_GPIO_BASE,
	.gpio_cfg = {
		(WM8903_GPn_FN_DMIC_LR_CLK_OUTPUT << WM8903_GP1_FN_SHIFT),
		(WM8903_GPn_FN_DMIC_LR_CLK_OUTPUT << WM8903_GP2_FN_SHIFT) |
			WM8903_GP2_DIR,
		0,
		WM8903_GPIO_NO_CONFIG,
		WM8903_GPIO_NO_CONFIG,
	},
};

static struct i2c_board_info __initdata wm8903_board_info = {
	I2C_BOARD_INFO("wm8903", 0x1a),
	.platform_data = &picasso_wm8903_pdata,
	.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_CDC_IRQ),
};

static struct i2c_board_info __initdata picasso_ec = {
	I2C_BOARD_INFO(PICASSO_EC_ID, 0x58),
};

static void picasso_i2c_init(void)
{
	tegra_i2c_device1.dev.platform_data = &picasso_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &picasso_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &picasso_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &picasso_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);

	i2c_register_board_info(0, &wm8903_board_info, 1);

	i2c_register_board_info(2, &picasso_ec, 1);
}

/* UART */

static struct platform_device *picasso_uart_devices[] __initdata = {
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};

static struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_m"},
	[2] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data picasso_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *c;

	/* UARTD is the debug port. */
	pr_info("Selecting UARTD as the debug console\n");
	picasso_uart_devices[2] = &debug_uartd_device;
	debug_uart_port_base = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->mapbase;
	debug_uart_clk = clk_get_sys("serial8250.0", "uartd");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = ((struct plat_serial8250_port *)(
			debug_uartd_device.dev.platform_data))->uartclk;
		pr_info("The debug console clock name is %s\n",
						debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_p");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_p\n");
		else
			clk_set_parent(debug_uart_clk, c);

		clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, rate);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
	}
}

static void __init picasso_uart_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	picasso_uart_pdata.parent_clk_list = uart_parent_clk;
	picasso_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);
	tegra_uartb_device.dev.platform_data = &picasso_uart_pdata;
	tegra_uartc_device.dev.platform_data = &picasso_uart_pdata;
	tegra_uartd_device.dev.platform_data = &picasso_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(picasso_uart_devices,
				ARRAY_SIZE(picasso_uart_devices));
}

#ifdef CONFIG_ROTATELOCK
static struct gpio_switch_platform_data rotationlock_switch_platform_data = {
	.gpio = TEGRA_GPIO_PQ2,
};

static struct platform_device rotationlock_switch = {
	.name	= "rotationlock",
	.id	= -1,
	.dev	= {
		.platform_data = &rotationlock_switch_platform_data,
	},
};

static void rotationlock_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PQ2);
}
#endif

#ifdef CONFIG_KEYBOARD_GPIO
#define GPIO_KEY(_id, _gpio, _isactivelow, _iswake, _debounce_msec)            \
	{                                       \
		.code = _id,                    \
		.gpio = TEGRA_GPIO_##_gpio,     \
		.active_low = _isactivelow,     \
		.desc = #_id,                   \
		.type = EV_KEY,                 \
		.wakeup = _iswake,              \
		.debounce_interval = _debounce_msec,        \
	}

#ifdef CONFIG_MACH_VANGOGH
static struct gpio_keys_button acer_keys[] = {
	[0] = GPIO_KEY(KEY_VOLUMEUP, PQ4, 1, 0, 10),
	[1] = GPIO_KEY(KEY_VOLUMEDOWN, PQ5, 1, 0, 10),
	[2] = GPIO_KEY(KEY_POWER, PC7, 0, 1, 0),
	[3] = GPIO_KEY(KEY_POWER, PI3, 0, 0, 0),
	[4] = GPIO_KEY(KEY_HOME, PQ1, 0, 0,0),
};
#else
static struct gpio_keys_button acer_keys[] = {
       [0] = GPIO_KEY(KEY_VOLUMEUP, PQ4, 1, 0, 10),
       [1] = GPIO_KEY(KEY_VOLUMEDOWN, PQ5, 1, 0, 10),
       [2] = GPIO_KEY(KEY_POWER, PC7, 0, 1, 0),
       [3] = GPIO_KEY(KEY_POWER, PI3, 0, 0, 0),
};
#endif

#define PMC_WAKE_STATUS 0x14

static int picasso_wakeup_key(void)
{
	unsigned long status =
		readl(IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	int clr_key_pwr = 0x100;
	pr_info("%s\n", __FUNCTION__);

	writel(clr_key_pwr, IO_ADDRESS(TEGRA_PMC_BASE) + PMC_WAKE_STATUS);

	return (status & (1 << TEGRA_WAKE_GPIO_PV2)) ?
		KEY_POWER : KEY_RESERVED;
}

static struct gpio_keys_platform_data acer_keys_platform_data = {
	.buttons	= acer_keys,
	.nbuttons	= ARRAY_SIZE(acer_keys),
	.wakeup_key	= picasso_wakeup_key,
};

static struct platform_device acer_keys_device = {
	.name	= "gpio-keys",
	.id	= 0,
	.dev	= {
		.platform_data	= &acer_keys_platform_data,
	},
};

static void acer_keys_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(acer_keys); i++)
		tegra_gpio_enable(acer_keys[i].gpio);
}
#endif

#if defined(CONFIG_ACER_VIBRATOR)
static struct timed_gpio vib_timed_gpios[] = {
	{
		.name = "vibrator",
		.gpio = TEGRA_GPIO_PV5,
		.max_timeout = 10000,
		.active_low = 0,
	},
};

static struct timed_gpio_platform_data vib_timed_gpio_platform_data = {
	.num_gpios      = ARRAY_SIZE(vib_timed_gpios),
	.gpios          = vib_timed_gpios,
};

static struct platform_device vib_timed_gpio_device = {
	.name   = TIMED_GPIO_NAME,
	.id     = 0,
	.dev    = {
		.platform_data  = &vib_timed_gpio_platform_data,
	},
};

static void vib_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PV5);
}
#endif

#ifdef CONFIG_DOCK_V1
static struct gpio_switch_platform_data dock_switch_platform_data = {
	.gpio = TEGRA_GPIO_PR0,
};

static struct platform_device dock_switch = {
	.name	= "acer-dock",
	.id	= -1,
	.dev	= {
		.platform_data  = &dock_switch_platform_data,
	},
};

static void acer_dock_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PR0);
	tegra_gpio_enable(TEGRA_GPIO_PR1);
	tegra_gpio_enable(TEGRA_GPIO_PX6);

	if (is_tegra_debug_uartport_hs()) {
		platform_device_register(&dock_switch);
	} else {
		pr_info("UART DEBUG MESSAGE ON!!!\n");
	}
}
#endif

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct tegra_asoc_platform_data picasso_audio_pdata = {
	.gpio_spkr_en		= TEGRA_GPIO_SPKR_EN,
	.gpio_hp_det		= TEGRA_GPIO_HP_DET,
	.gpio_hp_mute		= -1,
	.gpio_int_mic_en	= TEGRA_GPIO_INT_MIC_EN,
	.gpio_ext_mic_en	= TEGRA_GPIO_EXT_MIC_EN,
	.i2s_param[HIFI_CODEC]	= {
		.audio_port_id	= 0,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_I2S,
	},
	.i2s_param[BASEBAND]	= {
		.audio_port_id	= -1,
	},
	.i2s_param[BT_SCO]	= {
		.audio_port_id	= 3,
		.is_i2s_master	= 1,
		.i2s_mode	= TEGRA_DAIFMT_DSP_A,
	},
};

static struct platform_device picasso_audio_device = {
	.name	= "tegra-snd-wm8903",
	.id	= 0,
	.dev	= {
		.platform_data  = &picasso_audio_pdata,
	},
};

#ifdef CONFIG_PSENSOR
static struct gpio_switch_platform_data psensor_switch_platform_data = {
	.gpio = TEGRA_GPIO_PC1,
};

static struct platform_device psensor_switch = {
	.name   = "psensor",
	.id     = -1,
	.dev    = {
		.platform_data  = &psensor_switch_platform_data,
	},
};

static void p_sensor_init(void)
{
	// enable gpio for psensor
	tegra_gpio_enable(TEGRA_GPIO_PC1);
}
#endif

#ifdef CONFIG_SIMDETECT
static struct gpio_switch_platform_data simdetect_switch_platform_data = {
	.gpio = TEGRA_GPIO_PI7,
};
static struct platform_device picasso_simdetect_switch = {
	.name = "simdetect",
	.id   = -1,
	.dev  = {
		.platform_data = &simdetect_switch_platform_data,
	},
};

static void simdet_init(void)
{
	if(get_sku_id() == BOARD_PICASSO_3G || get_sku_id() == BOARD_VANGOGH_3G) {
		platform_device_register(&picasso_simdetect_switch);
	}
}
#endif

static struct platform_device *picasso_devices[] __initdata = {
	&tegra_pmu_device,
	&tegra_gart_device,
	&tegra_aes_device,
#ifdef CONFIG_KEYBOARD_GPIO
	&acer_keys_device,
#endif
#if defined(CONFIG_ACER_VIBRATOR)
	&vib_timed_gpio_device,
#endif
	&tegra_wdt_device,
	&tegra_avp_device,
#ifdef CONFIG_ROTATELOCK
	&rotationlock_switch,
#endif
	&tegra_camera,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&tegra_spdif_device,
	&tegra_das_device,
	&spdif_dit_device,
	&bluetooth_dit_device,
	&picasso_bt_rfkill_device,
	&tegra_pcm_device,
	&picasso_audio_device,
#ifdef CONFIG_PSENSOR
	&psensor_switch,
#endif

};

#ifdef CONFIG_TOUCHSCREEN_CYPRESS
#include <linux/input/cyttsp.h>
#define CY_I2C_IRQ_GPIO        TEGRA_GPIO_PV6
#define CY_I2C_ADR             CY_TCH_I2C_ADDR  /* LSTS Operational mode I2C address */
#define CY_I2C_VKEY_NAME       "virtualkeys.cyttsp-i2c" /* must match I2C name */
#define CY_MAXX 1024
#define CY_MAXY 600
#define CY_VK_SZ_X             60
#define CY_VK_SZ_Y             80
#define CY_VK_CNTR_X1          (CY_VK_SZ_X*0)+(CY_VK_SZ_X/2)
#define CY_VK_CNTR_X2          (CY_VK_SZ_X*1)+(CY_VK_SZ_X/2)
#define CY_VK_CNTR_X3          (CY_VK_SZ_X*2)+(CY_VK_SZ_X/2)
#define CY_VK_CNTR_X4          (CY_VK_SZ_X*3)+(CY_VK_SZ_X/2)
#define CY_VK_CNTR_Y1          CY_MAXY+(CY_VK_SZ_Y/2)
#define CY_VK_CNTR_Y2          CY_MAXY+(CY_VK_SZ_Y/2)
#define CY_VK_CNTR_Y3          CY_MAXY+(CY_VK_SZ_Y/2)
#define CY_VK_CNTR_Y4          CY_MAXY+(CY_VK_SZ_Y/2)
#define CY_VK1_POS             ":95:770:190:60"
#define CY_VK2_POS             ":285:770:190:60"
#define CY_VK3_POS             ":475:770:190:60"
#define CY_VK4_POS             ":665:770:190:60"

enum cyttsp_gest {
	CY_GEST_GRP_NONE = 0x0F,
	CY_GEST_GRP1 = 0x10,
	CY_GEST_GRP2 = 0x20,
	CY_GEST_GRP3 = 0x40,
	CY_GEST_GRP4 = 0x80,
};

/* default bootloader keys */
u8 dflt_bl_keys[] = {
	0, 1, 2, 3, 4, 5, 6, 7
};

static int cyttsp_i2c_init(void)
{
	int ret;

	ret = gpio_request(CY_I2C_IRQ_GPIO, "CYTTSP I2C IRQ GPIO");
	if (ret) {
		pr_err("%s: Failed to request GPIO %d\n", __func__, CY_I2C_IRQ_GPIO);
		return ret;
	}
	gpio_direction_input(CY_I2C_IRQ_GPIO);
	return 0;
}

static void cyttsp_i2c_exit(void)
{
	gpio_free(CY_I2C_IRQ_GPIO);
}

static int cyttsp_i2c_wakeup(void)
{
	return 0;
}

static struct cyttsp_platform_data cypress_i2c_ttsp_platform_data = {
	.wakeup = cyttsp_i2c_wakeup,
	.init = cyttsp_i2c_init,
	.exit = cyttsp_i2c_exit,
	.maxx = CY_MAXX,
	.maxy = CY_MAXY,
	.use_hndshk = false,
	.use_sleep = true,

	/* activate up to 4 groups and set active distance */
	.act_dist = CY_GEST_GRP_NONE & CY_ACT_DIST_DFLT,

	/*change act_intrvl to customize the Active power state
	 *scanning/processing refresh interval for Operating mode
	 */
	.act_intrvl = CY_ACT_INTRVL_DFLT,
	/* change tch_tmout to customize the touch timeout for the
	 * Active power state for Operating mode
	 */
	.tch_tmout = CY_TCH_TMOUT_DFLT,
	/* change lp_intrvl to customize the Low Power power state
	 * scanning/processing refresh interval for Operating mode
	 */
	.lp_intrvl = CY_LP_INTRVL_DFLT,
	.name = CY_I2C_NAME,
	.irq_gpio = CY_I2C_IRQ_GPIO,
	.bl_keys = dflt_bl_keys,
};

static const struct i2c_board_info picasso_i2c_bus1_touch_info[] = {
	{
		I2C_BOARD_INFO(CY_I2C_NAME, CY_I2C_ADR),
		.irq = TEGRA_GPIO_TO_IRQ(CY_I2C_IRQ_GPIO),
		.platform_data = &cypress_i2c_ttsp_platform_data,
	},
};

static int __init picasso_touch_init_cypress(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PV6);
	tegra_gpio_enable(TEGRA_GPIO_PQ7);
	i2c_register_board_info(0, picasso_i2c_bus1_touch_info, 1);
	return 0;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT1386)
static struct i2c_board_info __initdata atmel_mXT1386_i2c_info[] = {
	{
		I2C_BOARD_INFO("maXTouch", 0X4C),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	},
};

static int __init touch_init_atmel_mXT1386(void)
{
	int ret;

	tegra_gpio_enable(TEGRA_GPIO_PV6);
	tegra_gpio_enable(TEGRA_GPIO_PQ7);

	ret = gpio_request(TEGRA_GPIO_PV6, "atmel_maXTouch1386_irq_gpio");
	if (ret < 0)
		printk("atmel_maXTouch1386: gpio_request TEGRA_GPIO_PQ6 fail\n");

	ret = gpio_request(TEGRA_GPIO_PQ7, "atmel_maXTouch1386");
	if (ret < 0)
		printk("atmel_maXTouch1386: gpio_request fail\n");

	ret = gpio_direction_output(TEGRA_GPIO_PQ7, 0);
	if (ret < 0)
		printk("atmel_maXTouch1386: gpio_direction_output fail\n");
	gpio_set_value(TEGRA_GPIO_PQ7, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PQ7, 1);
	msleep(100);

	i2c_register_board_info(0, atmel_mXT1386_i2c_info, 1);

	return 0;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_768E)
static struct i2c_board_info __initdata atmel_mXT768e_i2c_info[] = {
	{
		I2C_BOARD_INFO("maXTouch", 0X4D),
		.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	},
};

static int __init touch_init_atmel_mXT768e(void)
{
	int ret;

	tegra_gpio_enable(TEGRA_GPIO_PV6); /* TP INTERRUPT PIN */
	tegra_gpio_enable(TEGRA_GPIO_PQ7); /* TP RESET PIN */

	ret = gpio_request(TEGRA_GPIO_PV6, "atmel_maXTouch768e_irq_gpio");
	if (ret < 0)
		printk("atmel_maXTouch768e: gpio_request TEGRA_GPIO_PQ6 fail\n");

	ret = gpio_request(TEGRA_GPIO_PQ7, "atmel_maXTouch768e_rst_gpio");
	if (ret < 0)
		printk("atmel_maXTouch768e: gpio_request fail\n");

	ret = gpio_direction_output(TEGRA_GPIO_PQ7, 0);
	if (ret < 0)
		printk("atmel_maXTouch768e: gpio_direction_output fail\n");

	gpio_set_value(TEGRA_GPIO_PQ7, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PQ7, 1);
	msleep(100);

	i2c_register_board_info(0, atmel_mXT768e_i2c_info, 1);

	return 0;
}
#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT_PICASSO)

static const u8 mxt_config_data[] = {
	/* MXT_GEN_COMMAND(6) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_GEN_POWER(7) */
	0x41, 0xff, 0x32,
	/* MXT_GEN_ACQUIRE(8) */
	0x09, 0x00, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
	/* MXT_TOUCH_MULTI(9) */
	0x8f, 0x00, 0x00, 0x1c, 0x29, 0x00, 0x10, 0x37, 0x03, 0x01,
	0x00, 0x05, 0x05, 0x20, 0x0a, 0x05, 0x0a, 0x05, 0x1f, 0x03,
	0xff, 0x04, 0x00, 0x00, 0x00, 0x00, 0x98, 0x22, 0xd4, 0x16,
	0x0a, 0x0a, 0x00, 0x00,
	/* MXT_TOUCH_KEYARRAY(15-1) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* MXT_TOUCH_KEYARRAY(15-2) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00,
	/* MXT_SPT_COMMSCONFIG(18) */
	0x00, 0x00,
	/* MXT_PROCG_NOISE(22) */
	0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x00,
	0x00, 0x00, 0x0a, 0x13, 0x19, 0x1e, 0x00,
	/* MXT_PROCI_ONETOUCH(24) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_SELFTEST(25) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_TWOTOUCH(27) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_SPT_CTECONFIG(28) */
	0x00, 0x00, 0x00, 0x08, 0x1c, 0x3c,
	/* MXT_PROCI_GRIP(40) */
	0x00, 0x00, 0x00, 0x00, 0x00,
	/* MXT_PROCI_PALM(41) */
	0x01, 0x00, 0x00, 0x23, 0x00, 0x00,
	/* MXT_TOUCH_PROXIMITY(43) */
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

static struct mxt_platform_data mxt_platform_data = {
	.x_line			= 0x1c,
	.y_line			= 0x29,
	.x_size			= 1280,
	.y_size			= 800,
	.blen			= 0x10,
	.threshold		= 0x37,
	.voltage		= 3300000,
	.orient			= MXT_DIAGONAL,
	.irqflags		= IRQF_TRIGGER_FALLING,
	.config			= mxt_config_data,
	.config_length	= sizeof(mxt_config_data),
};

static struct i2c_board_info mxt_device_picasso = {
	I2C_BOARD_INFO("atmel_mxt_ts", 0x4c),
	.platform_data = &mxt_platform_data,
	.irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
};

static void __init touch_init_atmel_picasso(void) {

	tegra_gpio_enable(TEGRA_GPIO_PV6); /* TP INTERRUPT PIN */
	tegra_gpio_enable(TEGRA_GPIO_PQ7); /* TP RESET PIN */

	gpio_request(TEGRA_GPIO_PV6, "atmel_touch_chg");
	gpio_request(TEGRA_GPIO_PQ7, "atmel_touch_reset");

	gpio_set_value(TEGRA_GPIO_PQ7, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PQ7, 1);
	msleep(100);

	i2c_register_board_info(0, &mxt_device_picasso, 1);

}

#endif

#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT)

static struct mxt_platform_data atmel_mxt_info = {
	.x_line		= 27,
	.y_line		= 42,
	.x_size		= 768,
	.y_size		= 1366,
	.blen		= 0x20,
	.threshold	= 0x3C,
	.voltage	= 3300000,
	.orient		= MXT_ROTATED_90,
	.irqflags	= IRQF_TRIGGER_FALLING,
};

static struct i2c_board_info __initdata i2c_info[] = {
	{
	 I2C_BOARD_INFO("atmel_mxt_ts", 0x5A),
	 .irq = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	 .platform_data = &atmel_mxt_info,
	 },
};

static int __init touch_init_atmel(void)
{
	gpio_request(TEGRA_GPIO_PV6, "atmel-irq");
	gpio_direction_input(TEGRA_GPIO_PV6);

	gpio_request(TEGRA_GPIO_PQ7, "atmel-reset");
	gpio_direction_output(TEGRA_GPIO_PQ7, 0);
	msleep(1);
	gpio_set_value(TEGRA_GPIO_PQ7, 1);
	msleep(100);

	i2c_register_board_info(0, i2c_info, 1);

	return 0;
}

#endif

static int __init picasso_gps_init(void)
{
	struct clk *clk32 = clk_get_sys(NULL, "blink");
	if (!IS_ERR(clk32)) {
		clk_set_rate(clk32,clk32->parent->rate);
		clk_enable(clk32);
	}

	tegra_gpio_enable(TEGRA_GPIO_PZ3);
	return 0;
}

static void __init picasso_power_off_init(void)
{
	//pm_power_off = SysShutdown;
}

int get_pin_value(unsigned int gpio, char *name)
{
	int pin_value = 0;

	tegra_gpio_enable(gpio);
	gpio_request(gpio, name);
	gpio_direction_input(gpio);
	pin_value = gpio_get_value(gpio);
	gpio_free(gpio);
	tegra_gpio_disable(gpio);
	return pin_value;
}

int get_sku_id(void)
{
#ifdef CONFIG_MACH_PICASSO
	/* Wifi=5, 3G=3, DVT2=7 */
	return (get_pin_value(TEGRA_GPIO_PQ0, "PIN0") << 2) + \
		 (get_pin_value(TEGRA_GPIO_PQ3, "PIN1") << 1) + \
		 get_pin_value(TEGRA_GPIO_PQ6, "PIN2");
#endif
#ifdef CONFIG_MACH_VANGOGH
	/* Wifi=0, 3G=1 */
	return (get_pin_value(TEGRA_GPIO_PQ0,"PIN0"));
#endif
}

void acer_board_info(void)
{
#ifdef CONFIG_MACH_PICASSO_E
	switch(get_sku_id()){
		case BOARD_PICASSO_WIFI:
			pr_info("Board Type: Picasso E Wifi\n");
			break;
		default:
			pr_info("Board Type: Unknown\n");
			break;
	}
#elif CONFIG_MACH_PICASSO
	switch(get_sku_id()){
		case BOARD_PICASSO_3G:
			pr_info("Board Type: Picasso 3G\n");
			break;
		case BOARD_PICASSO_WIFI:
			pr_info("Board Type: Picasso Wifi\n");
			break;
		case BOARD_PICASSO_DVT2:
			pr_info("Board Type: Picasso DVT2\n");
			break;
		default:
			pr_info("Board Type: Unknown\n");
			break;
	}
#endif
#ifdef CONFIG_MACH_VANGOGH
	switch(get_sku_id()){
		case BOARD_VANGOGH_3G:
			pr_info("Board Type: VanGogh 3G\n");
			break;
		case BOARD_VANGOGH_WIFI:
			pr_info("Board Type: VanGogh Wifi\n");
			break;
		default:
			pr_info("Board Type: Unknown\n");
			break;
	}
#endif
}

static void __init tegra_picasso_init(void)
{

	tegra_clk_init_from_table(picasso_clk_init_table);
	picasso_pinmux_init();
	picasso_i2c_init();
#ifndef CONFIG_DOCK_V1
	picasso_uart_init();
#endif

	platform_add_devices(picasso_devices, ARRAY_SIZE(picasso_devices));

	picasso_sdhci_init();

	picasso_charge_init();
	picasso_regulator_init();
	picasso_charger_init();


#ifdef CONFIG_TOUCHSCREEN_CYPRESS
	picasso_touch_init_cypress();
#endif
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT1386
	touch_init_atmel_mXT1386();
#endif
#ifdef CONFIG_TOUCHSCREEN_ATMEL_768E
	touch_init_atmel_mXT768e();
#endif
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT_PICASSO
	/* use older version of atmel mxt */
	touch_init_atmel_picasso();
#endif
#if defined(CONFIG_TOUCHSCREEN_ATMEL_MXT)
	touch_init_atmel();
#endif

#ifdef CONFIG_KEYBOARD_GPIO
	acer_keys_init();
#endif
#ifdef CONFIG_ROTATELOCK
        rotationlock_init();
#endif
#ifdef CONFIG_DOCK_V1
	acer_dock_init();
#endif
	acer_board_info();

#ifdef CONFIG_PSENSOR
	p_sensor_init();
#endif
	picasso_usb_init();
	picasso_gps_init();
	picasso_panel_init();
	picasso_sensors_init();
	picasso_bt_rfkill();
	picasso_power_off_init();
	picasso_emc_init();

	picasso_setup_bluesleep();
#if defined(CONFIG_ACER_VIBRATOR)
	vib_init();
#endif
#ifdef CONFIG_SIMDETECT
	simdet_init();
#endif
	tegra_release_bootloader_fb();
}

int __init tegra_picasso_protected_aperture_init(void)
{
	if (!machine_is_picasso())
		return 0;

	tegra_protected_aperture_init(tegra_grhost_aperture);
	return 0;
}
late_initcall(tegra_picasso_protected_aperture_init);

void __init tegra_picasso_reserve(void)
{
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	tegra_reserve(SZ_256M, SZ_8M, SZ_16M);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
	tegra_ram_console_debug_reserve(SZ_1M);
#endif
}

MACHINE_START(PICASSO, "picasso")
	.boot_params    = 0x00000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_picasso_reserve,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.timer          = &tegra_timer,
	.init_machine	= tegra_picasso_init,
MACHINE_END

MACHINE_START(VANGOGH, "vangogh")
	.boot_params    = 0x00000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_picasso_reserve,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.timer          = &tegra_timer,
	.init_machine	= tegra_picasso_init,
MACHINE_END

/*
MACHINE_START(PICASSO_E, "picasso_e")
	.boot_params    = 0x00000100,
	.map_io         = tegra_map_common_io,
	.reserve        = tegra_picasso_reserve,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.timer          = &tegra_timer,
	.init_machine	= tegra_picasso_init,
MACHINE_END
*/
