/*
 * (C) Copyright 2007
 *     Theobroma Systems <www.theobroma-systems.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef _LM32_ASM_PTRACE_H
#define _LM32_ASM_PTRACE_H

#include <asm/registers.h>

/* Arbitrarily choose the same ptrace numbers as used by the Sparc code. */
#define PTRACE_GETREGS            12
#define PTRACE_SETREGS            13

/* for gdb */
#define PT_TEXT_ADDR	50
#define PT_TEXT_END_ADDR	51
#define PT_DATA_ADDR	52

#ifdef __KERNEL__
#ifndef __ASSEMBLY__

#define user_mode(regs) (get_fs() == USER_DS)
#define instruction_pointer(regs) ((regs)->ea)
#define profile_pc(regs) instruction_pointer(regs)
extern void show_regs(struct pt_regs *);

#endif
#endif

#endif /* _LM32_PTRACE_H */
