#include <linux/kernel.h>
#include <linxu/timekeeping.h>

/*
 *设置挑选任务的时间时，开始时只有prev,所以在prev里加一个next_p_n_t_start,
 *等出现next时，再赋值给next里的p_n_t_start 
 */
static inline struct task_struct *
pick_next_task(struct rq *rq, struct task_struct *prev, struct pin_cookie cookie)
{
	//
	//
	do_gettimeofday(prev->time_s->next_p_n_t_start);
	const struct sched_class *class = &fair_sched_class;
	struct task_struct *p;

	/*
	 * Optimization: we know that if all tasks are in
	 * the fair class we can call that function directly:
	 */
	if (likely(prev->sched_class == class &&
		   rq->nr_running == rq->cfs.h_nr_running)) {
		p = fair_sched_class.pick_next_task(rq, prev, cookie);
		if (unlikely(p == RETRY_TASK))
			goto again;

		/* assumes fair_sched_class->next == idle_sched_class */
		if (unlikely(!p))
			p = idle_sched_class.pick_next_task(rq, prev, cookie);
		//
		//
		do_gettimeofday(p->time_s->p_n_t_end);
		p->time_s->p_n_t_start=prev->time_s->next_p_n_t_start; 
		return p;
	}

again:
	for_each_class(class) {
		p = class->pick_next_task(rq, prev, cookie);
		if (p) {
			if (unlikely(p == RETRY_TASK))
				goto again;
			//
			//
			do_gettimeofday(p->time_s->p_n_t_end);
			p->time_s->p_n_t_start=prev->time_s->next_p_n_t_start; 
			
			return p;
		}
	}

	BUG(); /* the idle class will always have a runnable task */
}

//记录切换上下文开始时间 
static __always_inline struct rq *
context_switch(struct rq *rq, struct task_struct *prev,
	       struct task_struct *next, struct pin_cookie cookie)
{
	//
	//
	do_gettimeofday(p->time_s->in_context_switch);
	struct mm_struct *mm, *oldmm;

	prepare_task_switch(rq, prev, next);

	mm = next->mm;
	oldmm = prev->active_mm;
	/*
	 * For paravirt, this is coupled with an exit in switch_to to
	 * combine the page table reload and the switch backend into
	 * one hypercall.
	 */
	arch_start_context_switch(prev);

	if (!mm) {
		next->active_mm = oldmm;
		atomic_inc(&oldmm->mm_count);
		enter_lazy_tlb(oldmm, next);
	} else
		switch_mm_irqs_off(oldmm, mm, next);

	if (!prev->mm) {
		prev->active_mm = NULL;
		rq->prev_mm = oldmm;
	}
	/*
	 * Since the runqueue lock will be released by the next
	 * task (which is an invalid locking op but in the case
	 * of the scheduler it's an obvious special-case), so we
	 * do an early lockdep release here:
	 */
	lockdep_unpin_lock(&rq->lock, cookie);
	spin_release(&rq->lock.dep_map, 1, _THIS_IP_);

	/* Here we just switch the register state and the stack. */
	switch_to(prev, next, prev);
	barrier();

	return finish_task_switch(prev);
}

//打印时间信息到trace文件里 
void time_printk(struct task_struct *prev, struct task_struct *next){
	trace_printk("%d <-> %d\n",prev->pid,next->pid);
	//考虑到一些进程可能会耗时很长，所以将tv_sec也打印，之后再进行数据处理 
	trace_printk("%lds %ldus %lds %ldus %lds %ldus %lds %ldus\n",
				p->time_s->in_queue_end->tv_sec-p->time_s->in_queue_start->tv_sec,
				p->time_s->in_queue_end.tv_usec-p->time_s->in_queue_start->tv_usec,
				p->time_s->out_queue_end->tv_sec-p->time_s->out_queue_start->tv_sec,
				p->time_s->out_queue_end->tv_usec-p->time_s->out_queue_start->tv_usec,
				p->time_s->out_context_switch->tv_sec-p->time_s->in_context_switch->tv_sec,
				p->time_s->out_context_switch->tv_usec-p->time_s->in_context_switch->tv_usec,
				p->time_s->p_n_t_end->tv_sec-p->time_s->p_n_t_start->tv_sec,
				p->time_s->p_n_t_end->tv_usec-p->time_s->p_n_t_start->tv_usec);
}
static void __sched notrace __schedule(bool preempt)
{
	struct task_struct *prev, *next;
	unsigned long *switch_count;
	struct pin_cookie cookie;
	struct rq *rq;
	int cpu;

	cpu = smp_processor_id();
	rq = cpu_rq(cpu);
	prev = rq->curr;

	schedule_debug(prev);

	if (sched_feat(HRTICK))
		hrtick_clear(rq);

	local_irq_disable();
	rcu_note_context_switch();

	/*
	 * Make sure that signal_pending_state()->signal_pending() below
	 * can't be reordered with __set_current_state(TASK_INTERRUPTIBLE)
	 * done by the caller to avoid the race with signal_wake_up().
	 */
	smp_mb__before_spinlock();
	raw_spin_lock(&rq->lock);
	cookie = lockdep_pin_lock(&rq->lock);

	rq->clock_skip_update <<= 1; /* promote REQ to ACT */

	switch_count = &prev->nivcsw;
	if (!preempt && prev->state) {
		if (unlikely(signal_pending_state(prev->state, prev))) {
			prev->state = TASK_RUNNING;
		} else {
			deactivate_task(rq, prev, DEQUEUE_SLEEP);
			prev->on_rq = 0;

			/*
			 * If a worker went to sleep, notify and ask workqueue
			 * whether it wants to wake up a task to maintain
			 * concurrency.
			 */
			if (prev->flags & PF_WQ_WORKER) {
				struct task_struct *to_wakeup;

				to_wakeup = wq_worker_sleeping(prev);
				if (to_wakeup)
					try_to_wake_up_local(to_wakeup, cookie);
			}
		}
		switch_count = &prev->nvcsw;
	}

	if (task_on_rq_queued(prev))
		update_rq_clock(rq);

	next = pick_next_task(rq, prev, cookie);
	clear_tsk_need_resched(prev);
	clear_preempt_need_resched();
	rq->clock_skip_update = 0;

	if (likely(prev != next)) {
		rq->nr_switches++;
		rq->curr = next;
		++*switch_count;

		trace_sched_switch(preempt, prev, next);
		rq = context_switch(rq, prev, next, cookie); /* unlocks the rq */
		//
		//
		do_gettimeofday(next->time_s->out_context_switch);
		time_printk(prev, next);

	} else {
		lockdep_unpin_lock(&rq->lock, cookie);
		raw_spin_unlock_irq(&rq->lock);
	}

	balance_callback(rq);
}
