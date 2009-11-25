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

#ifndef _LM32_ASM_ELF_H
#define _LM32_ASM_ELF_H

#include <asm/registers.h>
#include <asm/user.h>

/*
 * ELF register definitions..
 */


/* Processor specific flags for the ELF header e_flags field.  */
#define EF_LM32_PIC		0x00000001	/* TODO -fpic */
#define EF_LM32_FDPIC		0x00000002	/* TODO -mfdpic or -G */

#define ELF_FDPIC_CORE_EFLAGS EF_LM32_FDPIC

/*
 * ELF relocation types
 */
#define R_LM32_NONE                      0
#define R_LM32_8                         1
#define R_LM32_16                        2
#define R_LM32_32                        3
#define R_LM32_HI16                      4
#define R_LM32_LO16                      5
#define R_LM32_GPREL16                   6
#define R_LM32_CALL                      7
#define R_LM32_BRANCH                    8
#define R_LM32_GNU_VTINHERIT             9
#define R_LM32_GNU_VTENTRY               10

typedef unsigned long elf_greg_t;

#define ELF_NGREG (sizeof(struct pt_regs) / sizeof(elf_greg_t))
typedef elf_greg_t elf_gregset_t[ELF_NGREG];

typedef struct fp_regs elf_fpregset_t;

/*
 * This is used to ensure we don't load something for the wrong architecture.
 */
#define elf_check_arch(x) ((x)->e_machine == EM_LM32)
#define elf_check_fdpic(x) (1)
#define elf_check_const_displacement(x) (1)

/*
 * These are used to set parameters in the core dumps.
 */
#define ELF_CLASS	ELFCLASS32
#define ELF_DATA	ELFDATA2MSB
#define ELF_ARCH	EM_LM32
#define EM_LM32 0x666 // FIXME

#define ELF_PLAT_INIT(_r, load_addr)	do { } while(0)

#define ELF_FDPIC_PLAT_INIT(_regs, _exec_map_addr, _interp_map_addr, _dynamic_addr)	\
do { \
	_regs->r6 = 0; /* 6th argument to uclibc start: rtld_fini = NULL */ \
	_regs->r11	= _exec_map_addr;				\
	_regs->r12	= _interp_map_addr;				\
	_regs->r13	= _dynamic_addr;				\
} while(0)

#define USE_ELF_CORE_DUMP
#define ELF_EXEC_PAGESIZE	4096

/* This is the location that an ET_DYN program is loaded if exec'ed.  Typical
   use of this is to invoke "./ld.so someprog" to test out a new version of
   the loader.  We need to make sure that it is out of the way of the program
   that it will "exec", and that there is sufficient room for the brk.  */

// TODO: change this value as soon as we use ET_DYN
#define ELF_ET_DYN_BASE         0xD0000000UL

/* This yields a mask that user programs can use to figure out what
   instruction set this cpu supports.  */

#define ELF_HWCAP	(0)

/* This yields a string that ld.so will use to load implementation
   specific libraries for optimization.  This is more specific in
   intent than poking at uname or /proc/cpuinfo.  */

#define ELF_PLATFORM  (NULL)

// TODO SET_PERSONALITY
//#ifdef __KERNEL__
//#define SET_PERSONALITY(ex, ibcs2) set_personality((ibcs2)?PER_SVR4:PER_LINUX)
//#endif

#endif
