#include <linux/sys.h>
#include <linux/linkage.h>
#include <asm/traps.h>
#include <asm/unistd.h>
#include <asm/thread_info.h>
#include <asm/errno.h>
#include <asm/setup.h>
#include <asm/segment.h>
#include <asm/asm-offsets.h>

/* 
 * Exception vector table (see "LatticeMico32 Processor Reference Manual")
 */

#undef LM32_DEBUG_SYSCALL_CALL
#undef LM32_DEBUG_SYSCALL_RET
/* #undef LM32_BREAK_ON_SYSCALL /**/
/* #define LM32_BREAK_ON_SYSCALL 190 /**/

#define HW_JTAG_EXCEPTION_VECTOR(offset) \
	rcsr  r7, DEBA; \
	addi  r7, r7, offset; \
	b     r7 ; \
	nop ; \
	nop ; \
	nop ; \
	nop ; \
	nop

/* exception vector for os-aware gdb and kernel signals */
#define KERNEL_EXCEPTION_VECTOR(offset) \
	addi	sp,sp,-128; \
	sw		(sp+120), ra; \
	calli	_save_syscall_frame; \
	mvi		r1, offset; \
	addi	r2, sp, 4; \
	calli	asm_do_sig; \
	bi		_return_from_exception; \
	nop

.section ".exception.text"	

ENTRY(reset_handler)
	KERNEL_EXCEPTION_VECTOR(0)

ENTRY(breakpoint_handler)
#ifdef LM32_HW_JTAG
	HW_JTAG_EXCEPTION_VECTOR(32)
#else
	bi _long_breakpoint_handler
	nop
	nop
	nop

	nop
	nop
	nop
	nop
#endif

ENTRY(instruction_bus_error_handler)
#ifdef LM32_HW_JTAG
	HW_JTAG_EXCEPTION_VECTOR(64)
#else
	KERNEL_EXCEPTION_VECTOR(64)
#endif

ENTRY(watchpoint_handler)
	HW_JTAG_EXCEPTION_VECTOR(96)

ENTRY(data_bus_error_handler)
#ifdef LM32_HW_JTAG
	HW_JTAG_EXCEPTION_VECTOR(128)
#else
	KERNEL_EXCEPTION_VECTOR(128)
#endif

ENTRY(divide_by_zero_handler)
	KERNEL_EXCEPTION_VECTOR(160)

ENTRY(interrupt_handler)
	addi    sp, sp, -128
	sw      (sp+120), ra
	calli   _save_irq_frame
	rcsr    r1, IP

	addi    r2, sp, 4
	calli   asm_do_IRQ
	bi      _restore_irq_frame_and_return
	nop

ENTRY(system_call)
	/* break */
	/* store away r9,r10 so that we can use it here TODO: use clobbered ones*/
	sw (sp+0), r9 /* needed for various */
	sw (sp+-4), r10 /* needed for current = current_thread_info()->task */
	sw (sp+-8), r11 /* needed for user stack pointer, if switching */

#ifdef LM32_BREAK_ON_SYSCALL
	mvi r9, LM32_BREAK_ON_SYSCALL /* only break in this syscall no */
	bne r8, r9, 7f
	break
7:
#endif

	/* test if already on kernel stack: test current_thread_info->task->which_stack */
	mvhi r9, hi(lm32_current_thread)
	ori r9, r9, lo(lm32_current_thread)
	lw r9, (r9+0) /* dereference lm32_current_thread */
	lw r10, (r9+TI_TASK) /* load pointer to task */
	lw r9, (r10+TASK_WHICH_STACK)
	be r9, r0, 1f

	/* we are on user stack, have to switch */
	mv r11, sp /* remember sp for restoring r9, r10, r11 */
	sw (r10+TASK_USP), sp /* store usp */
	lw sp, (r10+TASK_KSP) /* load ksp */
	sw (r10+TASK_WHICH_STACK), r0 /* set which_stack to 0 */

	/* restore r9, r10, r11 */
	lw r9, (r11+0)
	lw r10, (r11+-4)
	lw r11, (r11+-8)
	bi 2f

1:/* already on kernel stack */

	/* restore r9, r10 */
	lw r9, (sp+0)
	lw r10, (sp+-4)
	/* no need to restore r11 as we did not use it */

2:/* we now are on kernel stack and registers are untainted */

  /* save registers */
	addi  sp, sp, -128
	sw    (sp + 120), ra
	calli _save_syscall_frame

	/* r7 always holds the pointer to struct pt_regs */
	addi  r7, sp, 4

#ifdef LM32_DEBUG_SYSCALL_CALL
	/* call debug */
	calli lm32_debug_syscall
	/* restore caller saved registers */
	lw    r1, (sp+8) 
	lw    r2, (sp+12) 
	lw    r3, (sp+16) 
	lw    r4, (sp+20) 
	lw    r5, (sp+24) 
	lw    r6, (sp+28) 
	addi  r7, sp, 4
	lw    r8, (sp+36) 
	lw    r9, (sp+40) 
	lw    r10,(sp+44)
	lw    ra, (sp+120)
	/* end debug */
#endif

	/* r8 always holds the syscall number */
	/* check if syscall number is valid */
	mvi r9, __NR_syscall
	bgeu r8, r9, .badsyscall
	mvhi r9, hi(_sys_call_table) /* load address of syscall table */
	ori r9, r9, lo(_sys_call_table)
	sli r10, r8, 2 /* TODO: only works with shifter enabled */
	add r9, r9, r10 /* add offset of syscall no to address */
	lw r9, (r9+0) /* fetch address of syscall function */
	call r9 /* execute syscall */

.syscallTail:
	/* store pt_regs* in r2 */
	addi      r2,  sp, 4
	calli manage_signals
	sw (sp+8), r1 /* store return value into pt_regs */

#ifdef LM32_DEBUG_SYSCALL_RET
	/* debug syscall return value */
	lw r2,  (sp+36) /* get saved syscall no from sp+36 for debug */
	addi  r3, sp, 4
	calli lm32_debug_syscall_ret /* debug */
#endif

	bi      _restore_and_return_exception

.badsyscall:
#ifdef LM32_DEBUG_SYSCALL_RET
	mvi r1, -ENOSYS
	mv r2, r8
	addi  r3, sp, 4
	calli lm32_debug_syscall_ret
#endif
	mvi r1, -ENOSYS

	bi      _restore_and_return_exception

/* end of exception handlers */


/********************************/
/* ensure to be on kernel stack */
/********************************/
#define ENSURE_TO_BE_ON_KERNEL_STACK \
	/* store away r9,r10 so that we can use it here TODO: use clobbered ones*/ \
	sw (sp+-4), r9; /* needed for various */ \
	sw (sp+-8), r10; /* needed for current = current_thread_info()->task */ \
	sw (sp+-12), r11; /* needed for user stack pointer, if switching */ \
	/* test if already on kernel stack: test current_thread_info->task->which_stack */ \
	mvhi r9, hi(lm32_current_thread); \
	ori r9, r9, lo(lm32_current_thread); \
	lw r9, (r9+0); /* dereference lm32_current_thread */ \
	lw r10, (r9+TI_TASK); /* load pointer to task */ \
	lw r9, (r10+TASK_WHICH_STACK); \
	be r9, r0, 1f; \
	/* we are on user stack, have to switch */ \
	mv r11, sp; /* remember sp for restoring r9, r10, r11 */ \
	sw (r10+TASK_USP), sp; /* store usp */ \
	lw sp, (r10+TASK_KSP); /* load ksp */ \
	sw (r10+TASK_WHICH_STACK), r0; /* set which_stack to 0 */ \
	/* restore r9, r10, r11 */ \
	lw r9, (r11+-4); \
	lw r10, (r11+-8); \
	lw r11, (r11+-12); \
	bi 2f; \
1:/* already on kernel stack */ \
	/* restore r9, r10 */ \
	lw r9, (sp+0); \
	lw r10, (sp+-4); \
	/* no need to restore r11 as we did not use it */ \
2:/* now for sure on kernel stack */

_long_breakpoint_handler:
	ENSURE_TO_BE_ON_KERNEL_STACK; \
	addi	sp,sp,-128; \
	calli	_save_syscall_frame; \
	mvi		r1, 32; /* 32 = breakpoint magic offset */ \
	addi	r2, sp, 4; \
	calli	asm_do_sig; \
	bi		_return_from_debug_exception

/**************************/
/* exception return paths */
/**************************/

#ifdef LM32_DEBUG_SYSCALL_RET
#define EXCEPTION_RETURN_PATH_DEBUG \
	/* debug syscall return value */ \
	mvi r2, 0; \
	addi  r3, sp, 4; \
	calli lm32_debug_syscall_ret; /* debug */
#else
#define EXCEPTION_RETURN_PATH_DEBUG
#endif

/* return path for debug or non-debug exceptions */
#define EXCEPTION_RETURN_PATH(label, branch_to) \
label: \
	/* store pt_regs* in r2 */ \
	addi      r2,  sp, 4; \
	/* store 0 into r8 (syscall no) in pt_regs */ \
	sw (sp+36), r0; \
	calli manage_signals; \
	sw (sp+8), r1; /* store return value into pt_regs */ \
	EXCEPTION_RETURN_PATH_DEBUG \
	bi branch_to

EXCEPTION_RETURN_PATH(_return_from_exception, _restore_and_return_exception)

EXCEPTION_RETURN_PATH(_return_from_debug_exception, _restore_and_return_debug_exception)

/* ret_from_fork(unused, arg2, arg3, arg1, continuation) */
/* calls schedule_tail and then manage_signals */
/* returns to continuation(arg1, arg2, arg3) */
ENTRY(ret_from_fork)
	addi	sp, sp, -16
	sw	(sp + 4), r2
	sw	(sp + 8), r3
	sw	(sp + 12), r4
	sw	(sp + 16), r5
	calli	schedule_tail
	lw	r1, (sp + 12)
	mv	r2, r0
	/* calli manage_signals TODO reactivate */
	lw	r2, (sp + 4)
	lw	r3, (sp + 8)
	lw	ra, (sp + 16)
	addi	sp, sp, 16
	ret

ENTRY(sys_fork)
	mvi r0, -EINVAL
	ret

ENTRY(sys_execve_wrapper)
	/* save ra to stack */
	addi sp,sp,-4
	sw (sp+4), ra
	/* store regs into 4th argument */
	mv r4, r7
	calli sys_execve
	/* load ra from stack */
	lw ra, (sp+4)
	addi sp,sp,4
	ret

ENTRY(sys_rt_sigsuspend_wrapper)
	/* save ra to kernel stack */
	addi sp,sp,-4
	sw (sp+4), ra
	/* store regs into 3rd argument */
	mv r3, r7
	calli sys_rt_sigsuspend
	/* load ra from kernel stack */
	lw ra, (sp+4)
	addi sp,sp,4
	ret

ENTRY(sys_vfork_wrapper)
	/* save ra to kernel stack */
	addi sp,sp,-4
	sw (sp+4), ra
	/* store regs into 1st argument */
	mv r1, r7
	/* store ra into 2nd argument */
	mv r2, ra
	calli sys_lm32_vfork
	/* load ra from kernel stack */
	lw ra, (sp+4)
	addi sp,sp,4
	ret

/* purpose of this wrapper: put struct pt_regs* into first argument */
ENTRY(sys_sigreturn_wrapper)
	/* save ra to stack */
	addi sp,sp,-4
	sw (sp+4), ra
	/* fix first argument */
	mv r1, r7
	calli sys_sigreturn
	/* load ra from stack */
	lw ra, (sp+4)
	addi sp,sp,4
	ret

ENTRY(sys_clone_wrapper)
	/* save ra to stack */
	addi sp,sp,-4
	sw (sp+4), ra
	/* store ra into 5th argument */
	mv r5, ra
	calli sys_lm32_clone
	/* load ra from stack */
	lw ra, (sp+4)
	addi sp,sp,4
	ret

/* in IRQ we call a function between save and restore */
/* we therefore only save and restore the caller saved registers */
/* (r1-r10, ra, ea because an interrupt could interrupt another one) */
_save_irq_frame:
	sw      (sp+8),   r1
	sw      (sp+12),  r2
	sw      (sp+16),  r3
	sw      (sp+20),  r4
	sw      (sp+24),  r5
	sw      (sp+28),  r6
	sw      (sp+32),  r7
	sw      (sp+36),  r8
	sw      (sp+40),  r9
	sw      (sp+44),  r10
	/* ra (sp + 120) has already been written */
	sw      (sp+124), ea
	ret

/* restore all caller saved registers saved in _save_irq_frame and return from exception */
_restore_irq_frame_and_return:
	lw      r1,  (sp+8);
	lw      r2,  (sp+12);
	lw      r3,  (sp+16);
	lw      r4,  (sp+20);
	lw      r5,  (sp+24);
	lw      r6,  (sp+28);
	lw      r7,  (sp+32);
	lw      r8,  (sp+36);
	lw      r9,  (sp+40);
	lw      r10, (sp+44);
	lw      ra,  (sp+120)
	lw      ea,  (sp+124)
	addi    sp, sp, 128
	eret

_save_syscall_frame:
	sw      (sp+4),   r0
	sw      (sp+8),   r1
	sw      (sp+12),  r2
	sw      (sp+16),  r3
	sw      (sp+20),  r4
	sw      (sp+24),  r5
	sw      (sp+28),  r6
	sw      (sp+32),  r7
	sw      (sp+36),  r8
	sw      (sp+40),  r9
	sw      (sp+44),  r10
	sw      (sp+48),  r11
	sw      (sp+52),  r12
	sw      (sp+56),  r13
	sw      (sp+60),  r14
	sw      (sp+64),  r15
	sw      (sp+68),  r16
	sw      (sp+72),  r17
	sw      (sp+76),  r18
	sw      (sp+80),  r19
	sw      (sp+84),  r20
	sw      (sp+88),  r21
	sw      (sp+92),  r22
	sw      (sp+96),  r23
	sw      (sp+100), r24
	sw      (sp+104), r25
	sw      (sp+108), r26
	sw      (sp+112), r27
	addi     r7, sp, 128 /* we could store usp here */
	sw      (sp+116), r7
	/* ra (sp + 120) has already been written */
	sw      (sp+124), ea
	sw      (sp+128), ba
	ret

/************************/
/* syscall return paths */
/************************/


#ifdef LM32_BREAK_ON_SYSCALL
#define RETURN_FROM_SYSCALL_OR_EXCEPTION_BREAK \
	mvi ra, LM32_BREAK_ON_SYSCALL; /* only break in this syscall no */ \
	bne ra, r8, 1f; \
	break; \
1:
#else
#define RETURN_FROM_SYSCALL_OR_EXCEPTION_BREAK
#endif

/* Restore all registers from syscall */
/* all interrupts are disabled upon entry */
/* we are on the kernel stack upon entry */

#define RETURN_FROM_SYSCALL_OR_EXCEPTION(label, addr_register, return_instr) \
label: \
	/* prepare switch to user stack but keep kernel stack pointer in r11 */ \
	/* r9: scratch register */ \
	/* r10: current = current_thread_info()->task */ \
	/* r11: ksp backup */ \
	/* setup r10 = current */ \
	mvhi r9, hi(lm32_current_thread); \
	ori r9, r9, lo(lm32_current_thread); \
	lw r9, (r9+0); /* dereference lm32_current_thread */ \
	lw r10, (r9+TI_TASK); /* load pointer to task */ \
	/* set task->thread.which_stack to 1 (user stack) */ \
	mvi r9, 1; \
	sw (r10+TASK_WHICH_STACK), r9; \
	/* store ksp (after restore of frame) into task->thread.ksp */ \
	addi r9, sp, 128; \
	sw (r10+TASK_KSP), r9; \
	/* save sp into r11 */ \
	mv r11, sp; \
	/* get usp into sp*/ \
	lw  sp, (r10+TASK_USP); \
	/* restore frame from original kernel stack */ \
	/* restore r1 as the return value is stored onto the stack */ \
	lw      r1,  (r11+8); \
	lw      r2,  (r11+12); \
	lw      r3,  (r11+16); \
	lw      r4,  (r11+20); \
	lw      r5,  (r11+24); \
	lw      r6,  (r11+28); \
	lw      r7,  (r11+32); \
	lw      r8,  (r11+36); \
	lw      r9,  (r11+40); \
	lw      r10, (r11+44); \
	/* skip r11 */; \
	lw      r12, (r11+52); \
	lw      r13, (r11+56); \
	lw      r14, (r11+60); \
	lw      r15, (r11+64); \
	lw      r16, (r11+68); \
	lw      r17, (r11+72); \
	lw      r18, (r11+76); \
	lw      r19, (r11+80); \
	lw      r20, (r11+84); \
	lw      r21, (r11+88); \
	lw      r22, (r11+92); \
	lw      r23, (r11+96); \
	lw      r24, (r11+100); \
	lw      r25, (r11+104); \
	lw      r26, (r11+108); \
	lw      r27, (r11+112); \
	/* skip sp as it was retrieved from TASK_USP */ \
	RETURN_FROM_SYSCALL_OR_EXCEPTION_BREAK \
	lw      ra,  (r11+120); \
	lw      ea,  (r11+124); \
	lw      ba,  (r11+128); \
	/* r11 must be restored last */ \
	lw      r11,  (r11+48); \
	/* scall stores pc into ea/ba register, not pc+4, so we have to add 4 */ \
	addi	addr_register, addr_register, 4; \
	return_instr

RETURN_FROM_SYSCALL_OR_EXCEPTION(_restore_and_return_exception,ea,eret)

/* also use "ea" here because "ba" should not be changed! */
RETURN_FROM_SYSCALL_OR_EXCEPTION(_restore_and_return_debug_exception,ea,bret)

/*
 * struct task_struct* resume(struct task_struct* prev, struct task_struct* next)
 * Returns the previous task
 */
ENTRY(resume)
	/* store whole state to current stack (may be usp or ksp) */
	addi sp, sp, -128
	sw  (sp+4),   r0
	sw  (sp+8),   r1
	sw  (sp+12),  r2
	sw  (sp+16),  r3
	sw  (sp+20),  r4
	sw  (sp+24),  r5
	sw  (sp+28),  r6
	sw  (sp+32),  r7
	sw  (sp+36),  r8
	sw  (sp+40),  r9
	sw  (sp+44),  r10
	sw  (sp+48),  r11
	sw  (sp+52),  r12
	sw  (sp+56),  r13
	sw  (sp+60),  r14
	sw  (sp+64),  r15
	sw  (sp+68),  r16
	sw  (sp+72),  r17
	sw  (sp+76),  r18
	sw  (sp+80),  r19
	sw  (sp+84),  r20
	sw  (sp+88),  r21
	sw  (sp+92),  r22
	sw  (sp+96),  r23
	sw  (sp+100), r24
	sw  (sp+104), r25
	sw  (sp+108), r26
	sw  (sp+112), r27
	addi r3, sp, 128 /* special case for stack pointer */
	sw  (sp+116), r3 /* special case for stack pointer */
	sw	(sp+120), ra
//	sw  (sp+124), ea
//	sw  (sp+128), ba

	/* find out whether we are on kernel or user stack */
	lw  r3, (r1 + TASK_WHICH_STACK)
	be  r3, r0, 1f

	/* we are on user stack */
	sw  (r1 + TASK_USP), sp
	bi 2f

1:/* we are on kernel stack */
	sw  (r1 + TASK_KSP), sp

2:/* we have stored stack pointer of prev */

  /* restore next */

	/* find out whether we will be on kernel or user stack */
	lw  r3, (r2 + TASK_WHICH_STACK)
	be  r3, r0, 3f

	/* we need user stack */
	lw  sp, (r2 + TASK_USP)
	bi 4f

3:/* we need kernel stack */
	lw  sp, (r2 + TASK_KSP)

4:/* we have restored sp of next */

	/* setup return value */
	mv	r1, r2

	lw  r2,  (sp+12)
	lw  r3,  (sp+16)
	lw  r4,  (sp+20)
	lw  r5,  (sp+24)
	lw  r6,  (sp+28)
	lw  r7,  (sp+32)
	lw  r8,  (sp+36)
	lw  r9,  (sp+40)
	lw  r10, (sp+44)
	lw  r11, (sp+48)
	lw  r12, (sp+52)
	lw  r13, (sp+56)
	lw  r14, (sp+60)
	lw  r15, (sp+64)
	lw  r16, (sp+68)
	lw  r17, (sp+72)
	lw  r18, (sp+76)
	lw  r19, (sp+80)
	lw  r20, (sp+84)
	lw  r21, (sp+88)
	lw  r22, (sp+92)
	lw  r23, (sp+96)
	lw  r24, (sp+100)
	lw  r25, (sp+104)
	lw  r26, (sp+108)
	lw  r27, (sp+112)
	/* skip sp for now */
	lw  ra,  (sp+120)
//	lw  ea,  (sp+124)
//	lw  ba,  (sp+128)
	/* Stack pointer must be restored last --- it will be updated */
	lw  sp,  (sp+116)

	ret

/* extern asmlinkage void break_label(void); */
ENTRY(break_label)
	break

.align 4
ENTRY(_sys_call_table)
	.long sys_ni_syscall	/* 0  -  old "setup()" system call*/
	.long sys_exit
	.long sys_fork
	.long sys_read
	.long sys_write
	.long sys_open		/* 5 */
	.long sys_close
	.long sys_ni_syscall	/* old waitpid */
	.long sys_creat
	.long sys_link
	.long sys_unlink	/* 10 */
	.long sys_execve_wrapper
	.long sys_chdir
	.long sys_time
	.long sys_mknod
	.long sys_chmod		/* 15 */
	.long sys_chown	/* chown16 */
	.long sys_ni_syscall	/* old break syscall holder */
	.long sys_ni_syscall	/* old stat */
	.long sys_lseek
	.long sys_getpid	/* 20 */
	.long sys_mount
	.long sys_ni_syscall	/* old umount */
	.long sys_setuid
	.long sys_getuid
	.long sys_stime		/* 25 */
	.long sys_ptrace
	.long sys_alarm
	.long sys_ni_syscall	/* old fstat */
	.long sys_pause
	.long sys_ni_syscall	/* old utime */ /* 30 */
	.long sys_ni_syscall	/* old stty syscall holder */
	.long sys_ni_syscall	/* old gtty syscall holder */
	.long sys_access
	.long sys_nice
	.long sys_ni_syscall	/* 35 */ /* old ftime syscall holder */
	.long sys_sync
	.long sys_kill
	.long sys_rename
	.long sys_mkdir
	.long sys_rmdir		/* 40 */
	.long sys_dup
	.long sys_pipe
	.long sys_times
	.long sys_ni_syscall	/* old prof syscall holder */
	.long sys_brk		/* 45 */
	.long sys_setgid
	.long sys_getgid
	.long sys_signal
	.long sys_geteuid	/* geteuid16 */
	.long sys_getegid	/* getegid16 */	/* 50 */
	.long sys_ni_syscall /* sys_acct */
	.long sys_umount	/* recycled never used phys() */
	.long sys_ni_syscall	/* old lock syscall holder */
	.long sys_ioctl
	.long sys_fcntl		/* 55 */
	.long sys_ni_syscall	/* old mpx syscall holder */
	.long sys_setpgid
	.long sys_ni_syscall	/* old ulimit syscall holder */
	.long sys_ni_syscall	/* old old uname */
	.long sys_umask		/* 60 */
	.long sys_chroot
	.long sys_ustat
	.long sys_dup2
	.long sys_getppid
	.long sys_getpgrp	/* 65 */
	.long sys_setsid
	.long sys_sigaction
	.long sys_sgetmask
	.long sys_ssetmask
	.long sys_setreuid	/* setreuid16 */	/* 70 */
	.long sys_setregid	/* setregid16 */
	.long sys_ni_syscall /* old sys_sigsuspend */
	.long sys_sigpending
	.long sys_sethostname
	.long sys_setrlimit	/* 75 */
	.long sys_ni_syscall	/* old getrlimit */
	.long sys_getrusage
	.long sys_gettimeofday
	.long sys_settimeofday
	.long sys_getgroups	/* getgroups16 */	/* 80 */
	.long sys_setgroups	/* setgroups16 */
	.long sys_select	/* backwards compatibility */
	.long sys_symlink
	.long sys_ni_syscall	/* old lstat */
	.long sys_readlink	/* 85 */
	.long sys_uselib
	.long sys_ni_syscall	/* sys_swapon */
	.long sys_reboot
	.long sys_ni_syscall	/* old_readdir */
	.long sys_ni_syscall	/* sys_mmap */	/* 90 */
	.long sys_munmap
	.long sys_truncate
	.long sys_ftruncate
	.long sys_fchmod
	.long sys_fchown	/* fchown16 */	/* 95 */
	.long sys_getpriority
	.long sys_setpriority
	.long sys_ni_syscall	/* old profil syscall holder */
	.long sys_statfs
	.long sys_fstatfs	/* 100 */
	.long sys_ni_syscall
	.long sys_ni_syscall	/* old sys_socketcall */
	.long sys_syslog
	.long sys_setitimer
	.long sys_getitimer	/* 105 */
	.long sys_newstat
	.long sys_newlstat
	.long sys_newfstat
	.long sys_ni_syscall	/* old uname */
	.long sys_ni_syscall	/* iopl for i386 */ /* 110 */
	.long sys_vhangup
	.long sys_ni_syscall	/* obsolete idle() syscall */
	.long sys_ni_syscall	/* vm86old for i386 */
	.long sys_wait4
	.long sys_ni_syscall	/* 115 */ /* sys_swapoff */
	.long sys_sysinfo
	.long sys_ni_syscall	/* old sys_ipc */
	.long sys_fsync
	.long sys_sigreturn_wrapper
	.long sys_clone_wrapper		/* 120 */
	.long sys_setdomainname
	.long sys_newuname
	.long sys_ni_syscall	/* old sys_modify_ldt */
	.long sys_adjtimex
	.long sys_ni_syscall	/* 125 */ /* sys_mprotect */
	.long sys_ni_syscall	/* old sys_sigprocmask */
	.long sys_ni_syscall	/* old "creat_module" */
	.long sys_init_module
	.long sys_delete_module
	.long sys_ni_syscall	/* 130: old "get_kernel_syms" */
	.long sys_ni_syscall /* sys_quotactl */
	.long sys_getpgid
	.long sys_fchdir
	.long sys_bdflush
	.long sys_ni_syscall	/* 135 */ /* sys_sysfs */
	.long sys_personality
	.long sys_ni_syscall	/* for afs_syscall */
	.long sys_setfsuid	/* setfsuid16 */
	.long sys_setfsgid	/* setfsgid16 */
	.long sys_llseek	/* 140 */
	.long sys_getdents
	.long sys_select	/* backwards compatibility */
	.long sys_flock
	.long sys_ni_syscall	/* sys_msync */
	.long sys_readv		/* 145 */
	.long sys_writev
	.long sys_getsid
	.long sys_fdatasync
	.long sys_sysctl
	.long sys_ni_syscall	/* 150 */ /* sys_mlock */
	.long sys_ni_syscall	/* sys_munlock */
	.long sys_ni_syscall	/* sys_mlockall */
	.long sys_ni_syscall	/* sys_munlockall */
	.long sys_sched_setparam
	.long sys_sched_getparam /* 155 */
	.long sys_sched_setscheduler
	.long sys_sched_getscheduler
	.long sys_sched_yield
	.long sys_sched_get_priority_max
	.long sys_sched_get_priority_min  /* 160 */
	.long sys_sched_rr_get_interval
	.long sys_nanosleep
	.long sys_ni_syscall	/* sys_mremap */
	.long sys_setresuid	/* setresuid16 */
	.long sys_getresuid	/* getresuid16 */	/* 165 */
	.long sys_ni_syscall	/* for vm86 */
	.long sys_ni_syscall	/* old "query_module" */
	.long sys_ni_syscall	/* sys_poll */
	.long sys_ni_syscall	/* sys_nfsservctl */
	.long sys_setresgid	/* setresgid16 */	/* 170 */
	.long sys_getresgid	/* getresgid16 */
	.long sys_prctl
	.long sys_ni_syscall /* sys_rt_sigreturn */
	.long sys_rt_sigaction
	.long sys_rt_sigprocmask /* 175 */
	.long sys_rt_sigpending
	.long sys_rt_sigtimedwait
	.long sys_rt_sigqueueinfo
	.long sys_rt_sigsuspend_wrapper
	.long sys_pread64	/* 180 */
	.long sys_pwrite64
	.long sys_lchown	/* lchown16 */
	.long sys_getcwd
	.long sys_capget
	.long sys_capset	/* 185 */
	.long sys_sigaltstack
	.long sys_sendfile
	.long sys_ni_syscall	/* streams1 */
	.long sys_ni_syscall	/* streams2 */
	.long sys_vfork_wrapper		/* 190 */
	.long sys_getrlimit
  .long sys_mmap2
	.long sys_truncate64
	.long sys_ftruncate64
	.long sys_stat64	/* 195 */
	.long sys_lstat64
	.long sys_fstat64
	.long sys_chown
	.long sys_getuid
	.long sys_getgid	/* 200 */
	.long sys_geteuid
	.long sys_getegid
	.long sys_setreuid
	.long sys_setregid
	.long sys_getgroups	/* 205 */
	.long sys_setgroups
	.long sys_fchown
	.long sys_setresuid
	.long sys_getresuid
	.long sys_setresgid	/* 210 */
	.long sys_getresgid
	.long sys_lchown
	.long sys_setuid
	.long sys_setgid
	.long sys_setfsuid	/* 215 */
	.long sys_setfsgid
	.long sys_pivot_root
	.long sys_ni_syscall	/* sys_mincore */
	.long sys_ni_syscall	/* sys_madvise */
	.long sys_getdents64	/* 220 */
	.long sys_fcntl64
	.long sys_ni_syscall	/* reserved for TUX */
	.long sys_ni_syscall
	.long sys_gettid
	.long sys_ni_syscall	/* 225 */ /* sys_readahead */
	.long sys_setxattr
	.long sys_lsetxattr
	.long sys_fsetxattr
	.long sys_getxattr
	.long sys_lgetxattr	/* 230 */
	.long sys_fgetxattr
	.long sys_listxattr
	.long sys_llistxattr
	.long sys_flistxattr
	.long sys_removexattr	/* 235 */
	.long sys_lremovexattr
	.long sys_fremovexattr
	.long sys_tkill
	.long sys_sendfile64
	.long sys_futex		/* 240 */
	.long sys_sched_setaffinity
	.long sys_sched_getaffinity
	.long sys_ni_syscall	/* sys_set_thread_area */
	.long sys_ni_syscall	/* sys_get_thread_area */
	.long sys_io_setup	/* 245 */
	.long sys_io_destroy
	.long sys_io_getevents
	.long sys_io_submit
	.long sys_io_cancel
	.long sys_ni_syscall	/* 250 */ /* sys_alloc_hugepages */
	.long sys_ni_syscall	/* sys_freec_hugepages */
	.long sys_exit_group
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_ni_syscall /* 255 */
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_ni_syscall /* remap_file_pages */
	.long sys_set_tid_address
	.long sys_timer_create	/* 260 */
	.long sys_timer_settime
	.long sys_timer_gettime
	.long sys_timer_getoverrun
	.long sys_timer_delete
	.long sys_clock_settime /* 265 */
	.long sys_clock_gettime
	.long sys_clock_getres
	.long sys_clock_nanosleep
	.long sys_statfs64
	.long sys_fstatfs64	/* 270 */
	.long sys_tgkill
	.long sys_utimes
	.long sys_fadvise64_64
	.long sys_ni_syscall /* vserver */
	.long sys_ni_syscall /* 275, mbind */
	.long sys_ni_syscall /* get_mempolicy */
	.long sys_ni_syscall /* set_mempolicy */
	.long sys_ni_syscall /* mq */
	.long sys_ni_syscall /* mq */
	.long sys_ni_syscall /* 280 */ /* mq */
	.long sys_ni_syscall /* mq */
	.long sys_ni_syscall /* mq */
	.long sys_ni_syscall /* mq */
	.long sys_ni_syscall /* kexec_load */
	.long sys_waitid	/* 285 */
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_ioprio_set
	.long sys_ioprio_get	/* 290 */
	.long sys_ni_syscall /* inotify */
	.long sys_ni_syscall /* inotify */
	.long sys_ni_syscall /* inotify */
	.long sys_ni_syscall /* migrate_pages */
	.long sys_openat	/* 295 */
	.long sys_mkdirat
	.long sys_mknodat
	.long sys_fchownat
	.long sys_futimesat
	.long sys_fstatat64	/* 300 */
	.long sys_unlinkat
	.long sys_renameat
	.long sys_linkat
	.long sys_symlinkat
	.long sys_readlinkat	/* 305 */
	.long sys_fchmodat
	.long sys_faccessat
	.long sys_ni_syscall /* sys_pselect6 */
	.long sys_ni_syscall /* sys_ppoll */
	.long sys_unshare	/* 310 */
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_ni_syscall
	.long sys_accept
	.long sys_bind		/* 315 */
	.long sys_connect
	.long sys_getpeername
	.long sys_getsockname
	.long sys_getsockopt
	.long sys_listen	/* 320 */
	.long sys_recv
	.long sys_recvfrom
	.long sys_recvmsg
	.long sys_send
	.long sys_sendmsg	/* 325 */
	.long sys_sendto
	.long sys_setsockopt
	.long sys_shutdown
	.long sys_socket
	.long sys_socketpair	/* 330 */
	.long sys_splice
	.long sys_sync_file_range2
	.long sys_tee
	.long sys_vmsplice
	.long sys_getcpu  /* 335 */
	.rept NR_syscalls-(.-_sys_call_table)/4
	.long sys_ni_syscall
	.endr
