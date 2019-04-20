/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/sbi_console.h>
#include <sbi/sbi_ecall.h>
#include <sbi/sbi_ecall_interface.h>
#include <sbi/sbi_error.h>
#include <sbi/sbi_ipi.h>
#include <sbi/sbi_system.h>
#include <sbi/sbi_timer.h>
#include <sbi/sbi_trap.h>
#include <sbi/riscv_asm.h>

#define SBI_ECALL_VERSION_MAJOR			0
#define SBI_ECALL_VERSION_MINOR			1

u16 sbi_ecall_version_major(void)
{
	return SBI_ECALL_VERSION_MAJOR;
}

u16 sbi_ecall_version_minor(void)
{
	return SBI_ECALL_VERSION_MINOR;
}

int sbi_ecall_handler(u32 hartid, ulong mcause,
		      struct sbi_trap_regs *regs,
		      struct sbi_scratch *scratch)
{
	int ret = SBI_ENOTSUPP;

	switch (regs->a7) {
	case SBI_ECALL_SET_TIMER:
#if __riscv_xlen == 32
		sbi_timer_event_start(scratch,
			(((u64)regs->a1 << 32) | (u64)regs->a0));
#else
		sbi_timer_event_start(scratch, (u64)regs->a0);
#endif
		ret = 0;
		break;
	case SBI_ECALL_CONSOLE_PUTCHAR:
		sbi_putc(regs->a0);
		ret = 0;
		break;
	case SBI_ECALL_CONSOLE_GETCHAR:
		regs->a0 = sbi_getc();
		ret = 0;
		break;
	case SBI_ECALL_CLEAR_IPI:
		sbi_ipi_clear_smode(scratch);
		ret = 0;
		break;
	case SBI_ECALL_SEND_IPI:
		ret = sbi_ipi_send_many(scratch, (ulong *)regs->a0,
					SBI_IPI_EVENT_SOFT);
		break;
	case SBI_ECALL_REMOTE_FENCE_I:
		ret = sbi_ipi_send_many(scratch, (ulong *)regs->a0,
					SBI_IPI_EVENT_FENCE_I);
		break;
	case SBI_ECALL_REMOTE_SFENCE_VMA:
	case SBI_ECALL_REMOTE_SFENCE_VMA_ASID:
		ret = sbi_ipi_send_many(scratch, (ulong *)regs->a0,
					SBI_IPI_EVENT_SFENCE_VMA);
		break;
	case SBI_ECALL_SHUTDOWN:
		sbi_system_shutdown(scratch, 0);
		ret = 0;
		break;
	case 23:
		csr_clear(CSR_MSTATUS, 0xf<<24); // MODE
		csr_set(CSR_MSTATUS, MSTATUS_MPRV|((regs->a0 & 0xf)<<24)); // MODE
		sbi_printf("mstatus=0x%lx\n", csr_read(CSR_MSTATUS));
		ret = 0;
		break;
	default:
		break;
	};

	if (!ret) {
		regs->mepc += 4;
	}

	return ret;
}
