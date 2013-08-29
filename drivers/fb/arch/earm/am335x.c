/* Architecture dependent part for the framebuffer on the am335x
 * First pass is aimed at the 4D cape (should also work with LCD4 cape).
 * Doesn't support HDMI output yet. The TDA19988 driver needs more work.
 *
 * This driver includes code from am335x_lcd.c and am335x_prcm.c from FreeBSD.
 * The following copyright notices and terms apply to the re-used code:
 *
 * Copyright 2013 Oleksandr Tymoshenko <gonzo@freebsd.org>
 * Copyright 2012 Damjan Marion <dmarion@Freebsd.org>
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <minix/chardriver.h>
#include <minix/drivers.h>
#include <minix/fb.h>
#include <minix/mmio.h>
#include <minix/padconf.h>
#include <minix/clkconf.h>
#include <minix/spin.h>
#include <minix/type.h>
#include <minix/vm.h>
#include <minix/log.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <dev/videomode/videomode.h>
#include <dev/videomode/edidvar.h>
#include <dev/videomode/edidreg.h>

#include "am335x.h"
#include "fb.h"

/* default / fallback resolution if EDID reading fails */
#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 272

struct panel_config {
	uint32_t panel_width;
	uint32_t panel_height;
	uint32_t panel_hfp;
	uint32_t panel_hbp;
	uint32_t panel_hsw;
	uint32_t panel_vfp;
	uint32_t panel_vbp;
	uint32_t panel_vsw;
	uint32_t ac_bias;
	uint32_t ac_bias_intrpt;
	uint32_t dma_burst_sz;
	uint32_t bpp;
	uint32_t fdd;
	uint32_t invert_line_clock;
	uint32_t invert_frm_clock;
	uint32_t panel_type;
	uint32_t lcd_size;
	uint32_t sync_edge;
	uint32_t sync_ctrl;
	uint32_t panel_pxl_clk;
	uint32_t panel_invert_pxl_clk;
};

static const struct panel_config default_cfg = {
	.panel_width = SCREEN_WIDTH,
	.panel_height = SCREEN_HEIGHT,
	.panel_hfp = 9,
	.panel_hbp = 44,
	.panel_hsw = 5,
	.panel_vfp = 4,
	.panel_vbp = 13,
	.panel_vsw = 10,
	.ac_bias = 255,
	.ac_bias_intrpt = 0,
	.dma_burst_sz = 16,
	.bpp = 24,
	.fdd = 0x80,
	.invert_line_clock = 0, /* TODO: verify that this is correct */
	.invert_frm_clock = 0, /* TODO: verify that this is correct */
	.panel_type = 0x01, 	/* Color */
	.lcd_size = ((SCREEN_HEIGHT - 1) << 16 | (SCREEN_WIDTH - 1)),
	.sync_edge = 0,
	.sync_ctrl = 1,
	.panel_pxl_clk = 9000000,	/* 9 MHz */
	.panel_invert_pxl_clk = 0
};

static struct panel_config am335x_cfg[FB_DEV_NR];

static const struct fb_fix_screeninfo default_fbfs = {
	.xpanstep	= 0,
	.ypanstep	= 0,
	.ywrapstep	= 0,
	.line_length	= SCREEN_WIDTH * 4,
	.mmio_start	= 0,	/* Not implemented for char. special, so */
	.mmio_len	= 0	/* these are set to 0 */
};

static struct fb_fix_screeninfo am335x_fbfs[FB_DEV_NR];

static const struct fb_var_screeninfo default_fbvs = {
	.xres		= SCREEN_WIDTH,
	.yres		= SCREEN_HEIGHT,
	.xres_virtual	= SCREEN_WIDTH,
	.yres_virtual	= SCREEN_HEIGHT*2,
	.xoffset	= 0,
	.yoffset	= 0,
	.bits_per_pixel = 24,
	.red =	{
		.offset = 12,
		.length = 6,
		.msb_right = 0
		},
	.green = {
		.offset = 6,
		.length = 6,
		.msb_right = 0
		},
	.blue =	{
		.offset = 0,
		.length = 6,
		.msb_right = 0
		},
	.transp = {
		.offset = 18,
		.length = 6,
		.msb_right = 0
		}
};

static struct fb_var_screeninfo am335x_fbvs[FB_DEV_NR];

/* logging - use with log_warn(), log_info(), log_debug(), log_trace() */
static struct log log = {
	.name = "am335x_fb",
	.log_level = LEVEL_DEBUG,
	.log_func = default_log
};

/* pin configuration for all of the pins used by the driver */
#define NPINMUX 21
static struct pinmux {
	u32_t padconf;
	u32_t mask;
	u32_t value;
} pinmux[NPINMUX] = {
	/* data: output, muxmode 0, PUDEN set to 1 to disable pullup */
	{CONTROL_CONF_LCD_DATA0, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA1, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA2, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA3, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA4, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA5, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA6, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA7, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA8, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA9, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA10, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA11, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA12, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA13, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA14, 0xffffffff, CONTROL_CONF_PUDEN},
	{CONTROL_CONF_LCD_DATA15, 0xffffffff, CONTROL_CONF_PUDEN},
	/* control: output, muxmode 0 */
	{CONTROL_CONF_LCD_VSYNC, 0xffffffff, 0},
	{CONTROL_CONF_LCD_HSYNC, 0xffffffff, 0},
	{CONTROL_CONF_LCD_PCLK, 0xffffffff, 0},
	{CONTROL_CONF_LCD_AC_BIAS_EN, 0xffffffff, 0}

	/* TODO handle GPIO in the gpio server. 
	 * TODO try using gpio to simulate 100% duty cycle of backlight pwm.
	 */
#if 0
	/* gpio (enable/disable LCD): output, putypesel, muxmode 7 */
	{CONTROL_CONF_MCASP0_FSX, 0xffffffff, (CONTROL_CONF_PUTYPESEL | 
	    CONTROL_CONF_MUXMODE(7))}
	/* TODO pwm */
#endif
};

/* globals */
static vir_bytes mapped_addr;

static int irq = 36;
static int irq_hook_id = 1;
static int irq_hook_kernel_id = 1;

static vir_bytes fb_vir;
static phys_bytes fb_phys;
static size_t fb_size;

static uint32_t ref_freq = 0;

static int initialized = 0;

static int am335x_lcd_padconf(void);
static int am335x_lcd_map(void);
static int am335x_lcd_clkconf(void);
static int am335x_lcd_intr_enable(void);
static uint32_t am335x_lcd_calc_divisor(uint32_t reference, uint32_t freq);
static uint32_t am335x_get_sysclk(void);
static uint32_t am335x_get_ref_freq(void);
static void am335x_configure_display(int minor);
static void configure_with_defaults(int minor);
static int configure_with_edid(int minor, struct edid_info *info);

/* Configure the pinmux for all of the pins. */
static int
am335x_lcd_padconf(void)
{
	int r, i;

	for (i = 0; i < NPINMUX; i++) {

		r = sys_padconf(pinmux[i].padconf, pinmux[i].mask,
		    pinmux[i].value);
		if (r != OK) {
			log_warn(&log, "Error configuring padconf (r=%d)\n", r);
			return -1;
		}
	}

	log_debug(&log, "padconf configured\n");

	return OK;
}

/* Map the LCDC memory to allow access to the LCDC registers at 'mapped_addr' */
static int
am335x_lcd_map(void)
{
	struct minix_mem_range mr;

	/* Configure memory access */
	mr.mr_base = AM335X_LCD_BASE;	/* start addr */
	mr.mr_limit = mr.mr_base + AM335X_LCD_SIZE;	/* end addr */

	/* ask for privileges to access the LCD memory range */
	if (sys_privctl(SELF, SYS_PRIV_ADD_MEM, &mr) != OK) {
		log_warn(&log, "Unable to obtain LCD memory range privileges");
		return -1;
	}

	/* map the memory into this process */
	mapped_addr = (vir_bytes) vm_map_phys(SELF, (void *) AM335X_LCD_BASE,
	    AM335X_LCD_SIZE);
	if (mapped_addr == (vir_bytes) MAP_FAILED) {
		log_warn(&log, "Unable to map LCD registers");
		return -1;
	}

	log_debug(&log, "Mapped AM335X_LCD_BASE\n");

	return OK;
}

/* Configure and enable the LCDC clock in the control module.
 * Clocks in CLKC_ENABLE are setup separately.
 */
static int
am335x_lcd_clkconf(void)
{
	spin_t spin;

	/* From TRM section 8.1.6.10.1 Configuring the Display PLL */

	clkconf_init();

	/* 1. Switch PLL to bypass mode - write 0x4 to CM_CLKMODE_DPLL_DISP */
	clkconf_set(CM_CLKMODE_DPLL_DISP, 0xffffffff, DPLL_MN_BYP_MODE);

	/* 2. Wait to ensure PLL is in bypass */
	spin_init(&spin, 250000);
	while ((clkconf_get(CM_IDLEST_DPLL_DISP) & (1 << ST_MN_BYPASS_BIT)) !=
	    (1 << ST_MN_BYPASS_BIT)) {
	
		if (!spin_check(&spin)) {
			log_warn(&log, "DPLL never went into bypass\n");
			clkconf_release();
			return EIO;
		}
	}

	/* 3. Configure Multiply and Divide values */
	clkconf_set(CM_CLKSEL_DPLL_DISP, DPLL_MULT_MASK, DPLL_MULT_VAL);

	/* 4. Configure M2 divider */
	/* nothing to configure here, reset value of 0x1 is OK */
	
	/* 5. Switch over to lock mode - write 0x7 to CM_CLKMODE_DPLL_DISP */
	clkconf_set(CM_CLKMODE_DPLL_DISP, 0xffffffff, DPLL_LOCK_MODE);

	/* 6. Wait to ensure PLL is locked */
	spin_init(&spin, 250000);
	while ((clkconf_get(CM_IDLEST_DPLL_DISP) & (1 << ST_DPLL_CLK_BIT)) !=
	    (1 << ST_DPLL_CLK_BIT)) {
	
		if (!spin_check(&spin)) {
			log_warn(&log, "DPLL never locked\n");
			clkconf_release();
			return EIO;
		}
	}

	/* Enable LCDC clock */
	clkconf_set(CM_PER_LCDC_CLKCTRL, LCDC_CLKCTRL_MODMODE_MASK,
	    LCDC_CLKCTRL_MODMODE_ENABLE);

	ref_freq = am335x_get_ref_freq();

	clkconf_release();

	log_debug(&log, "CM Clocks Enabled\n");

	return OK;
}

static uint32_t
am335x_get_sysclk(void)
{
	/* Simply hard code the value for now since there isn't an easy way
	 * to get the value at runtime. Definitely something to look into.
	 *
	 * XXX should look in CONTROL_STATUS register in CONTROL_MODULE.
	 * XXX this should be in a library somewhere, not in the fb driver.
	 */
	return 19200000; /* 19.2 MHz power-on default */
}

static uint32_t
am335x_get_ref_freq(void)
{
	uint32_t reg;
	uint32_t sysclk;

	log_debug(&log, "Determining clock frequency.\n");

	reg = clkconf_get(CM_CLKSEL_DPLL_DISP);
	sysclk = am335x_get_sysclk();

	return (DPLL_MULT(reg) * (sysclk / DPLL_DIV(reg)));
}

static uint32_t
am335x_lcd_calc_divisor(uint32_t reference, uint32_t freq)
{
	uint32_t div;
	/* Raster mode case: divisors are in range from 2 to 255 */
	for (div = 2; div < 255; div++)
		if (reference/div <= freq)
			return (div);

	return (255);
}

/* Enable the IRQ and configure the LCDC IRQ masks */
static int
am335x_lcd_intr_enable(void)
{
	int r;
	static int policy_set = 0;
	static int irq_enabled = 0;

	if (!policy_set) {
		r = sys_irqsetpolicy(irq, 0, &irq_hook_kernel_id);
		if (r == OK) {
			policy_set = 1;
		} else {
			log_warn(&log, "Couldn't set irq policy (r=%d)\n", r);
			return -1;
		}
	}
	if (policy_set && !irq_enabled) {
		r = sys_irqenable(&irq_hook_kernel_id);
		if (r == OK) {
			irq_enabled = 1;
		} else {
			log_warn(&log, "Couldn't enable irq %d (hooked)\n",
			    irq);
			return -1;
		}
	}

	log_debug(&log, "IRQ %d Enabled\n", irq);

	return OK;
}

static void
am335x_configure_display(int minor)
{
/* Tell hardware where frame buffer is and turn display on */

	uint32_t reg;

	if (!initialized) return;
	if (minor != 0) return;

	/* Enable LCD */
	reg = RASTER_CTRL_LCDTFT;
	reg |= (am335x_cfg[minor].fdd << RASTER_CTRL_REQDLY_SHIFT);
	reg |= (PALETTE_DATA_ONLY << RASTER_CTRL_PALMODE_SHIFT);
	if (am335x_cfg[minor].bpp >= 24)
		reg |= RASTER_CTRL_TFT24;
	if (am335x_cfg[minor].bpp == 32)
		reg |= RASTER_CTRL_TFT24_UNPACKED;
	write32(mapped_addr + RASTER_CTRL_REG, reg); 

	write32(mapped_addr + CLKC_ENABLE_REG,
	    CLKC_ENABLE_DMA | CLKC_ENABLE_LDID | CLKC_ENABLE_CORE);

	log_debug(&log, "LCD Enabled\n");

	write32(mapped_addr + CLKC_RESET_REG, CLKC_RESET_MAIN);
	micro_delay(100);
	write32(mapped_addr + CLKC_RESET_REG, 0);

	reg = IRQ_EOF1 | IRQ_EOF0 | IRQ_FUF | IRQ_PL |
	    IRQ_ACB | IRQ_SYNC_LOST |  IRQ_RASTER_DONE |
	    IRQ_FRAME_DONE;
	write32(mapped_addr + IRQENABLE_SET_REG, reg);

	reg = read32(mapped_addr + RASTER_CTRL_REG);
 	reg |= RASTER_CTRL_LCDEN;
	write32(mapped_addr + RASTER_CTRL_REG, reg); 

	write32(mapped_addr + SYSCONFIG_REG,
	    SYSCONFIG_STANDBY_SMART | SYSCONFIG_IDLE_SMART); 

	log_debug(&log, "Interrupts and sysconfig setup.\n");

	return;
}

static int
configure_with_edid(int minor, struct edid_info *info)
{
	struct videomode *mode;

	if (info == NULL || minor < 0 || minor >= FB_DEV_NR || info->edid_nmodes < 1) {
		log_warn(&log, "Invalid minor #%d or info == NULL\n", minor);
		return -1;
	}

	/* If debugging or tracing, print the contents of info */
	if (log.log_level >= LEVEL_DEBUG) {
		log_debug(&log, "--- EDID - START ---\n");
		edid_print(info);
		log_debug(&log, "--- EDID - END ---\n");
	}

	
	/* Choose the preferred mode. */
	mode = &(info->edid_modes[0]);

	/*
	 * apply the default settings since we don't overwrite every field
	 */
	configure_with_defaults(minor);

	/*
	 * apply the settings corresponding to the given EDID
	 */

	/* panel_config */
	am335x_cfg[minor].lcd_size    = ((mode->vdisplay - 1) << 16 | (mode->hdisplay - 1));

	if (EDID_FEATURES_DISP_TYPE(info->edid_features) ==
			EDID_FEATURES_DISP_TYPE_MONO) {
		am335x_cfg[minor].panel_type  = 0x00;		/* Mono */
	} else {
		am335x_cfg[minor].panel_type  = 0x01;		/* RGB/Color */
	}

	/* fb_fix_screeninfo */
	am335x_fbfs[minor].line_length = mode->hdisplay * 4;

	/* fb_var_screeninfo */
	am335x_fbvs[minor].xres		= mode->hdisplay;
	am335x_fbvs[minor].yres		= mode->vdisplay;
	am335x_fbvs[minor].xres_virtual	= mode->hdisplay;
	am335x_fbvs[minor].yres_virtual	= mode->vdisplay*2;

	return OK;
}

static void
configure_with_defaults(int minor)
{
	if (minor < 0 || minor >= FB_DEV_NR) {
		log_warn(&log, "Invalid minor #%d\n", minor);
		return;
	}

	/* copy the default values into this minor's configuration */
	memcpy(&am335x_cfg[minor], &default_cfg, sizeof(struct panel_config));
	memcpy(&am335x_fbfs[minor], &default_fbfs, sizeof(struct fb_fix_screeninfo));
	memcpy(&am335x_fbvs[minor], &default_fbvs, sizeof(struct fb_var_screeninfo));
}



/* Perform the initial setup of the display. */
int
am335x_fb_init(int minor, struct device *dev, struct edid_info *info)
{
	int r;
	uint32_t reg, timing0, timing1, timing2;
	uint32_t pid, div;
	uint32_t burst_log;

	const struct panel_config *panel_cfg = &am335x_cfg[minor];

	assert(dev != NULL);
	if (minor != 0) return ENXIO;	/* We support only one minor */

	if (initialized) {
		dev->dv_base = fb_vir;
		dev->dv_size = fb_size;
		return OK;
	} else if (info != NULL) {
		log_debug(&log, "Configuring Settings based on EDID...\n");
		r = configure_with_edid(minor, info);
		if (r != OK) {
			log_warn(&log, "EDID config failed. Using defaults.\n");
			configure_with_defaults(minor);
		}
	} else {
		log_debug(&log, "Loading Default Settings...\n");
		configure_with_defaults(minor);
	}

	initialized = 1;

	/* Configure Pins */
	r = am335x_lcd_padconf();
	if (r != OK) {
		return r;
	}

	/* Map LCD Registers */
	r = am335x_lcd_map();
	if (r != OK) {
		return r;
	}

	/* Enable Clocks */
	r = am335x_lcd_clkconf();
	if (r != OK) {
		return r;
	}

	/* Basic sanity check before writing to registers. */
	pid = read32(mapped_addr + PID_REG);
	if (AM335X_LCD_REV_MAJOR(pid) != AM335X_LCD_REV_MAJOR_EXPECTED ||
	    AM335X_LCD_REV_MINOR(pid) != AM335X_LCD_REV_MINOR_EXPECTED) {

		log_warn(&log, "Unexpected revision in PID register\n");
		log_warn(&log, "Got: 0x%x.0x%x | Expected 0x%x.0x%x\n",
		    AM335X_LCD_REV_MAJOR(pid), AM335X_LCD_REV_MINOR(pid),
		    AM335X_LCD_REV_MAJOR_EXPECTED,
		    AM335X_LCD_REV_MINOR_EXPECTED);

		return EINVAL;
	}

	log_debug(&log, "Read PID OK\n");

	/* Allocate contiguous physical memory for the display buffer */
	fb_size = am335x_fbvs[minor].yres_virtual * am335x_fbvs[minor].xres_virtual *
				(am335x_fbvs[minor].bits_per_pixel / 8);
	fb_vir = (vir_bytes) alloc_contig(fb_size, 0, &fb_phys);
	if (fb_vir == (vir_bytes) MAP_FAILED) {
		log_warn(&log, "Unable to allocate contiguous memory\n");
		return ENOMEM;
	}
	dev->dv_base = fb_vir;
	dev->dv_size = fb_size;

	log_debug(&log, "FB Memory allocated.\n");

	/* Only raster mode is supported */
	reg = CTRL_RASTER_MODE;
	div = am335x_lcd_calc_divisor(ref_freq, panel_cfg->panel_pxl_clk);
	reg |= (div << CTRL_DIV_SHIFT);
	write32(mapped_addr + CTRL_REG, reg);

	log_debug(&log, "Control Set\n");

	/* Set timing */
	timing0 = timing1 = timing2 = 0;

	/* Horizontal back porch */
	timing0 |= (panel_cfg->panel_hbp & 0xff) << RASTER_TIMING_0_HBP_SHIFT;
	timing2 |= ((panel_cfg->panel_hbp >> 8) & 3) << RASTER_TIMING_2_HBPHI_SHIFT;
	/* Horizontal front porch */
	timing0 |= (panel_cfg->panel_hfp & 0xff) << RASTER_TIMING_0_HFP_SHIFT;
	timing2 |= ((panel_cfg->panel_hfp >> 8) & 3) << RASTER_TIMING_2_HFPHI_SHIFT;
	/* Horizontal sync width */
	timing0 |= (panel_cfg->panel_hsw & 0x3f) << RASTER_TIMING_0_HSW_SHIFT;
	timing2 |= ((panel_cfg->panel_hsw >> 6) & 0xf) << RASTER_TIMING_2_HSWHI_SHIFT;

	/* Vertical back porch, front porch, sync width */
	timing1 |= (panel_cfg->panel_vbp & 0xff) << RASTER_TIMING_1_VBP_SHIFT;
	timing1 |= (panel_cfg->panel_vfp & 0xff) << RASTER_TIMING_1_VFP_SHIFT;
	timing1 |= (panel_cfg->panel_vsw & 0x3f) << RASTER_TIMING_1_VSW_SHIFT;

	/* Pixels per line */
	timing0 |= (((panel_cfg->panel_width - 1) >> 10) & 1)
	    << RASTER_TIMING_0_PPLMSB_SHIFT;
	timing0 |= (((panel_cfg->panel_width - 1) >> 4) & 0x3f)
	    << RASTER_TIMING_0_PPLLSB_SHIFT;
	
	/* Lines per panel */
	timing1 |= ((panel_cfg->panel_height - 1) & 0x3ff)
	    << RASTER_TIMING_1_LPP_SHIFT;
	timing2 |= (((panel_cfg->panel_height - 1) >> 10 ) & 1)
	    << RASTER_TIMING_2_LPP_B10_SHIFT;
	
	/* clock signal settings */
	if (panel_cfg->sync_ctrl)
		timing2 |= RASTER_TIMING_2_PHSVS;
	if (panel_cfg->sync_edge)
		timing2 |= RASTER_TIMING_2_PHSVS_RISE;
	else
		timing2 |= RASTER_TIMING_2_PHSVS_FALL;
	if (panel_cfg->invert_line_clock)
		timing2 |= RASTER_TIMING_2_IHS;
	if (panel_cfg->invert_frm_clock)
		timing2 |= RASTER_TIMING_2_IVS;
	if (panel_cfg->panel_invert_pxl_clk)
		timing2 |= RASTER_TIMING_2_IPC;
	
	/* AC bias */
	timing2 |= (panel_cfg->ac_bias << RASTER_TIMING_2_ACB_SHIFT);
	timing2 |= (panel_cfg->ac_bias_intrpt << RASTER_TIMING_2_ACBI_SHIFT);

	write32(mapped_addr + RASTER_TIMING_0_REG, timing0);
	write32(mapped_addr + RASTER_TIMING_1_REG, timing1);
	write32(mapped_addr + RASTER_TIMING_2_REG, timing2);

	log_debug(&log, "Timings Set\n");

	/* DMA settings */
	reg = LCDDMA_CTRL_FB0_FB1;
	/* Find power of 2 for current burst size */
	switch (panel_cfg->dma_burst_sz) {
	case 1:
		burst_log = 0;
		break;
	case 2:
		burst_log = 1;
		break;
	case 4:
		burst_log = 2;
		break;
	case 8:
		burst_log = 3;
		break;
	case 16:
	default:
		burst_log = 4;
		break;
	}
	reg |= (burst_log << LCDDMA_CTRL_BURST_SIZE_SHIFT);
	/* (Comment from FreeBSD Driver) XXX: FIFO TH */
	reg |= (0 << LCDDMA_CTRL_TH_FIFO_RDY_SHIFT);
	write32(mapped_addr + LCDDMA_CTRL_REG, reg); 

	/* Hardware supports double buffering, FreeBSD driver doesn't. 
	 * Use the same buffer for both frames.
	 */
	write32(mapped_addr + LCDDMA_FB0_BASE_REG, fb_phys); 
	write32(mapped_addr + LCDDMA_FB0_CEILING_REG, fb_phys + fb_size - 1); 
	write32(mapped_addr + LCDDMA_FB1_BASE_REG, fb_phys); 
	write32(mapped_addr + LCDDMA_FB1_CEILING_REG, fb_phys + fb_size - 1); 

	log_debug(&log, "DMA Setup\n");

	/* Setup Interrupts */
	r = am335x_lcd_intr_enable();
	if (r != OK) {
		return OK;
	}

	/* Configure buffer settings and turn on LCD/Digital */
	am335x_configure_display(minor);

	log_debug(&log, "Display Configured. Returning...\n");

	/* TODO setup GPIO for LCD_EN line (check when to do this step) */
	/* TODO setup PWM for backlight (if needed) */

	return OK;
}

int
am335x_get_device(int minor, struct device *dev)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;
	dev->dv_base = fb_vir;
	dev->dv_size = fb_size;
	return OK;
}

int
am335x_get_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;

	*fbvsp = am335x_fbvs[minor];
	return OK;
}

int
am335x_put_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp)
{
	int r = OK;
	
	assert(fbvsp != NULL);

	if (!initialized) return ENXIO;
	if (minor != 0)	return ENXIO;

	/* For now we only allow to play with the yoffset setting */
	if (fbvsp->yoffset != am335x_fbvs[minor].yoffset) {
		if (fbvsp->yoffset < 0 || fbvsp->yoffset > am335x_fbvs[minor].yres) {
			return EINVAL;
		}

		am335x_fbvs[minor].yoffset = fbvsp->yoffset;
	}
	
	/* Now update hardware with new settings */
	am335x_configure_display(minor);
	return OK;

}

int
am335x_get_fixscreeninfo(int minor, struct fb_fix_screeninfo *fbfsp)
{
	if (!initialized) return ENXIO;
	if (minor != 0) return ENXIO;

	*fbfsp = am335x_fbfs[minor];
	return OK;
}

int
am335x_pan_display(int minor, struct fb_var_screeninfo *fbvsp)
{
	return am335x_put_varscreeninfo(minor, fbvsp);
}

int
am335x_intr_handler(void)
{
	int r;
	uint32_t reg;

	/* Read and clear interrupt status. */
	reg = read32(mapped_addr + IRQSTATUS_REG);
	write32(mapped_addr + IRQSTATUS_REG, reg);

	log_debug(&log, "interrupt 0x%x\n", reg);

	/* Handle each type of interrupt */

	if (reg & IRQ_SYNC_LOST) {

		/* disable lcd */
		reg = read32(mapped_addr + RASTER_CTRL_REG);
		reg &= ~RASTER_CTRL_LCDEN;
		write32(mapped_addr + RASTER_CTRL_REG, reg);

		/* re-enable lcd */
		reg = read32(mapped_addr + RASTER_CTRL_REG);
		reg |= RASTER_CTRL_LCDEN;
		write32(mapped_addr + RASTER_CTRL_REG, reg);
		
		return OK; /* nothing else to handle after reset */
	}

	if (reg & IRQ_PL) {

		/* disable lcd */
		reg = read32(mapped_addr + RASTER_CTRL_REG);
		reg &= ~RASTER_CTRL_LCDEN;
		write32(mapped_addr + RASTER_CTRL_REG, reg);

		/* re-enable lcd */
		reg = read32(mapped_addr + RASTER_CTRL_REG);
		reg |= RASTER_CTRL_LCDEN;
		write32(mapped_addr + RASTER_CTRL_REG, reg);
		
		return OK; /* nothing else to handle after reset */
	}

	if (reg & IRQ_EOF0) {
		/* reset DMA address */
		write32(mapped_addr + LCDDMA_FB0_BASE_REG, fb_phys);
		write32(mapped_addr + LCDDMA_FB0_CEILING_REG, fb_phys + fb_size - 1);
	}

	if (reg & IRQ_EOF1) {
		/* reset DMA address */
		write32(mapped_addr + LCDDMA_FB1_BASE_REG, fb_phys);
		write32(mapped_addr + LCDDMA_FB1_CEILING_REG, fb_phys + fb_size - 1);
	}

	if (reg & IRQ_FUF) {
		/* TODO: Handle FUF */
	}

	if (reg & IRQ_ACB) {
		/* TODO: Handle ACB */
	}

	/* re-enable interrupt */
	r = sys_irqenable(&irq_hook_kernel_id);
	if (r != OK) {
		log_warn(&log, "Unable to renable IRQ (r=%d)\n", r);
		return -1;
	}

	return OK;
}
