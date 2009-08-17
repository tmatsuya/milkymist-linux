#ifndef _LM32_ASM_BITOPS_H
#define _LM32_ASM_BITOPS_H

/* local_irq_save_hw and local_irq_restore_hw */
#include <asm/system.h>

#define smp_mb__before_clear_bit()	barrier()
#define smp_mb__after_clear_bit()	barrier()

#include <asm-generic/bitops.h>

#endif /* _LM32_BITOPS_H */
