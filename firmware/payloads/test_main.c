/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 *
 * Authors:
 *   Anup Patel <anup.patel@wdc.com>
 */

#include <sbi/sbi_ecall_interface.h>

#define wfi()						\
do {							\
	__asm__ __volatile__ ("wfi" ::: "memory");	\
} while (0)

void test_main(unsigned long a0, unsigned long a1)
{
	sbi_ecall_console_puts("\nTest payload running\n");
	char *p = (void*)0x80000000;
	char str[] = "0    \n";
	for(; (unsigned long long)p < 0x80800000; p += 0x80000)
	{
		sbi_ecall_console_puts("mem test ");
		*p = str[0];
		sbi_ecall_console_puts(str);
		str[0]++;
	}
	
	while (1)
		wfi();
}
