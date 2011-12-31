.PHONY:	all install clean docs

PLATFORM ?= $(shell ./idplatform.sh)

VERSION			:=	1.2r10
SONAME_VERSION	:=	4
PREFIX			?=	/usr/local

CC=gcc
LD=gcc

############# end of user editable parameters

# Make sure the platform ID is valid
VALID_PLATFORMS	:=	osx linux
ifeq ($(filter $(PLATFORM),$(VALID_PLATFORMS)),)
    $(error Invalid PLATFORM '$(PLATFORM)'. Valid platforms are: $(VALID_PLATFORMS))
endif

# base name of the shared library name
SOLIB_PFX		:=	libdiscferret
ifeq ($(PLATFORM),osx)
    SOLIB		:=	$(SOLIB_PFX).dylib
    SONAME		:=	$(SOLIB_PFX).$(SONAME_VERSION).dylib
    SOVERS		:=	$(SOLIB_PFX).$(SONAME_VERSION).0.dylib
    LDFLAGS		?=	-dynamiclib -install_name $(SONAME)
endif

ifeq ($(PLATFORM),linux)
    SOLIB		:=	$(SOLIB_PFX).so
    SONAME		:=	$(SOLIB_PFX).so.$(SONAME_VERSION)
    SOVERS		:=	$(SOLIB_PFX).so.$(SONAME_VERSION).0
    LDFLAGS		?=	-shared
endif

# set CFLAGS based on the state of the DEBUG parameter.
# make {target} DEBUG=1 produces a debug build.
ifdef DEBUG
    CFLAGS	+=	-g -ggdb -Wall -pedantic -std=c99 -I./include/discferret
else
    CFLAGS	+=	-O2 -Wall -pedantic -std=c99 -DNDEBUG -I./include/discferret
endif

OBJS=discferret.o discferret_microcode.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))
OBJS_A=$(addprefix obj_a/,$(OBJS))

INCPTH=include/discferret

all:	output/$(SOLIB) output/test

install:	all
	mkdir -p $(PREFIX)/lib $(PREFIX)/include/discferret
	cp output/$(SOVERS) $(PREFIX)/lib
	ln -sf $(SOVERS) $(PREFIX)/lib/$(SONAME)
	ln -sf $(SONAME) $(PREFIX)/lib/$(SOLIB)
	cp $(INCPTH)/discferret.h $(PREFIX)/include/discferret
	cp $(INCPTH)/discferret_registers.h $(PREFIX)/include/discferret
	@echo
	@echo "Installation complete. Now run ldconfig."

docs:
	@echo
	@echo "### Building documentation"
	doxygen > doc/doxygen.log

output/$(SOVERS):	$(OBJS_SO)
	@echo
	@echo "### Linking shared library"
	$(LD) $(LDFLAGS) -o $@ $^ `pkg-config --libs libusb-1.0`

output/$(SONAME) output/$(SOLIB):	output/$(SOVERS)
	-rm output/$(SONAME) output/$(SOLIB) &>/dev/null
	ln -s $(notdir $<) output/$(SONAME)
	ln -s $(notdir $<) output/$(SOLIB)

output/test:	test/test.c output/$(SONAME) $(INCPTH)/discferret.h $(INCPTH)/discferret_version.h
	@echo
	@echo "### Building test application"
	$(CC) $(CFLAGS) -o $@ -Loutput $< -ldiscferret -lusb-1.0

#libdiscferret.a:	$(OBJS_A)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO) $(INCPTH)/discferret_version.h
	-rm -rf output/*
	-rm -f testapp
	-rm -rf doc/html
	-mkdir -p obj_so output doc

src/discferret_microcode.c: microcode.rbf
	srec_cat -Output $@ -C-Array discferret_microcode $< -Binary

obj_so/%.o:	src/%.c
	$(CC) -c -fPIC $(CFLAGS) -o $@ $<

obj_so/discferret.o:	$(INCPTH)/discferret.h $(INCPTH)/discferret_version.h

have_hg := $(wildcard .hg)
USE_HG ?= 1
ifeq ($(strip $(USE_HG)),0)
    have_hg :=
endif

ifeq ($(strip $(have_hg)),)
$(INCPTH)/discferret_version.h:
	echo "#define HG_REV \"NO_REV\"" > $@
	echo "#define HG_TAG \"NO_TAG\"" >> $@
	echo "#undef CHECKOUT" >> $@
	echo "#define VERSION \"$(VERSION)\"" >> $@
else
$(INCPTH)/discferret_version.h:	.hg/store/00manifest.i
	echo "#define HG_REV \"$(shell hg id | awk '{print $$1;}')\"" > $@
	echo "#define HG_TAG \"$(shell hg id | awk '{print $$2;}')\"" >> $@
	echo "#define CHECKOUT" >> $@
	echo "#define VERSION \"hg\"" >> $@
endif

