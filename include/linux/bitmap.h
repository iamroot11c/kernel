#ifndef __LINUX_BITMAP_H
#define __LINUX_BITMAP_H

#ifndef __ASSEMBLY__

#include <linux/types.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include <linux/kernel.h>

/*
 * bitmaps provide bit arrays that consume one or more unsigned
 * longs.  The bitmap interface and available operations are listed
 * here, in bitmap.h
 *
 * Function implementations generic to all architectures are in
 * lib/bitmap.c.  Functions implementations that are architecture
 * specific are in various include/asm-<arch>/bitops.h headers
 * and other arch/<arch> specific files.
 *
 * See lib/bitmap.c for more details.
 */

/*
 * The available bitmap operations and their rough meaning in the
 * case that the bitmap is a single unsigned long are thus:
 *
 * Note that nbits should be always a compile time evaluable constant.
 * Otherwise many inlines will generate horrible code.
 *
 * bitmap_zero(dst, nbits)			*dst = 0UL
 * bitmap_fill(dst, nbits)			*dst = ~0UL
 * bitmap_copy(dst, src, nbits)			*dst = *src
 * bitmap_and(dst, src1, src2, nbits)		*dst = *src1 & *src2
 * bitmap_or(dst, src1, src2, nbits)		*dst = *src1 | *src2
 * bitmap_xor(dst, src1, src2, nbits)		*dst = *src1 ^ *src2
 * bitmap_andnot(dst, src1, src2, nbits)	*dst = *src1 & ~(*src2)
 * bitmap_complement(dst, src, nbits)		*dst = ~(*src)
 * bitmap_equal(src1, src2, nbits)		Are *src1 and *src2 equal?
 * bitmap_intersects(src1, src2, nbits) 	Do *src1 and *src2 overlap?
 * bitmap_subset(src1, src2, nbits)		Is *src1 a subset of *src2?
 * bitmap_empty(src, nbits)			Are all bits zero in *src?
 * bitmap_full(src, nbits)			Are all bits set in *src?
 * bitmap_weight(src, nbits)			Hamming Weight: number set bits
 * bitmap_set(dst, pos, nbits)			Set specified bit area
 * bitmap_clear(dst, pos, nbits)		Clear specified bit area
 * bitmap_find_next_zero_area(buf, len, pos, n, mask)	Find bit free area
 * bitmap_shift_right(dst, src, n, nbits)	*dst = *src >> n
 * bitmap_shift_left(dst, src, n, nbits)	*dst = *src << n
 * bitmap_remap(dst, src, old, new, nbits)	*dst = map(old, new)(src)
 * bitmap_bitremap(oldbit, old, new, nbits)	newbit = map(old, new)(oldbit)
 * bitmap_onto(dst, orig, relmap, nbits)	*dst = orig relative to relmap
 * bitmap_fold(dst, orig, sz, nbits)		dst bits = orig bits mod sz
 * bitmap_scnprintf(buf, len, src, nbits)	Print bitmap src to buf
 * bitmap_parse(buf, buflen, dst, nbits)	Parse bitmap dst from kernel buf
 * bitmap_parse_user(ubuf, ulen, dst, nbits)	Parse bitmap dst from user buf
 * bitmap_scnlistprintf(buf, len, src, nbits)	Print bitmap src as list to buf
 * bitmap_parselist(buf, dst, nbits)		Parse bitmap dst from kernel buf
 * bitmap_parselist_user(buf, dst, nbits)	Parse bitmap dst from user buf
 * bitmap_find_free_region(bitmap, bits, order)	Find and allocate bit region
 * bitmap_release_region(bitmap, pos, order)	Free specified bit region
 * bitmap_allocate_region(bitmap, pos, order)	Allocate specified bit region
 */

/*
 * Also the following operations in asm/bitops.h apply to bitmaps.
 *
 * set_bit(bit, addr)			*addr |= bit
 * clear_bit(bit, addr)			*addr &= ~bit
 * change_bit(bit, addr)		*addr ^= bit
 * test_bit(bit, addr)			Is bit set in *addr?
 * test_and_set_bit(bit, addr)		Set bit and return old value
 * test_and_clear_bit(bit, addr)	Clear bit and return old value
 * test_and_change_bit(bit, addr)	Change bit and return old value
 * find_first_zero_bit(addr, nbits)	Position first zero bit in *addr
 * find_first_bit(addr, nbits)		Position first set bit in *addr
 * find_next_zero_bit(addr, nbits, bit)	Position next zero bit in *addr >= bit
 * find_next_bit(addr, nbits, bit)	Position next set bit in *addr >= bit
 */

/*
 * The DECLARE_BITMAP(name,bits) macro, in linux/types.h, can be used
 * to declare an array named 'name' of just enough unsigned longs to
 * contain all bit positions from 0 to 'bits' - 1.
 */

/*
 * lib/bitmap.c provides these functions:
 */

extern int __bitmap_empty(const unsigned long *bitmap, int bits);
extern int __bitmap_full(const unsigned long *bitmap, int bits);
extern int __bitmap_equal(const unsigned long *bitmap1,
                	const unsigned long *bitmap2, int bits);
extern void __bitmap_complement(unsigned long *dst, const unsigned long *src,
			int bits);
extern void __bitmap_shift_right(unsigned long *dst,
                        const unsigned long *src, int shift, int bits);
extern void __bitmap_shift_left(unsigned long *dst,
                        const unsigned long *src, int shift, int bits);
extern int __bitmap_and(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern void __bitmap_or(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern void __bitmap_xor(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_andnot(unsigned long *dst, const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_intersects(const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_subset(const unsigned long *bitmap1,
			const unsigned long *bitmap2, int bits);
extern int __bitmap_weight(const unsigned long *bitmap, int bits);

extern void bitmap_set(unsigned long *map, int i, int len);
extern void bitmap_clear(unsigned long *map, int start, int nr);
extern unsigned long bitmap_find_next_zero_area(unsigned long *map,
					 unsigned long size,
					 unsigned long start,
					 unsigned int nr,
					 unsigned long align_mask);

extern int bitmap_scnprintf(char *buf, unsigned int len,
			const unsigned long *src, int nbits);
extern int __bitmap_parse(const char *buf, unsigned int buflen, int is_user,
			unsigned long *dst, int nbits);
extern int bitmap_parse_user(const char __user *ubuf, unsigned int ulen,
			unsigned long *dst, int nbits);
extern int bitmap_scnlistprintf(char *buf, unsigned int len,
			const unsigned long *src, int nbits);
extern int bitmap_parselist(const char *buf, unsigned long *maskp,
			int nmaskbits);
extern int bitmap_parselist_user(const char __user *ubuf, unsigned int ulen,
			unsigned long *dst, int nbits);
extern void bitmap_remap(unsigned long *dst, const unsigned long *src,
		const unsigned long *old, const unsigned long *new, int bits);
extern int bitmap_bitremap(int oldbit,
		const unsigned long *old, const unsigned long *new, int bits);
extern void bitmap_onto(unsigned long *dst, const unsigned long *orig,
		const unsigned long *relmap, int bits);
extern void bitmap_fold(unsigned long *dst, const unsigned long *orig,
		int sz, int bits);
extern int bitmap_find_free_region(unsigned long *bitmap, int bits, int order);
extern void bitmap_release_region(unsigned long *bitmap, int pos, int order);
extern int bitmap_allocate_region(unsigned long *bitmap, int pos, int order);
extern void bitmap_copy_le(void *dst, const unsigned long *src, int nbits);
extern int bitmap_ord_to_pos(const unsigned long *bitmap, int n, int bits);

// 2016-04-16
#define BITMAP_FIRST_WORD_MASK(start) (~0UL << ((start) % BITS_PER_LONG))

// (2 % 32) ? (1 << (2%32)) - 1 : !0
//
// cpu 상태를 표시하는데 필요한 비트수를 십진수로 계산
// 즉 cpu가 2개일 때 비트 2개로 표현할 수 있으며 이를 
// 십진수로 표시하면 '3'이다
// 현재 사용 가능한 cpu 개수에 대해 1로 세팅
// 2015-08-29
// 2016-04-09
#define BITMAP_LAST_WORD_MASK(nbits)					\
(									\
	((nbits) % BITS_PER_LONG) ?					\
		(1UL<<((nbits) % BITS_PER_LONG))-1 : ~0UL		\
)

// BITS_PER_LONG - 32bit: 32
// __builtin_constant_p() 함수는 컴파일 타임에 nbits가 상수인지 검사
// 2015-03-21 확인. nbits가 상수이면서 BITS_PER_LONG보다 작은 경우 참
// 2015-08-29
// 2016-01-16
#define small_const_nbits(nbits) \
	(__builtin_constant_p(nbits) && (nbits) <= BITS_PER_LONG)

// 2016-01-16
// bitmap_zero(shrink->nodes_to_scan MAX_NUMNODES/*1*/)
static inline void bitmap_zero(unsigned long *dst, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = 0UL;
	else {
        // len = DIV_ROUND_UP(1, BITS_PER_BYTE/*8*/ * sizeof(long))/*1*/;
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
        // 메모리를 0으로 초기화
        // 2016-01-16; 1 byte 초기화
		memset(dst, 0, len);
	}
}
// 2015-03-21 확인
// 2016-08-06
// bitmap_fill(cpumask_bits(dstp), nr_cpumask_bits/*2*/)
// 2016-12-10
// nbits만큼을, 1로 설정
static inline void bitmap_fill(unsigned long *dst, int nbits)
{
	size_t nlongs = BITS_TO_LONGS(nbits);
    // nbits는 실행 시간에 값이 설정되기 때문에 small_const_nbits함수에서 항상 false가 리턴
    if (!small_const_nbits(nbits)) {
		// 지금 분석 대상의 사양에서는 nlongs의 값이 1이 나올 것이다.
        int len = (nlongs - 1) * sizeof(unsigned long);
        // no operation
        memset(dst, 0xff,  len);
	}
    // nbits로 나타낼 수 있는 최대 경우의 수 값을 설정.(ex. nbits : 3-> return : 7)
	dst[nlongs - 1] = BITMAP_LAST_WORD_MASK(nbits);
}

// 2015-08-29
// 2016-04-02
// 2016-04-09
// bitmap_copy(bitmap, chunk->populated, pcpu_unit_pages);
// 2016-04-16
// bitmap_copy(chunk->populated, populated, pcpu_unit_pages)
static inline void bitmap_copy(unsigned long *dst, const unsigned long *src,
			int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src;
	else {
        // nbits : 2로 가정
        // len = 1 * 4
		int len = BITS_TO_LONGS(nbits) * sizeof(unsigned long);
		memcpy(dst, src, len);
	}
}

// 2015-08-29
// src1, src2 비교해서, 범위 내에서 서로 1인 경우 유무를, bool 형태로 리턴
static inline int bitmap_and(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))   // 2015-08-29, nbits가 2임으로 여기를 탈 것 같다.
		return (*dst = *src1 & *src2) != 0;
	return __bitmap_and(dst, src1, src2, nbits);
}

static inline void bitmap_or(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src1 | *src2;
	else
		__bitmap_or(dst, src1, src2, nbits);
}

static inline void bitmap_xor(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src1 ^ *src2;
	else
		__bitmap_xor(dst, src1, src2, nbits);
}

static inline int bitmap_andnot(unsigned long *dst, const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return (*dst = *src1 & ~(*src2)) != 0;
	return __bitmap_andnot(dst, src1, src2, nbits);
}

static inline void bitmap_complement(unsigned long *dst, const unsigned long *src,
			int nbits)
{
	if (small_const_nbits(nbits))
		*dst = ~(*src) & BITMAP_LAST_WORD_MASK(nbits);
	else
		__bitmap_complement(dst, src, nbits);
}

// 2014-11-29;
static inline int bitmap_equal(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
//  if (small_const_nbits(2))    
	if (small_const_nbits(nbits))
		return ! ((*src1 ^ *src2) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_equal(src1, src2, nbits);
}

// 2016-11-26;
// bitmap_intersects(cpumask_bits(src1p), cpumask_bits(src2p),
//                                             nr_cpumask_bits/*2*/)
static inline int bitmap_intersects(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return ((*src1 & *src2) & BITMAP_LAST_WORD_MASK(nbits)/*3*/) != 0;
	else
		return __bitmap_intersects(src1, src2, nbits);
}

static inline int bitmap_subset(const unsigned long *src1,
			const unsigned long *src2, int nbits)
{
	if (small_const_nbits(nbits))
		return ! ((*src1 & ~(*src2)) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_subset(src1, src2, nbits);
}

static inline int bitmap_empty(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		return ! (*src & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_empty(src, nbits);
}

// 2016-08-20
// *src 값에 0~nbits만큼이 전부 1로 세팅되어있는지 확인
//
// 2016-09-10
// bitmap_full(p->bitmap, IDR_SIZE)
static inline int bitmap_full(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits))
		return ! (~(*src) & BITMAP_LAST_WORD_MASK(nbits));
	else
		return __bitmap_full(src, nbits);
}

// 2015-02-28;
// bitmap_weight(cpumask_bits(srcp), nr_cpumask_bits);
// 2015-08-29
// bitmap_weight(cpumask_bits(srcp), nr_cpumask_bits/*2*/);
// 2016-01-23
// 비트가 셋된 개수를 얻어온다.
static inline int bitmap_weight(const unsigned long *src, int nbits)
{
	if (small_const_nbits(nbits)) // nbits가 0에서 32 사이의 값 
		return hweight_long(*src & BITMAP_LAST_WORD_MASK(nbits));
        // src과 nbits의 마스크를 and 연산 후 셋 비트의 개수를 구함
	return __bitmap_weight(src, nbits);
    // src에서 셋 비트의 개수를 구함
}

static inline void bitmap_shift_right(unsigned long *dst,
			const unsigned long *src, int n, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = *src >> n;
	else
		__bitmap_shift_right(dst, src, n, nbits);
}

static inline void bitmap_shift_left(unsigned long *dst,
			const unsigned long *src, int n, int nbits)
{
	if (small_const_nbits(nbits))
		*dst = (*src << n) & BITMAP_LAST_WORD_MASK(nbits);
	else
		__bitmap_shift_left(dst, src, n, nbits);
}

static inline int bitmap_parse(const char *buf, unsigned int buflen,
			unsigned long *maskp, int nmaskbits)
{
	return __bitmap_parse(buf, buflen, 0, maskp, nmaskbits);
}

#endif /* __ASSEMBLY__ */

#endif /* __LINUX_BITMAP_H */
