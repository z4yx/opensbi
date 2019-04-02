/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/sbi_ecall_interface.h>
#include <sbi/riscv_encoding.h>
#include <sbi/riscv_asm.h>
#include <stdint.h>

#define wfi()						\
do {							\
	__asm__ __volatile__ ("wfi" ::: "memory");	\
} while (0)

#define CE_PAGE_SIZE (4096)

uint64_t volatile __attribute__((aligned(CE_PAGE_SIZE)))  ceLv1PageTable[(CE_PAGE_SIZE / 8) * (1)];

void ceResetCacheState() {
	asm volatile ("sfence.vm");
}

uint64_t ceEncodePTE(uint32_t physAddr, uint32_t flags) {
	// assert((physAddr % CE_PAGE_SIZE) == 0);
	return (((uint64_t)physAddr >> 12) << 10) | flags;
}

void ceSetupMMU() {
	const uint64_t stapModeSv39 = 9;

	sbi_ecall_console_puts("setup mmu...\n");

	//0 - 0xFFFFFFFF -> mirror to phys
	for (uint32_t i = 0; i < 4; i++) {
		ceLv1PageTable[i] = ceEncodePTE((0x40000000U) * i,  PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_U);
	}
	//0x100000000 - 0x1FFFFFFFF -> mirror to phys
	for (uint32_t i = 0; i < 4; i++) {
		ceLv1PageTable[i+4] = ceEncodePTE((0x40000000U) * i,  PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_U);
	}

	//0x100000000 (1GiB) -> lv2
	// ceLv1PageTable[4] = ceEncodePTE((uint32_t)ceLv2PageTables, PTE_V | PTE_G | PTE_U  );

	// //0x100000000 (2MiB * CE_LV3_PAGE_TABLE_COUNT) -> lv3
	// for (uint32_t i = 0; i < CE_LV3_PAGE_TABLE_COUNT; i++) {
	//     ceLv2PageTables[i] = ceEncodePTE(((uint32_t)ceLv3PageTables) + i * CE_PAGE_SIZE,  PTE_V | PTE_G | PTE_U);
	// }

	csr_write(sptbr, (uint64_t)ceLv1PageTable >> 12);
	sbi_ecall_console_puts("write sptbr\n");

	uint64_t msValue = csr_read(mstatus);
	msValue |= MSTATUS_MPRV | ((uint64_t)stapModeSv39 << 24);
	csr_write(mstatus, msValue);
	sbi_ecall_console_puts("write mstatus\n");

	ceResetCacheState();
	sbi_ecall_console_puts("fence\n");
}

void printHex(unsigned long num)
{
	char c = num & 0xf;
	c = c < 10 ? c + '0' : c - 10 + 'a';
	num >>= 4;
	if(num)
		printHex(num);
	sbi_ecall_console_putc(c);
}

void test_main(unsigned long a0, unsigned long a1)
{
	sbi_ecall_console_puts("\nTest payload running\n");
	volatile char *p = (void*)0x80100000;
	for(; (unsigned long)p < 0x80400000; p += 0x100000)
	{
		sbi_ecall_console_puts("mem access 0x");
		printHex((unsigned long)p);
		sbi_ecall_console_putc('\n');
		*(p+1) = *p;
	}
	
	volatile long *ptr = (void*)0x80100000;
	long rnd = 12345;
	for(; (long)ptr < 0x80400000; ptr++)
	{
		*ptr = rnd;
		rnd = rnd * 1103515245 + 12345;
	}
	ptr = (void*)0x80100000;
	rnd = 12345;
	for(; (long)ptr < 0x80400000; ptr++)
	{
		if(*ptr != rnd){
			sbi_ecall_console_puts("mem test fail @ 0x");
			printHex((unsigned long)ptr);
			sbi_ecall_console_puts("\n ");
			printHex(*ptr);
			sbi_ecall_console_puts(" != ");
			printHex(rnd);
			while (1)
				wfi();
		}
		rnd = rnd * 1103515245 + 12345;
	}
	sbi_ecall_console_puts("mem test ok");

	ceSetupMMU();
	ptr = (void*)(0x180100000U);
	rnd = 12345;
	for(; (long)ptr < 0x180400000U; ptr++)
	{
		if(*ptr != rnd){
			sbi_ecall_console_puts("mapped mem test fail @ 0x");
			printHex((unsigned long)ptr);
			while (1)
				wfi();
		}
		rnd = rnd * 1103515245 + 12345;
	}
	sbi_ecall_console_puts("mapped mem test ok\n");

	while (1)
		wfi();
}
