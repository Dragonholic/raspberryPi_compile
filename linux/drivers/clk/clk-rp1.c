// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2023 Raspberry Pi Ltd.
 *
 * Clock driver for RP1 PCIe multifunction chip.
 */

#include <linux/clk-provider.h>
#include <linux/clkdev.h>
#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/rp1_platform.h>
#include <linux/slab.h>

#include <asm/div64.h>

#include <dt-bindings/clock/rp1.h>

#define PLL_SYS_CS			0x08000
#define PLL_SYS_PWR			0x08004
#define PLL_SYS_FBDIV_INT		0x08008
#define PLL_SYS_FBDIV_FRAC		0x0800c
#define PLL_SYS_PRIM			0x08010
#define PLL_SYS_SEC			0x08014

#define PLL_AUDIO_CS			0x0c000
#define PLL_AUDIO_PWR			0x0c004
#define PLL_AUDIO_FBDIV_INT		0x0c008
#define PLL_AUDIO_FBDIV_FRAC		0x0c00c
#define PLL_AUDIO_PRIM			0x0c010
#define PLL_AUDIO_SEC			0x0c014
#define PLL_AUDIO_TERN			0x0c018

#define PLL_VIDEO_CS			0x10000
#define PLL_VIDEO_PWR			0x10004
#define PLL_VIDEO_FBDIV_INT		0x10008
#define PLL_VIDEO_FBDIV_FRAC		0x1000c
#define PLL_VIDEO_PRIM			0x10010
#define PLL_VIDEO_SEC			0x10014

#define GPCLK_OE_CTRL			0x00000

#define CLK_SYS_CTRL			0x00014
#define CLK_SYS_DIV_INT			0x00018
#define CLK_SYS_SEL			0x00020

#define CLK_SLOW_SYS_CTRL		0x00024
#define CLK_SLOW_SYS_DIV_INT		0x00028
#define CLK_SLOW_SYS_SEL		0x00030

#define CLK_DMA_CTRL			0x00044
#define CLK_DMA_DIV_INT			0x00048
#define CLK_DMA_SEL			0x00050

#define CLK_UART_CTRL			0x00054
#define CLK_UART_DIV_INT		0x00058
#define CLK_UART_SEL			0x00060

#define CLK_ETH_CTRL			0x00064
#define CLK_ETH_DIV_INT			0x00068
#define CLK_ETH_SEL			0x00070

#define CLK_PWM0_CTRL			0x00074
#define CLK_PWM0_DIV_INT		0x00078
#define CLK_PWM0_DIV_FRAC		0x0007c
#define CLK_PWM0_SEL			0x00080

#define CLK_PWM1_CTRL			0x00084
#define CLK_PWM1_DIV_INT		0x00088
#define CLK_PWM1_DIV_FRAC		0x0008c
#define CLK_PWM1_SEL			0x00090

#define CLK_AUDIO_IN_CTRL		0x00094
#define CLK_AUDIO_IN_DIV_INT		0x00098
#define CLK_AUDIO_IN_SEL		0x000a0

#define CLK_AUDIO_OUT_CTRL		0x000a4
#define CLK_AUDIO_OUT_DIV_INT		0x000a8
#define CLK_AUDIO_OUT_SEL		0x000b0

#define CLK_I2S_CTRL			0x000b4
#define CLK_I2S_DIV_INT			0x000b8
#define CLK_I2S_SEL			0x000c0

#define CLK_MIPI0_CFG_CTRL		0x000c4
#define CLK_MIPI0_CFG_DIV_INT		0x000c8
#define CLK_MIPI0_CFG_SEL		0x000d0

#define CLK_MIPI1_CFG_CTRL		0x000d4
#define CLK_MIPI1_CFG_DIV_INT		0x000d8
#define CLK_MIPI1_CFG_SEL		0x000e0

#define CLK_PCIE_AUX_CTRL		0x000e4
#define CLK_PCIE_AUX_DIV_INT		0x000e8
#define CLK_PCIE_AUX_SEL		0x000f0

#define CLK_USBH0_MICROFRAME_CTRL	0x000f4
#define CLK_USBH0_MICROFRAME_DIV_INT	0x000f8
#define CLK_USBH0_MICROFRAME_SEL	0x00100

#define CLK_USBH1_MICROFRAME_CTRL	0x00104
#define CLK_USBH1_MICROFRAME_DIV_INT	0x00108
#define CLK_USBH1_MICROFRAME_SEL	0x00110

#define CLK_USBH0_SUSPEND_CTRL		0x00114
#define CLK_USBH0_SUSPEND_DIV_INT	0x00118
#define CLK_USBH0_SUSPEND_SEL		0x00120

#define CLK_USBH1_SUSPEND_CTRL		0x00124
#define CLK_USBH1_SUSPEND_DIV_INT	0x00128
#define CLK_USBH1_SUSPEND_SEL		0x00130

#define CLK_ETH_TSU_CTRL		0x00134
#define CLK_ETH_TSU_DIV_INT		0x00138
#define CLK_ETH_TSU_SEL			0x00140

#define CLK_ADC_CTRL			0x00144
#define CLK_ADC_DIV_INT			0x00148
#define CLK_ADC_SEL			0x00150

#define CLK_SDIO_TIMER_CTRL		0x00154
#define CLK_SDIO_TIMER_DIV_INT		0x00158
#define CLK_SDIO_TIMER_SEL		0x00160

#define CLK_SDIO_ALT_SRC_CTRL		0x00164
#define CLK_SDIO_ALT_SRC_DIV_INT	0x00168
#define CLK_SDIO_ALT_SRC_SEL		0x00170

#define CLK_GP0_CTRL			0x00174
#define CLK_GP0_DIV_INT			0x00178
#define CLK_GP0_DIV_FRAC		0x0017c
#define CLK_GP0_SEL			0x00180

#define CLK_GP1_CTRL			0x00184
#define CLK_GP1_DIV_INT			0x00188
#define CLK_GP1_DIV_FRAC		0x0018c
#define CLK_GP1_SEL			0x00190

#define CLK_GP2_CTRL			0x00194
#define CLK_GP2_DIV_INT			0x00198
#define CLK_GP2_DIV_FRAC		0x0019c
#define CLK_GP2_SEL			0x001a0

#define CLK_GP3_CTRL			0x001a4
#define CLK_GP3_DIV_INT			0x001a8
#define CLK_GP3_DIV_FRAC		0x001ac
#define CLK_GP3_SEL			0x001b0

#define CLK_GP4_CTRL			0x001b4
#define CLK_GP4_DIV_INT			0x001b8
#define CLK_GP4_DIV_FRAC		0x001bc
#define CLK_GP4_SEL			0x001c0

#define CLK_GP5_CTRL			0x001c4
#define CLK_GP5_DIV_INT			0x001c8
#define CLK_GP5_DIV_FRAC		0x001cc
#define CLK_GP5_SEL			0x001d0

#define CLK_SYS_RESUS_CTRL		0x0020c

#define CLK_SLOW_SYS_RESUS_CTRL		0x00214

#define FC0_REF_KHZ			0x0021c
#define FC0_MIN_KHZ			0x00220
#define FC0_MAX_KHZ			0x00224
#define FC0_DELAY			0x00228
#define FC0_INTERVAL			0x0022c
#define FC0_SRC				0x00230
#define FC0_STATUS			0x00234
#define FC0_RESULT			0x00238
#define FC_SIZE				0x20
#define FC_COUNT			8
#define FC_NUM(idx, off)		((idx) * 32 + (off))

#define AUX_SEL				1

#define VIDEO_CLOCKS_OFFSET		0x4000
#define VIDEO_CLK_VEC_CTRL		(VIDEO_CLOCKS_OFFSET + 0x0000)
#define VIDEO_CLK_VEC_DIV_INT		(VIDEO_CLOCKS_OFFSET + 0x0004)
#define VIDEO_CLK_VEC_SEL		(VIDEO_CLOCKS_OFFSET + 0x000c)
#define VIDEO_CLK_DPI_CTRL		(VIDEO_CLOCKS_OFFSET + 0x0010)
#define VIDEO_CLK_DPI_DIV_INT		(VIDEO_CLOCKS_OFFSET + 0x0014)
#define VIDEO_CLK_DPI_SEL		(VIDEO_CLOCKS_OFFSET + 0x001c)
#define VIDEO_CLK_MIPI0_DPI_CTRL	(VIDEO_CLOCKS_OFFSET + 0x0020)
#define VIDEO_CLK_MIPI0_DPI_DIV_INT	(VIDEO_CLOCKS_OFFSET + 0x0024)
#define VIDEO_CLK_MIPI0_DPI_DIV_FRAC	(VIDEO_CLOCKS_OFFSET + 0x0028)
#define VIDEO_CLK_MIPI0_DPI_SEL		(VIDEO_CLOCKS_OFFSET + 0x002c)
#define VIDEO_CLK_MIPI1_DPI_CTRL	(VIDEO_CLOCKS_OFFSET + 0x0030)
#define VIDEO_CLK_MIPI1_DPI_DIV_INT	(VIDEO_CLOCKS_OFFSET + 0x0034)
#define VIDEO_CLK_MIPI1_DPI_DIV_FRAC	(VIDEO_CLOCKS_OFFSET + 0x0038)
#define VIDEO_CLK_MIPI1_DPI_SEL		(VIDEO_CLOCKS_OFFSET + 0x003c)

#define DIV_INT_8BIT_MAX		0x000000ffu /* max divide for most clocks */
#define DIV_INT_16BIT_MAX		0x0000ffffu /* max divide for GPx, PWM */
#define DIV_INT_24BIT_MAX               0x00ffffffu /* max divide for CLK_SYS */

#define FC0_STATUS_DONE			BIT(4)
#define FC0_STATUS_RUNNING		BIT(8)
#define FC0_RESULT_FRAC_SHIFT		5

#define PLL_PRIM_DIV1_SHIFT		16
#define PLL_PRIM_DIV1_MASK		0x00070000
#define PLL_PRIM_DIV2_SHIFT		12
#define PLL_PRIM_DIV2_MASK		0x00007000

#define PLL_SEC_DIV_SHIFT		8
#define PLL_SEC_DIV_WIDTH		5
#define PLL_SEC_DIV_MASK		0x00001f00

#define PLL_CS_LOCK			BIT(31)
#define PLL_CS_REFDIV_SHIFT		0

#define PLL_PWR_PD			BIT(0)
#define PLL_PWR_DACPD			BIT(1)
#define PLL_PWR_DSMPD			BIT(2)
#define PLL_PWR_POSTDIVPD		BIT(3)
#define PLL_PWR_4PHASEPD		BIT(4)
#define PLL_PWR_VCOPD			BIT(5)
#define PLL_PWR_MASK			0x0000003f

#define PLL_SEC_RST			BIT(16)
#define PLL_SEC_IMPL			BIT(31)

/* PLL phase output for both PRI and SEC */
#define PLL_PH_EN			BIT(4)
#define PLL_PH_PHASE_SHIFT		0

#define RP1_PLL_PHASE_0			0
#define RP1_PLL_PHASE_90		1
#define RP1_PLL_PHASE_180		2
#define RP1_PLL_PHASE_270		3

/* Clock fields for all clocks */
#define CLK_CTRL_ENABLE			BIT(11)
#define CLK_CTRL_AUXSRC_MASK		0x000003e0
#define CLK_CTRL_AUXSRC_SHIFT		5
#define CLK_CTRL_SRC_SHIFT		0
#define CLK_DIV_FRAC_BITS		16

#define KHz				1000
#define MHz				(KHz * KHz)
#define LOCK_TIMEOUT_NS			100000000
#define FC_TIMEOUT_NS			100000000

#define MAX_CLK_PARENTS	16

#define MEASURE_CLOCK_RATE
const char * const fc0_ref_clk_name = "clk_slow_sys";

#define ABS_DIFF(a, b) ((a) > (b) ? (a) - (b) : (b) - (a))
#define DIV_NEAREST(a, b) (((a) + ((b) >> 1)) / (b))
#define DIV_U64_NEAREST(a, b) div_u64(((a) + ((b) >> 1)), (b))

/*
 * Names of the reference clock for the pll cores.  This name must match
 * the DT reference clock-output-name.
 */
static const char *const ref_clock = "xosc";

/*
 * Secondary PLL channel output divider table.
 * Divider values range from 8 to 19.
 * Invalid values default to 19
 */
static const struct clk_div_table pll_sec_div_table[] = {
	{ 0x00, 19 },
	{ 0x01, 19 },
	{ 0x02, 19 },
	{ 0x03, 19 },
	{ 0x04, 19 },
	{ 0x05, 19 },
	{ 0x06, 19 },
	{ 0x07, 19 },
	{ 0x08,  8 },
	{ 0x09,  9 },
	{ 0x0a, 10 },
	{ 0x0b, 11 },
	{ 0x0c, 12 },
	{ 0x0d, 13 },
	{ 0x0e, 14 },
	{ 0x0f, 15 },
	{ 0x10, 16 },
	{ 0x11, 17 },
	{ 0x12, 18 },
	{ 0x13, 19 },
	{ 0x14, 19 },
	{ 0x15, 19 },
	{ 0x16, 19 },
	{ 0x17, 19 },
	{ 0x18, 19 },
	{ 0x19, 19 },
	{ 0x1a, 19 },
	{ 0x1b, 19 },
	{ 0x1c, 19 },
	{ 0x1d, 19 },
	{ 0x1e, 19 },
	{ 0x1f, 19 },
	{ 0 }
};

struct rp1_clockman {
	struct device *dev;
	void __iomem *regs;
	spinlock_t regs_lock; /* spinlock for all clocks */

	/* Must be last */
	struct clk_hw_onecell_data onecell;
};

struct rp1_pll_core_data {
	const char *name;
	u32 cs_reg;
	u32 pwr_reg;
	u32 fbdiv_int_reg;
	u32 fbdiv_frac_reg;
	unsigned long flags;
	u32 fc0_src;
};

struct rp1_pll_data {
	const char *name;
	const char *source_pll;
	u32 ctrl_reg;
	unsigned long flags;
	u32 fc0_src;
};

struct rp1_pll_ph_data {
	const char *name;
	const char *source_pll;
	unsigned int phase;
	unsigned int fixed_divider;
	u32 ph_reg;
	unsigned long flags;
	u32 fc0_src;
};

struct rp1_pll_divider_data {
	const char *name;
	const char *source_pll;
	u32 sec_reg;
	unsigned long flags;
	u32 fc0_src;
};

struct rp1_clock_data {
	const char *name;
	const char *const parents[MAX_CLK_PARENTS];
	int num_std_parents;
	int num_aux_parents;
	unsigned long flags;
	u32 oe_mask;
	u32 clk_src_mask;
	u32 ctrl_reg;
	u32 div_int_reg;
	u32 div_frac_reg;
	u32 sel_reg;
	u32 div_int_max;
	unsigned long max_freq;
	u32 fc0_src;
};

struct rp1_pll_core {
	struct clk_hw hw;
	struct rp1_clockman *clockman;
	const struct rp1_pll_core_data *data;
	unsigned long cached_rate;
};

struct rp1_pll {
	struct clk_hw hw;
	struct clk_divider div;
	struct rp1_clockman *clockman;
	const struct rp1_pll_data *data;
	unsigned long cached_rate;
};

struct rp1_pll_ph {
	struct clk_hw hw;
	struct rp1_clockman *clockman;
	const struct rp1_pll_ph_data *data;
};

struct rp1_clock {
	struct clk_hw hw;
	struct rp1_clockman *clockman;
	const struct rp1_clock_data *data;
	unsigned long cached_rate;
};

struct rp1_varsrc {
	struct clk_hw hw;
	struct rp1_clockman *clockman;
	unsigned long rate;
};

struct rp1_clk_change {
	struct clk_hw *hw;
	unsigned long new_rate;
};

struct rp1_clk_change rp1_clk_chg_tree[3];

static struct clk_hw *clk_xosc;
static struct clk_hw *clk_audio;
static struct clk_hw *clk_i2s;

static void rp1_debugfs_regset(struct rp1_clockman *clockman, u32 base,
			       const struct debugfs_reg32 *regs,
			       size_t nregs, struct dentry *dentry)
{
	struct debugfs_regset32 *regset;

	regset = devm_kzalloc(clockman->dev, sizeof(*regset), GFP_KERNEL);
	if (!regset)
		return;

	regset->regs = regs;
	regset->nregs = nregs;
	regset->base = clockman->regs + base;

	debugfs_create_regset32("regdump", 0444, dentry, regset);
}

static inline u32 set_register_field(u32 reg, u32 val, u32 mask, u32 shift)
{
	reg &= ~mask;
	reg |= (val << shift) & mask;
	return reg;
}

static inline
void clockman_write(struct rp1_clockman *clockman, u32 reg, u32 val)
{
	writel(val, clockman->regs + reg);
}

static inline u32 clockman_read(struct rp1_clockman *clockman, u32 reg)
{
	return readl(clockman->regs + reg);
}

#ifdef MEASURE_CLOCK_RATE
static unsigned long clockman_measure_clock(struct rp1_clockman *clockman,
					    const char *clk_name,
					    unsigned int fc0_src)
{
	struct clk *ref_clk = __clk_lookup(fc0_ref_clk_name);
	unsigned long result;
	ktime_t timeout;
	unsigned int fc_idx, fc_offset, fc_src;

	fc_idx = fc0_src / 32;
	fc_src = fc0_src % 32;

	/* fc_src == 0 is invalid. */
	if (!fc_src || fc_idx >= FC_COUNT)
		return 0;

	fc_offset = fc_idx * FC_SIZE;

	/* Ensure the frequency counter is idle. */
	timeout = ktime_add_ns(ktime_get(), FC_TIMEOUT_NS);
	while (clockman_read(clockman, fc_offset + FC0_STATUS) & FC0_STATUS_RUNNING) {
		if (ktime_after(ktime_get(), timeout)) {
			dev_err(clockman->dev, "%s: FC0 busy timeout\n",
				clk_name);
			return 0;
		}
		cpu_relax();
	}

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, fc_offset + FC0_REF_KHZ,
		       clk_get_rate(ref_clk) / KHz);
	clockman_write(clockman, fc_offset + FC0_MIN_KHZ, 0);
	clockman_write(clockman, fc_offset + FC0_MAX_KHZ, 0x1ffffff);
	clockman_write(clockman, fc_offset + FC0_INTERVAL, 8);
	clockman_write(clockman, fc_offset + FC0_DELAY, 7);
	clockman_write(clockman, fc_offset + FC0_SRC, fc_src);
	spin_unlock(&clockman->regs_lock);

	/* Ensure the frequency counter is idle. */
	timeout = ktime_add_ns(ktime_get(), FC_TIMEOUT_NS);
	while (!(clockman_read(clockman, fc_offset + FC0_STATUS) & FC0_STATUS_DONE)) {
		if (ktime_after(ktime_get(), timeout)) {
			dev_err(clockman->dev, "%s: FC0 wait timeout\n",
				clk_name);
			return 0;
		}
		cpu_relax();
	}

	result = clockman_read(clockman, fc_offset + FC0_RESULT);

	/* Disable FC0 */
	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, fc_offset + FC0_SRC, 0);
	spin_unlock(&clockman->regs_lock);

	return result;
}
#endif

static int rp1_pll_core_is_on(struct clk_hw *hw)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;
	u32 pwr = clockman_read(clockman, data->pwr_reg);

	return (pwr & PLL_PWR_PD) || (pwr & PLL_PWR_POSTDIVPD);
}

static int rp1_pll_core_on(struct clk_hw *hw)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;
	u32 fbdiv_frac;
	ktime_t timeout;

	spin_lock(&clockman->regs_lock);

	if (!(clockman_read(clockman, data->cs_reg) & PLL_CS_LOCK)) {
		/* Reset to a known state. */
		clockman_write(clockman, data->pwr_reg, PLL_PWR_MASK);
		clockman_write(clockman, data->fbdiv_int_reg, 20);
		clockman_write(clockman, data->fbdiv_frac_reg, 0);
		clockman_write(clockman, data->cs_reg, 1 << PLL_CS_REFDIV_SHIFT);
	}

	/* Come out of reset. */
	fbdiv_frac = clockman_read(clockman, data->fbdiv_frac_reg);
	clockman_write(clockman, data->pwr_reg, fbdiv_frac ? 0 : PLL_PWR_DSMPD);
	spin_unlock(&clockman->regs_lock);

	/* Wait for the PLL to lock. */
	timeout = ktime_add_ns(ktime_get(), LOCK_TIMEOUT_NS);
	while (!(clockman_read(clockman, data->cs_reg) & PLL_CS_LOCK)) {
		if (ktime_after(ktime_get(), timeout)) {
			dev_err(clockman->dev, "%s: can't lock PLL\n",
				clk_hw_get_name(hw));
			return -ETIMEDOUT;
		}
		cpu_relax();
	}

	return 0;
}

static void rp1_pll_core_off(struct clk_hw *hw)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->pwr_reg, 0);
	spin_unlock(&clockman->regs_lock);
}

static inline unsigned long get_pll_core_divider(struct clk_hw *hw,
						 unsigned long rate,
						 unsigned long parent_rate,
						 u32 *div_int, u32 *div_frac)
{
	unsigned long calc_rate;
	u32 fbdiv_int, fbdiv_frac;
	u64 div_fp64; /* 32.32 fixed point fraction. */

	/* Factor of reference clock to VCO frequency. */
	div_fp64 = (u64)(rate) << 32;
	div_fp64 = DIV_U64_NEAREST(div_fp64, parent_rate);

	/* Round the fractional component at 24 bits. */
	div_fp64 += 1 << (32 - 24 - 1);

	fbdiv_int = div_fp64 >> 32;
	fbdiv_frac = (div_fp64 >> (32 - 24)) & 0xffffff;

	calc_rate =
		((u64)parent_rate * (((u64)fbdiv_int << 24) + fbdiv_frac) + (1 << 23)) >> 24;

	*div_int = fbdiv_int;
	*div_frac = fbdiv_frac;

	return calc_rate;
}

static int rp1_pll_core_set_rate(struct clk_hw *hw,
				 unsigned long rate, unsigned long parent_rate)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;
	unsigned long calc_rate;
	u32 fbdiv_int, fbdiv_frac;

	// todo: is this needed??
	//rp1_pll_off(hw);

	/* Disable dividers to start with. */
	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->fbdiv_int_reg, 0);
	clockman_write(clockman, data->fbdiv_frac_reg, 0);
	spin_unlock(&clockman->regs_lock);

	calc_rate = get_pll_core_divider(hw, rate, parent_rate,
					 &fbdiv_int, &fbdiv_frac);

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->pwr_reg, fbdiv_frac ? 0 : PLL_PWR_DSMPD);
	clockman_write(clockman, data->fbdiv_int_reg, fbdiv_int);
	clockman_write(clockman, data->fbdiv_frac_reg, fbdiv_frac);
	spin_unlock(&clockman->regs_lock);

	/* Check that reference frequency is no greater than VCO / 16. */
	BUG_ON(parent_rate > (rate / 16));

	pll_core->cached_rate = calc_rate;

	spin_lock(&clockman->regs_lock);
	/* Don't need to divide ref unless parent_rate > (output freq / 16) */
	clockman_write(clockman, data->cs_reg,
		       clockman_read(clockman, data->cs_reg) |
				     (1 << PLL_CS_REFDIV_SHIFT));
	spin_unlock(&clockman->regs_lock);

	return 0;
}

static unsigned long rp1_pll_core_recalc_rate(struct clk_hw *hw,
					      unsigned long parent_rate)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;
	u32 fbdiv_int, fbdiv_frac;
	unsigned long calc_rate;

	fbdiv_int = clockman_read(clockman, data->fbdiv_int_reg);
	fbdiv_frac = clockman_read(clockman, data->fbdiv_frac_reg);
	calc_rate =
		((u64)parent_rate * (((u64)fbdiv_int << 24) + fbdiv_frac) + (1 << 23)) >> 24;

	return calc_rate;
}

static long rp1_pll_core_round_rate(struct clk_hw *hw, unsigned long rate,
				    unsigned long *parent_rate)
{
	u32 fbdiv_int, fbdiv_frac;
	long calc_rate;

	calc_rate = get_pll_core_divider(hw, rate, *parent_rate,
					 &fbdiv_int, &fbdiv_frac);
	return calc_rate;
}

static void rp1_pll_core_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct rp1_pll_core *pll_core = container_of(hw, struct rp1_pll_core, hw);
	struct rp1_clockman *clockman = pll_core->clockman;
	const struct rp1_pll_core_data *data = pll_core->data;
	struct debugfs_reg32 *regs;

	regs = devm_kcalloc(clockman->dev, 4, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return;

	regs[0].name = "cs";
	regs[0].offset = data->cs_reg;
	regs[1].name = "pwr";
	regs[1].offset = data->pwr_reg;
	regs[2].name = "fbdiv_int";
	regs[2].offset = data->fbdiv_int_reg;
	regs[3].name = "fbdiv_frac";
	regs[3].offset = data->fbdiv_frac_reg;

	rp1_debugfs_regset(clockman, 0, regs, 4, dentry);
}

static void get_pll_prim_dividers(unsigned long rate, unsigned long parent_rate,
				  u32 *divider1, u32 *divider2)
{
	unsigned int div1, div2;
	unsigned int best_div1 = 7, best_div2 = 7;
	unsigned long best_rate_diff =
		ABS_DIFF(DIV_ROUND_CLOSEST(parent_rate, best_div1 * best_div2), rate);
	long rate_diff, calc_rate;

	for (div1 = 1; div1 <= 7; div1++) {
		for (div2 = 1; div2 <= div1; div2++) {
			calc_rate = DIV_ROUND_CLOSEST(parent_rate, div1 * div2);
			rate_diff = ABS_DIFF(calc_rate, rate);

			if (calc_rate == rate) {
				best_div1 = div1;
				best_div2 = div2;
				goto done;
			} else if (rate_diff < best_rate_diff) {
				best_div1 = div1;
				best_div2 = div2;
				best_rate_diff = rate_diff;
			}
		}
	}

done:
	*divider1 = best_div1;
	*divider2 = best_div2;
}

static int rp1_pll_set_rate(struct clk_hw *hw,
			    unsigned long rate, unsigned long parent_rate)
{
	struct rp1_pll *pll = container_of(hw, struct rp1_pll, hw);
	struct rp1_clockman *clockman = pll->clockman;
	const struct rp1_pll_data *data = pll->data;
	u32 prim, prim_div1, prim_div2;

	get_pll_prim_dividers(rate, parent_rate, &prim_div1, &prim_div2);

	spin_lock(&clockman->regs_lock);
	prim = clockman_read(clockman, data->ctrl_reg);
	prim = set_register_field(prim, prim_div1, PLL_PRIM_DIV1_MASK,
				  PLL_PRIM_DIV1_SHIFT);
	prim = set_register_field(prim, prim_div2, PLL_PRIM_DIV2_MASK,
				  PLL_PRIM_DIV2_SHIFT);
	clockman_write(clockman, data->ctrl_reg, prim);
	spin_unlock(&clockman->regs_lock);

#ifdef MEASURE_CLOCK_RATE
	clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static unsigned long rp1_pll_recalc_rate(struct clk_hw *hw,
					 unsigned long parent_rate)
{
	struct rp1_pll *pll = container_of(hw, struct rp1_pll, hw);
	struct rp1_clockman *clockman = pll->clockman;
	const struct rp1_pll_data *data = pll->data;
	u32 prim, prim_div1, prim_div2;

	prim = clockman_read(clockman, data->ctrl_reg);
	prim_div1 = (prim & PLL_PRIM_DIV1_MASK) >> PLL_PRIM_DIV1_SHIFT;
	prim_div2 = (prim & PLL_PRIM_DIV2_MASK) >> PLL_PRIM_DIV2_SHIFT;

	if (!prim_div1 || !prim_div2) {
		dev_err(clockman->dev, "%s: (%s) zero divider value\n",
			__func__, data->name);
		return 0;
	}

	return DIV_ROUND_CLOSEST(parent_rate, prim_div1 * prim_div2);
}

static long rp1_pll_round_rate(struct clk_hw *hw, unsigned long rate,
			       unsigned long *parent_rate)
{
	const struct rp1_clk_change *chg = &rp1_clk_chg_tree[1];
	u32 div1, div2;

	if (chg->hw == hw && chg->new_rate == rate)
		*parent_rate = chg[1].new_rate;

	get_pll_prim_dividers(rate, *parent_rate, &div1, &div2);

	return DIV_ROUND_CLOSEST(*parent_rate, div1 * div2);
}

static void rp1_pll_debug_init(struct clk_hw *hw,
			       struct dentry *dentry)
{
	struct rp1_pll *pll = container_of(hw, struct rp1_pll, hw);
	struct rp1_clockman *clockman = pll->clockman;
	const struct rp1_pll_data *data = pll->data;
	struct debugfs_reg32 *regs;

	regs = devm_kcalloc(clockman->dev, 1, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return;

	regs[0].name = "prim";
	regs[0].offset = data->ctrl_reg;

	rp1_debugfs_regset(clockman, 0, regs, 1, dentry);
}

static int rp1_pll_ph_is_on(struct clk_hw *hw)
{
	struct rp1_pll_ph *pll = container_of(hw, struct rp1_pll_ph, hw);
	struct rp1_clockman *clockman = pll->clockman;
	const struct rp1_pll_ph_data *data = pll->data;

	return !!(clockman_read(clockman, data->ph_reg) & PLL_PH_EN);
}

static int rp1_pll_ph_on(struct clk_hw *hw)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	struct rp1_clockman *clockman = pll_ph->clockman;
	const struct rp1_pll_ph_data *data = pll_ph->data;
	u32 ph_reg;

	/* todo: ensure pri/sec is enabled! */
	spin_lock(&clockman->regs_lock);
	ph_reg = clockman_read(clockman, data->ph_reg);
	ph_reg |= data->phase << PLL_PH_PHASE_SHIFT;
	ph_reg |= PLL_PH_EN;
	clockman_write(clockman, data->ph_reg, ph_reg);
	spin_unlock(&clockman->regs_lock);

#ifdef MEASURE_CLOCK_RATE
	clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static void rp1_pll_ph_off(struct clk_hw *hw)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	struct rp1_clockman *clockman = pll_ph->clockman;
	const struct rp1_pll_ph_data *data = pll_ph->data;

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->ph_reg,
		       clockman_read(clockman, data->ph_reg) & ~PLL_PH_EN);
	spin_unlock(&clockman->regs_lock);
}

static int rp1_pll_ph_set_rate(struct clk_hw *hw,
			       unsigned long rate, unsigned long parent_rate)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	const struct rp1_pll_ph_data *data = pll_ph->data;
	struct rp1_clockman *clockman = pll_ph->clockman;

	/* Nothing really to do here! */
	WARN_ON(data->fixed_divider != 1 && data->fixed_divider != 2);
	WARN_ON(rate != parent_rate / data->fixed_divider);

#ifdef MEASURE_CLOCK_RATE
	if (rp1_pll_ph_is_on(hw))
		clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static unsigned long rp1_pll_ph_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	const struct rp1_pll_ph_data *data = pll_ph->data;

	return parent_rate / data->fixed_divider;
}

static long rp1_pll_ph_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *parent_rate)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	const struct rp1_pll_ph_data *data = pll_ph->data;

	return *parent_rate / data->fixed_divider;
}

static void rp1_pll_ph_debug_init(struct clk_hw *hw,
				  struct dentry *dentry)
{
	struct rp1_pll_ph *pll_ph = container_of(hw, struct rp1_pll_ph, hw);
	const struct rp1_pll_ph_data *data = pll_ph->data;
	struct rp1_clockman *clockman = pll_ph->clockman;
	struct debugfs_reg32 *regs;

	regs = devm_kcalloc(clockman->dev, 1, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return;

	regs[0].name = "ph_reg";
	regs[0].offset = data->ph_reg;

	rp1_debugfs_regset(clockman, 0, regs, 1, dentry);
}

static int rp1_pll_divider_is_on(struct clk_hw *hw)
{
	struct rp1_pll *divider = container_of(hw, struct rp1_pll, div.hw);
	struct rp1_clockman *clockman = divider->clockman;
	const struct rp1_pll_data *data = divider->data;

	return !(clockman_read(clockman, data->ctrl_reg) & PLL_SEC_RST);
}

static int rp1_pll_divider_on(struct clk_hw *hw)
{
	struct rp1_pll *divider = container_of(hw, struct rp1_pll, div.hw);
	struct rp1_clockman *clockman = divider->clockman;
	const struct rp1_pll_data *data = divider->data;

	spin_lock(&clockman->regs_lock);
	/* Check the implementation bit is set! */
	WARN_ON(!(clockman_read(clockman, data->ctrl_reg) & PLL_SEC_IMPL));
	clockman_write(clockman, data->ctrl_reg,
		       clockman_read(clockman, data->ctrl_reg) & ~PLL_SEC_RST);
	spin_unlock(&clockman->regs_lock);

#ifdef MEASURE_CLOCK_RATE
	clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static void rp1_pll_divider_off(struct clk_hw *hw)
{
	struct rp1_pll *divider = container_of(hw, struct rp1_pll, div.hw);
	struct rp1_clockman *clockman = divider->clockman;
	const struct rp1_pll_data *data = divider->data;

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->ctrl_reg, PLL_SEC_RST);
	spin_unlock(&clockman->regs_lock);
}

static int rp1_pll_divider_set_rate(struct clk_hw *hw,
				    unsigned long rate,
				    unsigned long parent_rate)
{
	struct rp1_pll *divider = container_of(hw, struct rp1_pll, div.hw);
	struct rp1_clockman *clockman = divider->clockman;
	const struct rp1_pll_data *data = divider->data;
	u32 div, sec;

	div = DIV_ROUND_UP_ULL(parent_rate, rate);
	div = clamp(div, 8u, 19u);

	spin_lock(&clockman->regs_lock);
	sec = clockman_read(clockman, data->ctrl_reg);
	sec = set_register_field(sec, div, PLL_SEC_DIV_MASK, PLL_SEC_DIV_SHIFT);

	/* Must keep the divider in reset to change the value. */
	sec |= PLL_SEC_RST;
	clockman_write(clockman, data->ctrl_reg, sec);

	// todo: must sleep 10 pll vco cycles
	sec &= ~PLL_SEC_RST;
	clockman_write(clockman, data->ctrl_reg, sec);
	spin_unlock(&clockman->regs_lock);

#ifdef MEASURE_CLOCK_RATE
	if (rp1_pll_divider_is_on(hw))
		clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static unsigned long rp1_pll_divider_recalc_rate(struct clk_hw *hw,
						 unsigned long parent_rate)
{
	return clk_divider_ops.recalc_rate(hw, parent_rate);
}

static long rp1_pll_divider_round_rate(struct clk_hw *hw,
				       unsigned long rate,
				       unsigned long *parent_rate)
{
	return clk_divider_ops.round_rate(hw, rate, parent_rate);
}

static void rp1_pll_divider_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct rp1_pll *divider = container_of(hw, struct rp1_pll, div.hw);
	struct rp1_clockman *clockman = divider->clockman;
	const struct rp1_pll_data *data = divider->data;
	struct debugfs_reg32 *regs;

	regs = devm_kcalloc(clockman->dev, 1, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return;

	regs[0].name = "sec";
	regs[0].offset = data->ctrl_reg;

	rp1_debugfs_regset(clockman, 0, regs, 1, dentry);
}

static int rp1_clock_is_on(struct clk_hw *hw)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;

	return !!(clockman_read(clockman, data->ctrl_reg) & CLK_CTRL_ENABLE);
}

static unsigned long rp1_clock_recalc_rate(struct clk_hw *hw,
					   unsigned long parent_rate)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;
	u64 calc_rate;
	u64 div;

	u32 frac;

	div = clockman_read(clockman, data->div_int_reg);
	frac = (data->div_frac_reg != 0) ?
		clockman_read(clockman, data->div_frac_reg) : 0;

	/* If the integer portion of the divider is 0, treat it as 2^16 */
	if (!div)
		div = 1 << 16;

	div = (div << CLK_DIV_FRAC_BITS) | (frac >> (32 - CLK_DIV_FRAC_BITS));

	calc_rate = (u64)parent_rate << CLK_DIV_FRAC_BITS;
	calc_rate = div64_u64(calc_rate, div);

	return calc_rate;
}

static int rp1_clock_on(struct clk_hw *hw)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->ctrl_reg,
		       clockman_read(clockman, data->ctrl_reg) | CLK_CTRL_ENABLE);
	/* If this is a GPCLK, turn on the output-enable */
	if (data->oe_mask)
		clockman_write(clockman, GPCLK_OE_CTRL,
			       clockman_read(clockman, GPCLK_OE_CTRL) | data->oe_mask);
	spin_unlock(&clockman->regs_lock);

#ifdef MEASURE_CLOCK_RATE
	clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static void rp1_clock_off(struct clk_hw *hw)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;

	spin_lock(&clockman->regs_lock);
	clockman_write(clockman, data->ctrl_reg,
		       clockman_read(clockman, data->ctrl_reg) & ~CLK_CTRL_ENABLE);
	/* If this is a GPCLK, turn off the output-enable */
	if (data->oe_mask)
		clockman_write(clockman, GPCLK_OE_CTRL,
			       clockman_read(clockman, GPCLK_OE_CTRL) & ~data->oe_mask);
	spin_unlock(&clockman->regs_lock);
}

static u32 rp1_clock_choose_div(unsigned long rate, unsigned long parent_rate,
				const struct rp1_clock_data *data)
{
	u64 div;

	/*
	 * Due to earlier rounding, calculated parent_rate may differ from
	 * expected value. Don't fail on a small discrepancy near unity divide.
	 */
	if (!rate || rate > parent_rate + (parent_rate >> CLK_DIV_FRAC_BITS))
		return 0;

	/*
	 * Always express div in fixed-point format for fractional division;
	 * If no fractional divider is present, the fraction part will be zero.
	 */
	if (data->div_frac_reg) {
		div = (u64)parent_rate << CLK_DIV_FRAC_BITS;
		div = DIV_U64_NEAREST(div, rate);
	} else {
		div = DIV_U64_NEAREST(parent_rate, rate);
		div <<= CLK_DIV_FRAC_BITS;
	}

	div = clamp(div,
		    1ull << CLK_DIV_FRAC_BITS,
		    (u64)data->div_int_max << CLK_DIV_FRAC_BITS);

	return div;
}

static u8 rp1_clock_get_parent(struct clk_hw *hw)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;
	u32 sel, ctrl;
	u8 parent;

	/* Sel is one-hot, so find the first bit set */
	sel = clockman_read(clockman, data->sel_reg);
	parent = ffs(sel) - 1;

	/* sel == 0 implies the parent clock is not enabled yet. */
	if (!sel) {
		/* Read the clock src from the CTRL register instead */
		ctrl = clockman_read(clockman, data->ctrl_reg);
		parent = (ctrl & data->clk_src_mask) >> CLK_CTRL_SRC_SHIFT;
	}

	if (parent >= data->num_std_parents)
		parent = AUX_SEL;

	if (parent == AUX_SEL) {
		/*
		 * Clock parent is an auxiliary source, so get the parent from
		 * the AUXSRC register field.
		 */
		ctrl = clockman_read(clockman, data->ctrl_reg);
		parent = (ctrl & CLK_CTRL_AUXSRC_MASK) >> CLK_CTRL_AUXSRC_SHIFT;
		parent += data->num_std_parents;
	}

	return parent;
}

static int rp1_clock_set_parent(struct clk_hw *hw, u8 index)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;
	u32 ctrl, sel;

	spin_lock(&clockman->regs_lock);
	ctrl = clockman_read(clockman, data->ctrl_reg);

	if (index >= data->num_std_parents) {
		/* This is an aux source request */
		if (index >= data->num_std_parents + data->num_aux_parents)
			return -EINVAL;

		/* Select parent from aux list */
		ctrl = set_register_field(ctrl, index - data->num_std_parents,
					  CLK_CTRL_AUXSRC_MASK,
					  CLK_CTRL_AUXSRC_SHIFT);
		/* Set src to aux list */
		ctrl = set_register_field(ctrl, AUX_SEL, data->clk_src_mask,
					  CLK_CTRL_SRC_SHIFT);
	} else {
		ctrl = set_register_field(ctrl, index, data->clk_src_mask,
					  CLK_CTRL_SRC_SHIFT);
	}

	clockman_write(clockman, data->ctrl_reg, ctrl);
	spin_unlock(&clockman->regs_lock);

	sel = rp1_clock_get_parent(hw);
	WARN(sel != index, "(%s): Parent index req %u returned back %u\n",
	     data->name, index, sel);

	return 0;
}

static int rp1_clock_set_rate_and_parent(struct clk_hw *hw,
					 unsigned long rate,
					 unsigned long parent_rate,
					 u8 parent)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;
	u32 div = rp1_clock_choose_div(rate, parent_rate, data);

	WARN(rate > 4000000000ll, "rate is -ve (%d)\n", (int)rate);

	if (WARN(!div,
		 "clk divider calculated as 0! (%s, rate %ld, parent rate %ld)\n",
		 data->name, rate, parent_rate))
		div = 1 << CLK_DIV_FRAC_BITS;

	spin_lock(&clockman->regs_lock);

	clockman_write(clockman, data->div_int_reg, div >> CLK_DIV_FRAC_BITS);
	if (data->div_frac_reg)
		clockman_write(clockman, data->div_frac_reg, div << (32 - CLK_DIV_FRAC_BITS));

	spin_unlock(&clockman->regs_lock);

	if (parent != 0xff)
		rp1_clock_set_parent(hw, parent);

#ifdef MEASURE_CLOCK_RATE
	if (rp1_clock_is_on(hw))
		clockman_measure_clock(clockman, data->name, data->fc0_src);
#endif
	return 0;
}

static int rp1_clock_set_rate(struct clk_hw *hw, unsigned long rate,
			      unsigned long parent_rate)
{
	return rp1_clock_set_rate_and_parent(hw, rate, parent_rate, 0xff);
}

static unsigned long calc_core_pll_rate(struct clk_hw *pll_hw,
					unsigned long target_rate,
					int *pdiv_prim, int *pdiv_clk)
{
	static const int prim_divs[] = {
		2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 15, 16,
		18, 20, 21, 24, 25, 28, 30, 35, 36, 42, 49,
	};
	const unsigned long xosc_rate = clk_hw_get_rate(clk_xosc);
	const unsigned long core_max = 2400000000;
	const unsigned long core_min = xosc_rate * 16;
	unsigned long best_rate = core_max + 1;
	int best_div_prim = 1, best_div_clk = 1;
	unsigned long core_rate = 0;
	int div_int, div_frac;
	u64 div;
	int i;

	/* Given the target rate, choose a set of divisors/multipliers */
	for (i = 0; i < ARRAY_SIZE(prim_divs); i++) {
		int div_prim = prim_divs[i];
		int div_clk;

		for (div_clk = 1; div_clk <= 256; div_clk++) {
			core_rate = target_rate * div_clk * div_prim;
			if (core_rate >= core_min) {
				if (core_rate < best_rate) {
					best_rate = core_rate;
					best_div_prim = div_prim;
					best_div_clk = div_clk;
				}
				break;
			}
		}
	}

	if (best_rate < core_max) {
		div = ((best_rate << 24) + xosc_rate / 2) / xosc_rate;
		div_int = div >> 24;
		div_frac = div % (1 << 24);
		core_rate = (xosc_rate * ((div_int << 24) + div_frac) + (1 << 23)) >> 24;
	} else {
		core_rate = 0;
	}

	if (pdiv_prim)
		*pdiv_prim = best_div_prim;
	if (pdiv_clk)
		*pdiv_clk = best_div_clk;

	return core_rate;
}

static void rp1_clock_choose_div_and_prate(struct clk_hw *hw,
					   int parent_idx,
					   unsigned long rate,
					   unsigned long *prate,
					   unsigned long *calc_rate)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	const struct rp1_clock_data *data = clock->data;
	struct clk_hw *parent;
	u32 div;
	u64 tmp;
	int i;

	parent = clk_hw_get_parent_by_index(hw, parent_idx);

	for (i = 0; i < ARRAY_SIZE(rp1_clk_chg_tree); i++) {
		const struct rp1_clk_change *chg = &rp1_clk_chg_tree[i];

		if (chg->hw == hw && chg->new_rate == rate) {
			if (i == 2)
				*prate = clk_hw_get_rate(clk_xosc);
			else if (parent == rp1_clk_chg_tree[i + 1].hw)
				*prate = rp1_clk_chg_tree[i + 1].new_rate;
			else
				continue;
			*calc_rate = chg->new_rate;
			return;
		}
	}

	if (hw == clk_i2s && parent == clk_audio) {
		unsigned long core_rate, audio_rate, i2s_rate;
		int div_prim, div_clk;

		core_rate = calc_core_pll_rate(parent, rate, &div_prim, &div_clk);
		audio_rate = DIV_NEAREST(core_rate, div_prim);
		i2s_rate = DIV_NEAREST(audio_rate, div_clk);
		rp1_clk_chg_tree[2].hw = clk_hw_get_parent(parent);
		rp1_clk_chg_tree[2].new_rate = core_rate;
		rp1_clk_chg_tree[1].hw = clk_audio;
		rp1_clk_chg_tree[1].new_rate = audio_rate;
		rp1_clk_chg_tree[0].hw = clk_i2s;
		rp1_clk_chg_tree[0].new_rate = i2s_rate;
		*prate = audio_rate;
		*calc_rate = i2s_rate;
		return;
	}

	*prate = clk_hw_get_rate(parent);
	div = rp1_clock_choose_div(rate, *prate, data);

	if (!div) {
		*calc_rate = 0;
		return;
	}

	/* Recalculate to account for rounding errors */
	tmp = (u64)*prate << CLK_DIV_FRAC_BITS;
	tmp = div_u64(tmp, div);
	/*
	 * Prevent overclocks - if all parent choices result in
	 * a downstream clock in excess of the maximum, then the
	 * call to set the clock will fail.
	 */
	if (tmp > clock->data->max_freq)
		*calc_rate = 0;
	else
		*calc_rate = tmp;
}

static int rp1_clock_determine_rate(struct clk_hw *hw,
				    struct clk_rate_request *req)
{
	struct clk_hw *parent, *best_parent = NULL;
	unsigned long best_rate = 0;
	unsigned long best_prate = 0;
	unsigned long best_rate_diff = ULONG_MAX;
	unsigned long prate, calc_rate;
	size_t i;

	/*
	 * If the NO_REPARENT flag is set, try to use existing parent.
	 */
	if ((clk_hw_get_flags(hw) & CLK_SET_RATE_NO_REPARENT)) {
		i = rp1_clock_get_parent(hw);
		parent = clk_hw_get_parent_by_index(hw, i);
		if (parent) {
			rp1_clock_choose_div_and_prate(hw, i, req->rate, &prate,
						       &calc_rate);
			if (calc_rate > 0) {
				req->best_parent_hw = parent;
				req->best_parent_rate = prate;
				req->rate = calc_rate;
				return 0;
			}
		}
	}

	/*
	 * Select parent clock that results in the closest rate (lower or
	 * higher)
	 */
	for (i = 0; i < clk_hw_get_num_parents(hw); i++) {
		parent = clk_hw_get_parent_by_index(hw, i);
		if (!parent)
			continue;

		rp1_clock_choose_div_and_prate(hw, i, req->rate, &prate,
					       &calc_rate);

		if (ABS_DIFF(calc_rate, req->rate) < best_rate_diff) {
			best_parent = parent;
			best_prate = prate;
			best_rate = calc_rate;
			best_rate_diff = ABS_DIFF(calc_rate, req->rate);

			if (best_rate_diff == 0)
				break;
		}
	}

	if (best_rate == 0)
		return -EINVAL;

	req->best_parent_hw = best_parent;
	req->best_parent_rate = best_prate;
	req->rate = best_rate;

	return 0;
}

static void rp1_clk_debug_init(struct clk_hw *hw, struct dentry *dentry)
{
	struct rp1_clock *clock = container_of(hw, struct rp1_clock, hw);
	struct rp1_clockman *clockman = clock->clockman;
	const struct rp1_clock_data *data = clock->data;
	struct debugfs_reg32 *regs;
	int i;

	regs = devm_kcalloc(clockman->dev, 4, sizeof(*regs), GFP_KERNEL);
	if (!regs)
		return;

	i = 0;
	regs[i].name = "ctrl";
	regs[i++].offset = data->ctrl_reg;
	regs[i].name = "div_int";
	regs[i++].offset = data->div_int_reg;
	regs[i].name = "div_frac";
	regs[i++].offset = data->div_frac_reg;
	regs[i].name = "sel";
	regs[i++].offset = data->sel_reg;

	rp1_debugfs_regset(clockman, 0, regs, i, dentry);
}

static int rp1_varsrc_set_rate(struct clk_hw *hw,
			       unsigned long rate, unsigned long parent_rate)
{
	struct rp1_varsrc *varsrc = container_of(hw, struct rp1_varsrc, hw);

	/*
	 * "varsrc" exists purely to let clock dividers know the frequency
	 * of an externally-managed clock source (such as MIPI DSI byte-clock)
	 * which may change at run-time as a side-effect of some other driver.
	 */
	varsrc->rate = rate;
	return 0;
}

static unsigned long rp1_varsrc_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct rp1_varsrc *varsrc = container_of(hw, struct rp1_varsrc, hw);

	return varsrc->rate;
}

static long rp1_varsrc_round_rate(struct clk_hw *hw, unsigned long rate,
				  unsigned long *parent_rate)
{
	return rate;
}

static const struct clk_ops rp1_pll_core_ops = {
	.is_prepared = rp1_pll_core_is_on,
	.prepare = rp1_pll_core_on,
	.unprepare = rp1_pll_core_off,
	.set_rate = rp1_pll_core_set_rate,
	.recalc_rate = rp1_pll_core_recalc_rate,
	.round_rate = rp1_pll_core_round_rate,
	.debug_init = rp1_pll_core_debug_init,
};

static const struct clk_ops rp1_pll_ops = {
	.set_rate = rp1_pll_set_rate,
	.recalc_rate = rp1_pll_recalc_rate,
	.round_rate = rp1_pll_round_rate,
	.debug_init = rp1_pll_debug_init,
};

static const struct clk_ops rp1_pll_ph_ops = {
	.is_prepared = rp1_pll_ph_is_on,
	.prepare = rp1_pll_ph_on,
	.unprepare = rp1_pll_ph_off,
	.set_rate = rp1_pll_ph_set_rate,
	.recalc_rate = rp1_pll_ph_recalc_rate,
	.round_rate = rp1_pll_ph_round_rate,
	.debug_init = rp1_pll_ph_debug_init,
};

static const struct clk_ops rp1_pll_divider_ops = {
	.is_prepared = rp1_pll_divider_is_on,
	.prepare = rp1_pll_divider_on,
	.unprepare = rp1_pll_divider_off,
	.set_rate = rp1_pll_divider_set_rate,
	.recalc_rate = rp1_pll_divider_recalc_rate,
	.round_rate = rp1_pll_divider_round_rate,
	.debug_init = rp1_pll_divider_debug_init,
};

static const struct clk_ops rp1_clk_ops = {
	.is_prepared = rp1_clock_is_on,
	.prepare = rp1_clock_on,
	.unprepare = rp1_clock_off,
	.recalc_rate = rp1_clock_recalc_rate,
	.get_parent = rp1_clock_get_parent,
	.set_parent = rp1_clock_set_parent,
	.set_rate_and_parent = rp1_clock_set_rate_and_parent,
	.set_rate = rp1_clock_set_rate,
	.determine_rate = rp1_clock_determine_rate,
	.debug_init = rp1_clk_debug_init,
};

static const struct clk_ops rp1_varsrc_ops = {
	.set_rate = rp1_varsrc_set_rate,
	.recalc_rate = rp1_varsrc_recalc_rate,
	.round_rate = rp1_varsrc_round_rate,
};

static bool rp1_clk_is_claimed(const char *name);

static struct clk_hw *rp1_register_pll_core(struct rp1_clockman *clockman,
					    const void *data)
{
	const struct rp1_pll_core_data *pll_core_data = data;
	struct rp1_pll_core *pll_core;
	struct clk_init_data init;
	int ret;

	memset(&init, 0, sizeof(init));

	/* All of the PLL cores derive from the external oscillator. */
	init.parent_names = &ref_clock;
	init.num_parents = 1;
	init.name = pll_core_data->name;
	init.ops = &rp1_pll_core_ops;
	init.flags = pll_core_data->flags | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL;

	pll_core = kzalloc(sizeof(*pll_core), GFP_KERNEL);
	if (!pll_core)
		return NULL;

	pll_core->clockman = clockman;
	pll_core->data = pll_core_data;
	pll_core->hw.init = &init;

	ret = devm_clk_hw_register(clockman->dev, &pll_core->hw);
	if (ret) {
		kfree(pll_core);
		return NULL;
	}

	return &pll_core->hw;
}

static struct clk_hw *rp1_register_pll(struct rp1_clockman *clockman,
				       const void *data)
{
	const struct rp1_pll_data *pll_data = data;
	struct rp1_pll *pll;
	struct clk_init_data init;
	int ret;

	memset(&init, 0, sizeof(init));

	init.parent_names = &pll_data->source_pll;
	init.num_parents = 1;
	init.name = pll_data->name;
	init.ops = &rp1_pll_ops;
	init.flags = pll_data->flags | CLK_IGNORE_UNUSED | CLK_IS_CRITICAL;

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return NULL;

	pll->clockman = clockman;
	pll->data = pll_data;
	pll->hw.init = &init;

	ret = devm_clk_hw_register(clockman->dev, &pll->hw);
	if (ret) {
		kfree(pll);
		return NULL;
	}

	return &pll->hw;
}

static struct clk_hw *rp1_register_pll_ph(struct rp1_clockman *clockman,
					  const void *data)
{
	const struct rp1_pll_ph_data *ph_data = data;
	struct rp1_pll_ph *ph;
	struct clk_init_data init;
	int ret;

	memset(&init, 0, sizeof(init));

	/* All of the PLLs derive from the external oscillator. */
	init.parent_names = &ph_data->source_pll;
	init.num_parents = 1;
	init.name = ph_data->name;
	init.ops = &rp1_pll_ph_ops;
	init.flags = ph_data->flags | CLK_IGNORE_UNUSED;

	ph = kzalloc(sizeof(*ph), GFP_KERNEL);
	if (!ph)
		return NULL;

	ph->clockman = clockman;
	ph->data = ph_data;
	ph->hw.init = &init;

	ret = devm_clk_hw_register(clockman->dev, &ph->hw);
	if (ret) {
		kfree(ph);
		return NULL;
	}

	return &ph->hw;
}

static struct clk_hw *rp1_register_pll_divider(struct rp1_clockman *clockman,
					       const void *data)
{
	const struct rp1_pll_data *divider_data = data;
	struct rp1_pll *divider;
	struct clk_init_data init;
	int ret;

	memset(&init, 0, sizeof(init));

	init.parent_names = &divider_data->source_pll;
	init.num_parents = 1;
	init.name = divider_data->name;
	init.ops = &rp1_pll_divider_ops;
	init.flags = divider_data->flags | CLK_IGNORE_UNUSED;

	divider = devm_kzalloc(clockman->dev, sizeof(*divider), GFP_KERNEL);
	if (!divider)
		return NULL;

	divider->div.reg = clockman->regs + divider_data->ctrl_reg;
	divider->div.shift = PLL_SEC_DIV_SHIFT;
	divider->div.width = PLL_SEC_DIV_WIDTH;
	divider->div.flags = CLK_DIVIDER_ROUND_CLOSEST;
	divider->div.lock = &clockman->regs_lock;
	divider->div.hw.init = &init;
	divider->div.table = pll_sec_div_table;

	if (!rp1_clk_is_claimed(divider_data->source_pll))
		init.flags |= CLK_IS_CRITICAL;
	if (!rp1_clk_is_claimed(divider_data->name))
		divider->div.flags |= CLK_IS_CRITICAL;

	divider->clockman = clockman;
	divider->data = divider_data;

	ret = devm_clk_hw_register(clockman->dev, &divider->div.hw);
	if (ret)
		return ERR_PTR(ret);

	return &divider->div.hw;
}

static struct clk_hw *rp1_register_clock(struct rp1_clockman *clockman,
					 const void *data)
{
	const struct rp1_clock_data *clock_data = data;
	struct rp1_clock *clock;
	struct clk_init_data init;
	int ret;

	BUG_ON(MAX_CLK_PARENTS <
	       clock_data->num_std_parents + clock_data->num_aux_parents);
	/* There must be a gap for the AUX selector */
	BUG_ON((clock_data->num_std_parents > AUX_SEL) &&
	       strcmp("-", clock_data->parents[AUX_SEL]));

	memset(&init, 0, sizeof(init));
	init.parent_names = clock_data->parents;
	init.num_parents =
		clock_data->num_std_parents + clock_data->num_aux_parents;
	init.name = clock_data->name;
	init.flags = clock_data->flags | CLK_IGNORE_UNUSED;
	init.ops = &rp1_clk_ops;

	clock = devm_kzalloc(clockman->dev, sizeof(*clock), GFP_KERNEL);
	if (!clock)
		return NULL;

	clock->clockman = clockman;
	clock->data = clock_data;
	clock->hw.init = &init;

	ret = devm_clk_hw_register(clockman->dev, &clock->hw);
	if (ret)
		return ERR_PTR(ret);

	return &clock->hw;
}

static struct clk_hw *rp1_register_varsrc(struct rp1_clockman *clockman,
					  const void *data)
{
	const char *name = *(char const * const *)data;
	struct rp1_varsrc *clock;
	struct clk_init_data init;
	int ret;

	memset(&init, 0, sizeof(init));
	init.parent_names = &ref_clock;
	init.num_parents = 1;
	init.name = name;
	init.flags = CLK_IGNORE_UNUSED;
	init.ops = &rp1_varsrc_ops;

	clock = devm_kzalloc(clockman->dev, sizeof(*clock), GFP_KERNEL);
	if (!clock)
		return NULL;

	clock->clockman = clockman;
	clock->hw.init = &init;

	ret = devm_clk_hw_register(clockman->dev, &clock->hw);
	if (ret)
		return ERR_PTR(ret);

	return &clock->hw;
}

struct rp1_clk_desc {
	struct clk_hw *(*clk_register)(struct rp1_clockman *clockman,
				       const void *data);
	const void *data;
};

/* Assignment helper macros for different clock types. */
#define _REGISTER(f, ...) { .clk_register = f, .data = __VA_ARGS__ }

#define REGISTER_PLL_CORE(...)	_REGISTER(&rp1_register_pll_core,	\
					  &(struct rp1_pll_core_data)	\
					  {__VA_ARGS__})

#define REGISTER_PLL(...)	_REGISTER(&rp1_register_pll,		\
					  &(struct rp1_pll_data)		\
					  {__VA_ARGS__})

#define REGISTER_PLL_PH(...)	_REGISTER(&rp1_register_pll_ph,		\
					  &(struct rp1_pll_ph_data)	\
					  {__VA_ARGS__})

#define REGISTER_PLL_DIV(...)	_REGISTER(&rp1_register_pll_divider,	\
					  &(struct rp1_pll_data)	\
					  {__VA_ARGS__})

#define REGISTER_CLK(...)	_REGISTER(&rp1_register_clock,		\
					  &(struct rp1_clock_data)	\
					  {__VA_ARGS__})

#define REGISTER_VARSRC(n)	_REGISTER(&rp1_register_varsrc,	&(const char *){n})

static const struct rp1_clk_desc clk_desc_array[] = {
	[RP1_PLL_SYS_CORE] = REGISTER_PLL_CORE(
				.name = "pll_sys_core",
				.cs_reg = PLL_SYS_CS,
				.pwr_reg = PLL_SYS_PWR,
				.fbdiv_int_reg = PLL_SYS_FBDIV_INT,
				.fbdiv_frac_reg = PLL_SYS_FBDIV_FRAC,
				),

	[RP1_PLL_AUDIO_CORE] = REGISTER_PLL_CORE(
				.name = "pll_audio_core",
				.cs_reg = PLL_AUDIO_CS,
				.pwr_reg = PLL_AUDIO_PWR,
				.fbdiv_int_reg = PLL_AUDIO_FBDIV_INT,
				.fbdiv_frac_reg = PLL_AUDIO_FBDIV_FRAC,
				),

	[RP1_PLL_VIDEO_CORE] = REGISTER_PLL_CORE(
				.name = "pll_video_core",
				.cs_reg = PLL_VIDEO_CS,
				.pwr_reg = PLL_VIDEO_PWR,
				.fbdiv_int_reg = PLL_VIDEO_FBDIV_INT,
				.fbdiv_frac_reg = PLL_VIDEO_FBDIV_FRAC,
				),

	[RP1_PLL_SYS] = REGISTER_PLL(
				.name = "pll_sys",
				.source_pll = "pll_sys_core",
				.ctrl_reg = PLL_SYS_PRIM,
				.fc0_src = FC_NUM(0, 2),
				),

	[RP1_PLL_AUDIO] = REGISTER_PLL(
				.name = "pll_audio",
				.source_pll = "pll_audio_core",
				.ctrl_reg = PLL_AUDIO_PRIM,
				.fc0_src = FC_NUM(4, 2),
				.flags = CLK_SET_RATE_PARENT,
				),

	[RP1_PLL_VIDEO] = REGISTER_PLL(
				.name = "pll_video",
				.source_pll = "pll_video_core",
				.ctrl_reg = PLL_VIDEO_PRIM,
				.fc0_src = FC_NUM(3, 2),
				),

	[RP1_PLL_SYS_PRI_PH] = REGISTER_PLL_PH(
				.name = "pll_sys_pri_ph",
				.source_pll = "pll_sys",
				.ph_reg = PLL_SYS_PRIM,
				.fixed_divider = 2,
				.phase = RP1_PLL_PHASE_0,
				.fc0_src = FC_NUM(1, 2),
				),

	[RP1_PLL_AUDIO_PRI_PH] = REGISTER_PLL_PH(
				.name = "pll_audio_pri_ph",
				.source_pll = "pll_audio",
				.ph_reg = PLL_AUDIO_PRIM,
				.fixed_divider = 2,
				.phase = RP1_PLL_PHASE_0,
				.fc0_src = FC_NUM(5, 1),
				),

	[RP1_PLL_VIDEO_PRI_PH] = REGISTER_PLL_PH(
				.name = "pll_video_pri_ph",
				.source_pll = "pll_video",
				.ph_reg = PLL_VIDEO_PRIM,
				.fixed_divider = 2,
				.phase = RP1_PLL_PHASE_0,
				.fc0_src = FC_NUM(4, 3),
				),

	[RP1_PLL_SYS_SEC] = REGISTER_PLL_DIV(
				.name = "pll_sys_sec",
				.source_pll = "pll_sys_core",
				.ctrl_reg = PLL_SYS_SEC,
				.fc0_src = FC_NUM(2, 2),
				),

	[RP1_PLL_AUDIO_SEC] = REGISTER_PLL_DIV(
				.name = "pll_audio_sec",
				.source_pll = "pll_audio_core",
				.ctrl_reg = PLL_AUDIO_SEC,
				.fc0_src = FC_NUM(6, 2),
				),

	[RP1_PLL_VIDEO_SEC] = REGISTER_PLL_DIV(
				.name = "pll_video_sec",
				.source_pll = "pll_video_core",
				.ctrl_reg = PLL_VIDEO_SEC,
				.fc0_src = FC_NUM(5, 3),
				),

	[RP1_PLL_AUDIO_TERN] = REGISTER_PLL_DIV(
				.name = "pll_audio_tern",
				.source_pll = "pll_audio_core",
				.ctrl_reg = PLL_AUDIO_TERN,
				.fc0_src = FC_NUM(6, 2),
				),

	[RP1_CLK_SYS] = REGISTER_CLK(
				.name = "clk_sys",
				.parents = {"xosc", "-", "pll_sys"},
				.num_std_parents = 3,
				.num_aux_parents = 0,
				.ctrl_reg = CLK_SYS_CTRL,
				.div_int_reg = CLK_SYS_DIV_INT,
				.sel_reg = CLK_SYS_SEL,
				.div_int_max = DIV_INT_24BIT_MAX,
				.max_freq = 200 * MHz,
				.fc0_src = FC_NUM(0, 4),
				.clk_src_mask = 0x3,
				),

	[RP1_CLK_SLOW_SYS] = REGISTER_CLK(
				.name = "clk_slow_sys",
				.parents = {"xosc"},
				.num_std_parents = 1,
				.num_aux_parents = 0,
				.ctrl_reg = CLK_SLOW_SYS_CTRL,
				.div_int_reg = CLK_SLOW_SYS_DIV_INT,
				.sel_reg = CLK_SLOW_SYS_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(1, 4),
				.clk_src_mask = 0x1,
				),

	[RP1_CLK_UART] = REGISTER_CLK(
				.name = "clk_uart",
				.parents = {"pll_sys_pri_ph",
					    "pll_video",
					    "xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 9,
				.ctrl_reg = CLK_UART_CTRL,
				.div_int_reg = CLK_UART_DIV_INT,
				.sel_reg = CLK_UART_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(6, 7),
				),

	[RP1_CLK_ETH] = REGISTER_CLK(
				.name = "clk_eth",
				.parents = {"pll_sys_sec",
					    "pll_sys",
					    "pll_video_sec",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 9,
				.ctrl_reg = CLK_ETH_CTRL,
				.div_int_reg = CLK_ETH_DIV_INT,
				.sel_reg = CLK_ETH_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 125 * MHz,
				.fc0_src = FC_NUM(4, 6),
				),

	[RP1_CLK_PWM0] = REGISTER_CLK(
				.name = "clk_pwm0",
				.parents = {"", // "pll_audio_pri_ph",
					    "pll_video_sec",
					    "xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 9,
				.ctrl_reg = CLK_PWM0_CTRL,
				.div_int_reg = CLK_PWM0_DIV_INT,
				.div_frac_reg = CLK_PWM0_DIV_FRAC,
				.sel_reg = CLK_PWM0_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 76800 * KHz,
				.fc0_src = FC_NUM(0, 5),
				),

	[RP1_CLK_PWM1] = REGISTER_CLK(
				.name = "clk_pwm1",
				.parents = {"", // "pll_audio_pri_ph",
					    "pll_video_sec",
					    "xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 9,
				.ctrl_reg = CLK_PWM1_CTRL,
				.div_int_reg = CLK_PWM1_DIV_INT,
				.div_frac_reg = CLK_PWM1_DIV_FRAC,
				.sel_reg = CLK_PWM1_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 76800 * KHz,
				.fc0_src = FC_NUM(1, 5),
				),

	[RP1_CLK_AUDIO_IN] = REGISTER_CLK(
				.name = "clk_audio_in",
				.parents = {"", //"pll_audio",
					    "", //"pll_audio_pri_ph",
					    "", //"pll_audio_sec",
					    "pll_video_sec",
					    "xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 11,
				.ctrl_reg = CLK_AUDIO_IN_CTRL,
				.div_int_reg = CLK_AUDIO_IN_DIV_INT,
				.sel_reg = CLK_AUDIO_IN_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 76800 * KHz,
				.fc0_src = FC_NUM(2, 5),
				),

	[RP1_CLK_AUDIO_OUT] = REGISTER_CLK(
				.name = "clk_audio_out",
				.parents = {"", //"pll_audio",
					    "", //"pll_audio_sec",
					    "pll_video_sec",
					    "xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 10,
				.ctrl_reg = CLK_AUDIO_OUT_CTRL,
				.div_int_reg = CLK_AUDIO_OUT_DIV_INT,
				.sel_reg = CLK_AUDIO_OUT_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 153600 * KHz,
				.fc0_src = FC_NUM(3, 5),
				),

	[RP1_CLK_I2S] = REGISTER_CLK(
				.name = "clk_i2s",
				.parents = {"xosc",
					    "pll_audio",
					    "pll_audio_sec",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 9,
				.ctrl_reg = CLK_I2S_CTRL,
				.div_int_reg = CLK_I2S_DIV_INT,
				.sel_reg = CLK_I2S_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(4, 4),
				.flags = CLK_SET_RATE_PARENT,
				),

	[RP1_CLK_MIPI0_CFG] = REGISTER_CLK(
				.name = "clk_mipi0_cfg",
				.parents = {"xosc"},
				.num_std_parents = 0,
				.num_aux_parents = 1,
				.ctrl_reg = CLK_MIPI0_CFG_CTRL,
				.div_int_reg = CLK_MIPI0_CFG_DIV_INT,
				.sel_reg = CLK_MIPI0_CFG_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(4, 5),
				),

	[RP1_CLK_MIPI1_CFG] = REGISTER_CLK(
				.name = "clk_mipi1_cfg",
				.parents = {"xosc"},
				.num_std_parents = 0,
				.num_aux_parents = 1,
				.ctrl_reg = CLK_MIPI1_CFG_CTRL,
				.div_int_reg = CLK_MIPI1_CFG_DIV_INT,
				.sel_reg = CLK_MIPI1_CFG_SEL,
				.clk_src_mask = 1,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(5, 6),
				),

	[RP1_CLK_ETH_TSU] = REGISTER_CLK(
				.name = "clk_eth_tsu",
				.parents = {"xosc",
					    "pll_video_sec",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 8,
				.ctrl_reg = CLK_ETH_TSU_CTRL,
				.div_int_reg = CLK_ETH_TSU_DIV_INT,
				.sel_reg = CLK_ETH_TSU_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(5, 7),
				),

	[RP1_CLK_ADC] = REGISTER_CLK(
				.name = "clk_adc",
				.parents = {"xosc",
					    "", //"pll_audio_tern",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5"},
				.num_std_parents = 0,
				.num_aux_parents = 8,
				.ctrl_reg = CLK_ADC_CTRL,
				.div_int_reg = CLK_ADC_DIV_INT,
				.sel_reg = CLK_ADC_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(5, 5),
				),

	[RP1_CLK_SDIO_TIMER] = REGISTER_CLK(
				.name = "clk_sdio_timer",
				.parents = {"xosc"},
				.num_std_parents = 0,
				.num_aux_parents = 1,
				.ctrl_reg = CLK_SDIO_TIMER_CTRL,
				.div_int_reg = CLK_SDIO_TIMER_DIV_INT,
				.sel_reg = CLK_SDIO_TIMER_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 50 * MHz,
				.fc0_src = FC_NUM(3, 4),
				),

	[RP1_CLK_SDIO_ALT_SRC] = REGISTER_CLK(
				.name = "clk_sdio_alt_src",
				.parents = {"pll_sys"},
				.num_std_parents = 0,
				.num_aux_parents = 1,
				.ctrl_reg = CLK_SDIO_ALT_SRC_CTRL,
				.div_int_reg = CLK_SDIO_ALT_SRC_DIV_INT,
				.sel_reg = CLK_SDIO_ALT_SRC_SEL,
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 200 * MHz,
				.fc0_src = FC_NUM(5, 4),
				),

	[RP1_CLK_GP0] = REGISTER_CLK(
				.name = "clk_gp0",
				.parents = {"xosc",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5",
					    "pll_sys",
					    "", //"pll_audio",
					    "",
					    "",
					    "clk_i2s",
					    "clk_adc",
					    "",
					    "",
					    "",
					    "clk_sys"},
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(0),
				.ctrl_reg = CLK_GP0_CTRL,
				.div_int_reg = CLK_GP0_DIV_INT,
				.div_frac_reg = CLK_GP0_DIV_FRAC,
				.sel_reg = CLK_GP0_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(0, 1),
				),

	[RP1_CLK_GP1] = REGISTER_CLK(
				.name = "clk_gp1",
				.parents = {"clk_sdio_timer",
					    "clksrc_gp0",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5",
					    "pll_sys_pri_ph",
					    "", //"pll_audio_pri_ph",
					    "",
					    "",
					    "clk_adc",
					    "clk_dpi",
					    "clk_pwm0",
					    "",
					    "",
					    ""},
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(1),
				.ctrl_reg = CLK_GP1_CTRL,
				.div_int_reg = CLK_GP1_DIV_INT,
				.div_frac_reg = CLK_GP1_DIV_FRAC,
				.sel_reg = CLK_GP1_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(1, 1),
				),

	[RP1_CLK_GP2] = REGISTER_CLK(
				.name = "clk_gp2",
				.parents = {"clk_sdio_alt_src",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "clksrc_gp5",
					    "pll_sys_sec",
					    "", //"pll_audio_sec",
					    "pll_video",
					    "clk_audio_in",
					    "clk_dpi",
					    "clk_pwm0",
					    "clk_pwm1",
					    "clk_mipi0_dpi",
					    "clk_mipi1_cfg",
					    "clk_sys"},
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(2),
				.ctrl_reg = CLK_GP2_CTRL,
				.div_int_reg = CLK_GP2_DIV_INT,
				.div_frac_reg = CLK_GP2_DIV_FRAC,
				.sel_reg = CLK_GP2_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(2, 1),
				),

	[RP1_CLK_GP3] = REGISTER_CLK(
				.name = "clk_gp3",
				.parents = {"xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp4",
					    "clksrc_gp5",
					    "",
					    "",
					    "pll_video_pri_ph",
					    "clk_audio_out",
					    "",
					    "",
					    "clk_mipi1_dpi",
					    "",
					    "",
					    ""},
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(3),
				.ctrl_reg = CLK_GP3_CTRL,
				.div_int_reg = CLK_GP3_DIV_INT,
				.div_frac_reg = CLK_GP3_DIV_FRAC,
				.sel_reg = CLK_GP3_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(3, 1),
				),

	[RP1_CLK_GP4] = REGISTER_CLK(
				.name = "clk_gp4",
				.parents = {"xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp5",
					    "", //"pll_audio_tern",
					    "pll_video_sec",
					    "",
					    "",
					    "",
					    "clk_mipi0_cfg",
					    "clk_uart",
					    "",
					    "",
					    "clk_sys",
					    },
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(4),
				.ctrl_reg = CLK_GP4_CTRL,
				.div_int_reg = CLK_GP4_DIV_INT,
				.div_frac_reg = CLK_GP4_DIV_FRAC,
				.sel_reg = CLK_GP4_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(4, 1),
				),

	[RP1_CLK_GP5] = REGISTER_CLK(
				.name = "clk_gp5",
				.parents = {"xosc",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4",
					    "", //"pll_audio_tern",
					    "pll_video_sec",
					    "clk_eth_tsu",
					    "",
					    "clk_vec",
					    "",
					    "",
					    "",
					    "",
					    ""},
				.num_std_parents = 0,
				.num_aux_parents = 16,
				.oe_mask = BIT(5),
				.ctrl_reg = CLK_GP5_CTRL,
				.div_int_reg = CLK_GP5_DIV_INT,
				.div_frac_reg = CLK_GP5_DIV_FRAC,
				.sel_reg = CLK_GP5_SEL,
				.div_int_max = DIV_INT_16BIT_MAX,
				.max_freq = 100 * MHz,
				.fc0_src = FC_NUM(5, 1),
				),

	[RP1_CLK_VEC] = REGISTER_CLK(
				.name = "clk_vec",
				.parents = {"pll_sys_pri_ph",
					    "pll_video_sec",
					    "pll_video",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4"},
				.num_std_parents = 0,
				.num_aux_parents = 8, /* XXX in fact there are more than 8 */
				.ctrl_reg = VIDEO_CLK_VEC_CTRL,
				.div_int_reg = VIDEO_CLK_VEC_DIV_INT,
				.sel_reg = VIDEO_CLK_VEC_SEL,
				.flags = CLK_SET_RATE_NO_REPARENT, /* Let VEC driver set parent */
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 108 * MHz,
				.fc0_src = FC_NUM(0, 6),
				),

	[RP1_CLK_DPI] = REGISTER_CLK(
				.name = "clk_dpi",
				.parents = {"pll_sys",
					    "pll_video_sec",
					    "pll_video",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3",
					    "clksrc_gp4"},
				.num_std_parents = 0,
				.num_aux_parents = 8, /* XXX in fact there are more than 8 */
				.ctrl_reg = VIDEO_CLK_DPI_CTRL,
				.div_int_reg = VIDEO_CLK_DPI_DIV_INT,
				.sel_reg = VIDEO_CLK_DPI_SEL,
				.flags = CLK_SET_RATE_NO_REPARENT, /* Let DPI driver set parent */
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 200 * MHz,
				.fc0_src = FC_NUM(1, 6),
				),

	[RP1_CLK_MIPI0_DPI] = REGISTER_CLK(
				.name = "clk_mipi0_dpi",
				.parents = {"pll_sys",
					    "pll_video_sec",
					    "pll_video",
					    "clksrc_mipi0_dsi_byteclk",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3"},
				.num_std_parents = 0,
				.num_aux_parents = 8, /* XXX in fact there are more than 8 */
				.ctrl_reg = VIDEO_CLK_MIPI0_DPI_CTRL,
				.div_int_reg = VIDEO_CLK_MIPI0_DPI_DIV_INT,
				.div_frac_reg = VIDEO_CLK_MIPI0_DPI_DIV_FRAC,
				.sel_reg = VIDEO_CLK_MIPI0_DPI_SEL,
				.flags = CLK_SET_RATE_NO_REPARENT, /* Let DSI driver set parent */
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 200 * MHz,
				.fc0_src = FC_NUM(2, 6),
				),

	[RP1_CLK_MIPI1_DPI] = REGISTER_CLK(
				.name = "clk_mipi1_dpi",
				.parents = {"pll_sys",
					    "pll_video_sec",
					    "pll_video",
					    "clksrc_mipi1_dsi_byteclk",
					    "clksrc_gp0",
					    "clksrc_gp1",
					    "clksrc_gp2",
					    "clksrc_gp3"},
				.num_std_parents = 0,
				.num_aux_parents = 8, /* XXX in fact there are more than 8 */
				.ctrl_reg = VIDEO_CLK_MIPI1_DPI_CTRL,
				.div_int_reg = VIDEO_CLK_MIPI1_DPI_DIV_INT,
				.div_frac_reg = VIDEO_CLK_MIPI1_DPI_DIV_FRAC,
				.sel_reg = VIDEO_CLK_MIPI1_DPI_SEL,
				.flags = CLK_SET_RATE_NO_REPARENT, /* Let DSI driver set parent */
				.div_int_max = DIV_INT_8BIT_MAX,
				.max_freq = 200 * MHz,
				.fc0_src = FC_NUM(3, 6),
				),

	[RP1_CLK_MIPI0_DSI_BYTECLOCK] = REGISTER_VARSRC("clksrc_mipi0_dsi_byteclk"),
	[RP1_CLK_MIPI1_DSI_BYTECLOCK] = REGISTER_VARSRC("clksrc_mipi1_dsi_byteclk"),
};

static bool rp1_clk_claimed[ARRAY_SIZE(clk_desc_array)];

static bool rp1_clk_is_claimed(const char *name)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(clk_desc_array); i++) {
		if (clk_desc_array[i].data) {
			const char *clk_name = *(const char **)(clk_desc_array[i].data);

			if (!strcmp(name, clk_name))
				return rp1_clk_claimed[i];
		}
	}

	return false;
}

static int rp1_clk_probe(struct platform_device *pdev)
{
	const struct rp1_clk_desc *desc;
	struct device *dev = &pdev->dev;
	struct rp1_clockman *clockman;
	struct resource *res;
	struct clk_hw **hws;
	const size_t asize = ARRAY_SIZE(clk_desc_array);
	u32 chip_id, platform;
	unsigned int i;
	u32 clk_id;
	int ret;

	clockman = devm_kzalloc(dev, struct_size(clockman, onecell.hws, asize),
				GFP_KERNEL);
	if (!clockman)
		return -ENOMEM;

	rp1_get_platform(&chip_id, &platform);

	spin_lock_init(&clockman->regs_lock);
	clockman->dev = dev;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	clockman->regs = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(clockman->regs))
		return PTR_ERR(clockman->regs);

	memset(rp1_clk_claimed, 0, sizeof(rp1_clk_claimed));
	for (i = 0;
	     !of_property_read_u32_index(pdev->dev.of_node, "claim-clocks",
					 i, &clk_id);
	     i++)
		rp1_clk_claimed[clk_id] = true;

	platform_set_drvdata(pdev, clockman);

	clockman->onecell.num = asize;
	hws = clockman->onecell.hws;

	for (i = 0; i < asize; i++) {
		desc = &clk_desc_array[i];
		if (desc->clk_register && desc->data) {
			hws[i] = desc->clk_register(clockman, desc->data);
			if (!strcmp(clk_hw_get_name(hws[i]), "clk_i2s")) {
				clk_i2s = hws[i];
				clk_xosc = clk_hw_get_parent_by_index(clk_i2s, 0);
				clk_audio = clk_hw_get_parent_by_index(clk_i2s, 1);
			}
		}
	}

	ret = of_clk_add_hw_provider(dev->of_node, of_clk_hw_onecell_get,
				     &clockman->onecell);
	if (ret)
		return ret;

	return 0;
}

static const struct of_device_id rp1_clk_of_match[] = {
	{ .compatible = "raspberrypi,rp1-clocks" },
	{}
};
MODULE_DEVICE_TABLE(of, rp1_clk_of_match);

static struct platform_driver rp1_clk_driver = {
	.driver = {
		.name = "rp1-clk",
		.of_match_table = rp1_clk_of_match,
	},
	.probe = rp1_clk_probe,
};

static int __init __rp1_clk_driver_init(void)
{
	return platform_driver_register(&rp1_clk_driver);
}
postcore_initcall(__rp1_clk_driver_init);

MODULE_AUTHOR("Naushir Patuck <naush@raspberrypi.com>");
MODULE_DESCRIPTION("RP1 clock driver");
MODULE_LICENSE("GPL");
