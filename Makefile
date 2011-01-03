.PHONY:	all clean doc

SOLIB=libdiscferret.so
SONAME=$(SOLIB).1
SOVERS=$(SONAME).0

VERSION=1.0r1

OBJS=discferret.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))
OBJS_A=$(addprefix obj_a/,$(OBJS))

CC=gcc
# debug flags
ifdef DEBUG
CFLAGS=-g -ggdb -Wall -pedantic -std=c99
else
CFLAGS=-O2 -Wall -pedantic -std=c99 -DNDEBUG
LDFLAGS=-s
endif

all:	output/$(SOVERS) output/test doc

doc:
	@echo
	@echo "### Building documentation"
	doxygen > doc/doxygen.log

output/$(SOLIB):	$(OBJS_SO)
	@echo
	@echo "### Linking shared library"
	$(LD) $(LDFLAGS) -shared -soname $(SONAME) -o $@ $<

output/$(SONAME):	output/$(SOLIB)
	ln -s $(notdir $<) $@

output/$(SOVERS):	output/$(SONAME)
	ln -s $(notdir $<) $@

output/test:	test/test.c output/$(SONAME).0 src/discferret.h src/discferret_version.h
	@echo
	@echo "### Building test application"
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ -Loutput -ldiscferret -lusb-1.0 $<

#libdiscferret.a:	$(OBJS_A)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO) src/discferret_version.h
	-rm -rf output/*
	-rm -f testapp
	-rm -rf doc/html
	-mkdir -p obj_so output doc

obj_so/%.o:	src/%.c
	$(CC) -c -fPIC $(CFLAGS) -o $@ $<

obj_so/discferret.o:	src/discferret.h src/discferret_version.h

have_hg := $(wildcard .hg)
ifeq ($(strip $(have_hg)),)
src/discferret_version.h:
	echo "#define HG_REV \"NO_REV\"" > $@
	echo "#define HG_TAG \"NO_TAG\"" >> $@
	echo "#undef CHECKOUT" >> $@
	echo "#define VERSION \"$(VERSION)\"" >> $@
else
src/discferret_version.h:	.hg/store/00manifest.i
	echo "#define HG_REV \"$(shell hg id | awk '{print $$1;}')\"" > $@
	echo "#define HG_TAG \"$(shell hg id | awk '{print $$2;}')\"" >> $@
	echo "#define CHECKOUT" >> $@
	echo "#define VERSION \"hg\"" >> $@
endif

