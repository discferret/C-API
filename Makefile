.PHONY:	all clean doc

SOLIB=libdiscferret.so
SONAME=$(SOLIB).1
SOVERS=$(SONAME).0

OBJS=discferret.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))
OBJS_A=$(addprefix obj_a/,$(OBJS))

CC=gcc
CFLAGS=-g -ggdb -Wall -pedantic -std=c99

all:	output/$(SOVERS) output/test doc

doc:
	@echo
	@echo "### Building documentation"
	doxygen > doc/doxygen.log

output/$(SOLIB):	$(OBJS_SO)
	@echo
	@echo "### Linking shared library"
	$(LD) -shared -soname $(SONAME) -o $@ $<

output/$(SONAME):	output/$(SOLIB)
	cp $< $@

output/$(SOVERS):	output/$(SONAME)
	cp $< $@

output/test:	test/test.c output/$(SONAME).0 src/discferret.h src/discferret_version.h
	@echo
	@echo "### Building test application"
	$(CC) $(CFLAGS) -o $@ -Loutput -ldiscferret -lusb-1.0 $<

#libdiscferret.a:	$(OBJS_A)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO) src/discferret_version.h
	-rm -f output/*
	-rm -f testapp
	-rm -rf doc/*
	-mkdir -p obj_so output doc

obj_so/%.o:	src/%.c
	$(CC) -c -fPIC $(CFLAGS) -o $@ $<

obj_so/discferret.o:	src/discferret.h src/discferret_version.h

src/discferret_version.h:	.hg/store/00manifest.i
	echo "#define HG_REV \"$(shell hg tip --template '{node|short}')\"" > $@
	echo "#define HG_TAG \"$(shell hg tip --template '{tags}')\"" >> $@

