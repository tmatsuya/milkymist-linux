#ifndef _LM32_ASM_PARAM_H
#define _LM32_ASM_PARAM_H 

#define CONFIG_SPLIT_PTLOCK_CPUS 4096

#ifdef __KERNEL__
#define HZ		CONFIG_HZ	/* Internal kernel timer frequency */
#define	USER_HZ		HZ
#define	CLOCKS_PER_SEC	(USER_HZ)
#endif

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */

#endif /* _LM32_PARAM_H */
