/*
 *  linux/arch/arm/lib/copypage.S
 *
 *  Copyright (C) 1995-1999 Russell King
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  ASM optimised string functions
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/cache.h>

#define COPY_COUNT (PAGE_SZ/*4096*/ / (2 * L1_CACHE_BYTES/*0x40, 64Byte*/) PLD( -1 )) 
                   // 31

		.text
		.align	5
/*
 * StrongARM optimised copy_page routine
 * now 1.78bytes/cycle, was 1.60 bytes/cycle (50MHz bus -> 89MB/s)
 * Note that we probably achieve closer to the 100MB/s target with
 * the core clock switching.
 */
// 2015-10-24;
// copy_page(vto, vfrom);
ENTRY(copy_page)
		stmfd	sp!, {r4, lr}			@	2
		                                        @// *(++sp) = lr;
							@// *(++sp) = r4;
	PLD(	pld	[r1, #0]		)         @// pod: 데이터나 명령어를 곧 로드할 것이라는 
	PLD(	pld	[r1, #L1_CACHE_BYTES]		) @// 신호를 메모리 시스템에 보낼
	                                                  @// r1이 가르키는 주소 부터 L1_CACHE_BYTES까지
							  @// 로드 할 것을 시스템에 알림
		mov	r2, #COPY_COUNT			@	1 @// r2 = #COPY_COUNT/*31*/
		ldmia	r1!, {r3, r4, ip, lr}		@	4+1 @// r3 = *(r1++);
		                                                    @// r4 = *(r1++);
								    @// ip = *(r1++);
								    @// lr = *(r1++);
1:	PLD(	pld	[r1, #2 * L1_CACHE_BYTES])              
	PLD(	pld	[r1, #3 * L1_CACHE_BYTES])
2:
	.rept	(2 * L1_CACHE_BYTES / 16 - 1)                       @// stmia, ldmia 명령어 쌍을
	                                                            @// 반복하는 코드를 만듬
								    @// 명령어 쌍이 7번 반복
		stmia	r0!, {r3, r4, ip, lr}		@	4   @// *(r0++) = r3;
		                                                    @// *(r0++) = r4;
								    @// *(r0++) = ip;
								    @// *(r0++) = lr;
		ldmia	r1!, {r3, r4, ip, lr}		@	4
	.endr
		subs	r2, r2, #1			@	1 @// r2 = r2 -1;
		stmia	r0!, {r3, r4, ip, lr}		@	4
		ldmgtia	r1!, {r3, r4, ip, lr}		@	4
		bgt	1b				@	1
	PLD(	ldmeqia r1!, {r3, r4, ip, lr}	)
	PLD(	beq	2b			)
		ldmfd	sp!, {r4, pc}			@	3 @// r4 = *(sp++);
		                                                  @// lr = *(sp++);
ENDPROC(copy_page)
