#
# arch/lm32/kernel/Makefile
#

extra-y		:= head.o init_task.o vmlinux.lds

obj-y 		+= semaphore.o sys_lm32.o setup.o traps.o signal.o \
		   time.o ptrace.o irq.o process.o entry.o 
obj-$(CONFIG_EARLY_PRINTK) += early_printk.o 

obj-$(CONFIG_MODULES)	+= module.o
