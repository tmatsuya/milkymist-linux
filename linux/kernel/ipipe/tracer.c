/* -*- linux-c -*-
 * kernel/ipipe/tracer.c
 *
 * Copyright (C) 2005 Luotao Fu.
 *               2005-2007 Jan Kiszka.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, Inc., 675 Mass Ave, Cambridge MA 02139,
 * USA; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kallsyms.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/ctype.h>
#include <linux/vmalloc.h>
#include <linux/utsrelease.h>
#include <linux/sched.h>
#include <linux/ipipe.h>
#include <asm/uaccess.h>

#define IPIPE_TRACE_PATHS           4 /* <!> Do not lower below 3 */
#define IPIPE_DEFAULT_ACTIVE        0
#define IPIPE_DEFAULT_MAX           1
#define IPIPE_DEFAULT_FROZEN        2

#define IPIPE_TRACE_POINTS          (1 << CONFIG_IPIPE_TRACE_SHIFT)
#define WRAP_POINT_NO(point)        ((point) & (IPIPE_TRACE_POINTS-1))

#define IPIPE_DEFAULT_PRE_TRACE     10
#define IPIPE_DEFAULT_POST_TRACE    10
#define IPIPE_DEFAULT_BACK_TRACE    100

#define IPIPE_DELAY_NOTE            1000  /* in nanoseconds */
#define IPIPE_DELAY_WARN            10000 /* in nanoseconds */

#define IPIPE_TFLG_NMI_LOCK         0x0001
#define IPIPE_TFLG_NMI_HIT          0x0002
#define IPIPE_TFLG_NMI_FREEZE_REQ   0x0004

#define IPIPE_TFLG_HWIRQ_OFF        0x0100
#define IPIPE_TFLG_FREEZING         0x0200
#define IPIPE_TFLG_CURRDOM_SHIFT    10   /* bits 10..11: current domain */
#define IPIPE_TFLG_CURRDOM_MASK     0x0C00
#define IPIPE_TFLG_DOMSTATE_SHIFT   12   /* bits 12..15: domain stalled? */
#define IPIPE_TFLG_DOMSTATE_BITS    3

#define IPIPE_TFLG_DOMAIN_STALLED(point, n) \
	(point->flags & (1 << (n + IPIPE_TFLG_DOMSTATE_SHIFT)))
#define IPIPE_TFLG_CURRENT_DOMAIN(point) \
	((point->flags & IPIPE_TFLG_CURRDOM_MASK) >> IPIPE_TFLG_CURRDOM_SHIFT)


struct ipipe_trace_point{
	short type;
	short flags;
	unsigned long eip;
	unsigned long parent_eip;
	unsigned long v;
	unsigned long long timestamp;
};

struct ipipe_trace_path{
	volatile int flags;
	int dump_lock; /* separated from flags due to cross-cpu access */
	int trace_pos; /* next point to fill */
	int begin, end; /* finalised path begin and end */
	int post_trace; /* non-zero when in post-trace phase */
	unsigned long long length; /* max path length in cycles */
	unsigned long nmi_saved_eip; /* for deferred requests from NMIs */
	unsigned long nmi_saved_parent_eip;
	unsigned long nmi_saved_v;
	struct ipipe_trace_point point[IPIPE_TRACE_POINTS];
} ____cacheline_aligned_in_smp;

enum ipipe_trace_type
{
	IPIPE_TRACE_FUNC = 0,
	IPIPE_TRACE_BEGIN,
	IPIPE_TRACE_END,
	IPIPE_TRACE_FREEZE,
	IPIPE_TRACE_SPECIAL,
	IPIPE_TRACE_PID,
};

#define IPIPE_TYPE_MASK             0x0007
#define IPIPE_TYPE_BITS             3


#ifdef CONFIG_IPIPE_TRACE_VMALLOC

static struct ipipe_trace_path *trace_paths[NR_CPUS];

#else /* !CONFIG_IPIPE_TRACE_VMALLOC */

static struct ipipe_trace_path trace_paths[NR_CPUS][IPIPE_TRACE_PATHS] =
	{ [0 ... NR_CPUS-1] =
		{ [0 ... IPIPE_TRACE_PATHS-1] =
			{ .begin = -1, .end = -1 }
		}
	};
#endif /* CONFIG_IPIPE_TRACE_VMALLOC */

int ipipe_trace_enable = 0;

static int active_path[NR_CPUS] =
	{ [0 ... NR_CPUS-1] = IPIPE_DEFAULT_ACTIVE };
static int max_path[NR_CPUS] =
	{ [0 ... NR_CPUS-1] = IPIPE_DEFAULT_MAX };
static int frozen_path[NR_CPUS] =
	{ [0 ... NR_CPUS-1] = IPIPE_DEFAULT_FROZEN };
static IPIPE_DEFINE_SPINLOCK(global_path_lock);
static int pre_trace = IPIPE_DEFAULT_PRE_TRACE;
static int post_trace = IPIPE_DEFAULT_POST_TRACE;
static int back_trace = IPIPE_DEFAULT_BACK_TRACE;
static int verbose_trace = 1;
static unsigned long trace_overhead;

static unsigned long trigger_begin;
static unsigned long trigger_end;

static DEFINE_MUTEX(out_mutex);
static struct ipipe_trace_path *print_path;
#ifdef CONFIG_IPIPE_TRACE_PANIC
static struct ipipe_trace_path *panic_path;
#endif /* CONFIG_IPIPE_TRACE_PANIC */
static int print_pre_trace;
static int print_post_trace;


static long __ipipe_signed_tsc2us(long long tsc);
static void
__ipipe_trace_point_type(char *buf, struct ipipe_trace_point *point);
static void __ipipe_print_symname(struct seq_file *m, unsigned long eip);


static notrace void
__ipipe_store_domain_states(struct ipipe_trace_point *point)
{
	struct ipipe_domain *ipd;
	struct list_head *pos;
	int i = 0;

	list_for_each_prev(pos, &__ipipe_pipeline) {
		ipd = list_entry(pos, struct ipipe_domain, p_link);

		if (test_bit(IPIPE_STALL_FLAG, &ipipe_cpudom_var(ipd, status)))
			point->flags |= 1 << (i + IPIPE_TFLG_DOMSTATE_SHIFT);

		if (ipd == ipipe_current_domain)
			point->flags |= i << IPIPE_TFLG_CURRDOM_SHIFT;

		if (++i > IPIPE_TFLG_DOMSTATE_BITS)
			break;
	}
}

static notrace int __ipipe_get_free_trace_path(int old, int cpu_id)
{
	int new_active = old;
	struct ipipe_trace_path *tp;

	do {
		if (++new_active == IPIPE_TRACE_PATHS)
			new_active = 0;
		tp = &trace_paths[cpu_id][new_active];
	} while ((new_active == max_path[cpu_id]) ||
	         (new_active == frozen_path[cpu_id]) ||
	         tp->dump_lock);

	return new_active;
}

static notrace void
__ipipe_migrate_pre_trace(struct ipipe_trace_path *new_tp,
                          struct ipipe_trace_path *old_tp, int old_pos)
{
	int i;

	new_tp->trace_pos = pre_trace+1;

	for (i = new_tp->trace_pos; i > 0; i--)
		memcpy(&new_tp->point[WRAP_POINT_NO(new_tp->trace_pos-i)],
		       &old_tp->point[WRAP_POINT_NO(old_pos-i)],
		       sizeof(struct ipipe_trace_point));

	/* mark the end (i.e. the point before point[0]) invalid */
	new_tp->point[IPIPE_TRACE_POINTS-1].eip = 0;
}

static notrace struct ipipe_trace_path *
__ipipe_trace_end(int cpu_id, struct ipipe_trace_path *tp, int pos)
{
	struct ipipe_trace_path *old_tp = tp;
	long active = active_path[cpu_id];
	unsigned long long length;

	/* do we have a new worst case? */
	length = tp->point[tp->end].timestamp -
	         tp->point[tp->begin].timestamp;
	if (length > (trace_paths[cpu_id][max_path[cpu_id]]).length) {
		/* we need protection here against other cpus trying
		   to start a proc dump */
		spin_lock(&global_path_lock);

		/* active path holds new worst case */
		tp->length = length;
		max_path[cpu_id] = active;

		/* find next unused trace path */
		active = __ipipe_get_free_trace_path(active, cpu_id);

		spin_unlock(&global_path_lock);

		tp = &trace_paths[cpu_id][active];

		/* migrate last entries for pre-tracing */
		__ipipe_migrate_pre_trace(tp, old_tp, pos);
	}

	return tp;
}

static notrace struct ipipe_trace_path *
__ipipe_trace_freeze(int cpu_id, struct ipipe_trace_path *tp, int pos)
{
	struct ipipe_trace_path *old_tp = tp;
	long active = active_path[cpu_id];
	int i;

	/* frozen paths have no core (begin=end) */
	tp->begin = tp->end;

	/* we need protection here against other cpus trying
	 * to set their frozen path or to start a proc dump */
	spin_lock(&global_path_lock);

	frozen_path[cpu_id] = active;

	/* find next unused trace path */
	active = __ipipe_get_free_trace_path(active, cpu_id);

	/* check if this is the first frozen path */
	for (i = 0; i < NR_CPUS && trace_paths[i] != NULL; i++) {
		if ((i != cpu_id) &&
		    (trace_paths[i][frozen_path[i]].end >= 0))
			tp->end = -1;
	}

	spin_unlock(&global_path_lock);

	tp = &trace_paths[cpu_id][active];

	/* migrate last entries for pre-tracing */
	__ipipe_migrate_pre_trace(tp, old_tp, pos);

	return tp;
}

void notrace
__ipipe_trace(enum ipipe_trace_type type, unsigned long eip,
              unsigned long parent_eip, unsigned long v)
{
	struct ipipe_trace_path *tp, *old_tp;
	int pos, next_pos, begin;
	struct ipipe_trace_point *point;
	unsigned long flags;
	int cpu_id;

	local_irq_save_hw_notrace(flags);

	cpu_id = ipipe_processor_id();
 restart:
	tp = old_tp = &trace_paths[cpu_id][active_path[cpu_id]];

	/* here starts a race window with NMIs - catched below */

	/* check for NMI recursion */
	if (unlikely(tp->flags & IPIPE_TFLG_NMI_LOCK)) {
		tp->flags |= IPIPE_TFLG_NMI_HIT;

		/* first freeze request from NMI context? */
		if ((type == IPIPE_TRACE_FREEZE) &&
		    !(tp->flags & IPIPE_TFLG_NMI_FREEZE_REQ)) {
			/* save arguments and mark deferred freezing */
			tp->flags |= IPIPE_TFLG_NMI_FREEZE_REQ;
			tp->nmi_saved_eip = eip;
			tp->nmi_saved_parent_eip = parent_eip;
			tp->nmi_saved_v = v;
		}
		return; /* no need for restoring flags inside IRQ */
	}

	/* clear NMI events and set lock (atomically per cpu) */
	tp->flags = (tp->flags & ~(IPIPE_TFLG_NMI_HIT |
	                           IPIPE_TFLG_NMI_FREEZE_REQ))
	                       | IPIPE_TFLG_NMI_LOCK;

	/* check active_path again - some nasty NMI may have switched
	 * it meanwhile */
	if (unlikely(tp != &trace_paths[cpu_id][active_path[cpu_id]])) {
		/* release lock on wrong path and restart */
		tp->flags &= ~IPIPE_TFLG_NMI_LOCK;

		/* there is no chance that the NMI got deferred
		 * => no need to check for pending freeze requests */
		goto restart;
	}

	/* get the point buffer */
	pos = tp->trace_pos;
	point = &tp->point[pos];

	/* store all trace point data */
	point->type = type;
	point->flags = raw_irqs_disabled_flags(flags) ? IPIPE_TFLG_HWIRQ_OFF : 0;
	point->eip = eip;
	point->parent_eip = parent_eip;
	point->v = v;
	ipipe_read_tsc(point->timestamp);

	__ipipe_store_domain_states(point);

	/* forward to next point buffer */
	next_pos = WRAP_POINT_NO(pos+1);
	tp->trace_pos = next_pos;

	/* only mark beginning if we haven't started yet */
	begin = tp->begin;
	if (unlikely(type == IPIPE_TRACE_BEGIN) && (begin < 0))
		tp->begin = pos;

	/* end of critical path, start post-trace if not already started */
	if (unlikely(type == IPIPE_TRACE_END) &&
	    (begin >= 0) && !tp->post_trace)
		tp->post_trace = post_trace + 1;

	/* freeze only if the slot is free and we are not already freezing */
	if ((unlikely(type == IPIPE_TRACE_FREEZE) ||
	     (unlikely(eip >= trigger_begin && eip <= trigger_end) &&
	     type == IPIPE_TRACE_FUNC)) &&
	    (trace_paths[cpu_id][frozen_path[cpu_id]].begin < 0) &&
	    !(tp->flags & IPIPE_TFLG_FREEZING)) {
		tp->post_trace = post_trace + 1;
		tp->flags |= IPIPE_TFLG_FREEZING;
	}

	/* enforce end of trace in case of overflow */
	if (unlikely(WRAP_POINT_NO(next_pos + 1) == begin)) {
		tp->end = pos;
		goto enforce_end;
	}

	/* stop tracing this path if we are in post-trace and
	 *  a) that phase is over now or
	 *  b) a new TRACE_BEGIN came in but we are not freezing this path */
	if (unlikely((tp->post_trace > 0) && ((--tp->post_trace == 0) ||
	             ((type == IPIPE_TRACE_BEGIN) &&
	              !(tp->flags & IPIPE_TFLG_FREEZING))))) {
		/* store the path's end (i.e. excluding post-trace) */
		tp->end = WRAP_POINT_NO(pos - post_trace + tp->post_trace);

 enforce_end:
		if (tp->flags & IPIPE_TFLG_FREEZING)
			tp = __ipipe_trace_freeze(cpu_id, tp, pos);
		else
			tp = __ipipe_trace_end(cpu_id, tp, pos);

		/* reset the active path, maybe already start a new one */
		tp->begin = (type == IPIPE_TRACE_BEGIN) ?
			WRAP_POINT_NO(tp->trace_pos - 1) : -1;
		tp->end = -1;
		tp->post_trace = 0;
		tp->flags = 0;

		/* update active_path not earlier to avoid races with NMIs */
		active_path[cpu_id] = tp - trace_paths[cpu_id];
	}

	/* we still have old_tp and point,
	 * let's reset NMI lock and check for catches */
	old_tp->flags &= ~IPIPE_TFLG_NMI_LOCK;
	if (unlikely(old_tp->flags & IPIPE_TFLG_NMI_HIT)) {
		/* well, this late tagging may not immediately be visible for
		 * other cpus already dumping this path - a minor issue */
		point->flags |= IPIPE_TFLG_NMI_HIT;

		/* handle deferred freezing from NMI context */
		if (old_tp->flags & IPIPE_TFLG_NMI_FREEZE_REQ)
			__ipipe_trace(IPIPE_TRACE_FREEZE, old_tp->nmi_saved_eip,
			              old_tp->nmi_saved_parent_eip,
			              old_tp->nmi_saved_v);
	}

	local_irq_restore_hw_notrace(flags);
}

static unsigned long __ipipe_global_path_lock(void)
{
	unsigned long flags;
	int cpu_id;
	struct ipipe_trace_path *tp;

	spin_lock_irqsave(&global_path_lock, flags);

	cpu_id = ipipe_processor_id();
 restart:
	tp = &trace_paths[cpu_id][active_path[cpu_id]];

	/* here is small race window with NMIs - catched below */

	/* clear NMI events and set lock (atomically per cpu) */
	tp->flags = (tp->flags & ~(IPIPE_TFLG_NMI_HIT |
	                           IPIPE_TFLG_NMI_FREEZE_REQ))
	                       | IPIPE_TFLG_NMI_LOCK;

	/* check active_path again - some nasty NMI may have switched
	 * it meanwhile */
	if (tp != &trace_paths[cpu_id][active_path[cpu_id]]) {
		/* release lock on wrong path and restart */
		tp->flags &= ~IPIPE_TFLG_NMI_LOCK;

		/* there is no chance that the NMI got deferred
		 * => no need to check for pending freeze requests */
		goto restart;
	}

	return flags;
}

static void __ipipe_global_path_unlock(unsigned long flags)
{
	int cpu_id;
	struct ipipe_trace_path *tp;

	/* release spinlock first - it's not involved in the NMI issue */
	__ipipe_spin_unlock_irqbegin(&global_path_lock);

	cpu_id = ipipe_processor_id();
	tp = &trace_paths[cpu_id][active_path[cpu_id]];

	tp->flags &= ~IPIPE_TFLG_NMI_LOCK;

	/* handle deferred freezing from NMI context */
	if (tp->flags & IPIPE_TFLG_NMI_FREEZE_REQ)
		__ipipe_trace(IPIPE_TRACE_FREEZE, tp->nmi_saved_eip,
		              tp->nmi_saved_parent_eip, tp->nmi_saved_v);

	/* See __ipipe_spin_lock_irqsave() and friends. */
	__ipipe_spin_unlock_irqcomplete(flags);
}

void notrace ipipe_trace_begin(unsigned long v)
{
	if (!ipipe_trace_enable)
		return;
	__ipipe_trace(IPIPE_TRACE_BEGIN, __BUILTIN_RETURN_ADDRESS0,
	              __BUILTIN_RETURN_ADDRESS1, v);
}
EXPORT_SYMBOL(ipipe_trace_begin);

void notrace ipipe_trace_end(unsigned long v)
{
	if (!ipipe_trace_enable)
		return;
	__ipipe_trace(IPIPE_TRACE_END, __BUILTIN_RETURN_ADDRESS0,
	              __BUILTIN_RETURN_ADDRESS1, v);
}
EXPORT_SYMBOL(ipipe_trace_end);

void notrace ipipe_trace_freeze(unsigned long v)
{
	if (!ipipe_trace_enable)
		return;
	__ipipe_trace(IPIPE_TRACE_FREEZE, __BUILTIN_RETURN_ADDRESS0,
	              __BUILTIN_RETURN_ADDRESS1, v);
}
EXPORT_SYMBOL(ipipe_trace_freeze);

void notrace ipipe_trace_special(unsigned char id, unsigned long v)
{
	if (!ipipe_trace_enable)
		return;
	__ipipe_trace(IPIPE_TRACE_SPECIAL | (id << IPIPE_TYPE_BITS),
	              __BUILTIN_RETURN_ADDRESS0,
	              __BUILTIN_RETURN_ADDRESS1, v);
}
EXPORT_SYMBOL(ipipe_trace_special);

void notrace ipipe_trace_pid(pid_t pid, short prio)
{
	if (!ipipe_trace_enable)
		return;
	__ipipe_trace(IPIPE_TRACE_PID | (prio << IPIPE_TYPE_BITS),
	              __BUILTIN_RETURN_ADDRESS0,
	              __BUILTIN_RETURN_ADDRESS1, pid);
}
EXPORT_SYMBOL(ipipe_trace_pid);

int ipipe_trace_max_reset(void)
{
	int cpu_id;
	unsigned long flags;
	struct ipipe_trace_path *path;
	int ret = 0;

	flags = __ipipe_global_path_lock();

	for (cpu_id = 0; cpu_id < NR_CPUS && trace_paths[cpu_id] != NULL; cpu_id++) {
		path = &trace_paths[cpu_id][max_path[cpu_id]];

		if (path->dump_lock) {
			ret = -EBUSY;
			break;
		}

		path->begin     = -1;
		path->end       = -1;
		path->trace_pos = 0;
		path->length    = 0;
	}

	__ipipe_global_path_unlock(flags);

	return ret;
}
EXPORT_SYMBOL(ipipe_trace_max_reset);

int ipipe_trace_frozen_reset(void)
{
	int cpu_id;
	unsigned long flags;
	struct ipipe_trace_path *path;
	int ret = 0;

	flags = __ipipe_global_path_lock();

	for_each_online_cpu(cpu_id) {
		path = &trace_paths[cpu_id][frozen_path[cpu_id]];

		if (path->dump_lock) {
			ret = -EBUSY;
			break;
		}

		path->begin = -1;
		path->end = -1;
		path->trace_pos = 0;
		path->length    = 0;
	}

	__ipipe_global_path_unlock(flags);

	return ret;
}
EXPORT_SYMBOL(ipipe_trace_frozen_reset);

static void
__ipipe_get_task_info(char *task_info, struct ipipe_trace_point *point,
                      int trylock)
{
	struct task_struct *task = NULL;
	char buf[8];
	int i;
	int locked = 1;

	if (trylock) {
		if (!read_trylock(&tasklist_lock))
			locked = 0;
	} else
		read_lock(&tasklist_lock);

	if (locked)
		task = find_task_by_pid((pid_t)point->v);

	if (task)
		strncpy(task_info, task->comm, 11);
	else
		strcpy(task_info, "-<?>-");

	if (locked)
		read_unlock(&tasklist_lock);

	for (i = strlen(task_info); i < 11; i++)
		task_info[i] = ' ';

	sprintf(buf, " %d ", point->type >> IPIPE_TYPE_BITS);
	strcpy(task_info + (11 - strlen(buf)), buf);
}

#ifdef CONFIG_IPIPE_TRACE_PANIC
void ipipe_trace_panic_freeze(void)
{
	unsigned long flags;
	int cpu_id;

	if (!ipipe_trace_enable)
		return;

	ipipe_trace_enable = 0;
	local_irq_save_hw_notrace(flags);

	cpu_id = ipipe_processor_id();

	panic_path = &trace_paths[cpu_id][active_path[cpu_id]];

	local_irq_restore_hw(flags);
}
EXPORT_SYMBOL(ipipe_trace_panic_freeze);

void ipipe_trace_panic_dump(void)
{
	int cnt = back_trace;
	int start, pos;
	char task_info[12];

	if (!panic_path)
		return;

	ipipe_context_check_off();

	printk("I-pipe tracer log (%d points):\n", cnt);

	start = pos = WRAP_POINT_NO(panic_path->trace_pos-1);

	while (cnt-- > 0) {
		struct ipipe_trace_point *point = &panic_path->point[pos];
		long time;
		char buf[16];
		int i;

		printk(" %c",
		       (point->flags & IPIPE_TFLG_HWIRQ_OFF) ? '|' : ' ');

		for (i = IPIPE_TFLG_DOMSTATE_BITS; i >= 0; i--)
			printk("%c",
			       (IPIPE_TFLG_CURRENT_DOMAIN(point) == i) ?
				(IPIPE_TFLG_DOMAIN_STALLED(point, i) ?
					'#' : '+') :
				(IPIPE_TFLG_DOMAIN_STALLED(point, i) ?
					'*' : ' '));

		if (!point->eip)
			printk("-<invalid>-\n");
		else {
			__ipipe_trace_point_type(buf, point);
			printk(buf);

			switch (point->type & IPIPE_TYPE_MASK) {
				case IPIPE_TRACE_FUNC:
					printk("           ");
					break;

				case IPIPE_TRACE_PID:
					__ipipe_get_task_info(task_info,
							      point, 1);
					printk(task_info);
					break;

				default:
					printk("0x%08lx ", point->v);
			}

			time = __ipipe_signed_tsc2us(point->timestamp -
				panic_path->point[start].timestamp);
			printk(" %5ld ", time);

			__ipipe_print_symname(NULL, point->eip);
			printk(" (");
			__ipipe_print_symname(NULL, point->parent_eip);
			printk(")\n");
		}
		pos = WRAP_POINT_NO(pos - 1);
	}

	panic_path = NULL;
}
EXPORT_SYMBOL(ipipe_trace_panic_dump);
#endif /* CONFIG_IPIPE_TRACE_PANIC */


/* --- /proc output --- */

static notrace int __ipipe_in_critical_trpath(long point_no)
{
	return ((WRAP_POINT_NO(point_no-print_path->begin) <
	         WRAP_POINT_NO(print_path->end-print_path->begin)) ||
	        ((print_path->end == print_path->begin) &&
	         (WRAP_POINT_NO(point_no-print_path->end) >
	          print_post_trace)));
}

static long __ipipe_signed_tsc2us(long long tsc)
{
        unsigned long long abs_tsc;
        long us;

	/* ipipe_tsc2us works on unsigned => handle sign separately */
        abs_tsc = (tsc >= 0) ? tsc : -tsc;
        us = ipipe_tsc2us(abs_tsc);
        if (tsc < 0)
                return -us;
        else
                return us;
}

static void
__ipipe_trace_point_type(char *buf, struct ipipe_trace_point *point)
{
	switch (point->type & IPIPE_TYPE_MASK) {
		case IPIPE_TRACE_FUNC:
			strcpy(buf, "func    ");
			break;

		case IPIPE_TRACE_BEGIN:
			strcpy(buf, "begin   ");
			break;

		case IPIPE_TRACE_END:
			strcpy(buf, "end     ");
			break;

		case IPIPE_TRACE_FREEZE:
			strcpy(buf, "freeze  ");
			break;

		case IPIPE_TRACE_SPECIAL:
			sprintf(buf, "(0x%02x)  ",
				point->type >> IPIPE_TYPE_BITS);
			break;

		case IPIPE_TRACE_PID:
			sprintf(buf, "[%5d] ", (pid_t)point->v);
			break;
	}
}

static void
__ipipe_print_pathmark(struct seq_file *m, struct ipipe_trace_point *point)
{
	char mark = ' ';
	int point_no = point - print_path->point;
	int i;

	if (print_path->end == point_no)
		mark = '<';
	else if (print_path->begin == point_no)
		mark = '>';
	else if (__ipipe_in_critical_trpath(point_no))
		mark = ':';
	seq_printf(m, "%c%c", mark,
	           (point->flags & IPIPE_TFLG_HWIRQ_OFF) ? '|' : ' ');

	if (!verbose_trace)
		return;

	for (i = IPIPE_TFLG_DOMSTATE_BITS; i >= 0; i--)
		seq_printf(m, "%c",
			(IPIPE_TFLG_CURRENT_DOMAIN(point) == i) ?
			    (IPIPE_TFLG_DOMAIN_STALLED(point, i) ?
				'#' : '+') :
			(IPIPE_TFLG_DOMAIN_STALLED(point, i) ? '*' : ' '));
}

static void
__ipipe_print_delay(struct seq_file *m, struct ipipe_trace_point *point)
{
	unsigned long delay = 0;
	int next;
	char *mark = "  ";

	next = WRAP_POINT_NO(point+1 - print_path->point);

	if (next != print_path->trace_pos)
		delay = ipipe_tsc2ns(print_path->point[next].timestamp -
		                     point->timestamp);

	if (__ipipe_in_critical_trpath(point - print_path->point)) {
		if (delay > IPIPE_DELAY_WARN)
			mark = "! ";
		else if (delay > IPIPE_DELAY_NOTE)
			mark = "+ ";
	}
	seq_puts(m, mark);

	if (verbose_trace)
		seq_printf(m, "%3lu.%03lu%c ", delay/1000, delay%1000,
		           (point->flags & IPIPE_TFLG_NMI_HIT) ? 'N' : ' ');
	else
		seq_puts(m, " ");
}

static void __ipipe_print_symname(struct seq_file *m, unsigned long eip)
{
	char namebuf[KSYM_NAME_LEN+1];
	unsigned long size, offset;
	const char *sym_name;
	char *modname;

	sym_name = kallsyms_lookup(eip, &size, &offset, &modname, namebuf);

#ifdef CONFIG_IPIPE_TRACE_PANIC
	if (!m) {
		/* panic dump */
		if (sym_name) {
			printk("%s+0x%lx", sym_name, offset);
			if (modname)
				printk(" [%s]", modname);
		}
	} else
#endif /* CONFIG_IPIPE_TRACE_PANIC */
	{
		if (sym_name) {
			if (verbose_trace) {
				seq_printf(m, "%s+0x%lx", sym_name, offset);
				if (modname)
					seq_printf(m, " [%s]", modname);
			} else
				seq_puts(m, sym_name);
		} else
			seq_printf(m, "<%08lx>", eip);
	}
}

static void __ipipe_print_headline(struct seq_file *m)
{
	seq_printf(m, "Calibrated minimum trace-point overhead: %lu.%03lu "
		   "us\n\n", trace_overhead/1000, trace_overhead%1000);

	if (verbose_trace) {
		const char *name[4] = { [0 ... 3] = "<unused>" };
		struct list_head *pos;
		int i = 0;

		list_for_each_prev(pos, &__ipipe_pipeline) {
			struct ipipe_domain *ipd =
				list_entry(pos, struct ipipe_domain, p_link);

			name[i] = ipd->name;
			if (++i > 3)
				break;
		}

		seq_printf(m,
		           " +----- Hard IRQs ('|': locked)\n"
		           " |+---- %s\n"
		           " ||+--- %s\n"
		           " |||+-- %s\n"
		           " ||||+- %s%s\n"
		           " |||||                        +---------- "
		               "Delay flag ('+': > %d us, '!': > %d us)\n"
		           " |||||                        |        +- "
		               "NMI noise ('N')\n"
		           " |||||                        |        |\n"
		           "      Type    User Val.   Time    Delay  Function "
		               "(Parent)\n",
		           name[3], name[2], name[1], name[0],
		           name[0] ? " ('*': domain stalled, '+': current, "
		               "'#': current+stalled)" : "",
		           IPIPE_DELAY_NOTE/1000, IPIPE_DELAY_WARN/1000);
	} else
		seq_printf(m,
		           " +--------------- Hard IRQs ('|': locked)\n"
		           " |             +- Delay flag "
		               "('+': > %d us, '!': > %d us)\n"
		           " |             |\n"
		           "  Type     Time   Function (Parent)\n",
		           IPIPE_DELAY_NOTE/1000, IPIPE_DELAY_WARN/1000);
}

static void *__ipipe_max_prtrace_start(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;

	mutex_lock(&out_mutex);

	if (!n) {
		struct ipipe_trace_path *path;
		unsigned long length_usecs;
		int points, cpu;
		unsigned long flags;

		/* protect against max_path/frozen_path updates while we
		 * haven't locked our target path, also avoid recursively
		 * taking global_path_lock from NMI context */
		flags = __ipipe_global_path_lock();

		/* find the longest of all per-cpu paths */
		print_path = NULL;
		for_each_online_cpu(cpu) {
			path = &trace_paths[cpu][max_path[cpu]];
			if ((print_path == NULL) ||
			    (path->length > print_path->length)) {
				print_path = path;
				break;
			}
		}
		print_path->dump_lock = 1;

		__ipipe_global_path_unlock(flags);

		/* does this path actually contain data? */
		if (print_path->end == print_path->begin)
			return NULL;

		/* number of points inside the critical path */
		points = WRAP_POINT_NO(print_path->end-print_path->begin+1);

		/* pre- and post-tracing length, post-trace length was frozen
		   in __ipipe_trace, pre-trace may have to be reduced due to
		   buffer overrun */
		print_pre_trace  = pre_trace;
		print_post_trace = WRAP_POINT_NO(print_path->trace_pos -
		                                 print_path->end - 1);
		if (points+pre_trace+print_post_trace > IPIPE_TRACE_POINTS - 1)
			print_pre_trace = IPIPE_TRACE_POINTS - 1 - points -
				print_post_trace;

		length_usecs = ipipe_tsc2us(print_path->length);
		seq_printf(m, "I-pipe worst-case tracing service on %s/ipipe-%s\n"
			"------------------------------------------------------------\n",
			UTS_RELEASE, IPIPE_ARCH_STRING);
		seq_printf(m, "CPU: %d, Begin: %lld cycles, Trace Points: "
			"%d (-%d/+%d), Length: %lu us\n",
			cpu, print_path->point[print_path->begin].timestamp,
			points, print_pre_trace, print_post_trace, length_usecs);
		__ipipe_print_headline(m);
	}

	/* check if we are inside the trace range */
	if (n >= WRAP_POINT_NO(print_path->end - print_path->begin + 1 +
	                       print_pre_trace + print_post_trace))
		return NULL;

	/* return the next point to be shown */
	return &print_path->point[WRAP_POINT_NO(print_path->begin -
	                                        print_pre_trace + n)];
}

static void *__ipipe_prtrace_next(struct seq_file *m, void *p, loff_t *pos)
{
	loff_t n = ++*pos;

	/* check if we are inside the trace range with the next entry */
	if (n >= WRAP_POINT_NO(print_path->end - print_path->begin + 1 +
	                       print_pre_trace + print_post_trace))
		return NULL;

	/* return the next point to be shown */
	return &print_path->point[WRAP_POINT_NO(print_path->begin -
	                                        print_pre_trace + *pos)];
}

static void __ipipe_prtrace_stop(struct seq_file *m, void *p)
{
	if (print_path)
		print_path->dump_lock = 0;
	mutex_unlock(&out_mutex);
}

static int __ipipe_prtrace_show(struct seq_file *m, void *p)
{
	long time;
	struct ipipe_trace_point *point = p;
	char buf[16];

	if (!point->eip) {
		seq_puts(m, "-<invalid>-\n");
		return 0;
	}

	__ipipe_print_pathmark(m, point);
	__ipipe_trace_point_type(buf, point);
	seq_puts(m, buf);
	if (verbose_trace)
		switch (point->type & IPIPE_TYPE_MASK) {
			case IPIPE_TRACE_FUNC:
				seq_puts(m, "           ");
				break;

			case IPIPE_TRACE_PID:
				__ipipe_get_task_info(buf, point, 0);
				seq_puts(m, buf);
				break;

			default:
				seq_printf(m, "0x%08lx ", point->v);
		}

	time = __ipipe_signed_tsc2us(point->timestamp -
		print_path->point[print_path->begin].timestamp);
	seq_printf(m, "%5ld", time);

	__ipipe_print_delay(m, point);
	__ipipe_print_symname(m, point->eip);
	seq_puts(m, " (");
	__ipipe_print_symname(m, point->parent_eip);
	seq_puts(m, ")\n");

	return 0;
}

static struct seq_operations __ipipe_max_ptrace_ops = {
	.start = __ipipe_max_prtrace_start,
	.next  = __ipipe_prtrace_next,
	.stop  = __ipipe_prtrace_stop,
	.show  = __ipipe_prtrace_show
};

static int __ipipe_max_prtrace_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &__ipipe_max_ptrace_ops);
}

static ssize_t
__ipipe_max_reset(struct file *file, const char __user *pbuffer,
                  size_t count, loff_t *data)
{
	mutex_lock(&out_mutex);
	ipipe_trace_max_reset();
	mutex_unlock(&out_mutex);

	return count;
}

struct file_operations __ipipe_max_prtrace_fops = {
	.open       = __ipipe_max_prtrace_open,
	.read       = seq_read,
	.write      = __ipipe_max_reset,
	.llseek     = seq_lseek,
	.release    = seq_release,
};

static void *__ipipe_frozen_prtrace_start(struct seq_file *m, loff_t *pos)
{
	loff_t n = *pos;

	mutex_lock(&out_mutex);

	if (!n) {
		struct ipipe_trace_path *path;
		int cpu;
		unsigned long flags;

		/* protect against max_path/frozen_path updates while we
		 * haven't locked our target path, also avoid recursively
		 * taking global_path_lock from NMI context */
		flags = __ipipe_global_path_lock();

		/* find the first of all per-cpu frozen paths */
		print_path = NULL;
		for_each_online_cpu(cpu) {
			path = &trace_paths[cpu][frozen_path[cpu]];
			if (path->end >= 0) {
				print_path = path;
				break;
			}
		}
		if (print_path)
			print_path->dump_lock = 1;

		__ipipe_global_path_unlock(flags);

		if (!print_path)
			return NULL;

		/* back- and post-tracing length, post-trace length was frozen
		   in __ipipe_trace, back-trace may have to be reduced due to
		   buffer overrun */
		print_pre_trace  = back_trace-1; /* substract freeze point */
		print_post_trace = WRAP_POINT_NO(print_path->trace_pos -
		                                 print_path->end - 1);
		if (1+pre_trace+print_post_trace > IPIPE_TRACE_POINTS - 1)
			print_pre_trace = IPIPE_TRACE_POINTS - 2 -
				print_post_trace;

		seq_printf(m, "I-pipe frozen back-tracing service on %s/ipipe-%s\n"
			"------------------------------------------------------"
			"------\n",
			UTS_RELEASE, IPIPE_ARCH_STRING);
		seq_printf(m, "CPU: %d, Freeze: %lld cycles, Trace Points: %d (+%d)\n",
			cpu, print_path->point[print_path->begin].timestamp,
			print_pre_trace+1, print_post_trace);
		__ipipe_print_headline(m);
	}

	/* check if we are inside the trace range */
	if (n >= print_pre_trace + 1 + print_post_trace)
		return NULL;

	/* return the next point to be shown */
	return &print_path->point[WRAP_POINT_NO(print_path->begin-
	                                        print_pre_trace+n)];
}

static struct seq_operations __ipipe_frozen_ptrace_ops = {
	.start = __ipipe_frozen_prtrace_start,
	.next  = __ipipe_prtrace_next,
	.stop  = __ipipe_prtrace_stop,
	.show  = __ipipe_prtrace_show
};

static int __ipipe_frozen_prtrace_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &__ipipe_frozen_ptrace_ops);
}

static ssize_t
__ipipe_frozen_ctrl(struct file *file, const char __user *pbuffer,
                    size_t count, loff_t *data)
{
	char *end, buf[16];
	int val;
	int n;

	n = (count > sizeof(buf) - 1) ? sizeof(buf) - 1 : count;

	if (copy_from_user(buf, pbuffer, n))
		return -EFAULT;

	buf[n] = '\0';
	val = simple_strtol(buf, &end, 0);

	if (((*end != '\0') && !isspace(*end)) || (val < 0))
		return -EINVAL;

	mutex_lock(&out_mutex);
	ipipe_trace_frozen_reset();
	if (val > 0)
		ipipe_trace_freeze(-1);
	mutex_unlock(&out_mutex);

	return count;
}

struct file_operations __ipipe_frozen_prtrace_fops = {
	.open       = __ipipe_frozen_prtrace_open,
	.read       = seq_read,
	.write      = __ipipe_frozen_ctrl,
	.llseek     = seq_lseek,
	.release    = seq_release,
};

static int __ipipe_rd_proc_val(char *page, char **start, off_t off,
                               int count, int *eof, void *data)
{
	int len;

	len = sprintf(page, "%u\n", *(int *)data);
	len -= off;
	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

static int __ipipe_wr_proc_val(struct file *file, const char __user *buffer,
                               unsigned long count, void *data)
{
	char *end, buf[16];
	int val;
	int n;

	n = (count > sizeof(buf) - 1) ? sizeof(buf) - 1 : count;

	if (copy_from_user(buf, buffer, n))
		return -EFAULT;

	buf[n] = '\0';
	val = simple_strtol(buf, &end, 0);

	if (((*end != '\0') && !isspace(*end)) || (val < 0))
		return -EINVAL;

	mutex_lock(&out_mutex);
	*(int *)data = val;
	mutex_unlock(&out_mutex);

	return count;
}

static int __ipipe_rd_trigger(char *page, char **start, off_t off, int count,
			      int *eof, void *data)
{
	int len;

	if (!trigger_begin)
		return 0;

	len = sprint_symbol(page, trigger_begin);
	page[len++] = '\n';

	len -= off;
	if (len <= off + count)
		*eof = 1;
	*start = page + off;
	if (len > count)
		len = count;
	if (len < 0)
		len = 0;

	return len;
}

static int __ipipe_wr_trigger(struct file *file, const char __user *buffer,
			      unsigned long count, void *data)
{
	char buf[KSYM_SYMBOL_LEN];
	unsigned long begin, end;

	if (count > sizeof(buf) - 1)
		count = sizeof(buf) - 1;
	if (copy_from_user(buf, buffer, count) < 0)
		return -EFAULT;
	buf[count] = 0;
	if (buf[count-1] == '\n')
		buf[count-1] = 0;

	begin = kallsyms_lookup_name(buf);
	if (!begin || !kallsyms_lookup_size_offset(begin, &end, NULL))
		return -ENOENT;
	end += begin - 1;

	mutex_lock(&out_mutex);
	/* invalidate the current range before setting a new one */
	trigger_end = 0;
	wmb();
	/* set new range */
	trigger_begin = begin;
	wmb();
	trigger_end = end;
	mutex_unlock(&out_mutex);

	return count;
}

extern struct proc_dir_entry *ipipe_proc_root;

static void __init
__ipipe_create_trace_proc_val(struct proc_dir_entry *trace_dir,
                              const char *name, int *value_ptr)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry(name, 0644, trace_dir);
	if (entry) {
		entry->data = value_ptr;
		entry->read_proc = __ipipe_rd_proc_val;
		entry->write_proc = __ipipe_wr_proc_val;
		entry->owner = THIS_MODULE;
	}
}

void __init __ipipe_init_tracer(void)
{
	struct proc_dir_entry *trace_dir;
	struct proc_dir_entry *entry;
	unsigned long long start, end, min = ULLONG_MAX;
	int i;
#ifdef CONFIG_IPIPE_TRACE_VMALLOC
	int cpu, path;

	for_each_possible_cpu(cpu) {
		trace_paths[cpu] = vmalloc(
			sizeof(struct ipipe_trace_path) * IPIPE_TRACE_PATHS);
		if (trace_paths[cpu] == NULL) {
			printk(KERN_ERR "I-pipe: "
			       "insufficient memory for trace buffer.\n");
			return;
		}
		memset(trace_paths[cpu], 0,
			sizeof(struct ipipe_trace_path) * IPIPE_TRACE_PATHS);
		for (path = 0; path < IPIPE_TRACE_PATHS; path++) {
			trace_paths[cpu][path].begin = -1;
			trace_paths[cpu][path].end   = -1;
		}
	}
#endif /* CONFIG_IPIPE_TRACE_VMALLOC */
	ipipe_trace_enable = CONFIG_IPIPE_TRACE_ENABLE_VALUE;

	/* Calculate minimum overhead of __ipipe_trace() */
	local_irq_disable_hw();
	for (i = 0; i < 100; i++) {
		ipipe_read_tsc(start);
		__ipipe_trace(IPIPE_TRACE_FUNC, __BUILTIN_RETURN_ADDRESS0,
			      __BUILTIN_RETURN_ADDRESS1, 0);
		ipipe_read_tsc(end);

		end -= start;
		if (end < min)
			min = end;
	}
	local_irq_enable_hw();
	trace_overhead = ipipe_tsc2ns(min);

	trace_dir = create_proc_entry("trace", S_IFDIR, ipipe_proc_root);

	entry = create_proc_entry("max", 0644, trace_dir);
	if (entry)
		entry->proc_fops = &__ipipe_max_prtrace_fops;

	entry = create_proc_entry("frozen", 0644, trace_dir);
	if (entry)
		entry->proc_fops = &__ipipe_frozen_prtrace_fops;

	entry = create_proc_entry("trigger", 0644, trace_dir);
	if (entry) {
		entry->read_proc = __ipipe_rd_trigger;
		entry->write_proc = __ipipe_wr_trigger;
		entry->owner = THIS_MODULE;
	}

	__ipipe_create_trace_proc_val(trace_dir, "pre_trace_points",
	                              &pre_trace);
	__ipipe_create_trace_proc_val(trace_dir, "post_trace_points",
	                              &post_trace);
	__ipipe_create_trace_proc_val(trace_dir, "back_trace_points",
	                              &back_trace);
	__ipipe_create_trace_proc_val(trace_dir, "verbose",
	                              &verbose_trace);
	__ipipe_create_trace_proc_val(trace_dir, "enable",
	                              &ipipe_trace_enable);
}
