/*
 *  linux/arch/arm/kernel/head.S
 *
 *  Copyright (c) 2007 Theobroma Systems.
 *  All Rights Reserved
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  Kernel startup code for the Lattice Mico 32 architecture
 */
#include <linux/linkage.h>
#include <linux/init.h>
#include <linux/threads.h>

#include <asm/ptrace.h>
#include <asm/asm-offsets.h>
#include <asm/thread_info.h>
#include <asm/system.h>

/*
 * Kernel startup entry point.
 * ---------------------------
 *
 * This is normally called from the decompressor code or boot-loader.
 * The requirements are: 
 *   r1 = hardware parameters (list of structs from hwsetup_kernel.h)
 *   r2 = kernel commandline (zero terminated)
 *   r3 = initrd start
 *   r4 = initrd end
 *
 * We currently clobber r23, r24 and r25
 *
 * Note: This code is not (yet) position-independent (position
 * 	 independence will require some additional magic to
 *       correct the __bss_start, _end and _init_thread_union
 *       addresses).
 */

	.section ".text.head"
	.type stext, %function
ENTRY(stext)
	xor	r0, r0, r0	/* make sure that r0 is set to 0. */
	wcsr IE, r0 /* disable interrupts */
	wcsr IM, r0 /* disable each maskable interrupt */
	calli	setup_stack
	calli	clear_bss
	calli save_args
	calli	setup_exception_vectors
	calli	start_kernel
	/* should never get here */
hang:
	be	r0, r0, hang		/* how did we get here? */

save_args:
	/* TODO: make this PIC */
	mvhi	r23, hi(_kernel_arg_cmdline)
	ori	r23, r23, lo(_kernel_arg_cmdline)
	sw (r23+0), r1 /* store commandline parameter into _kernel_arg_cmdline */
	/* store initrd parameters */
	mvhi	r23, hi(_kernel_arg_initrd_start)
	ori	r23, r23, lo(_kernel_arg_initrd_start)
	sw (r23+0), r2
	mvhi	r23, hi(_kernel_arg_initrd_end)
	ori	r23, r23, lo(_kernel_arg_initrd_end)
	sw (r23+0), r3
	ret
	
setup_exception_vectors:
	/* TODO: implement me */
	/* TODO: irq handler has to call asm_do_IRQ */
	/* activate watchpoint on write to address 0 */
	wcsr WP0, r0
	rcsr r23, DC
	ori r23, r23, 0x8
	/*wcsr DC, r23*/
	/*mvhi r23, 0x1000*/
	/*wcsr DEBA, r23*/
	ret

clear_bss:
	addi	sp, sp, -8
	mv	r25, ra
	calli	clear_bss_1

	.global	__bss_start
	.word	__bss_start
	.global	_end
	.word	_end
clear_bss_1:
	lw	r23, (ra + 0)   	/* load _bss_start */
	lw	r24, (ra + 4)		/* load _end */
	/* TODO: correct the BSS addresses for position independence */
clear_bss_2:					
	sw	(r23 + 0), r0
	addi	r23, r23, 4
	bgu	r24, r23, clear_bss_2

	mv	ra, r25
	ret

setup_stack:
	/* TODO: make this PIC */
	mvhi	r23, hi(init_thread_union)
	ori	r23, r23, lo(init_thread_union)
	mvi	sp, THREAD_SIZE - 32
	add	sp, sp, r23
	/* init the stack pointer */
	addi	sp, sp, -32 /* SZREGS */
	ret
