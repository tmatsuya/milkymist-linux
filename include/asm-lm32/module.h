/*
 * Based on:
 * include/asm-arm/module.h
 */

#ifndef _LM32_ASM_MODULE_H
#define _LM32_ASM_MODULE_H

struct mod_arch_specific
{
	int foo;
};

#define Elf_Shdr	Elf32_Shdr
#define Elf_Sym		Elf32_Sym
#define Elf_Ehdr	Elf32_Ehdr

#define MODULE_ARCH_VERMAGIC	"LM32v1"

#endif /* _LM32_ASM_MODULE_H */
