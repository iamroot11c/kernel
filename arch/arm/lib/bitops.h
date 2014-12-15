#include <asm/unwind.h>

/* 
* ref: iamroot 9차 분석 내용 정리 
* http://u-city.tistory.com/143 
*/

#if __LINUX_ARM_ARCH__ >= 6
	.macro	bitop, name, instr
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	mov	r2, #1
	and	r3, r0, #31		@ Get bit offset
	mov	r0, r0, lsr #5
	add	r1, r1, r0, lsl #2	@ Get word offset
	mov	r3, r2, lsl r3
1:	ldrex	r2, [r1]
	\instr	r2, r2, r3
	strex	r0, r2, [r1]
	cmp	r0, #0
	bne	1b
	bx	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm

/* 9차분들의 분석 내용 참고 */
	.macro	testop, name, instr, store
ENTRY(	\name		)
UNWIND(	.fnstart	)
	/*
		하위2비트에 대해서 and 연산 후 ip(r12)에 저장
		CPSR 업데이트

		정렬되어 있다면 0이므로 z가 1, 정렬되어 있지 않다면 z가 0
		곧, 4byte align을 의미함.
	*/
	ands	ip, r1, #3

	// NE(Z clear) - 정렬 되어 있지 않다면 실행된다.
	// 4byte align이 되지 않았다면, assert를 발생시킨다.
	// ex) 0x1234_123F   // assert
	strneb	r1, [ip]		@ assert word-aligned
	mov	r2, #1
	// #31(0x1F - 하위 5비트) - 31이라는 것은, 한 word 32bit와 연관된 것 아닐까?
	// 특정 word 상의 offset으로 삼기 위함
	// 32bit 머신, 하나의 word에 대해서, 0 ~ 31bit까지 컨트롤 할 수 있다.
	and	r3, r0, #31		@ Get bit offset
	// r0를 32로 나눈 인덱스로 변환. 몇 번째 word를 찾아내야 하는지 구함
	// 이렇게 함으로써, r0가 31을 넘는 경우도 bit set/clear가 가능하다
	// ex) _test_and_clear_bit(32, p), r0 = 1
	mov	r0, r0, lsr #5
	// r1을 r0이 가리키는 워드로 변경함
	// r1 = r1 << r0;
	// ex) r1 = 0x1234_1240, r0 = 1 
	// => 0x1234_1244(offset만큼, 다음 word로 이동한다)
	add	r1, r1, r0, lsl #2	@ Get word offset
	// 실제 clear할 bit에대한 mask값, 16번째를 클리어 하려 했다면, 1 << 16(0x0001_0000)
	mov	r3, r2, lsl r3		@ create mask
	smp_dmb
	// 2014-12-13, 식사전
1:	ldrex	r2, [r1]
	ands	r0, r2, r3		@ save old value of bit
	// \instr(bicne), 특정 비트를 클리어 
	// \instr(orreq)
	\instr	r2, r2, r3		@ toggle bit
	// r2 = *(r1), 상태값을 ip에 저장
	strex	ip, r2, [r1]
	cmp	ip, #0
	// 상태값을 읽어서, 값변경에 실패했다면 1b하여, retry
	bne	1b
	smp_dmb
	// 호출 규약(AAPCS)에 의해서 r0에는 리턴값인데
	// 이 경우, old(bit)값을 리턴하는 것으로 보임
	cmp	r0, #0
	movne	r0, #1
2:	bx	lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm
#else
	.macro	bitop, name, instr
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	and	r2, r0, #31
	mov	r0, r0, lsr #5
	mov	r3, #1
	mov	r3, r3, lsl r2
	save_and_disable_irqs ip
	ldr	r2, [r1, r0, lsl #2]
	\instr	r2, r2, r3
	str	r2, [r1, r0, lsl #2]
	restore_irqs ip
	mov	pc, lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm

/**
 * testop - implement a test_and_xxx_bit operation.
 * @instr: operational instruction
 * @store: store instruction
 *
 * Note: we can trivially conditionalise the store instruction
 * to avoid dirtying the data cache.
 */
	.macro	testop, name, instr, store
ENTRY(	\name		)
UNWIND(	.fnstart	)
	ands	ip, r1, #3
	strneb	r1, [ip]		@ assert word-aligned
	and	r3, r0, #31
	mov	r0, r0, lsr #5
	save_and_disable_irqs ip
	ldr	r2, [r1, r0, lsl #2]!
	mov	r0, #1
	tst	r2, r0, lsl r3
	\instr	r2, r2, r0, lsl r3
	\store	r2, [r1]
	moveq	r0, #0
	restore_irqs ip
	mov	pc, lr
UNWIND(	.fnend		)
ENDPROC(\name		)
	.endm
#endif
