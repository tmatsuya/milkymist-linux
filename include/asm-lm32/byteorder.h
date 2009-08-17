#ifndef _LM32_ASM_BYTEORDER_H
#define _LM32_ASM_BYTEORDER_H

#include <asm/types.h>

#if defined(__GNUC__) && !defined(__STRICT_ANSI__) || defined(__KERNEL__)
// there is a 64bit data type supported by the processor
#  define __BYTEORDER_HAS_U64__

// so we don't have to implement swab64 in assembler ;) 
#  define __SWAB_64_THRU_32__
#endif

#include <linux/byteorder/big_endian.h>

#endif /* _LM32_BYTEORDER_H */
