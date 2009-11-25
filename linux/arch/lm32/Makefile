#
# arch/lm32/Makefile
#
# (C) Copyright 2007, Theobroma Systems <www.theobroma-systems.com>
#
# See file CREDITS for list of people who contributed to this
# project.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation; either version 2 of
# the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston,
# MA 02111-1307 USA
#

board-$(CONFIG_BOARD_XILINX_ML401)	:= ml401
board-$(CONFIG_BOARD_MILKYMIST_ONE)	:= milkymist-one
BOARD := $(board-y)

export BOARD

cflags-y := -mmultiply-enabled -mdivide-enabled \
	-mbarrel-shift-enabled -msign-extend-enabled

AFLAGS += $(cflags-y)

OBJCOPYFLAGS	:=-O binary -R .note -R .comment -S

CFLAGS += $(cflags-y)
CFLAGS += -D__linux__
CFLAGS += -DUTS_SYSNAME=\"uClinux\"

head-y := arch/lm32/kernel/head.o arch/lm32/kernel/init_task.o

core-y	+= arch/lm32/kernel/ arch/lm32/mm/
libs-y	+= arch/lm32/lib/

vmlinux.bin: vmlinux
	$(OBJCOPY) $(OBJCOPYFLAGS) $< $@

vmlinux.gz: vmlinux.bin
	cat $< |gzip >$@
