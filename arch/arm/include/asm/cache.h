/*
 *  arch/arm/include/asm/cache.h
 */
#ifndef __ASMARM_CACHE_H
#define __ASMARM_CACHE_H

/* CONFIG_ARM_L1_CACHE_SHIFT=6 */
#define L1_CACHE_SHIFT		CONFIG_ARM_L1_CACHE_SHIFT
/* L1_CACHE_BYTES == 64 */
#define L1_CACHE_BYTES		(1 << L1_CACHE_SHIFT) // 0x40 = 1 << 6
                                                  // 64Byte

/*
 * Memory returned by kmalloc() may be used for DMA, so we must make
 * sure that all such allocations are cache aligned. Otherwise,
 * unrelated code may cause parts of the buffer to be read into the
 * cache before the transfer is done, causing old data to be seen by
 * the CPU.
 */
// 2015-05-06
#define ARCH_DMA_MINALIGN/*0x40, 64*/	L1_CACHE_BYTES

/*
 * With EABI on ARMv5 and above we must have 64-bit aligned slab pointers.
 */
// 2015-05-16
#if defined(CONFIG_AEABI) && (__LINUX_ARM_ARCH__ >= 5)
#define ARCH_SLAB_MINALIGN 8
#endif

// 2015-09-12
// .data 섹션 영역이기 때문에 읽고 쓰기가 가능
#define __read_mostly __attribute__((__section__(".data..read_mostly")))

#endif
