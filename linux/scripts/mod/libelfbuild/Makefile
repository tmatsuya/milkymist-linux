
.PHONY: all

all: ${objtree}/scripts/mod/libelfbuild/libelf/install/include/elf.h

${objtree}/scripts/mod/libelfbuild/libelf/install/include/elf.h: ${objtree}/scripts/mod/libelfbuild/libelf/Makefile 
	$(MAKE) -j 1 -C ${srctree}/scripts/mod/libelfbuild/libelf all install 
	-ln -s gelf.h ${objtree}/scripts/mod/libelfbuild/libelf/install/include/elf.h 

EXTRA_CYGWIN_FIXUP := 
ifeq ($(findstring $(shell uname -a),Cygwin),)
EXTRA_CYGWIN_FIXUP := ac_cv_header_elf_h=no
endif

${objtree}/scripts/mod/libelfbuild/libelf/Makefile:
	{ \
		cd ${objtree}/scripts/mod/libelfbuild/libelf; \
		CC=gcc CFLAGS="" CPPFLAGS="" LDFLAGS="" $(EXTRA_CYGWIN_FIXUP) \
			${srctree}/scripts/mod/libelfbuild/libelf/configure \
			--prefix=${objtree}/scripts/mod/libelfbuild/libelf/install/; \
	} 
