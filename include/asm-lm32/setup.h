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

#ifndef _LM32_ASM_SETUP_H
#define _LM32_ASM_SETUP_H

#ifndef __ASSEMBLY__

#ifdef __KERNEL__

extern unsigned int cpu_frequency;
extern unsigned int sdram_start;
extern unsigned int sdram_size;
extern struct platform_device* milkymistuart_default_console_device;

#endif /* __KERNEL__ */
#endif /* #ifndef __ASSEMBLY__ */

#define COMMAND_LINE_SIZE 256

#endif
