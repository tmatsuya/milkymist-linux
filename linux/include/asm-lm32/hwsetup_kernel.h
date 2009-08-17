/*
 * (C) Copyright 2007
 *     Theobroma Systems  <www.theobroma-systems.com>
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
#ifndef _ASM_LM32_HARDWARESETUP_KERNEL_H
#define _ASM_LM32_HARDWARESETUP_KERNEL_H

#include <asm-lm32/hwsetup_payload.h>

/*
 * This file defines the LM32 Hardware Setup structs to be passed to the Linux
 * kernel. The payload (= configuration info) is defined in
 * asm-lm32/hwsetup_payload.h.
 */

/*
 * LM32 Hardware Setup Tags
 *
 * These tags are used to identify the configuration structs from
 * hwsetup_payload.h to pass them to the Linux kernel.
 */
enum
{
	/* end marker, indicates end of list */
	LM32TAG_EOL = 0x0,
	/* CPU, the struct type is LM32TAG_CPU_t */
	LM32TAG_CPU = 0x1,
	/* Asynchronous SRAM controller, the struct type is LM32TAG_ASRAM_t */
	LM32TAG_ASRAM = 0x2,
	/* Parallel Flash controller, the struct type is LM32TAG_Flash_t */
	LM32TAG_FLASH = 0x3,
	/* SDRAM controller, the struct type is LM32TAG_SDRAM_t */
	LM32TAG_SDRAM = 0x4,
	/* On-Chip memory, the struct type is LM32TAG_OCM_t */
	LM32TAG_OCM = 0x5,
	/* DDR SDRAM controller, the struct type is LM32TAG_DDR_SDRAM_t */
	LM32TAG_DDR_SDRAM = 0x6,
	/* DDR2 SDRAM controller, the struct type is LM32TAG_DDR2_SDRAM_t */
	LM32TAG_DDR2_SDRAM = 0x7,
	/* Timer, the struct type is LM32TAG_Timer_t */
	LM32TAG_TIMER = 0x8,
	/* UART, the struct type is LM32TAG_UART_t */
	LM32TAG_UART = 0x9,
	/* GPIO controller, the struct type is LM32TAG_GPIO_t */
	LM32TAG_GPIO = 0xA,
	/* Tri-Speed Ethernet MAC, the struct type is LM32TAG_TriSpeedMAC_t */
	LM32TAG_TRISPEEDMAC = 0xB,
	/* I2CM (I2C Master) controller, the struct type is LM32TAG_I2CM_t */
	LM32TAG_I2CM = 0xC,
	/* GPIO with leds attached, the struct type is LM32TAG_LEDS_t */
	LM32TAG_LEDS = 0xD,
	/* GPIO with 7 segment display attached, the struct type is LM32TAG_7SEG_t */
	LM32TAG_7SEG = 0xE,
	/* SPI slave, the struct type is LM32TAG_SPIS_t */
	LM32TAG_SPI_S = 0xF,
	/* SPI master, the struct type is LM32TAG_SPIM_t */
	LM32TAG_SPI_M = 0x10,
};

/*
 * LM32 Hardware Setup information header
 * This struct starts one item in the list of information passed from U-Boot to
 * the kernel about the underlying hardware. After each header one of the
 * payload struct from hwsetup_payload.h follows.
 */
typedef struct LM32Tag_Header
{
	/* Size of the information struct, including this whole struct */
	u32 size;
	/* Type of the information struct, one of the LM32TAG_... values */
	u32 tag;
} LM32Tag_Header_t;

/*
 * Define all wrapper structs around the payload structs.
 *
 * Each line creates a struct LM32TAG_##type and a type LM32TAG_##type##_t.
 *
 * Each struct contains the header and the payload.
 * The size will be set to sizeof(LM32Tag_..._t) and is used for list navigation.
 * The tag will be set to LM32TAG_... and is used to detect the type of struct.
 */
#define DECLARE_LIST_ENTRY_STRUCT(type) \
	typedef struct LM32TagWrap_##type { \
	u32 size; \
	u32 tag; \
	LM32Tag_##type##_t d; \
} LM32TagWrap_##type##_t;

DECLARE_LIST_ENTRY_STRUCT(CPU)
DECLARE_LIST_ENTRY_STRUCT(ASRAM)
DECLARE_LIST_ENTRY_STRUCT(Flash)
DECLARE_LIST_ENTRY_STRUCT(SDRAM)
DECLARE_LIST_ENTRY_STRUCT(OCM)
DECLARE_LIST_ENTRY_STRUCT(DDR_SDRAM)
DECLARE_LIST_ENTRY_STRUCT(DDR2_SDRAM)
DECLARE_LIST_ENTRY_STRUCT(Timer)
DECLARE_LIST_ENTRY_STRUCT(UART)
DECLARE_LIST_ENTRY_STRUCT(GPIO)
DECLARE_LIST_ENTRY_STRUCT(LEDS)
DECLARE_LIST_ENTRY_STRUCT(7SEG)
DECLARE_LIST_ENTRY_STRUCT(TriSpeedMAC)
DECLARE_LIST_ENTRY_STRUCT(I2CM)
DECLARE_LIST_ENTRY_STRUCT(SPI_S)
DECLARE_LIST_ENTRY_STRUCT(SPI_M)

/* cleanup preprocessor namespace */
#undef DECLARE_LIST_ENTRY_STRUCT

#endif
