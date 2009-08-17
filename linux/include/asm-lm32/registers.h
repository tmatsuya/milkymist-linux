
/* registers.h: register frame declarations
 *
 * Copyright (C) 2003 Red Hat, Inc. All Rights Reserved.
 * Written by David Howells (dhowells@redhat.com)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef _ASM_REGISTERS_H
#define _ASM_REGISTERS_H

#ifndef __ASSEMBLY__

/* this struct defines the way the registers are stored on the
   stack during a system call. */
struct pt_regs {
  long     r0;
  long     r1;
  long     r2;
  long     r3;
  long     r4;
  long     r5;
  long     r6;
  long     r7;
  long     r8;
  long     r9;
  long     r10;
  long     r11;
  long     r12;
  long     r13;
  long     r14;
  long     r15;
  long     r16;
  long     r17;
  long     r18;
  long     r19;
  long     r20;
  long     r21;
  long     r22;
  long     r23;
  long     r24;
  long     r25;
  long     gp;
  long     fp;
  long     sp;
  long     ra;
  long     ea;
  long     ba;
};

/* this defines the registers stored during an interrupt */
struct int_regs {
  long     r1;
  long     r2;
  long     r3;
  long     r4;
  long     r5;
  long     r6;
  long     r7;
  long     r8;
  long     r9;
  long     r10;
  long     ra;
  long     ea;
};

/* no fp_regs but the kernel likes to have them */
struct fp_regs
{
};

struct user_context
{
	struct pt_regs regs;
	struct fp_regs fpregs;
} __attribute__((aligned(4)));

#endif /* __ASSEMBLY__ */

#endif
