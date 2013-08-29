#ifndef _CLKCONF_H
#define _CLKCONF_H

/* Clock configuration */
#define CM_FCLKEN1_CORE 0xA00
#define CM_ICLKEN1_CORE 0xA10
#define CM_FCLKEN_WKUP 0xC00
#define CM_ICLKEN_WKUP 0xC10

/* lcdc clocks */
#define CM_PER_LCDC_CLKCTRL 0x018
#define LCDC_CLKCTRL_MODMODE_MASK ((1<<1)|(1<<0))
#define LCDC_CLKCTRL_MODMODE_ENABLE 0x2

#define CM_IDLEST_DPLL_DISP 0x448
#define ST_MN_BYPASS_BIT 8
#define ST_DPLL_CLK_BIT 0

#define CM_CLKSEL_DPLL_DISP 0x454
#define DPLL_MULT_MASK ((0x3ff) << 8)
#define DPLL_MULT_VAL (5 << 8)

#define CM_CLKMODE_DPLL_DISP 0x498
#define DPLL_MN_BYP_MODE 0x4
#define DPLL_LOCK_MODE 0x7


/* i2c clocks */
#define CM_PER_I2C2_CLKCTRL 0x044
#define CM_PER_I2C1_CLKCTRL 0x048
#define CM_WKUP_I2C0_CLKCTRL 0x4B8

int clkconf_init();
int clkconf_get(u32_t clk);
int clkconf_set(u32_t clk, u32_t mask, u32_t value);
int clkconf_release();

#endif
