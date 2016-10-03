# raw_spin_trylock 분석

## Code
```
// kernel/include/linux/spinlock.h
// 2016-10-01
#define raw_spin_lock(lock) _raw_spin_lock(lock)

```

## 분석
기본 코드는 아래와 같다.

### _raw_spin_lock()

```
// kernel/spinlock.c
#ifndef CONFIG_INLINE_SPIN_LOCK
// 2016-10-01
void __lockfunc _raw_spin_lock(raw_spinlock_t *lock)
{
        __raw_spin_lock(lock);
}
EXPORT_SYMBOL(_raw_spin_lock);
#endif

// kernel/include/linux/spinlock_api_smp.h
// 2016-10-01
static inline void __raw_spin_lock(raw_spinlock_t *lock)
{
    preempt_disable();
    spin_acquire(&lock->dep_map, 0, 0, _RET_IP_);
    /*  
     *#define LOCK_CONTENDED(_lock, try, lock) \
         lock(_lock)
     * */

    // == do_raw_spin_lock(do_raw_spin_trylock)
    LOCK_CONTENDED(lock, do_raw_spin_trylock, do_raw_spin_lock);
}

//lib/spinlock_debug.c 
// 2015-11-07
// 2016-10-01
// 동작 : 1) 스핀 락 시도 
//       - 실패 시 : (__spin_lock_debug 내부 구현)
//              1) 실패 시 약 40만번 재시도 
//              2) 1) 실패 시 무기한 스핀락 걸음
void do_raw_spin_lock(raw_spinlock_t *lock)
{
        debug_spin_lock_before(lock);
        if (unlikely(!arch_spin_trylock(&lock->raw_lock)))
                __spin_lock_debug(lock);
        // 2015-11-07 여기까지
        debug_spin_lock_after(lock);
}

```

### __spin_lock_debug()
do_raw_spin_lock의 알고리즘을 통해서, trylock을 1회 시도하고,  실패 시, __spin_lock_debug을 통해서 디버그 정보를 디스플레이 한다.
자 이제 좀 더 자세한 내용을 보자.

약 40만번 정도 trylock을 시도하고, 실패 시, delay(1)을 반복하는 구조다.
그래도 실패하면, arch_spin_lock()을 시도한다.

```
static void __spin_lock_debug(raw_spinlock_t *lock)
{       
        u64 i;
        // loops_per_jiffy 초기값 : 4096(1 << 12), HZ : 100
        // 최소 40만번 루프 시도
        u64 loops = loops_per_jiffy * HZ;

        for (i = 0; i < loops; i++) {
                if (arch_spin_trylock(&lock->raw_lock))
                        return;
                __delay(1);
        }
        /* lockup suspected: */
        spin_dump(lock, "lockup suspected");
#ifdef CONFIG_SMP
        trigger_all_cpu_backtrace();
#endif

        /*
         * The trylock above was causing a livelock.  Give the lower level arch
         * specific lock code a chance to acquire the lock. We have already
         * printed a warning/backtrace at this point. The non-debug arch
         * specific code might actually succeed in acquiring the lock.  If it is
         * not successful, the end-result is the same - there is no forward
         * progress.
         */
        // 2015-11-07
        arch_spin_lock(&lock->raw_lock);
        // 2015-11-07 여기까지
}
```

lock->slock에 저장되어 있는 값을 읽어와서 16번째 비트 설정 유무를 확인
설정되지 않았을 경우, 16번째 비트를 세팅하고, 그것을 lock->slock에 저장한다.
저장 실패 시, while 루프가 다시 실행된다.
NOTE: strex의 return은 성공 시 0, 실패 시 1
```
// arch/arm/include/asm/spinlock.h
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
    */

    // lock->slock에 저장되어 있는 값을 읽어와서 16번째 비트 설정 유무를 확인
    // 설정되지 않았을 경우, 16번째 비트를 세팅하고, 그것을 lock->slock에 저장한다.
    // 저장 실패 시, while 루프가 다시 실행된다.
    // NOTE: strex의 return은 성공 시 0, 실패 시 1

    // (TICKET_SHIFT) 번째 비트 설정 여부로 락이 걸렸는지 여부를 확인하고 있다.
    do {
        __asm__ __volatile__(
        "   ldrex   %0, [%3]\n"
        "   mov %2, #0\n"
        "   subs    %1, %0, %0, ror #16\n"
        "   addeq   %0, %0, %4\n"
        "   strexeq %2, %0, [%3]"
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
```

### arch_spin_lock()

이것은 lock을 확보할 때까지 무한정 루프를 실행 한다.

static inline void arch_spin_lock(arch_spinlock_t *lock)
{
    unsigned long tmp;
    u32 newval;
    arch_spinlock_t lockval;

    __asm__ __volatile__(
"1: ldrex   %0, [%3]\n"
"   add %1, %0, %4\n"
"   strex   %2, %1, [%3]\n"
"   teq %2, #0\n"
"   bne 1b"
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


###  __delay()
여기서 Delay 함수까지 따라가 보자.

```
// arch/arm/include/asm/delay.h 
extern struct arm_delay_ops {
    void (*delay)(unsigned long);
    void (*const_udelay)(unsigned long);
    void (*udelay)(unsigned long);
    unsigned long ticks_per_jiffy;
} arm_delay_ops;

#define __delay(n)      arm_delay_ops.delay(n)

//arch/arm/lib/delay.c
struct arm_delay_ops arm_delay_ops = {
        .delay          = __loop_delay,
        .const_udelay   = __loop_const_udelay,
        .udelay         = __loop_udelay,
};

// arch/arm/lib/delay-loop.S
// __delay(1)인 경우, r0에 1이 입력되어 있다.
// 그래서 subs 1회 수행 후, 복귀하게 될 것이다.
@ Delay routine
ENTRY(__loop_delay)
                subs    r0, r0, #1
                bhi     __loop_delay
                mov     pc, lr
ENDPROC(__loop_delay)
```




## 결론

* 디버깅용 spin lock은 최초 1회 Lock을 시도하고, 실패 시, 디버깅 정보를 보여주면서 시도
   - 약 40만번 시도 후, 실패 시, trylock대신 arch_spin_lock()을 시도한다.
* SPINLOCK은 해당 변수의 16번째 비트 셋팅 유무로 판단한다.
* delay()함수는 arm sub instruction을 사용해수 구현했다.

lock코드를 분석해 보았으니, 나만의 spin lock 정도는 구현할 수 있지 않을까?




