/*
 *  linux/arch/arm/lib/findbit.S
 *
 *  Copyright (C) 1995-2000 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * 16th March 2001 - John Ripley <jripley@sonicblue.com>
 *   Fixed so that "size" is an exclusive not an inclusive quantity.
 *   All users of these functions expect exclusive sizes, and may
 *   also call with zero size.
 * Reworked by rmk.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
                .text

/*
 * Purpose  : Find a 'zero' bit
 * Prototype: int find_first_zero_bit(void *addr, unsigned int maxbit);
 */
ENTRY(_find_first_zero_bit_le)
		teq	r1, #0	
		beq	3f
		mov	r2, #0
1:
 ARM(		ldrb	r3, [r0, r2, lsr #3]	)
 THUMB(		lsr	r3, r2, #3		)
 THUMB(		ldrb	r3, [r0, r3]		)
		eors	r3, r3, #0xff		@ invert bits
						@// LSB부터 찾기위해 비트를 반전시킴
		bne	.L_found		@ any now set - found zero bit
		add	r2, r2, #8		@ next bit pointer
2:		cmp	r2, r1			@ any more?
		blo	1b
3:		mov	r0, r1			@ no free bits
		mov	pc, lr
ENDPROC(_find_first_zero_bit_le)

/*
 * Purpose  : Find next 'zero' bit
 * Prototype: int find_next_zero_bit(void *addr, unsigned int maxbit, int offset)
 */
/* 
 * 참고: http://u-city.tistory.com/143
 *       http://pinocc.tistory.com/archive/201305
 *       모기향: 261 ~ 263쪽
 * bdata->node_bootmem_map, midx, sidx
 */
ENTRY(_find_next_zero_bit_le)
		teq	r1, #0
		beq	3b                      @// 레이블 3으로 이동하여 종료
		ands	ip, r2, #7              @ ip = r2 & #7 
		beq	1b			@ If new byte, goto old routine
 ARM(		ldrb	r3, [r0, r2, lsr #3]	) @// 주소가 8의 배수로 구성됨
 THUMB(		lsr	r3, r2, #3		)
 THUMB(		ldrb	r3, [r0, r3]		)
		eor	r3, r3, #0xff		@ now looking for a 1 bit
		                                @// 1byte만 비트를 바꿈
		movs	r3, r3, lsr ip		@ shift off unused bits 
		                                @// 최대 우측 쉬프트
		bne	.L_found
		orr	r2, r2, #7		@ if zero, then no bits here
		add	r2, r2, #1		@ align bit pointer
		b	2b			@ loop for next bit
ENDPROC(_find_next_zero_bit_le)

/*
 * Purpose  : Find a 'one' bit
 * Prototype: int find_first_bit(const unsigned long *addr, unsigned int maxbit);
 */
// 2015-02-28; 
ENTRY(_find_first_bit_le)
		teq	r1, #0	
		beq	3f
		mov	r2, #0
1:
 ARM(		ldrb	r3, [r0, r2, lsr #3]	)
 THUMB(		lsr	r3, r2, #3		)
 THUMB(		ldrb	r3, [r0, r3]		)
		movs	r3, r3                  @// 값을 우측으로 쉬프트 하지 않음
		bne	.L_found		@ any now set - found zero bit
						@// if (r3 > 0) .L_found;
		add	r2, r2, #8		@ next bit pointer
						@// r2 += 8;
2:		cmp	r2, r1			@ any more?
						@// if (r2 < r1)
		blo	1b			@//   .1b
3:		mov	r0, r1			@ no free bits
		mov	pc, lr
ENDPROC(_find_first_bit_le)

/*
 * Purpose  : Find next 'one' bit
 * Prototype: int find_next_zero_bit(void *addr, unsigned int maxbit, int offset)
 */
// 2015-02-28;
//
// _find_next_bit_le(cpumask_bits(srcp)/*maskp->bits*/, nr_cpumask_bits/*2*/, n+1)
// r0 = maskp->bits
// r1 = 2
// r2 = n+1 // 0, 1, 2, 3, ... , n;
ENTRY(_find_next_bit_le)
		teq	r1, #0
		beq	3b                      @// backward 3 label
		ands	ip, r2, #7              @// ip = r2 & 7; 
		                                @// 오프셋이 0 또는 8(2^3), 
						@// 16(2^4), 32(2^5), ... , 2^n 이면
						@// 값을 우측으로 쉬프트 하지 않음
		beq	1b			@ If new byte, goto old routine
 ARM(		ldrb	r3, [r0, r2, lsr #3]	) @// r3 = *(r0 + (r2 >> 3)) // *(r0 + (r2/8))
 THUMB(		lsr	r3, r2, #3		)
 THUMB(		ldrb	r3, [r0, r3]		)
		movs	r3, r3, lsr ip		@ shift off unused bits
		                                @// r3 = r3 >> ip;
						@// 오프셋 & 7 연산 후 나머지 만큼
						@// 우측으로 쉬프트
						@// 이는 찾는 위치를 0번째 비트에서
						@// 하기위해 우측으로 쉬프트
		bne	.L_found                @// if (r3 > 0) .L_fouund;
		orr	r2, r2, #7		@ if zero, then no bits here
		add	r2, r2, #1		@ align bit pointer
		                                @// r2 = (r2|0b0111) + 1;
						@// 8비트(0~7, 8~15, 16~21, 22~32)단위로
					        @// 검사를 먼저하여 다음 구역으로 이동
						@// 검사는 위치가 8번 비트에서 15번 비트라면
						@// 16번 비트에서 21번 비트사이를 검사하기
						@// 위해 오프셋을 조정
		b	2b			@ loop for next bit
						@// backward 2 label
ENDPROC(_find_next_bit_le)

#ifdef __ARMEB__

ENTRY(_find_first_zero_bit_be)
		teq	r1, #0
		beq	3f
		mov	r2, #0
1:		eor	r3, r2, #0x18		@ big endian byte ordering
 ARM(		ldrb	r3, [r0, r3, lsr #3]	)
 THUMB(		lsr	r3, #3			)
 THUMB(		ldrb	r3, [r0, r3]		)
		eors	r3, r3, #0xff		@ invert bits
		bne	.L_found		@ any now set - found zero bit
		add	r2, r2, #8		@ next bit pointer
2:		cmp	r2, r1			@ any more?
		blo	1b
3:		mov	r0, r1			@ no free bits
		mov	pc, lr
ENDPROC(_find_first_zero_bit_be)

ENTRY(_find_next_zero_bit_be)
		teq	r1, #0
		beq	3b
		ands	ip, r2, #7
		beq	1b			@ If new byte, goto old routine
		eor	r3, r2, #0x18		@ big endian byte ordering
 ARM(		ldrb	r3, [r0, r3, lsr #3]	)
 THUMB(		lsr	r3, #3			)
 THUMB(		ldrb	r3, [r0, r3]		)
		eor	r3, r3, #0xff		@ now looking for a 1 bit
		movs	r3, r3, lsr ip		@ shift off unused bits
		bne	.L_found
		orr	r2, r2, #7		@ if zero, then no bits here
		add	r2, r2, #1		@ align bit pointer
		b	2b			@ loop for next bit
ENDPROC(_find_next_zero_bit_be)

ENTRY(_find_first_bit_be)
		teq	r1, #0
		beq	3f
		mov	r2, #0
1:		eor	r3, r2, #0x18		@ big endian byte ordering
 ARM(		ldrb	r3, [r0, r3, lsr #3]	)
 THUMB(		lsr	r3, #3			)
 THUMB(		ldrb	r3, [r0, r3]		)
		movs	r3, r3
		bne	.L_found		@ any now set - found zero bit
		add	r2, r2, #8		@ next bit pointer
2:		cmp	r2, r1			@ any more?
		blo	1b
3:		mov	r0, r1			@ no free bits
		mov	pc, lr
ENDPROC(_find_first_bit_be)

ENTRY(_find_next_bit_be)
		teq	r1, #0
		beq	3b
		ands	ip, r2, #7
		beq	1b			@ If new byte, goto old routine
		eor	r3, r2, #0x18		@ big endian byte ordering
 ARM(		ldrb	r3, [r0, r3, lsr #3]	)
 THUMB(		lsr	r3, #3			)
 THUMB(		ldrb	r3, [r0, r3]		)
		movs	r3, r3, lsr ip		@ shift off unused bits
		bne	.L_found
		orr	r2, r2, #7		@ if zero, then no bits here
		add	r2, r2, #1		@ align bit pointer
		b	2b			@ loop for next bit
ENDPROC(_find_next_bit_be)

#endif

/*
 * One or more bits in the LSB of r3 are assumed to be set.
 */
/*
 * 참고: http://forum.falinux.com/zbxe/index.php?document_srl=574399&mid=lecture_tip
 */
.L_found:
#if __LINUX_ARM_ARCH__ >= 5
		rsb	r0, r3, #0            @// r0 = #0 - r3
		and	r3, r3, r0            @// r3 = r3 & (-r3);
	                                      @//  13 = 0b0000_1101
				              @// -13 = 0b1111_0011
					      @// 0b0000_0001 = 0b0000_1101 & 0b1111_0011;
		clz	r3, r3                @// 최상위 비트에서 최초로 1이 나올때까지의
		                              @// 비트개수로 위의 예제에서는 31이 나옴
					      @//  13 = 0b0000_1101     44 = 0b0010_1100
					      @// -13 = 0b1111_0011    -44 = 0b1101_0100
					      @// -----------------    -----------------
					      @//       0b0000_0001          0b0000_0100
					      @// r3 = 32 - 31              r3 = 32 - 29
					      @//    = 1                       = 3
		rsb	r3, r3, #31           @// r3 = #31 - r3; // 0 <= r3 < 31
					      @// 31에서 찾은 위치를 빼면 맨 처음 셋되어있는
					      @// 비트를 찾게됨
					      @// fls(), ffs(), ffz() 함수 참고
		add	r0, r2, r3            @// r0 = r2 + r3
#else
		tst	r3, #0x0f
		addeq	r2, r2, #4
		movne	r3, r3, lsl #4
		tst	r3, #0x30
		addeq	r2, r2, #2
		movne	r3, r3, lsl #2
		tst	r3, #0x40
		addeq	r2, r2, #1
		mov	r0, r2
#endif
		cmp	r1, r0			@ Clamp to maxbit
		movlo	r0, r1                  @// if (r1 < r0)
		                                @//  r0 = r1
						@// 조사 범위를 넘었는지 조사
		mov	pc, lr

