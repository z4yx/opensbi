/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Damien Le Moal <damien.lemoal@wdc.com>
 */

#include <sbi/riscv_encoding.h>
#include <sbi/sbi_const.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_console.h>
#include <plat/irqchip/plic.h>
#include <plat/sys/clint.h>
#include "platform.h"
#include "sysctl.h"
#include "uarths.h"

#define K210_UART_BAUDRATE		115200
#define PLL0_OUTPUT_FREQ		800000000UL
#define PLL1_OUTPUT_FREQ		400000000UL

static void k210_paging_init()
{
	static __attribute__((aligned(RISCV_PGSIZE))) uint64_t BOOT_PAGE_TABLE[1 << RISCV_PGLEVEL_BITS] = {0};

  	BOOT_PAGE_TABLE[   0] = (0x000000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   1] = (0x040000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   2] = (0x080000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   3] = (0x0C0000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   4] = (0x000000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   5] = (0x040000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	BOOT_PAGE_TABLE[   6] = (0x080000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
  	// BOOT_PAGE_TABLE[0777] = (0x80000000ul >> 12 << 10) | PTE_V | PTE_R | PTE_W | PTE_X | PTE_G;
	csr_write(CSR_SATP, (uint64_t)BOOT_PAGE_TABLE >> RISCV_PGSHIFT);

	// enable Sv39
	long mstatus = csr_read(CSR_MSTATUS);
	mstatus |= 9 << 24;
	csr_write(CSR_MSTATUS, mstatus);
	asm volatile ("sfence.vm");
}


static int k210_console_init(void)
{
	uarths_init(K210_UART_BAUDRATE, UARTHS_STOP_1);

	k210_paging_init();
	csr_set(CSR_MEDELEG,
		(1 << CAUSE_LOAD_ACCESS) |
		(1 << CAUSE_FETCH_ACCESS) |
		(1 << CAUSE_STORE_ACCESS)
	);
	csr_write(0x321, -1); //mscounteren in priv-1.9

	return 0;
}

static void k210_console_putc(char c)
{
	uarths_putc(c);
}

static int k210_console_getc(void)
{
	return uarths_getc();
}

static int k210_irqchip_init(bool cold_boot)
{
	int rc;
	u32 hartid = sbi_current_hartid();

	if (cold_boot) {
		rc = plic_cold_irqchip_init(PLIC_BASE_ADDR,
					    PLIC_NUM_SOURCES,
					    K210_HART_COUNT);
		if (rc)
			return rc;
	}

	return plic_warm_irqchip_init(hartid,
				      (2 * hartid),
				      (2 * hartid + 1));
}

static int k210_ipi_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = clint_cold_ipi_init(CLINT_BASE_ADDR,
					 K210_HART_COUNT);
		if (rc)
			return rc;
	}

	return clint_warm_ipi_init();
}

static int k210_timer_init(bool cold_boot)
{
	int rc;

	if (cold_boot) {
		rc = clint_cold_timer_init(CLINT_BASE_ADDR,
					   K210_HART_COUNT);
		if (rc)
			return rc;
	}

	return clint_warm_timer_init();
}

static int k210_system_reboot(u32 type)
{
	/* For now nothing to do. */
	sbi_printf("System reboot\n");

	return 0;
}

static int k210_system_shutdown(u32 type)
{
	/* For now nothing to do. */
	sbi_printf("System shutdown\n");

	return 0;
}

static int k210_early_init(bool cold_boot)
{
	sysctl_pll_set_freq(SYSCTL_PLL0, PLL0_OUTPUT_FREQ);
	sysctl_pll_set_freq(SYSCTL_PLL1, PLL1_OUTPUT_FREQ);
	return 0;
}

const struct sbi_platform platform = {

	.name = "Kendryte K210",
	.features = SBI_PLATFORM_HAS_TIMER_VALUE,

	.hart_count = K210_HART_COUNT,
	.hart_stack_size = K210_HART_STACK_SIZE,
	.disabled_hart_mask = 0,

	.early_init = k210_early_init,

	.console_init = k210_console_init,
	.console_putc = k210_console_putc,
	.console_getc = k210_console_getc,

	.irqchip_init = k210_irqchip_init,

	.ipi_init = k210_ipi_init,
	.ipi_send = clint_ipi_send,
	.ipi_sync = clint_ipi_sync,
	.ipi_clear = clint_ipi_clear,

	.timer_init = k210_timer_init,
	.timer_value = clint_timer_value,
	.timer_event_stop = clint_timer_event_stop,
	.timer_event_start = clint_timer_event_start,

	.system_reboot = k210_system_reboot,
	.system_shutdown = k210_system_shutdown
};
