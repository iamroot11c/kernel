#ifndef _ASM_GENERIC_BITOPS_NON_ATOMIC_H_
#define _ASM_GENERIC_BITOPS_NON_ATOMIC_H_

#include <asm/types.h>

/**
 * __set_bit - Set a bit in memory
 * @nr: the bit to set
 * @addr: the address to start counting from
 *
 * Unlike set_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void __set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p  |= mask;
}

static inline void __clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p &= ~mask;
}

/**
 * __change_bit - Toggle a bit in memory
 * @nr: the bit to change
 * @addr: the address to start counting from
 *
 * Unlike change_bit(), this function is non-atomic and may be reordered.
 * If it's called on the same region of memory simultaneously, the effect
 * may be that only one operation succeeds.
 */
static inline void __change_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);

	*p ^= mask;
}

/**
 * __test_and_set_bit - Set a bit and return its old value
 * @nr: Bit to set
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static inline int __test_and_set_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old | mask;
	return (old & mask) != 0;
}

/**
 * __test_and_clear_bit - Clear a bit and return its old value
 * @nr: Bit to clear
 * @addr: Address to count from
 *
 * This operation is non-atomic and can be reordered.
 * If two examples of this operation race, one can appear to succeed
 * but actually fail.  You must protect multiple accesses with a lock.
 */
static inline int __test_and_clear_bit(int nr, volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old & ~mask;
	return (old & mask) != 0;
}

/* WARNING: non atomic and it can be reordered! */
static inline int __test_and_change_bit(int nr,
					    volatile unsigned long *addr)
{
	unsigned long mask = BIT_MASK(nr);
	unsigned long *p = ((unsigned long *)addr) + BIT_WORD(nr);
	unsigned long old = *p;

	*p = old ^ mask;
	return (old & mask) != 0;
}

/**
 * test_bit - Determine whether a bit is set
 * @nr: bit number to test
 * @addr: Address to start counting from
 */
// 2014-12-20;
static inline int test_bit(int nr, const volatile unsigned long *addr)
{
	return 1UL & (addr[BIT_WORD(nr)] >> (nr & (BITS_PER_LONG-1)));
//               addr[(nr/BITS_PER_LONG)]
//               |---    가     ---|    |---        나       ---|
//
//               몫을 구해 addr에서 몫만큼 이동해서 값의 구한후
//               나머지만큼 쉬프트해서 비트 조사
//
//               36개의 페이지가 있을때 각 각을 1개의 비트로 표현하면
//               36개의 비트가 필요하여 총 5바이트가 필요
//
//               32비트 아키텍처에서는 32개씩 나누어서 접근을 해야하기
//               때문에 위의 '가'에서는 검사하는 비트가 1 ~ 32 사이에 
//               있다면 인덱스를 0으로 33 ~ 36 사이에 있으면 인덱스를 1로 지정
//
//               '나'는 비트수가 32를 넘었을 때 다시 계산되도록 나머지를 구함
//               즉 'nr'이 2일 때는 배열의 0번 인덱스에서
//               34일 때는 1번 인덱스에서 값을 가져오며 검사하는 비트는 
//               2번 비트를 구할 수 있음
}

#endif /* _ASM_GENERIC_BITOPS_NON_ATOMIC_H_ */
