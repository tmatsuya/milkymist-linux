
# Copyright (C) 2006-2008 Junjiro Okajima
# Copyright (C) 2006-2008 Tomas Matejicek, slax.org
#
# LICENSE follows the described ones in lzma and squashfs.

# $Id: Makefile,v 1.30 2008-03-12 16:24:54 jro Exp $

# paths
Sqlzma = ${CURDIR}

LzmaVer = lzma457
Lzma = ${Sqlzma}/${LzmaVer}
SqVer = squashfs3.4
Squashfs = ${Sqlzma}/${SqVer}

LzmaC = ${Lzma}/C/Compress/Lzma
LzmaAlone = ${Lzma}/CPP/7zip/Compress/LZMA_Alone
SqTools = ${Squashfs}/squashfs-tools

export

all:
	${MAKE} -C ${LzmaC} -f sqlzma.mk $@
	${MAKE} -C ${LzmaAlone} -f sqlzma.mk $@
	${MAKE} -C ${SqTools} $@

clean:
	${MAKE} -C ${LzmaC} -f sqlzma.mk $@
	${MAKE} -C ${LzmaAlone} -f sqlzma.mk $@
	${MAKE} -C ${SqTools} $@
	${RM} *~

