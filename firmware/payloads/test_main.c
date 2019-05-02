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

#define sp_read()						\
({								\
	register unsigned long __v;				\
	__asm__ __volatile__ ("or %0, sp, zero"	\
			      : "=r" (__v) :			\
			      : "memory");			\
	__v;							\
})

#define CE_PAGE_SIZE (4096)

uint64_t volatile __attribute__((aligned(CE_PAGE_SIZE)))  ceLv1PageTable[(CE_PAGE_SIZE / 8)],
	ceLv2PageTables[(CE_PAGE_SIZE / 8)],
	ceLv3PageTables[(CE_PAGE_SIZE / 8)];

void printHex(unsigned long num)
{
	char c = num & 0xf;
	c = c < 10 ? c + '0' : c - 10 + 'a';
	num >>= 4;
	if(num)
		printHex(num);
	sbi_ecall_console_putc(c);
}

uint64_t ceEncodePTE(uint64_t physAddr, uint32_t flags) {
	// assert((physAddr % CE_PAGE_SIZE) == 0);
	return ((physAddr >> 12) << 10) | flags;
}

void ceSetupMMU() {
	const uint64_t stapModeSv39 = 9;

	sbi_ecall_console_puts("setup mmu...\n");

	// [0,0x100000000) -> [0,0x100000000) //mirror to phys
	for (uint32_t i = 0; i < 4; i++) {
		ceLv1PageTable[i] = ceEncodePTE((0x40000000U) * i,  PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_U);
	}
	//0x100000000 - 0x1FFFFFFFF -> vaddr-0x100000000
	// for (uint32_t i = 0; i < 4; i++) {
	// 	ceLv1PageTable[i+4] = ceEncodePTE((0x40000000U) * i,  PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_U);
	// }

	//0x100000000 (1GiB) -> lv2
	ceLv1PageTable[4] = ceEncodePTE((uintptr_t)ceLv2PageTables, PTE_V | PTE_G | PTE_U  );

	const uint32_t CE_LV3_PAGE_TABLE_COUNT = 1;
	// //0x100000000 (2MiB * CE_LV3_PAGE_TABLE_COUNT) -> lv3
	for (uint32_t i = 0; i < CE_LV3_PAGE_TABLE_COUNT; i++) {
	    ceLv2PageTables[i] = ceEncodePTE(((uintptr_t)ceLv3PageTables) + i * CE_PAGE_SIZE,  PTE_V | PTE_G | PTE_U);
	}

	// [0x100000000,0x100200000) -> [0x80001000,0x80201000)
	for (uint32_t i = 0; i < CE_PAGE_SIZE / 8; i++) {
		ceLv3PageTables[i] = ceEncodePTE(0x80001000U + (0x1000) * (i),  PTE_V | PTE_R | PTE_W | PTE_X | PTE_G | PTE_U);
	}


	csr_write(sptbr, (uint64_t)ceLv1PageTable >> 12);
	sbi_ecall_console_puts("write sptbr\n");
	asm volatile ("sfence.vm");
	sbi_ecall_console_puts("fence\n");

	// uint64_t msValue = csr_read(mstatus);
	// sbi_ecall_console_puts("read mstatus=0x");
	// printHex(msValue);
	// sbi_ecall_console_putc('\n');
	// msValue |= MSTATUS_MPRV | ((uint64_t)stapModeSv39 << 24);
	// csr_write(mstatus, msValue);
	// sbi_ecall_console_puts("write mstatus\n");
	SBI_ECALL_1(23, stapModeSv39);

}

void test_func()
{
	sbi_ecall_console_puts("test_func\n");
}

void test_main(unsigned long a0, unsigned long a1)
{
	sbi_ecall_console_puts("\nTest payload running\n");
	volatile char *p = (void*)0x80100000;
	for(; (uint64_t)p < 0x80800000; p += 0x100000)
	{
		sbi_ecall_console_puts("mem access 0x");
		printHex((uint64_t)p);
		sbi_ecall_console_putc('\n');
		*(p+1) = *p;
	}
	
	sbi_ecall_console_puts("stack pointer 0x");
	printHex(sp_read());
	sbi_ecall_console_putc('\n');

	uint64_t mask = 0;
	volatile uint64_t *ptr;
	uint64_t rnd;
	for(int i=0; i<2; i++){
		ptr = (volatile uint64_t*)0x80100000;
		rnd = 12345;
		mask = ~mask;
		for(; (uint64_t)ptr < 0x80800000; ptr+=1)
		{
			*ptr = rnd^mask; // Write MEM
			rnd = rnd * 1103515245 + 12345;
		}
		ptr = (void*)0x80100000;
		rnd = 12345;
		for(; (uint64_t)ptr < 0x80800000; ptr+=1)
		{
			if(*ptr != (rnd^mask)){ // Read MEM
				sbi_ecall_console_puts("mem test fail @ 0x");
				printHex((uint64_t)ptr);
				sbi_ecall_console_puts("\n ");
				printHex(*ptr);
				sbi_ecall_console_puts(" != ");
				printHex(rnd^mask);
				sbi_ecall_console_putc('\n');
				while (1)
					wfi();
			}
			rnd = rnd * 1103515245 + 12345;
		}
	}
	sbi_ecall_console_puts("mem test ok\n");

	ceSetupMMU();
	ptr = (void*)(0x180100000U);
	rnd = 12345;
	for(; (uint64_t)ptr < 0x180800000U; ptr+=1)
	{
		if(*ptr != (rnd^mask)){
			sbi_ecall_console_puts("mapped mem test fail @ 0x");
			printHex((uint64_t)ptr);
			sbi_ecall_console_puts("\n ");
			printHex(*ptr);
			sbi_ecall_console_puts(" != ");
			printHex(rnd);
			sbi_ecall_console_putc('\n');
			while (1)
				wfi();
		}
		rnd = rnd * 1103515245 + 12345;
	}
	sbi_ecall_console_puts("mapped mem test ok\n");

	((void(*)(void))(0x100000000U+(uintptr_t)test_func))();

	ptr = (void*)0xAFFAAA00000000U;
	*ptr = 0xdead;
	sbi_ecall_console_puts("written\n");
	printHex(*ptr);


	while (1)
		wfi();
}
