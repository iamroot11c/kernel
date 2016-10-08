#ifndef __ASM_SPINLOCK_H
#define __ASM_SPINLOCK_H

#if __LINUX_ARM_ARCH__ < 6
#error SMP not supported on pre-ARMv6 CPUs
#endif

#include <asm/processor.h>

/*
 * sev and wfe are ARMv6K extensions.  Uniprocessor ARMv6 may not have the K
 * extensions, so when running on UP, we have to patch these instructions away.
 */
// 2015-11-07
// ALT_SMP("sev", "nop")
/* ex)
 * 9998:   sev  
 * .pushsection ".alt.smp.init", "a"  
 * .long   9998b            
 * nop
 * .popsection
 * */
// http://www.iamroot.org/xe/Kernel_10_ARM/180812
// http://stackoverflow.com/questions/17083941/what-does-alt-smp-and-alt-up-does
// alt.smp.init 섹션에 원하는 명령어 패치를 하는 것으로 보인다.
#define ALT_SMP(smp, up)					\
	"9998:	" smp "\n"					\
	"	.pushsection \".alt.smp.init\", \"a\"\n"	\
	"	.long	9998b\n"				\
	"	" up "\n"					\
	"	.popsection\n"

#ifdef CONFIG_THUMB2_KERNEL
#define SEV		ALT_SMP("sev.w", "nop.w")
/*
 * For Thumb-2, special care is needed to ensure that the conditional WFE
 * instruction really does assemble to exactly 4 bytes (as required by
 * the SMP_ON_UP fixup code).   By itself "wfene" might cause the
 * assembler to insert a extra (16-bit) IT instruction, depending on the
 * presence or absence of neighbouring conditional instructions.
 *
 * To avoid this unpredictableness, an approprite IT is inserted explicitly:
 * the assembler won't change IT instructions which are explicitly present
 * in the input.
 */
#define WFE(cond)	ALT_SMP(		\
	"it " cond "\n\t"			\
	"wfe" cond ".n",			\
						\
	"nop.w"					\
)
#else
// 2015-11-07
// 모든 프로세서에 시그널을 보내는 명령
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.faqs/ka15473.html
// SEV causes an event to be signaled to all cores within a multiprocessor system.
// If SEV is implemented, WFE must also be implemented.
// SEV - Set Event
// 2016-10-08 
#define SEV		ALT_SMP("sev", "nop")
// 2016-02-06
// http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dui0489f/CIHEGBBF.html
// If the Event Register is not set, WFE suspends execution until one of the following events occurs:
//  - an IRQ interrupt, unless masked by the CPSR I-bit
//  - an FIQ interrupt, unless masked by the CPSR F-bit
//  - an Imprecise Data abort, unless masked by the CPSR A-bit
//  - a Debug Entry request, if Debug is enabled
//  - an Event signaled by another processor using the SEV instruction.
//
// If the Event Register is set, WFE clears it and returns immediately.
//
// If WFE is implemented, SEV must also be implemented.
// WFE - Wait For Event,
#define WFE(cond)	ALT_SMP("wfe" cond, "nop")
#endif

// 2015-11-07
// 2016-10-08
static inline void dsb_sev(void)
{
	dsb(ishst);
	__asm__(SEV);
}

/*
 * ARMv6 ticket-based spin-locking.
 *
 * A memory barrier is required after we get a lock, and before we
 * release it, because V6 CPUs are assumed to have weakly ordered
 * memory.
 */

#define arch_spin_unlock_wait(lock) \
	do { while (arch_spin_is_locked(lock)) cpu_relax(); } while (0)

#define arch_spin_lock_flags(lock, flags) arch_spin_lock(lock)

// 2015-11-07
// lock을 얻을 때까지 무한루프를 돈다.
static inline void arch_spin_lock(arch_spinlock_t *lock)
{
	unsigned long tmp;
	u32 newval;
	arch_spinlock_t lockval;

    /*
    1: ldrex    lockval, [&lock->slock]
    add         newval, lockval, 1 << TICKET_SHIFT
    strex       tmp, newval, [&lock->slock]
    teq         tmp, #0
    bne         1b
    */
	__asm__ __volatile__(
"1:	ldrex	%0, [%3]\n"
"	add	%1, %0, %4\n"
"	strex	%2, %1, [%3]\n"
"	teq	%2, #0\n"
"	bne	1b"
	: "=&r" (lockval), "=&r" (newval), "=&r" (tmp)
	: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
	: "cc");

	while (lockval.tickets.next != lockval.tickets.owner) {
		wfe(); // 다른 프로세서에게 시그널 전송
		lockval.tickets.owner = ACCESS_ONCE(lock->tickets.owner);
	}

	smp_mb();
    // 2015-11-07 여기까지
}
// 2015-11-07
// lock의 owner와 next가 같은 경우 참. lock의 16번 비트에 1을 설정하여 락 플래그를 갱신
// lock의 owner와 next가 다른 경우 거짓. 그냥 빠져나온다.
// 2016-10-01
// 간단히 말해서 lock이 걸려있으면 즉시! 빠져나오고 안걸려있으면 락을 건다.
static inline int arch_spin_trylock(arch_spinlock_t *lock)
{
	unsigned long contended, res;
	u32 slock;
    
    /*
    ldrex   slock, [&lock->slock]
    mov     res, #0
    subs    contended, slock, slock, ror #16
    addeq   slock, slock, (1 << TICKET_SHIFT)
    strexeq res, slock, [&lock->slock]

    == 
    while(true)
    {
        slock = *(&lock->slock);
        res = 0;
        // slock이 하위 16비트와 상위 16비트가 같은 경우만
        // (arch_spinlock_t타입을 보면 slock 변수가 16비트 단위로
        // owner, next값이 설정되어 있기 때문에
        // owner == next 값이 같은 경우만..) 
        // contended가 0이다.
        contended = (slock - (slock ror 16));
        if (!contended)
        {
            // 16번째 비트를 설정해서 락에 대한 상태를 변경
             slock += (1 << 16);
            *(&lock->slock) = slock;
            if (!res)
                break;
        }
    }
    */

    // (TICKET_SHIFT) 번째 비트 설정 여부로 락이 걸렸는지 여부를 확인하고 있다.
	do {
		__asm__ __volatile__(
		"	ldrex	%0, [%3]\n"
		"	mov	%2, #0\n"
		"	subs	%1, %0, %0, ror #16\n"
		"	addeq	%0, %0, %4\n"
		"	strexeq	%2, %0, [%3]"
		: "=&r" (slock), "=&r" (contended), "=&r" (res)
		: "r" (&lock->slock), "I" (1 << TICKET_SHIFT)
		: "cc");
	} while (res);

	if (!contended) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}
// 2015-11-07
// 2016-10-08
// arch_spin_unlock(&lock->raw_lock)
static inline void arch_spin_unlock(arch_spinlock_t *lock)
{
	smp_mb();
	lock->tickets.owner++;
	dsb_sev();
}

static inline int arch_spin_is_locked(arch_spinlock_t *lock)
{
	struct __raw_tickets tickets = ACCESS_ONCE(lock->tickets);
	return tickets.owner != tickets.next;
}

static inline int arch_spin_is_contended(arch_spinlock_t *lock)
{
	struct __raw_tickets tickets = ACCESS_ONCE(lock->tickets);
	return (tickets.next - tickets.owner) > 1;
}
#define arch_spin_is_contended	arch_spin_is_contended

/*
 * RWLOCKS
 *
 *
 * Write locks are easy - we just set bit 31.  When unlocking, we can
 * just write zero since the lock is exclusively held.
 */
// 2016-02-13;
static inline void arch_write_lock(arch_rwlock_t *rw)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
	WFE("ne") /* reader가 없을 때까지 대기  */
/*"9998:  " wfene "\n"                  \
"   .pushsection \".alt.smp.init\", \"a\"\n"    \
"   .long   9998b\n"                \
"   nop \n"                   \
"   .popsection\n"
// pl - positive or zero
// mi - negative
// ne - Not equal
*/
"	strexeq	%0, %2, [%1]\n"
"	teq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&rw->lock), "r" (0x80000000/*음수*/)
	: "cc");

	smp_mb();
}

static inline int arch_write_trylock(arch_rwlock_t *rw)
{
	unsigned long contended, res;

	do {
		__asm__ __volatile__(
		"	ldrex	%0, [%2]\n"
		"	mov	%1, #0\n"
		"	teq	%0, #0\n"
		"	strexeq	%1, %3, [%2]"
		: "=&r" (contended), "=&r" (res)
		: "r" (&rw->lock), "r" (0x80000000)
		: "cc");
	} while (res);

	if (!contended) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}

// 2016-02-13;
static inline void arch_write_unlock(arch_rwlock_t *rw)
{
	smp_mb();

	__asm__ __volatile__(
	"str	%1, [%0]\n"
	:
	: "r" (&rw->lock), "r" (0)
	: "cc");

	dsb_sev();
}

/* write_can_lock - would write_trylock() succeed? */
#define arch_write_can_lock(x)		((x)->lock == 0)

/*
 * Read locks are a bit more hairy:
 *  - Exclusively load the lock value.
 *  - Increment it.
 *  - Store new lock value if positive, and we still own this location.
 *    If the value is negative, we've already failed.
 *  - If we failed to store the value, we want a negative result.
 *  - If we failed, try again.
 * Unlocking is similarly hairy.  We may have multiple read locks
 * currently active.  However, we know we won't have any write
 * locks.
 */
// 2016-02-06;
static inline void arch_read_lock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2;

    /* writer가 락을 획득하면 0x8000_0000 값을 lock 멤버에 기록을 해서
     * 최상위 비트를 1로 바꿔 음의 상태(N flags)로 변경된다
     * 그러면 reader는 lock 멤버를 갱신하지 못하며 'wfemi'명령어로
     * 대기하도록 만들어 졌다.
     *
     * MI Minus
     * Instructions with this condition only execute if the N (negative) flag is set.
     * Such a condition would occur when the last data operation gave a result 
     * which was negative. That is, the N flag reflects the state of bit 31 of the result.
     * (All data operations work on 32-bit numbers.)
     * [http://www.peter-cockerell.net/aalp/html/ch-3.html]
     */
	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	adds	%0, %0, #1\n"
"	strexpl	%1, %0, [%2]\n"
	WFE("mi") /* tmp의 값이 증가해도 마이너스이면 대기함 */
/*"9998:  " wfemi "\n"                  \
"   .pushsection \".alt.smp.init\", \"a\"\n"    \
"   .long   9998b\n"                \
"   nop \n"                   \
"   .popsection\n"
// pl - positive or zero
// mi - negative
*/
"	rsbpls	%0, %1, #0\n" /* lock 멤버의 갱신 결과인 tmp의 값이 양(+)일 때 조건 검사를 함 */
"	bmi	1b"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&rw->lock)
	: "cc");

	smp_mb();
}

// 2016-02-06
// arch_read_unlock(&lock->raw_lock)
static inline void arch_read_unlock(arch_rwlock_t *rw)
{
	unsigned long tmp, tmp2;

    // 락을 풀때는 먼저 실행
	smp_mb();

	__asm__ __volatile__(
"1:	ldrex	%0, [%2]\n"
"	sub	%0, %0, #1\n"
"	strex	%1, %0, [%2]\n"
"	teq	%1, #0\n"
"	bne	1b"
	: "=&r" (tmp), "=&r" (tmp2)
	: "r" (&rw->lock)
	: "cc");

    // rw->lock이 감소한 값이 0일 때 시그널 발생
	if (tmp == 0)
		dsb_sev();
}

static inline int arch_read_trylock(arch_rwlock_t *rw)
{
	unsigned long contended, res;

	do {
		__asm__ __volatile__(
		"	ldrex	%0, [%2]\n"
		"	mov	%1, #0\n"
		"	adds	%0, %0, #1\n"
		"	strexpl	%1, %0, [%2]"
		: "=&r" (contended), "=&r" (res)
		: "r" (&rw->lock)
		: "cc");
	} while (res);

	/* If the lock is negative, then it is already held for write. */
	if (contended < 0x80000000) {
		smp_mb();
		return 1;
	} else {
		return 0;
	}
}

/* read_can_lock - would read_trylock() succeed? */
#define arch_read_can_lock(x)		((x)->lock < 0x80000000)

#define arch_read_lock_flags(lock, flags) arch_read_lock(lock)
#define arch_write_lock_flags(lock, flags) arch_write_lock(lock)

#define arch_spin_relax(lock)	cpu_relax()
#define arch_read_relax(lock)	cpu_relax()
#define arch_write_relax(lock)	cpu_relax()

#endif /* __ASM_SPINLOCK_H */
