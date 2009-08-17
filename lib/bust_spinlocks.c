/*
 * lib/bust_spinlocks.c
 *
 * Provides a minimal bust_spinlocks for architectures which don't have one of their own.
 *
 * bust_spinlocks() clears any spinlocks which would prevent oops, die(), BUG()
 * and panic() information from reaching the user.
 */

#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/wait.h>
#include <linux/vt_kern.h>
#include <linux/ipipe_trace.h>


void __attribute__((weak)) bust_spinlocks(int yes)
{
	if (yes) {
 		ipipe_trace_panic_freeze();
		oops_in_progress = 1;
	} else {
#ifdef CONFIG_VT
		unblank_screen();
#endif
		ipipe_trace_panic_dump();
		oops_in_progress = 0;
		wake_up_klogd();
	}
}


