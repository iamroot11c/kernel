#ifndef __ASM_SPINLOCK_TYPES_H
#define __ASM_SPINLOCK_TYPES_H

#ifndef __LINUX_SPINLOCK_TYPES_H
# error "please don't include this file directly"
#endif

#define TICKET_SHIFT	16

// 2015-11-07
// 2016-01-30
// 2016-10-08
typedef struct {
	union {
		u32 slock;
		struct __raw_tickets {
#ifdef __ARMEB__	// not set
			u16 next;
			u16 owner;
#else
			u16 owner; // 하위 16bit
			u16 next;  // 상위 16bit

            /**
             * 161008
             * owner가 0xFFFF 일 때 값을 1 증가 시키면 
             * 오버플로어가 발생되어도 next 멤버에 영향을
             * 주지 않는다
             *
             * 0xFFFF + 0x01 == 0x0000
             * 0xFFFF_FFFF + 0x01 == 0x0000_0000
             */
#endif
		} tickets;
	};
} arch_spinlock_t;

#define __ARCH_SPIN_LOCK_UNLOCKED	{ { 0 } }

// 2016-02-06;
typedef struct {
	volatile unsigned int lock;
} arch_rwlock_t;

#define __ARCH_RW_LOCK_UNLOCKED		{ 0 }

#endif
