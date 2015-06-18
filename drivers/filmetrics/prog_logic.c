/*******************************************************************************
*
*  prog_logic.c - Filmetrics Programmable Logic IC Driver
*
*  Author: Raphael Oberson
*
*  Cross-Compiler: arm-linux-gcc (Buildroot 2012.11.1) 4.7.2
*
*  License: GNU General Public License
*
*  Copyright (C) 2015, Filmetrics Inc.
*
*******************************************************************************/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/io.h>

#define DEVICE_NAME	"prog_logic"

#define PORT_OUT_SHUTDOWN  AT91_PIN_PD0

#define DEBUG		0

#define HSMC_PULSE0	(AT91_BASE_SYS + 0x604)
#define HSMC_CYCLE0	(AT91_BASE_SYS + 0x608)

#if DEBUG
#define HSMC_SETUP0	(AT91_BASE_SYS + 0x600)
#define HSMC_MODE0	(AT91_BASE_SYS + 0x610)
#define PIO_PSR_D	(AT91_BASE_SYS + 0x3808)
#define PIO_ABCDSR1_D	(AT91_BASE_SYS + 0x3870)
#define PIO_ABCDSR2_D	(AT91_BASE_SYS + 0x3874)
#define PMC_PCK1	(AT91_BASE_SYS + 0x3C44)

#define PRINT_REG(reg) \
	printk(KERN_INFO DEVICE_NAME ": " #reg " = 0x%08X\n", read_reg(reg));
#endif

static struct clk *prog_logic_clock = NULL;

#if DEBUG
static unsigned int read_reg(unsigned int reg_addr)
{
	void __iomem *reg = ioremap(reg_addr, sizeof(unsigned int));
	return ioread32(reg);
}

static void print_regs(void)
{
	PRINT_REG(HSMC_SETUP0);
	PRINT_REG(HSMC_PULSE0);
	PRINT_REG(HSMC_CYCLE0);
	PRINT_REG(HSMC_MODE0);
	PRINT_REG(PIO_PSR_D);
	PRINT_REG(PIO_ABCDSR1_D);
	PRINT_REG(PIO_ABCDSR2_D);
	PRINT_REG(PMC_PCK1);
}
#endif

static void write_reg(unsigned int reg_addr, unsigned int value)
{
	void __iomem *reg = ioremap(reg_addr, sizeof(unsigned int));
	iowrite32(value, reg);
}

static int __init prog_logic_init(void)
{
	int err;
	struct clk *mck = NULL;

#if DEBUG
	printk(KERN_INFO DEVICE_NAME ": init [12]\n"); ////
#endif

	/* Set NRD and NCS0 pulse width to 3 clock cycles (22.7 ns) and total
	 * read cycle time to 5 clock cycles (37.9 ns), which leaves 1 clock
	 * cycle setup time and 1 clock cycle hold time.
	 * - Setting pulse width to 1 clock cycle and read cycle time to 3 clock
	 *   cycles (at 132 MHz MCK) prevents the read cycle from completing
	 *   properly and 0xFFFF is read out of the programmable logic chip.
	 * - Setting pulse width to 2 clock cycles and read cycle time to 4
	 *   clock cycles makes the read cycle work properly (0x22 read from
	 *   Register 2), but the timing might still be marginal for the
	 *   programmable logic chip, so we increase the read timing to 3/5 to
	 *   have some margin.
	 */
	write_reg(HSMC_PULSE0, 0x03030101);
	write_reg(HSMC_CYCLE0, 0x00050003);

	/* Set up Shutdown GPIO pin: "LOW" turns programmable logic chip ON */
	err = gpio_request_one(PORT_OUT_SHUTDOWN, GPIOF_OUT_INIT_LOW,
				"GPIO_SHUTDOWN");
	if (err != 0) goto gpio_fail;

	// Set pin PD31 to Peripheral B (PCK1)
	// Note: calling at91_set_B_periph(AT91_PIN_PD31, 0) didn't seem to
	// work, and returned -EINVAL. Hack: clock pin set by sama5d3_wm8904
	// sound driver instead (modified in the DTSI). TODO FIXME

	/* Set up programmable logic's 132 MHz clock on PCK1 */
	prog_logic_clock = clk_get(NULL, "pck1");
	if (prog_logic_clock == NULL) {
		err = -EINVAL;
		goto clk_fail;
	}

	mck = clk_get(NULL, "mck");  /* mck is the 132 MHz Master Clock */
	if (mck == NULL) {
		err = -EINVAL;
		goto clk_fail;
	}

	err = clk_set_parent(prog_logic_clock, mck);
	if (err != 0) goto clk_fail;

	err = clk_set_rate(prog_logic_clock, 132000000);  /* 132 MHz */
	if (err != 0 && err != 132000000)
		goto clk_fail;

	err = clk_prepare(prog_logic_clock);
	if (err != 0)
		goto clk_fail;

	err = clk_enable(prog_logic_clock);
	if (err != 0)
		goto clk_fail;

#if DEBUG
	print_regs();
#endif

	return 0;  /* Init successful */

clk_fail:
	gpio_set_value(PORT_OUT_SHUTDOWN, 1);
	gpio_free(PORT_OUT_SHUTDOWN);

gpio_fail:
	printk(KERN_WARNING DEVICE_NAME ": init returns error %d\n", err);
	return err;
}

static void __exit prog_logic_exit(void)
{
	clk_disable(prog_logic_clock);
	gpio_set_value(PORT_OUT_SHUTDOWN, 1);
	gpio_free(PORT_OUT_SHUTDOWN);
#if DEBUG
	printk(KERN_INFO DEVICE_NAME ": exit\n"); ////
#endif
}

module_init(prog_logic_init);
module_exit(prog_logic_exit);

MODULE_AUTHOR("Raphael Oberson, Filmetrics Inc.");
MODULE_DESCRIPTION("Filmetrics Programmable Logic IC Driver");
MODULE_LICENSE("GPL");
