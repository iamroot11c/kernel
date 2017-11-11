/*
 * Copyright 1995, Russell King.
 * Various bits and pieces copyrights include:
 *  Linus Torvalds (test_bit).
 * Big endian support: Copyright 2001, Nicolas Pitre
 *  reworked by rmk.
 *
 * bit 0 is the LSB of an "unsigned long" quantity.
 *
 * Please note that the code in this file should never be included
 * from user space.  Many of these are not implemented in assembler
 * since they would be too costly.  Also, they require privileged
 * instructions (which are not available from user mode) to ensure
 * that they are atomic.
 */

#ifndef __ASM_ARM_BITOPS_H
#define __ASM_ARM_BITOPS_H

#ifdef __KERNEL__

#ifndef _LINUX_BITOPS_H
#error only <linux/bitops.h> can be included directly
#endif

#include <linux/compiler.h>
#include <linux/irqflags.h>

#define smp_mb__before_clear_bit()	smp_mb()
// 2015-10-17
// 2015-10-24;
#define smp_mb__after_clear_bit()	smp_mb()

/*
 * These functions are the basis of our bit ops.
 *
 * First, the atomic bitops. These use native endian.
 */
static inline void ____atomic_set_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p |= mask;
	raw_local_irq_restore(flags);
}

static inline void ____atomic_clear_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p &= ~mask;
	raw_local_irq_restore(flags);
}

static inline void ____atomic_change_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	*p ^= mask;
	raw_local_irq_restore(flags);
}

// 2017-11-11
static inline int
____atomic_test_and_set_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res | mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

static inline int
____atomic_test_and_clear_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res & ~mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

static inline int
____atomic_test_and_change_bit(unsigned int bit, volatile unsigned long *p)
{
	unsigned long flags;
	unsigned int res;
	unsigned long mask = 1UL << (bit & 31);

	p += bit >> 5;

	raw_local_irq_save(flags);
	res = *p;
	*p = res ^ mask;
	raw_local_irq_restore(flags);

	return (res & mask) != 0;
}

#include <asm-generic/bitops/non-atomic.h>

/*
 *  A note about Endian-ness.
 *  -------------------------
 *
 * When the ARM is put into big endian mode via CR15, the processor
 * merely swaps the order of bytes within words, thus:
 *
 *          ------------ physical data bus bits -----------
 *          D31 ... D24  D23 ... D16  D15 ... D8  D7 ... D0
 * little     byte 3       byte 2       byte 1      byte 0
 * big        byte 0       byte 1       byte 2      byte 3
 *
 * This means that reading a 32-bit word at address 0 returns the same
 * value irrespective of the endian mode bit.
 *
 * Peripheral devices should be connected with the data bus reversed in
 * "Big Endian" mode.  ARM Application Note 61 is applicable, and is
 * available from http://www.arm.com/.
 *
 * The following assumes that the data bus connectivity for big endian
 * mode has been followed.
 *
 * Note that bit 0 is defined to be 32-bit word bit 0, not byte 0 bit 0.
 */

/*
 * Native endian assembly bitops.  nr = 0 -> word 0 bit 0.
 */
extern void _set_bit(int nr, volatile unsigned long * p);
extern void _clear_bit(int nr, volatile unsigned long * p);
extern void _change_bit(int nr, volatile unsigned long * p);
extern int _test_and_set_bit(int nr, volatile unsigned long * p);
extern int _test_and_clear_bit(int nr, volatile unsigned long * p);
extern int _test_and_change_bit(int nr, volatile unsigned long * p);

/*
 * Little endian assembly bitops.  nr = 0 -> byte 0 bit 0.
 */
extern int _find_first_zero_bit_le(const void * p, unsigned size);
extern int _find_next_zero_bit_le(const void * p, int size, int offset);
extern int _find_first_bit_le(const unsigned long *p, unsigned size);
extern int _find_next_bit_le(const unsigned long *p, int size, int offset);

/*
 * Big endian assembly bitops.  nr = 0 -> byte 3 bit 0.
 */
extern int _find_first_zero_bit_be(const void * p, unsigned size);
extern int _find_next_zero_bit_be(const void * p, int size, int offset);
extern int _find_first_bit_be(const unsigned long *p, unsigned size);
extern int _find_next_bit_be(const unsigned long *p, int size, int offset);

#ifndef CONFIG_SMP // defined
/*
 * The __* form of bitops are non-atomic and may be reordered.
 */
#define ATOMIC_BITOP(name,nr,p)			\
	(__builtin_constant_p(nr) ? ____atomic_##name(nr, p) : _##name(nr,p))
#else 
// 2016-01-16
// 2016-03-26
// _test_and_set_bit(PG_locked, &page->flags)
#define ATOMIC_BITOP(name,nr,p)		_##name(nr,p)
#endif

/*
 * Native endian atomic definitions.
 */
// 2016-01-16
#define set_bit(nr,p)			ATOMIC_BITOP(set_bit,nr,p) // _set_bit(nr, p);
#define clear_bit(nr,p)			ATOMIC_BITOP(clear_bit,nr,p)
#define change_bit(nr,p)		ATOMIC_BITOP(change_bit,nr,p)
// 2014-12-13
// _test_and_set_bit(nr, p)
// 2016-03-26
// test_and_set_bit(PG_locked, &page->flags)
// ->  ____atomic_test_and_set_bit(PG_locked, &page->flags)
// 1) nr 번째 비트를 p에 세팅 2) 세팅되기 이전, 해당 비트가 설정되어 있었는지를 반환
#define test_and_set_bit(nr,p)		ATOMIC_BITOP(test_and_set_bit,nr,p)
// 2014-12-13
// _test_and_clear_bit(nr, p)
#define test_and_clear_bit(nr,p)	ATOMIC_BITOP(test_and_clear_bit,nr,p)
#define test_and_change_bit(nr,p)	ATOMIC_BITOP(test_and_change_bit,nr,p)

#ifndef __ARMEB__
/*
 * These are the little endian, atomic definitions.
 */
#define find_first_zero_bit(p,sz)	_find_first_zero_bit_le(p,sz)
// 2016-04-02
// 첫번째 0인 비트인 값을 찾는다.
// 0x7fffffff; => 31
// 2016-09-10
#define find_next_zero_bit(p,sz,off)	_find_next_zero_bit_le(p,sz,off)
#define find_first_bit(p,sz)		_find_first_bit_le(p,sz)
// 2015-02-28;
// find_next_bit(cpu_possible_mask->bits, 2, n+1);
// 2015-08-29
// find_next_bit(cpu_possible_mask->bits, 2, n+1);
// 메모리 p 의 off 번째 비트부터 첫번째 찾은 1 인 비트값을 돌려줌
// ref: http://forum.falinux.com/zbxe/index.php?document_srl=533512&mid=Kernel_API
#define find_next_bit(p,sz,off)		_find_next_bit_le(p,sz,off)

#else
/*
 * These are the big endian, atomic definitions.
 */
#define find_first_zero_bit(p,sz)	_find_first_zero_bit_be(p,sz)
#define find_next_zero_bit(p,sz,off)	_find_next_zero_bit_be(p,sz,off)
#define find_first_bit(p,sz)		_find_first_bit_be(p,sz)
#define find_next_bit(p,sz,off)		_find_next_bit_be(p,sz,off)

#endif

#if __LINUX_ARM_ARCH__ < 5

#include <asm-generic/bitops/ffz.h>
#include <asm-generic/bitops/__fls.h>
#include <asm-generic/bitops/__ffs.h>
#include <asm-generic/bitops/fls.h>
#include <asm-generic/bitops/ffs.h>

#else

static inline int constant_fls(int x)
{
	int r = 32;

	if (!x)
		return 0;
	if (!(x & 0xffff0000u)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff000000u)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf0000000u)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc0000000u)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x80000000u)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

/*
 * On ARMv5 and above those functions can be implemented around
 * the clz instruction for much better code efficiency.
 */

// 2015-01-17
// x : 4096 -> return 13;
// 처음으로 1이 나온 비트 number를 리턴
// 2015-02-28; 내용 보충
// fls (find last set)
// ffs (find first set)
// ffz (find first zero)
//
// 명명 규칙은 아래와 같다.
// MSB ............... LSB
// LSB부터 찾을 때 first
// MSB부터 찾을 때 last

// set - 1을 찾는다.
// zero - 0을 찾는다.
// 참고: http://u-city.tistory.com/143
//
// 셋되어있는 맨 마지막 비트의 위치를 찾음
static inline int fls(int x)
{
	int ret;

    // // http://blog.dasomoli.org/335
    // http://gcc.gnu.org/onlinedocs/gcc-4.6.3/gcc/Other-Builtins.html
    // 컴파일 타임에 상수로 정해질 수 있는 경우에 1을, 아닌 경우에는 0을 리턴
	if (__builtin_constant_p(x))
	       return constant_fls(x);

	asm("clz\t%0, %1" : "=r" (ret) : "r" (x));
       	ret = 32 - ret;
	return ret;
}

#define __fls(x) (fls(x) - 1)
//2015-02-28; 처으으로 셋되어 있는 비트를 찾음
#define ffs(x) ({ unsigned long __t = (x); fls(__t & -__t); })
#define __ffs(x) (ffs(x) - 1)
// find first zero
// 첫번째 1이 나오기까지 0의 개수를 구함
#define ffz(x) __ffs( ~(x) )

#endif

#include <asm-generic/bitops/fls64.h>

#include <asm-generic/bitops/sched.h>
#include <asm-generic/bitops/hweight.h>
#include <asm-generic/bitops/lock.h>

#ifdef __ARMEB__

static inline int find_first_zero_bit_le(const void *p, unsigned size)
{
	return _find_first_zero_bit_le(p, size);
}
#define find_first_zero_bit_le find_first_zero_bit_le

static inline int find_next_zero_bit_le(const void *p, int size, int offset)
{
	return _find_next_zero_bit_le(p, size, offset);
}
#define find_next_zero_bit_le find_next_zero_bit_le

static inline int find_next_bit_le(const void *p, int size, int offset)
{
	return _find_next_bit_le(p, size, offset);
}
#define find_next_bit_le find_next_bit_le

#endif

#include <asm-generic/bitops/le.h>

/*
 * Ext2 is defined to use little-endian byte ordering.
 */
#include <asm-generic/bitops/ext2-atomic-setbit.h>

#endif /* __KERNEL__ */

#endif /* _ARM_BITOPS_H */
