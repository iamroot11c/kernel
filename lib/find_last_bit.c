/* find_last_bit.c: fallback find next bit implementation
 *
 * Copyright (C) 2008 IBM Corporation
 * Written by Rusty Russell <rusty@rustcorp.com.au>
 * (Inspired by David Howell's find_next_bit implementation)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#include <linux/bitops.h>
#include <linux/export.h>
#include <asm/types.h>
#include <asm/byteorder.h>

#ifndef find_last_bit
// 2015-03-14; 시작
// find_last_bit(cpumask_bits(cpu_possible_mask),NR_CPUS);
// 2015-09-05;
// MSB 부터 LSB 방향으로 찾음
unsigned long find_last_bit(const unsigned long *addr, unsigned long size)
{
	unsigned long words;
	unsigned long tmp;

	/* Start at final word. */
	words = size / BITS_PER_LONG; // 0 = 2/32

	/* Partial final word? */
	// if (2 & 0x1F) { // 32 < size < 0
	if (size & (BITS_PER_LONG-1)) {
		// addr = 0b0000_0000_0001_1101 
		// size =2 
		// -> (BITS_PER_LONG-1)  
		//     0001 1111
		//   & 0000 0010
		//   -----------
		//     0000 0010
		// 
		// -> (~0UL) >> (BITS_PER_LONG - (size & (BITS_PER_LONG-1)) )
		//  30 = 32 - 2
		//   
		//   0b11 = ~0UL >> 30
		//   (0b11 =  0b1111_1111_1111_1111_1111_1111_1111_1111 >> 30)
		//
		// -> addr[words] & (~0UL >> (BITS_PER_LONG - (size & (BITS_PER_LONG-1)) ) )
		//     0b0000 0000 0001 1101 
		//   & 0b0000 0000 0000 0011
		//   ----------------------- 
		//     0b0000 0000 0000 0001 
		//
		tmp = (addr[words] & (~0UL >> (BITS_PER_LONG
					 - (size & (BITS_PER_LONG-1)))));
		if (tmp)
			goto found;
	}

	while (words) {
		tmp = addr[--words];
		if (tmp) {
found:
			// 셋되어있는 맨 마지막 비트의 위치를 찾아 1을 뺀 값이
			// 대부분 리턴됨
			// tmp = 0b0000_0000_0000_0001;
			// 1 = 0 * 32 + 1;
			return words * BITS_PER_LONG + __fls(tmp);
		}
	}

	/* Not found */
	return size;
}
EXPORT_SYMBOL(find_last_bit);

#endif
