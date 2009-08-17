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
#include <asm/hwsetup_kernel.h>

/* The maximum amount of structs expected in the 
 * hardware setup data for each type of struct. */
#define LM32MAX_HWSETUP_ITEMS	32

/*
 * Each line creates:
 * extern unsigned long lm32tag_num_##name;
 * extern LM32Tag_##type##_t* lm32tag_##name[];
 */
#define DECLARE_CONFIG_ITEM(type,name) \
	extern unsigned long lm32tag_num_##name; \
	extern LM32Tag_##type##_t* lm32tag_##name[];
DECLARE_CONFIG_ITEM(CPU, cpu)
DECLARE_CONFIG_ITEM(ASRAM, asram)
DECLARE_CONFIG_ITEM(Flash, flash)
DECLARE_CONFIG_ITEM(SDRAM, sdram)
DECLARE_CONFIG_ITEM(OCM, ocm)
DECLARE_CONFIG_ITEM(DDR_SDRAM, ddr_sdram)
DECLARE_CONFIG_ITEM(DDR2_SDRAM, ddr2_sdram)
DECLARE_CONFIG_ITEM(Timer, timer)
DECLARE_CONFIG_ITEM(UART, uart)
DECLARE_CONFIG_ITEM(GPIO, gpio)
DECLARE_CONFIG_ITEM(TriSpeedMAC, trispeedmac)
DECLARE_CONFIG_ITEM(I2CM, i2cm)
DECLARE_CONFIG_ITEM(LEDS, leds)
DECLARE_CONFIG_ITEM(7SEG, 7seg)
#undef DECLARE_CONFIG_ITEM

struct platform_device;
extern struct platform_device* lm32uart_default_console_device;

#endif /* __KERNEL__ */
#endif /* #ifndef __ASSEMBLY__ */

#define COMMAND_LINE_SIZE 256

#endif
