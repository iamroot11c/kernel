## IAMROOT.ORG Kernel 분석 스터디 `11차 C조` (ARM) ##
## Study History(일부 기록이 맞지 않을 수 있음) ##
+ [120주차]() `2016.12.10`
 - drive: 홍진우님
 - 사설 스터디 룸 이용으로 4시간만 진행
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 완료
 - sched/core.c:put_prev_task() 진행 완료
 - kernel/sched/fair.c:put_prev_task_fair() 진행 완료
 - kernel/sched/fair.c:put_prev_entity() 진행 완료
 - sched/core.c:pick_next_task() 진행 완료
 - kernel/sched/fair.c:pick_next_task_fair() 진행 완료
 - sched/core.c:context_switch() 진행 중
 - arch/arm/include/asm/mmu_context.h:switch_mm() 진행 중
 - arch/arm/mm/context.c:check_and_switch_context() 진행 중

```
        /*
         * 31                         7          0
         * +-------------------------+-----------+
         * |      process ID         |   ASID    |
         * +-------------------------+-----------+
         * |              context ID             |
         * +-------------------------------------+
         */

Fluch
   - dirty bit -> 0
   - Memory X
Clean
   - search dirty bit 1
   - Memory O
Drain
   - If Write Buffer
   - Memory O
---------------------------
Invalidate
   1. Flush, same meaning with flush
   2. Valid bit -> 0
      => Cache line 전체를 무효화
      cf. 1번과는 완전히 다르다. 1번은 dirty bit만을 다룬다.
          2번은 전체 비트(valid bit)을 다룬다.
   Q. memory 반영이 일어 나는가?
```

+ [119주차](http://bit.ly/2fLbpnq) `2016.12.03`
 - drive: 홍진우님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 완료
 - sched/core.c:put_prev_task() 진행 완료
 - kernel/sched/fair.c:put_prev_task_fair() 진행 완료
 - kernel/sched/fair.c:put_prev_entity() 진행 완료
 - sched/core.c:pick_next_task() 진행 완료
 - kernel/sched/fair.c:pick_next_task_fair() 진행 완료
 - sched/core.c:context_switch() 진행 중
 - sched/core.c:prepare_task_switch() 진행 완료

+ [118주차](http://bit.ly/2ehUev0) `2016.11.26`
 - drive: 양만철님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/stop_machine.c:stop_one_cpu_nowait() 진행 완료
 - kernel/stop_machine.c:cpu_stop_queue_work() 진행 완료
 - kernel/sched/core.c:wake_up_process() 진행 완료
 - kernel/sched/core.c:try_to_wake_up() 진행 완료
 - kernel/sched/core.c:select_task_rq() 진행 완료
 - kernel/sched/fair.c:select_task_rq_fair() 진행 완료
 - kernel/sched/core.c:select_fallback_rq() 훑어봄
 - kernel/sched/fair.c:wake_affine() 진행 완료
 - kernel/sched/fair.c:select_idle_sibling() 진행 완료
 - kernel/sched/fair.c:find_idlest_group() 진행 완료
 - kernel/sched/fair.c:find_idlest_cpu() 진행 완료

+ [117주차](http://bit.ly/2fFqo1B) `2016.11.19`
 - drive: 보름달님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:move_tasks() 진행 완료
 - kernel/sched/fair.c:move_task() 진행 완료
 - kernel/sched/core.c:deactivate_task() 진행 완료
 - kernel/sched/core.c:set_task_cpu() 진행 완료
 - kernel/sched/fair.c:migrate_task_rq_fair() 진행 완료
 - kernel/sched/sched.h:__set_task_cpu() 진행 완료
 - kernel/sched/core.c:activate_task() 진행 완료
 - kernel/sched/fair.c:update_curr() 진행 완료
 - kernel/sched/fair.c:enqueue_task_fair() 진행 완료
 - kernel/sched/fair.c:enqueue_entity_load_avg() 진행 완료
 - kernel/sched/fair.c:update_entity_load_avg() 진행 완료 
 - kernel/sched/core.c:check_preempt_curr() 진행 완료
 - kernel/sched/fair.c:check_preempt_wakeup() 진행 완료
 - kernel/sched/fair.c:wakeup_preempt_entity() 진행 완료
 - kernel/stop_machine.c:stop_one_cpu_nowait() 진행 중
 - kernel/stop_machine.c:cpu_stop_queue_work() 진행 중
 - kernel/sched/core.c:wake_up_process() 진행 중
 - kernel/sched/core.c:try_to_wake_up() 진행 중
 - p->sched_class->task_waking(p) 완료
 - kernel/sched/fair.c:task_waking_fair() 완료
 - kernel/sched/core.c:select_task_rq() 진행 중

+ [116주차](http://bit.ly/2fLbpnq) `2016.11.12`
 - drive: 홍진우님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:move_tasks() 진행 중
 - kernel/sched/fair.c:move_task() 진행 중
 - kernel/sched/core.c:deactivate_task() 진행 완료
 - kernel/sched/core.c:set_task_cpu() 진행 완료
 - kernel/sched/fair.c:migrate_task_rq_fair() 진행 완료
 - kernel/sched/sched.h:__set_task_cpu() 진행 완료
 - kernel/sched/core.c:activate_task() 진행 완료
 - kernel/sched/fair.c:update_curr() 진행 완료
 - kernel/sched/fair.c:enqueue_task_fair() 진행 완료
 - kernel/sched/fair.c:enqueue_entity_load_avg() 진행 완료
 - kernel/sched/fair.c:update_entity_load_avg() 진행 완료 
- kernel/sched/core.c:check_preempt_curr() 진행 중
 - kernel/sched/fair.c:check_preempt_wakeup() 진행 중
 - kernel/sched/fair.c:wakeup_preempt_entity() 진행 예정
 
+ [115주차](goo.gl/lQAAEO) `2016.11.05`
 - drive: 홍진우님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:move_tasks() 진행 중
 - kernel/sched/fair.c:move_task() 진행 중
 - kernel/sched/core.c:deactivate_task() 진행 완료
 - kernel/sched/core.c:set_task_cpu() 진행 완료
 - kernel/sched/fair.c:migrate_task_rq_fair() 진행 완료
 - kernel/sched/sched.h:__set_task_cpu() 진행 완료
 - kernel/sched/core.c:activate_task() 진행 중
 - kernel/kernel/sched/core.c:enqueue_task() 진행 중
 - kernel/kernel/sched/core.c:update_rq_clock() 진행 완료
 - kernel/sched/fair.c:enqueue_task_fair() 진행 중
 - kernel/sched/fair.c:enqueue_entity() 진행 중
 - kernel/sched/fair.c:update_curr() 진행 완료
 - kernel/sched/fair.c:enqueue_entity_load_avg() 진행 중
 - kernel/sched/fair.c:update_entity_load_avg() 진행 완료

+ [114주차](http://bit.ly/2ehUev0) `2016.10.22`
 - drive: 양만철님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:move_tasks() 진행 중
 - kernel/sched/fair.c:move_task() 진행 중
 - kernel/sched/core.c:deactivate_task() 진행 중
 - kernel/sched/core.c:dequeue_task() 진행 중
 - kernel/sched/dequeue_task_fair() 진행 중 *fair_sched_class 기준으로 분석
 - kernel/sched/dequeue_entity() 진행 중
 - kernel/sched/update_curr() 분석 완
 - kernel/sched/dequeue_entity_load_avg() 진행 중
 - kernel/sched/update_entity_load_avg() 진행 중
 - kernel/sched/__update_entity_runnable_avg() 분석완료
 - kernel/sched/decay_load() 분석완료
 - kernel/sched/__compute_runnable_contrib() 분석완료
 - kernel/sched/update_cfs_rq_blocked_load() 진행 중

+ [113주차](http://bit.ly/2e9v904) `2016.10.15`
 - drive: 보름달님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - sched/core.c:try_to_wake_up_local() 진행 완료
 - sched/core.c:ttwu_do_wakeup() 진행 완료
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:find_busiest_group() 완료
 - kernel/sched/fair.c:update_sg_lb_stats() 완료
 - kernel/sched/sched.h:double_rq_lock() 완료
 - kernel/sched/fair.c:move_tasks() 진행 중
 - kernel/sched/fair.c:can_migrate_task() 완료
 - kernel/sched/fair.c:move_task() 진행중

+ [112주차](http://bit.ly/2dAA7rq) `2016.10.08`
 - drive: 양만철님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - sched/core.c:try_to_wake_up_local() 진행 완료
 - sched/core.c:ttwu_do_wakeup() 진행 완료
 - kernel/sched/fair.c:idle_balance() 진행 중
 - kernel/sched/fair.c:load_balance() 진행 중
 - kernel/sched/fair.c:find_busiest_group() 진행 중
 - kernel/sched/fair.c:update_sd_lb_stats() 진행 중
 - kernel/sched/fair.c:update_sg_lb_stats() 분석 중

+ [111주차](http://bit.ly/2bBHh99) `2016.10.01`
 - drive : 홍진우님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_start() 완료
 - fs/sysfs/dir.c:sysfs_find_dirent() 완료
 - fs/sysfs/dir.c:sysfs_remove_one() 완료
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행 중
 - sched/core.c:deactivate_task() 진행 완료
 - sched/core.c:dequeue_task() 진행 완료
 - sched/core.c:wq_worker_sleeping() 진행 완료
 - sched/core.c:deactivate() 진행 완료
 - sched/core.c:try_to_wake_up_local() 진행 중
 - sched/core.c:ttwu_activate() 진행 완료
 - sched/core.c:activte_task() 진행 완료
 - kernel/workqueue.c:wq_worker_walking_up() 진행 완료
 - sched/core.c:ttwu_do_wakeup() 진행 중


+ [110주차](http://bit.ly/2cYYJaN) `2016.09.24`
 - drive : 보름달님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행 중
 - drivers/base/core.c:device_del() 진행 중
 - drivers/base/power/sysfs.c:dpm_sysfs_remove() 진행 중
 - drivers/base/power/qos.c:dev_pm_qos_constraints_destroy() 진행 중
 - drivers/base/power/sysfs.c:pm_qos_sysfs_remove_latency() 진행 중
 - fs/sysfs/group.c:sysfs_unmerge_group() 진행 중
 - fs/sysfs/inode.c:sysfs_hash_and_remove() 진행 중
 - fs/sysfs/dir.c:sysfs_addrm_start() 완료
 - fs/sysfs/dir.c:sysfs_find_dirent() 완료
 - fs/sysfs/dir.c:sysfs_remove_one() 완료
 - fs/sysfs/dir.c:sysfs_addrm_finish() 진행 중
 - fs/sysfs/dir.c:sysfs_deactivate() 진행 중
 - sched/core.c:wait_for_completion() 진행중,  action(timeout);
 - kernel/timer.c:schedule_timeout() 진행중
 - sched/core.c:__schedule() 진행중

+ [109주차](http://bit.ly/2c5q3TI) `2016.09.10`
 - drive : 양만철님
 - [perf_pmu_register() 진행중]()
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/events/core.c:perf_pmu_register() 진행
 - lib/idr.c:idr_alloc() 진행 완료
 - lib/idr.c:idr_get_empty_slot() 진행 완료
 - lib/idr.c:sub_alloc() 진행 완료
 - kernel/events/core.c:__perf_event_init_context() 진행 완료
 - kernel/events/core.c:__perf_cpu_hrtimer_init() 진행 완료

+ [108주차](http://bit.ly/2bBHh99) `2016.08.20`
 - [sched_clock_postinit() 진행중]()
 - kernel/time/sched_clock.c:sched_clock_postinit() 진행중
 - kernel/time/sched_clock.c:setup_sched_clock() 진행 완료
 - kernel/time/sched_clock.c:sched_clock_poll() 진행 완료
 - kernel/events/core.c:pref_event_init() 진행 중
 - kernel/lib/idr.c:idr_init() 진행 완료
 - kernel/events/core.c:perf_event_init_all_cpus() 진행 완료
 - kernel/kernel/srcu.c:init_srcu_struct() 진행 완료
 - kernel/events/core.c:perf_pmu_register() 진행 

+ [107주차](http://bit.ly/2aSelOn) `2016.08.13`
 - [sched_clock_postinit() 진행중]()
 - kernel/timer.c:init_timers() 진행 완료
 - kernel/timer.c:timer_cpu_notify() 진행 완료
 - kernel/hrtimer.c:hrtimers_init() 진행 완료
 - kernel/hrtimer.c:hrtimer_cpu_notify() 진행 완료
 - kernel/hrtimer.c:init_hrtimers_cpu() 진행 완료
 - kernel/softirq.c:softirq_init() 진행 완료
 - time/timekeeping.c:timekeeping_init() 진행 완료
 - arch/arm/kernel/time.c:time_init() 진행 완료
 - arch/arm/mach-exynos/common.c:exynos_init_time() 진행 완료
 - kernel/time/sched_clock.c:sched_clock_postinit() 진행중
 - kernel/time/sched_clock.c:setup_sched_clock() 진행 완료
 - kernel/time/sched_clock.c:sched_clock_poll() 진행 중

+ [106주차](http://bit.ly/2b18bLS) `2016.08.06`
 - [init_timers() 진행중]()
 - kernel/timer.c:init_timers() 진행 중
 - kernel/timer.c:timer_cpu_notify() 진행 중
 - kernel/timer.c:init_timers_cpu() 분석 완료
 - kernel/irq/irqdesc.c:early_irq_init() 완료
 - arch/arm/kernel/irq.c:init_IRQ() 완료
 - kernel/time/tick-common.c:tick_init() 완료

 - 차주 early_irq_init() 진행 예정

+ [105주차](http://bit.ly/2a3LzHl) `2016.07.23`
 - [early_irq_init() 진행중]()
 - kernel/rcutree.c: rcu_init() 완료
 - kernel/rcutree_plugin.h: __rcu_init_preempt() 완료
 - include/linux/cpu.h:cpu_notifier() 완료
 - kernel/notifier.c: raw_notifier_chain_register() 완료
 - kernel/notifier.c: notifier_chain_register() 완료
 - include/linux/suspend.h: pm_notifier() 완료
 - kernel/notifier.c: blocking_notifier_chain_register() 완료
 - kernel/rcutree.c: rcu_cpu_notify() 완료
 - kernel/rcutree.c: rcu_prepare_cpu() 완료
 - lib/radix-tree.c: radix_tree_init() 완료
 - 차주 early_irq_init() 진행 예정

+ [104주차](http://bit.ly/29Kux3X) `2016.07.16`
 - [rcu_init() 진행중]()
 - kernel/lib/idr.c: idr_init_cache() 분석 완료
 - kernel/mm/slab_common.c: kmem_cache_create() 분석 완료
 - kernel/mm/slab_common.c: kmem_cache_create_memcg() 분석 완료
 - kernel/rcutree.c: rcu_init() 진행중
 - kernel/rcutree_plugin.h: rcu_bootup_announce() 분석 완료
 - kernel/rcutree.c: rcu_init_geometry() 분석 완료
 - kernel/rcutree.c: rcu_init_one() 분석 완료
 - kernel/rcutree.c: rcu_boot_init_percpu_data() 분석 완료


+ [103주차](http://bit.ly/29FsV8c) `2016.07.09`
 - [idr_init_cache() 진행중]()
 - kernel/sched/core.c: sched_init() 분석 완료
 - sched_init() 모기향 리뷰
 - preempt_disable() 완료
 - kernel/lib/idr.c: idr_init_cache() 진행중
 - kernel/mm/slab_common.c: kmem_cache_create() 진행중
 - kernel/mm/slab_common.c: kmem_cache_create_memcg() 진행중
 - kernel/cpu.c: get_online_cpus(), put_online_cpus() 분석 완료
 - kernel/mm/slub.c: __kmem_cache_alias() 분석 완료
 - kernel/mm/slub.c: find_mergeable() 분석 완료
 - kernel/mm/slub.c: sysfs_slab_alias() 분석 완료
 - radix tree: http://timewizhan.tistory.com/entry/%EB%9D%BC%EB%94%95%EC%8A%A4-%ED%8A%B8%EB%A6%ACRadix-Tree
 - 진우님 회사 사정으로 스터디를 일찍 마쳤습니다.

+ [102주차]() `2016.06.25`
 - [sched_init() 진행중](http://bit.ly/29aaOFd)
 - kernel/sched/core.c: sched_init() 분석 완료
 - kernel/sched/core.c: init_defrootdomain() 분석 완료
 - kernel/sched/core.c: init_rootdomain() 분석 완료
 - kernel/sched/core.c: init_rt_bandwidth() 분석 완료
 - cfs / runtime 스케줄러 동작 정리
 - http://egloos.zum.com/tory45/v/5167290
 

+ [101주차]() `2016.06.25`
 - [sched_init() 진행중](http://bit.ly/28UP1Te)
 - kernel/sched/core.c: sched_init() 진행 중
 - kernel/sched/core.c: init_defrootdomain() 진행 중
 - kernel/sched/core.c: init_rootdomain() 분석 중
 - init/main.c:mm_init() 분석완료
 - mm/vmalloc.c:vmalloc_init() 분석완료
 - mm/vmalloc.c:__insert_vmap_area() 분석완료
 - lib/rbtree.c:__rb_insert() 분석 완료

 
+ [100주차]() `2016.06.18`
 - [vmalloc_init() 진행중]()
 - mm/vmalloc.c:vmalloc_init() 진행 중
 - mm/vmalloc.c:__insert_vmap_area() 진행 중
 - lib/rbtree.c:__rb_insert() 분석 중
 - 트리의 개념과 용어 정리
 - Red Black Tree 동영상 강의 시청(http://ddmix.blogspot.kr/2015/02/cppalgo-19-red-black-tree.html)
 - 저번주 내용 review
 - Animation: https://www.cs.usfca.edu/~galles/visualization/RedBlack.html

+ [99주차]() `2016.06.11`
 - [vmalloc_init() 진행중]()
 - mm/vmalloc.c:vmalloc_init() 진행 중
 - mm/vmalloc.c:__insert_vmap_area() 진행 중
 - lib/rbtree.c:__rb_insert() 분석 중
 - 트리의 개념과 용어 정리
 - Red Black Tree 동영상 강의 시청(http://ddmix.blogspot.kr/2015/02/cppalgo-19-red-black-tree.html)

+ [98주차](http://bit.ly/1TLGBOP) `2016.05.28`
 - [vmalloc_init() 진행중]()
 - mm/vmalloc.c:vmalloc_init() 진행 중
 - mm/vmalloc.c:__insert_vmap_area() 진행 중
 - lib/rbtree.c:__rb_insert() 분석 중
 - mm/percpu.c:percpu_init_late() 완료
 - mm/slub.c:kmem_cache_init() 완료
 - mm/slub.c:create_kmalloc_caches() 완료
 
+ [97주차](http://bit.ly/1Ot4lmv) `2016.05.14`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:bootstrap() 완료
 - mm/slub.c:__flush_cpu_slab() 완료
 - mm/slub.c:flush_slab() 완료
 - mm/slub.c:deactivate_slab() 완료
 - mm/slub.c:create_kmalloc_caches() 진행 중

+ [96주차](http://bit.ly/1T5cDlF) `2016.04.23`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:bootstrap() 시작
 - mm/slub.c:__flush_cpu_slab() 진행중
 - mm/slub.c:flush_slab() 진행중
 - mm/slub.c:eactivate_slab() 진행중
 - mm/slab_common.c:create_boot_cache() 완료
 - mm/slub.c:__kmem_cache_create() 완료
 - mm/slub.c:kmem_cache_open() 완료
 - mm/slub.c:free_kmem_cache_nodes() 완료
 - mm/slub.c:alloc_kmem_cache_cpus() - 완료
 - mm/percpu.c:__alloc_percpu() - 완료
 - mm/percup.c:pcpu_alloc() - 완료
 - mm/percpu-vm.c:pcpu_populate_chunk() - 완료
 - mm/percpu-vm.c:pcpu_post_unmap_tlb_flush() - 완료
 - mm/percpu-vm.c:flush_tlb_kernel_range() - 완료
 - arch/arm/kernel/smp_tlb.c: flush_tlb_kernel_range() 완료
 - arch/arm/mm/tlb-v7.S:v7wbi_flush_kern_tlb_range() 완료

+ [95주차](http://bit.ly/1qO2NxU) `2016.04.16`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/percpu-vm.c:pcpu_populate_chunk() - 진행중
 - mm/percpu-vm.c:pcpu_post_unmap_tlb_flush() - 진행 중
 - mm/percpu-vm.c:flush_tlb_kernel_range() - 진행 중
 - arch/arm/kernel/smp_tlb.c: flush_tlb_kernel_range() 진행 중
 - arch/arm/mm/tlb-v7.S:v7wbi_flush_kern_tlb_range() 다음주 분석 예정
 - mm/percpu-vm.c:pcpu_map_pages() 완료
 - mm/vmalloc.c:vmap_page_range_noflush() 완료
 - mm/vmalloc.c:vmap_page_range_noflush() 완료
 - mm/vmalloc.c:vmap_pmd_range() 완료
 - mm/vmalloc.c:vmap_pmd_range() 완료
 
+ [94주차](http://bit.ly/1UPSKoA) `2016.04.09`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/percpu-vm.c:pcpu_populate_chunk() - 진행중
 - mm/percpu-vm.c:pcpu_map_pages() 진행 중
 - mm/vmalloc.c:vmap_page_range_noflush() 진행 중
 - mm/vmalloc.c:vmap_page_range_noflush() 진행 중
 - mm/vmalloc.c:vmap_pmd_range() 진행 중
 - mm/vmalloc.c:vmap_pmd_range() 진행 중

+ [93주차](bit.ly/1qd50TH) `2016.04.02`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/percpu-vm.c:pcpu_populate_chunk() - 진행중
 - mm/percpu-vm.c:pcpu_next_pop() 완료
 - mm/percpu-vm.c:pcpu_get_pages_and_bitmap() 진행중
 - mm/percpu-vm.c:pcpu_mem_zalloc() 진행중
 - mm/slub.c:pcpu_extend_area_map() - 완료
 - mm/percpu.c:pcpu_mem_zalloc() - 완료
 - mm/percpu.c:pcpu_mem_free() - 완료
 - mm/slub.c:kfree() - 완료
 - mm/slub.c:slab_free() - 완료
 - mm/slub.c:__slab_free() - 완료
 - mm/slub.c:put_cpu_partial - 완료
 - mm/slub.c:unfreeze_partials - 완료

+ [92주차](bit.ly/1SmB1kR) `2016.03.26`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/slub.c:pcpu_extend_area_map() - 진행 중
 - mm/percpu.c:pcpu_mem_zalloc() - 분석 완료
 - mm/percpu.c:pcpu_mem_free() - 진행 중
 - mm/slub.c:kfree() - 진행 중
 - mm/slub.c:slab_free() - 진행 중
 - mm/slub.c:__slab_free() - 분석 중
 - mm/slub.c:free_debug_processing() 분석완료

+ [91주차](bit.ly/1VmUeWy) `2016.03.19`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/slub.c:pcpu_extend_area_map() - 진행 중
 - mm/percpu.c:pcpu_mem_zalloc() - 분석 완료
 - mm/percpu.c:pcpu_mem_free() - 진행 중
 - mm/slub.c:kfree() - 진행 중

+ [91주차](bit.ly/1SIgclZ) `2016.03.12`
 - [kmem_cache_init() 진행중]()
 - mm/slub.c:alloc_kmem_cache_cpus() - 진행 중
 - mm/percpu.c:__alloc_percpu() - 진행 중
 - mm/percup.c:pcpu_alloc() - 진행 중
 - mm/slub.c:init_kmem_cache_nodes() - 분석 완료
 - mm/slub.c:early_kmem_cache_node_alloc() - 분석 완료
 - mm/slub.c:new_slab() - 분석 완료
 - mm/slub.c:allocate_slab() - 분석 완료 
 - mm/slub.c:alloc_slab_page() - 분석 완료
 - include/linux/gfp.h:alloc_pages_exact_node() - 분석 완료
 - include/linux/gfp.h:__alloc_pages() - 분석 완료
 - mm/page_alloc.c:__alloc_pages_nodemask - 분석 완료
  
+ [90주차](bit.ly/1oXHaKr) `2016.03.05`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_nodemask() 진행 중
 - page_alloc.c:__alloc_pages_slowpath() 분석 완료
 - mm/page_alloc.c:__alloc_pages_may_oom 분석 완료
 - mm/oom_kill.c:out_of_memory() 분석 완료
 - mm/oom_kill.c:oom_kill_process() 분석 완료
 - mm/oom_kill.c:do_send_sig_info() 분석 완료
 
+ [89주차]() `2016.02.27`
   - 리눅스 커널 디바이스트리 이해
   - 디바이스드라이버 소스 분석 방법
   - 디바이스드라이버 base 소스 및 설계사상 이해 

+ [88주차]() `2016.02.20`
   - 커널 연구회 교육
   - 리눅스 커널 소스 디버깅 방법, kgdb 특강 

+ [87주차](bit.ly/1PT5SG9) `2016.02.13`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - mm/page_alloc.c:__alloc_pages_may_oom 진행중
 - mm/oom_kill.c:out_of_memory() 진행중
 - mm/oom_kill.c:oom_kill_process() 분석 중
 - mm/oom_kill.c:oom_badness() 분석 완료
 - `Reader-Writer lock` 분석

+ [86주차](bit.ly/1PZhaEA) `2016.02.06`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - mm/page_alloc.c:__alloc_pages_may_oom 진행중
 - mm/oom_kill.c:out_of_memory() 진행중
 - mm/oom_kill.c:oom_kill_process() 분석 중

+ [85주차](http://bit.ly/20dlgny) `2016.01.30`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 완료
 - page_alloc.c:drain_all_pages() 완료
 - kernel/smp.c:on_each_cpu_mask() 완료
 - kernel/smp.c:smp_call_function_many() drain_local_pages() 함수 제외한 분석 완료
 - mm/page_alloc.c:drain_local_pages() 진행 완료
 - mm/page_alloc.c:__alloc_pages_may_oom 진행중
 - mm/oom_kill.c:try_set_zonelist_oom() 완료
 - mm/oom_kill.c:out_of_memory() 진행중
 - kernel/notifier.c:blocking_notifier_call_chain() 완료
 - mm/oom_kill.c:oom_kill_process() 차주 진행 예정

+ [84주차](bit.ly/1Nrm8s4) `2016.01.23`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:drain_all_pages() 분석중
 - kernel/smp.c:on_each_cpu_mask() 분석중
 - kernel/smp.c:smp_call_function_many() drain_local_pages() 함수 제외한 분석 완료
 - mm/page_alloc.c:drain_local_pages() 다음주 진행
 - page_alloc.c:get_page_from_freelist() 분석완료
 
+ [83주차](bit.ly/2064Hqb) `2016.01.16`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:get_page_from_freelist() 다음주 진행
 - page_alloc.c:__perform_reclaim() 분석완료
 - vmscan.c:try_to_free_pages() 분석완료
 - vmscan.c:do_try_to_free_pages() 분석완료
 - vmscan.c:shrink_zones() 분석완료
 - vmscan.c:shrink_zone() 분석완료
 - vmscan.c:should_continue_reclaim() 분석완료
 
+ [82주차](http://bit.ly/1mKCGq8) `2016.01.09`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 완료
 - vmscan.c:shrink_list() 완료
 - vmscan.c:shrink_inactive_list() 완료
 - vmscan.c:shrink_page_list() 완료
 - swap_state.c:add_to_swap() 진행완료
 - swap_state.c:pageout() 완료
 - vmscan.c:__remove_mapping() 완료
 - vmscan.c:should_continue_reclaim() 진행중

+ [81주차](http://bit.ly/1SfXIZa) `2015.12.26`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - vmscan.c:shrink_list() 진행 중
 - vmscan.c:shrink_inactive_list() 진행 중
 - vmscan.c:shrink_page_list() 진행 중
 - swap_state.c:add_to_swap() 진행완료
 - swap_state.c:pageout() 완료
 - __remove_mapping() 진행중

+ [80주차](http://bit.ly/1NDb3Fy) `2015.12.19`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - vmscan.c:shrink_list() 진행 중
 - vmscan.c:shrink_inactive_list() 진행 중
 - vmscan.c:shrink_page_list() 진행 중
 - swap_state.c:add_to_swap() 진행완료
 - swap_state.c:pageout() 진행 중

+ [80주차](http://bit.ly/1NhxthQ) `2015.12.12`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - vmscan.c:get_scan_count() 진행 완료
 - vmscan.c:blk_start_plug() 진행 완료
 - vmscan.c:shrink_list() 진행 중
 - vmscan.c:shrink_active_list() 진행완료
 - vmscan.c:shrink_inactive_list() 진행 중
 - vmscan.c:shrink_page_list() 진행 중
 - swap_state.c:add_to_swap() 진행중
 - swapfile.c:get_swap_page() 진행완료
 - swapfile.c:scan_swap_map() 진행완료

+ [79주차](http://bit.ly/1OMs1ny) `2015.12.05`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - vmscan.c:get_scan_count() 진행 완료
 - vmscan.c:blk_start_plug() 진행 완료
 - vmscan.c:shrink_list() 진행 중
 - vmscan.c:shrink_active_list() 진행완료
 - vmscan.c:shrink_inactive_list() 진행 중
 - vmscan.c:shrink_page_list() 진행 중
 - vmscan.c:add_to_swap() 진행중

+ [78주차](http://bit.ly/1NAoR93) `2015.11.28`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - vmscan.c:get_scan_count() 진행 완료
 - vmscan.c:blk_start_plug() 진행 완료
 - vmscan.c:shrink_list() 진행 중
 - vmscan.c:shrink_active_list() 진행 중

+ [77주차](http://bit.ly/1MIhruL) `2015.11.21`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - vmscan.c:shrink_zones() 진행중
 - vmscan.c:shrink_zone() 진행중
 - vmscan.c:shrink_lruvec() 진행중
 - 멤버 개인사정으로 식사전까지 행진

+ [76주차](http://bit.ly/1RVgP8I) `2015.11.14`
 - [kmem_cache_init() 진행중]()
 - page_alloc.c:__alloc_pages_slowpath() 진행중
 - page_alloc.c:__alloc_pages_direct_reclaim() 진행중
 - page_alloc.c:__perform_reclaim() 진행중
 - vmscan.c:try_to_free_pages() 분석 중
 - page_alloc.c:__alloc_pages_direct_compact() 분석 완료
 - compaction.c:try_to_compact_pages() 분석 완료
 - compaction.c:compact_zone_order() 분석 완료
 - compaction.c:compact_zone() 분석 완료
 - mm/migrate.c:migrate_pages() 분석 완료
 - mm/migrate.c:unmap_and_move() 분석 완료


+ [75주차](https://goo.gl/0JCKNY) `2015.11.07`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:__unmap_and_move() 진행중
 - mm/rmap.c:try_to_unmap() 분석 완료
 - mm/migrate.c:move_to_new_page() 분석중
 - mm/rmap.c:rmap_walk_anon() 분석중
 - mm/migrate.c:remove_migration_pte() 분석중

+ [73주차](http://bit.ly/1XpOmLF) `2015.10.24`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:__unmap_and_move() 진행중
 - mm/rmap.c:try_to_unmap() 분석 완료
 - mm/migrate.c:move_to_new_page() 분석중
 - mm/rmap.c:rmap_walk_anon() 분석중
 - mm/migrate.c:remove_migration_pte() 분석중

+ [72주차]() `2015.10.17`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/rmap.c:try_to_unmap_one() 분석 완료
 - mm/rmap.c:try_to_unmap_anon() 분석 완료
 - mm/rmap.c:try_to_unmap_file() 분석 료완


+ + [71주차](http://bit.ly/1VKDp4A) `2015.10.10`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/rmap.c:try_to_unmap_one() 분석 완료
 - mm/rmap.c:try_to_unmap_anon() 분석 완료
 - mm/rmap.c:try_to_unmap_file() 분석중

+ [70주차](http://bit.ly/1Oe25Dk) `2015.10.03`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/swapfile.c:swap_duplicate() 분석 완료
 - mm/swapfile.c:add_swap_count_continuation() 분석 완료 
 - include/linux/gfp.h:alloc_page() 함수를 통해 page 할당 리뷰
 - mm/rmap.c:set_pte_at() 완료
 - pge, pte, memsection, page를 통해 물리주소 구하는 방식 분석

+ [69주차](https://goo.gl/YcS9pK) `2015.09.19`
 - [kmem_cache_init() 진행중]()
 - mm/rmap.c:try_to_unmap_one() 진행중
 - mm/swapfile.c:swap_duplicate() 진행중
 - mm/swapfile.c:add_swap_count_continuation() 분석중
 - include/linux/gfp.h:alloc_page() 함수를 통해 page 할당 리뷰
 
+ [68주차](http://bit.ly/1Q8FUM5) `2015.09.12`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - mod_timer 동작 분석 완료
 - include/linux/workqueue.h:queue_delayed_work() 진행완료

+ [67주차](http://bit.ly/1FpdBmU) `2015.09.05`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - include/linux/workqueue.h:queue_delayed_work() 진행중

+ [66주차](http://bit.ly/1KsZanU) `2015.08.29`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중
 - __flush_tlb_page() 완료

+ [65주차](http://bit.ly/1LBKIIM) `2015.08.22`
 - [kmem_cache_init() 진행중]()
 - mm/migrate.c:__unmap_and_move() 진행중

+ [64주차]() `2015.08.15`
 - [kmem_cache_init() 진행중](http://bit.ly/1IQSM7s)
 - mm/migrate.c:__unmap_and_move() 진행중
 - TODO

+ [63주차]() `2015.08.08`
 - [kmem_cache_init() 진행중](http://bit.ly/1EfFUDJ)
 - mm/migrate.c:__unmap_and_move() 진행중
 - mm/memcontrol.c:mem_cgroup_prepare_migration() 진행중
 - mm/slub.c:kmem_cache_free() 진행중
+ [62주차]() `2015.07.25`
 - [kmem_cache_init() 진행중](http://bit.ly/1enUphX)
 - mm/migrate.c:__unmap_and_move() 진행중
 - get_new_page() 함수 분석완료
 - lock_page() 함수 분석 완료
 - 금주 회식으로 7(19)시 종료 및 차주 휴가
+ [61차]() `2015.07.18`
 - [kmem_cache_init() 진행중](http://bit.ly/1fT7MIE)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
 - isolate_migratepageblock() 함수 분석 진행
+ [60주차]() `2015.07.11`
 - [kmem_cache_init() 진행중](http://bit.ly/1UNGhju)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
 - putback_movable_pages() 완료
+ [59주차]() `2015.07.04`
 - [kmem_cache_init() 진행중](http://bit.ly/1NXKzjv)
 - compaction.c:compact_zone() 내부 진행
 - compact_finished() 함수의 리턴값이 COMPACT_CONTINUE 일 때 while문 실행
 - isolate_migratepages() 함수 내부 분석중
+ [58주차]() `2015.06.27`
 - [kmem_cache_init() 진행중](http://bit.ly/1JcHISc)
 - __alloc_pages_slowpath() 진행중
+ [57주차]() `2015.06.20` 
 - [kmem_cache_init() 진행중](http://bit.ly/1GykIvK)
 - __alloc_pages_slowpath() 진행중
+ [56주차]() `2015.06.13`
 - [kmem_cache_init() 진행중](http://bit.ly/1TiYgxA)
 - __alloc_pages_slowpath() 진행중
+ [55주차]() `2015.06.06` 
 - [kmem_cache_init() 진행중](http://bit.ly/1GqTZob)
 - kmem_cache_init() 진행중
+ [54주차]() `2015.05.30` 
 - [kmem_cache_init() 진행중](http://bit.ly/1Qhjo2P)
 - kmem_cache_init() 진행중
+ [53주차]() `2015.05.23` 
 - [kmem_cache_init() 진행중](https://goo.gl/EyFOvb)
 - kmem_cache_init() 진행중
+ [52주차]() `2015.05.16` 
 - [kmem_cache_init() 진행중](https://goo.gl/lVgK8v)
 - kmem_cache_init() 진행중
+ [51주차]() `2015.05.09` 
 - [mem_init() 진행중](https://goo.gl/BIS8O2)
 - mem_init() 진행중
+ [50주차]() `2015.05.02` 
 - [init/main.c](https://goo.gl/CWZolG)
 - parse_args(..., &unknown_bootoption) 
 - sort_main_extable()
 - vfs_caches_init_early()
+ [49주차]() `2015.04.25` 
 - [init/main.c](http://goo.gl/BSNhP3)
 - page_alloc_init() 완료
+ [48주차]() `2015.04.18` 
 - [page_alloc_init() 진행중](http://goo.gl/F3H2T6)
 - page_alloc_cpu_notify() 분석중
 - page_alloc_init() 함수에서는page_alloc_cpu_notify() 함수를 등록하는데 등록된 함수는 CPU가 종료될 때 호출되어 부팅단계에서 분석하기에는 어려움이 있어 중단함
+ [47주차]() `2015.04.11` 
 - [page_alloc_init() 진행중](http://goo.gl/J6axwE)
 - page_alloc_cpu_notify() 분석중
+ [46주차]() `2015.04.06` 
 - [page_alloc_init() 진행중]( http://goo.gl/90fl06)
 -  page_alloc_init() 함수에서는 page_alloc_cpu_notify() 함수를 등록
+ [45주차]() `2015.03.28` 
 - mm/page_alloc.c build_all_zonelists() 분석중
 - __build_all_zonelists()
 - mminit_verify_zonelist()
+ [44주차]() `2015.03.21`
 - [Something]( http://goo.gl/0E3rBp)
+ [43주차]() `2015.03.14`
 - [mm/percpu.c pcpu_embed_first_chunk()](http://goo.gl/520gnX)
 - setup_per_cpu_areas() 분석 중
+ [42주차]() `2015.03.07`
 - [init/main.c kernel/smp.c setup_nr_cpu_ids()]( http://goo.gl/o4RuKP)
 - init_mm, init_task 분석
+ [41주차]() `2015.02.28`
 - [init/main.c, arch/arm/kernel/setup.c]( http://goo.gl/cc433Q)
 - setup_arch() 분석 완료
+ [40주차]() `2015.02.14`
 - [arch/arm/kernel/setup.c, arch/arm/kernel/devtree.c](http://goo.gl/psYle3)
 - arm_dt_init_cpu_maps() 분석 완료
+ [39주차]() `2015.02.07`
 - [mm/flush.c:drivers/of/fdt.c](http://goo.gl/2KBzee)
 -  paging_init() 분석 완료
 - __unflatten_device_tree() 진행중
+ [38주차]() `2015.01.31` 
 - [mm/flush.c]( http://goo.gl/NWj838)
 -  __flush_dcache_page()
 - __flush_dcache_page()
+ [37주차]() `2015.01.24` 
 - [page_alloc.c \\ arm/mm/page_alloc.c](http://goo.gl/vDJUm7)
 - bootmem_init() 분석 완료
 - __flush_dcache_page() 진행중
+ [36주차]() `2015.01.17`
 - [mm/page_alloc.c](http://goo.gl/sJ3pBR)
 - free_area_init_core()
 - memmap_init_zone()
+ [35주차]() `2015.01.10`
 - [mm/page_alloc.c](http://goo.gl/mqFj8S)
 - free_area_init_core()
+ [34주차]() `2014.01.03`
 - [arch/arm/mm/init.c](http://goo.gl/CjjZ7e)
 - arm_bootmem_free() 진행중
 - sparse_init() 완료
+ [33주차]() `2014.12.27` 
 - [arch/arm/mm/init.c](http://goo.gl/CjjZ7e)
 - bootmem_init() 진행중
 - arm_memory_present() 완료 
+ [32주차]() `2014.12.20` 
 - [arch/arm/mm/init.c](http://goo.gl/9GOgQt)
 - bootmem_init() 진행중]] 
 - arm_memory_present() 진행중 |
+ [31주차]() `2014.12.13` 
 - [arch/arm/mm/init.c](http://goo.gl/ei1qZ7)
 - bootmem_init() 진행중
 - arm_bootmem_init() 완료
+ [27주차]() `2014.11.15`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/5R3NNV)
 - arm_bootmem_init() 진행 중
+ [26주차]() `2014.11.08` 
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/RrJpgJ)
 - arm_bootmem_init() 진행 중
+ [25주차]() `2014.11.01` | 
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/UwIPp9)
  - bootmem_init() 진행중
+ [24주차]() `2014.10.27`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/UwIPp9)
 - devicemaps_init 진행중
+ [23주차]() `2014.10.18`
 - [kernel/arch/arm/mm/mmu.c](http://goo.gl/4VHMHV)
 - init alloc_init_pmd() 진행중
+ [22주차](http://goo.gl/vqkScg) `2014.10.11`
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)
 - `init alloc_init_pmd()` 진행중
+ [21주차](http://goo.gl/vqkScg) `2014.09.20` 
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)|
 - ??() 진행중|
+ [20주차](http://goo.gl/vqkScg) `2014.09.13` 
 - [kernel/arch/arm/mm/mmu.c]( http://goo.gl/FNzZBW)
 - build_mem_type_table() 진행완료
 - paging_init() 진행중
+ [19주차](http://goo.gl/vqkScg) `2014.08.30` 
 - [kernel/init/main.c](http://goo.gl/FNzZBW)
 - setup_arch()진행중
 - arm_memblock_init() 진행중
+ [18주차](http://goo.gl/vqkScg) `2014.08.16` 
 - [kernel/init/main.c](http://goo.gl/FNzZBW)
 - setup_arch()진행중
 - arm_memblock_init() 진행중
+ [17주차](http://goo.gl/6pJIdQ) `2014.08.09` 
 - [kernel/init/main.c]()
 - setup_arch()진행중
 - sanity_check_meminfo() 완료
+ [16주차](http://goo.gl/fJZ6Ru) `2014.08.02` 
 - [kernel/init/main.c](http://goo.gl/o9olRn)
 - setup_arch()진행중
+ [15주차](http://goo.gl/p4FgfN) `2014.07.26`
 - [kernel/init/main.c](http://goo.gl/BIyVOX)
 - setup_arch()진행중
+ [14주차](http://goo.gl/pXxnbi) `2014.07.19`
 - kernel/init/main.c]()
 - kernel/init/main.c start_kernel()진행중
+ [13주차](http://goo.gl/iqqBge) `2014.07.12`
 - [kernel/init/main.c](http://goo.gl/QOMzXQ)
 - kernel/init/main.c start_kernel()진행중
+ [12주차](http://goo.gl/PC7oKz) `2014.07.05`
 - [arch/arm/kernel/head.s]( http://goo.gl/QOMzXQ)
 - arch/arm/kenel/head.s 분석완료
+ [11주차](http://goo.gl/ucH5T5) `2014.06.29`
 - [arch/arm/kernel/head.s]( http://goo.gl/9kRxgC)
 - arch/arm/kenel/head.s 분석중
 - __v7_setup전까지
+ [10주차](http://goo.gl/yywmQP) `2014.06.21`
 - [arch/arm/kernel/head.s]( http://goo.gl/5QKsfz)
 - arch/arm/kenel/head.s 분석중
 - __v7_setup 진행중
+ [09주차](http://goo.gl/hKYnCi) `2014.06.14` 
 - [arch/arm/kernel/head.s](http://goo.gl/fKSvYh)
 - kenel/head.s 분석중
 - Create_page_table전까지
+ [08주차](http://goo.gl/WPNekV) `2014.06.07` 
 - [arch/arm/boot/compressed/head.s](http://goo.gl/Sg6T0G)
 - compressed/head.s 분석 완료
+ [07주차](http://goo.gl/JWoShf) `2014.05.31` 
 - [arch/arm/boot/compressed/head.s](http://goo.gl/jSJpQK)
 - compressed/head.s 분석중]]|
+ [06주차](http://goo.gl/cYMWF8) `2014.05.24` 
 - arch/arm/boot/compressed/head.s 진행중
+ [05주차](http://goo.gl/gBNIqi) `2014.05.17` 
 - ARM System Developer's Guide 완료 
+ [04주차](http://goo.gl/wSKdl7) `2014.05.09`
 - ARM System Developer's Guide p385~428, p454~499
+ [03주차](http://goo.gl/KYLf6C) `2014.04.26`
 - ARM System Developer's Guide ch1,2,3, 5.4, 5.5, ch9-p385까지 진행 

# The Linux Kernel review for ARMv7 3.13.0 (exynos 5420)
* Community name: IAMROOT.ORG ARM kernel study 11th C team
* Target Soc    : Samsung Exynos 5420 (ARMv7 A7&A15)

> $ make kernelversion
>> 3.12.20

#### [IAMROOT 10차 C조](https://github.com/arm10c/linux-stable) 참조 ####
