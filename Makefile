.PHONY:	all install clean doc

SOLIB=libdiscferret.so
SONAME=$(SOLIB).3
SOVERS=$(SONAME).0

VERSION=1.1r1

PREFIX?=/usr/local

OBJS=discferret.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))
OBJS_A=$(addprefix obj_a/,$(OBJS))

INCPTH=include/discferret

CC=gcc
# debug flags
ifdef DEBUG
CFLAGS=-g -ggdb -Wall -pedantic -std=c99 -I./include/discferret
else
CFLAGS=-O2 -Wall -pedantic -std=c99 -DNDEBUG -I./include/discferret
LDFLAGS=-s
endif

all:	output/$(SOLIB) output/test doc

install:	all
	mkdir -p $(PREFIX)/lib $(PREFIX)/include/discferret
	cp output/$(SOVERS) $(PREFIX)/lib
	ln -sf $(SOVERS) $(PREFIX)/lib/$(SONAME)
	ln -sf $(SONAME) $(PREFIX)/lib/$(SOLIB)
	cp $(INCPTH)/discferret.h $(PREFIX)/include/discferret
	cp $(INCPTH)/discferret_registers.h $(PREFIX)/include/discferret
	@echo
	@echo "Installation complete. Now run ldconfig."

doc:
	@echo
	@echo "### Building documentation"
	doxygen > doc/doxygen.log

output/$(SOVERS):	$(OBJS_SO)
	@echo
	@echo "### Linking shared library"
	$(LD) $(LDFLAGS) -shared -soname $(SONAME) -o $@ $<

output/$(SONAME) output/$(SOLIB):	output/$(SOVERS)
	-rm output/$(SONAME) output/$(SOLIB)
	ln -s $(notdir $<) output/$(SONAME)
	ln -s $(notdir $<) output/$(SOLIB)

output/test:	test/test.c output/$(SONAME) $(INCPTH)/discferret.h $(INCPTH)/discferret_version.h
	@echo
	@echo "### Building test application"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -Loutput -ldiscferret -lusb-1.0 $<

#libdiscferret.a:	$(OBJS_A)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO) $(INCPTH)/discferret_version.h
	-rm -rf output/*
	-rm -f testapp
	-rm -rf doc/html
	-mkdir -p obj_so output doc

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

