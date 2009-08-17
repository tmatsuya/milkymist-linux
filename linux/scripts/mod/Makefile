hostprogs-y	:= modpost mk_elfconfig
always		:= $(hostprogs-y) empty.o

modpost-objs	:= modpost.o file2alias.o sumversion.o

# dependencies on generated files need to be listed explicitly

$(obj)/modpost.o $(obj)/file2alias.o $(obj)/sumversion.o: $(obj)/elfconfig.h

quiet_cmd_elfconfig = MKELF   $@
      cmd_elfconfig = $(obj)/mk_elfconfig $(ARCH) < $< > $@

LIBELF_DEP :=
ifeq (,$(findstring Linux,$(shell uname -a)))
# need builtin libelf
LIBELF_DEP := $(objtree)/scripts/mod/libelfbuild/libelf/install/include/elf.h
HOSTCFLAGS += -I$(srctree)/scripts/mod/libelfbuild/libelf/install/include/

.PHONY: $(objtree)/scripts/mod/libelfbuild/libelf/install/include/elf.h
$(objtree)/scripts/mod/libelfbuild/libelf/install/include/elf.h:
	$(MAKE) objtree=$(objtree) srctree=$(srctree) -C $(srctree)/scripts/mod/libelfbuild all
	
endif

$(obj)/mk_elfconfig: $(LIBELF_DEP)

$(obj)/elfconfig.h: $(obj)/empty.o $(obj)/mk_elfconfig FORCE 
	$(call if_changed,elfconfig)

targets += elfconfig.h
