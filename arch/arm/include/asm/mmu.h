#ifndef __ARM_MMU_H
#define __ARM_MMU_H

#ifdef CONFIG_MMU   // y

// 2015-08-22
typedef struct {
#ifdef CONFIG_CPU_HAS_ASID  // y
	atomic64_t	id;
#else
	int		switch_pending;
#endif
	unsigned int	vmalloc_seq;
	unsigned long	sigpage;
} mm_context_t;

#ifdef CONFIG_CPU_HAS_ASID  // y
#define ASID_BITS	8
// 2015-08-22
#define ASID_MASK	((~0ULL) << ASID_BITS)  // 0xFFFF_FF00
// 2015-08-22
// counter를 얻어올때는 하위 8비트(1byte)값만 취한다.
#define ASID(mm)	((mm)->context.id.counter & ~ASID_MASK/*0xFFFF_FF00*//*=>0x0000_00FF*/)
#else
#define ASID(mm)	(0)
#endif

#else

/*
 * From nommu.h:
 *  Copyright (C) 2002, David McCullough <davidm@snapgear.com>
 *  modified for 2.6 by Hyok S. Choi <hyok.choi@samsung.com>
 */
typedef struct {
	unsigned long	end_brk;
} mm_context_t;

#endif

#endif
