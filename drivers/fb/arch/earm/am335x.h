#ifndef __FB_AM335X_H
#define __FB_AM335X_H

#include <minix/fb.h>

int am335x_fb_init(int minor, struct device *dev, struct edid_info *info);
int am335x_get_device(int minor, struct device *dev);
int am335x_get_varscreeninfo(int minor, struct fb_var_screeninfo *fbvsp);
int am335x_put_varscreeninfo(int minor, struct fb_var_screeninfo *fbvs_copy);
int am335x_get_fixscreeninfo(int minor, struct fb_fix_screeninfo *fbfsp);
int am335x_pan_display(int minor, struct fb_var_screeninfo *fbvs_copy);
int am335x_intr_handler(void);

#define AM335X_LCD_BASE 0x4830e000
#define AM335X_LCD_SIZE 0x1000

#define PID_REG 0x00
#define CTRL_REG 0x04
#define LIDD_CTRL_REG 0x0C
#define LIDD_CS0_CONF_REG 0x10
#define LIDD_CS0_ADDR_REG 0x14
#define LIDD_CS0_DATA_REG 0x18
#define LIDD_CS1_CONF_REG 0x1C
#define LIDD_CS1_ADDR_REG 0x20
#define LIDD_CS1_DATA_REG 0x24
#define RASTER_CTRL_REG 0x28
#define RASTER_TIMING_0_REG 0x2C
#define RASTER_TIMING_1_REG 0x30
#define RASTER_TIMING_2_REG 0x34
#define RASTER_SUBPANEL_REG 0x38
#define RASTER_SUBPANEL2_REG 0x3C
#define LCDDMA_CTRL_REG 0x40
#define LCDDMA_FB0_BASE_REG 0x44
#define LCDDMA_FB0_CEILING_REG 0x48
#define LCDDMA_FB1_BASE_REG 0x4C
#define LCDDMA_FB1_CEILING_REG 0x50
#define SYSCONFIG_REG 0x54
#define IRQSTATUS_RAW_REG 0x58
#define IRQSTATUS_REG 0x5C
#define IRQENABLE_SET_REG 0x60
#define IRQENABLE_CLEAR_REG 0x64
#define CLKC_ENABLE_REG 0x6C
#define CLKC_RESET_REG 0x70

#define AM335X_LCD_REV_MAJOR(x) ((x >> 8) & 7) /* [10:8] */
#define AM335X_LCD_REV_MINOR(x) (x & 0x3f) /* [5:0] */

/* Expect rev 0.0 -- AM3359 on BBB has PID=0x4f201000 (maj=0x00,min=0x00) */
#define AM335X_LCD_REV_MAJOR_EXPECTED 0x00
#define AM335X_LCD_REV_MINOR_EXPECTED 0x00

#define DPLL_DIV(reg)	((reg & 0x7f)+1)
#define DPLL_MULT(reg)	((reg>>8) & 0x7FF)

#define CTRL_DIV_SHIFT 8

#define RASTER_TIMING_0_HBP_SHIFT 24
#define RASTER_TIMING_0_HFP_SHIFT 16
#define RASTER_TIMING_0_HSW_SHIFT 10
#define RASTER_TIMING_0_PPLLSB_SHIFT 4
#define RASTER_TIMING_0_PPLMSB_SHIFT 3

#define RASTER_TIMING_1_VBP_SHIFT 24
#define RASTER_TIMING_1_VFP_SHIFT 16
#define RASTER_TIMING_1_VSW_SHIFT 10
#define RASTER_TIMING_1_LPP_SHIFT 0

#define RASTER_TIMING_2_HSWHI_SHIFT 27
#define RASTER_TIMING_2_PHSVS (1 << 25)
#define RASTER_TIMING_2_PHSVS_RISE (1 << 24)
#define RASTER_TIMING_2_PHSVS_FALL (0 << 24)
#define RASTER_TIMING_2_IPC (1 << 22)
#define RASTER_TIMING_2_IHS (1 << 21)
#define RASTER_TIMING_2_IVS (1 << 20)
#define RASTER_TIMING_2_ACBI_SHIFT 16
#define RASTER_TIMING_2_ACB_SHIFT 8
#define RASTER_TIMING_2_HBPHI_SHIFT 4
#define RASTER_TIMING_2_LPP_B10_SHIFT 1
#define RASTER_TIMING_2_HFPHI_SHIFT 0

#define CTRL_RASTER_MODE 1

#define LCDDMA_CTRL_TH_FIFO_RDY_SHIFT 8
#define LCDDMA_CTRL_BURST_SIZE_SHIFT 4
#define LCDDMA_CTRL_FB0_FB1 (1 << 0)

#define	RASTER_CTRL_TFT24_UNPACKED	(1 << 26)
#define	RASTER_CTRL_TFT24		(1 << 25)
#define	RASTER_CTRL_STN565		(1 << 24)
#define	RASTER_CTRL_TFTPMAP		(1 << 23)
#define	RASTER_CTRL_NIBMODE		(1 << 22)
#define	RASTER_CTRL_PALMODE_SHIFT	20
#define	PALETTE_PALETTE_AND_DATA	0x00
#define	PALETTE_PALETTE_ONLY		0x01
#define	PALETTE_DATA_ONLY		0x02
#define	RASTER_CTRL_REQDLY_SHIFT	12
#define	RASTER_CTRL_MONO8B		(1 << 9)
#define	RASTER_CTRL_RBORDER		(1 << 8)
#define	RASTER_CTRL_LCDTFT		(1 << 7)
#define	RASTER_CTRL_LCDBW		(1 << 1)
#define	RASTER_CTRL_LCDEN		(1 << 0)

#define	CLKC_ENABLE_DMA		(1 << 2)
#define	CLKC_ENABLE_LDID	(1 << 1)
#define CLKC_ENABLE_CORE	(1 << 0)

#define	CLKC_RESET_MAIN		(1 << 3)
#define	CLKC_RESET_DMA		(1 << 2)
#define	CLKC_RESET_LDID		(1 << 1)
#define	CLKC_RESET_CORE		(1 << 0)

#define	IRQ_EOF1		(1 << 9)
#define	IRQ_EOF0		(1 << 8)
#define	IRQ_PL			(1 << 6)
#define	IRQ_FUF			(1 << 5)
#define	IRQ_ACB			(1 << 3)
#define	IRQ_SYNC_LOST		(1 << 2)
#define	IRQ_RASTER_DONE		(1 << 1)
#define	IRQ_FRAME_DONE		(1 << 0)

#define	SYSCONFIG_STANDBY_FORCE		(0 << 4)
#define	SYSCONFIG_STANDBY_NONE		(1 << 4)
#define	SYSCONFIG_STANDBY_SMART		(2 << 4)
#define	SYSCONFIG_IDLE_FORCE		(0 << 2)
#define	SYSCONFIG_IDLE_NONE		(1 << 2)
#define	SYSCONFIG_IDLE_SMART		(2 << 2)

#endif /* __FB_AM335X_H */
