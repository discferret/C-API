.PHONY:	all clean

SONAME=libdiscferret.so.1

OBJS=discferret.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))

CC=gcc
CFLAGS=-g -ggdb -Wall -pedantic

all:	output/$(SONAME).0

output/$(SONAME).0:	$(OBJS_SO)
	$(LD) -shared -soname $(SONAME) -o $@ $<

#libdiscferret.a:	$(OBJS_SO)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO) src/discferret_version.h
	-mkdir -p obj_so output

obj_so/%.o:	src/%.c
	$(CC) -c -fPIC $(CFLAGS) -o $@ $<

obj_so/discferret.o:	src/discferret.h src/discferret_version.h

src/discferret_version.h:	.hg/store/00manifest.i
	echo "#define HG_REV \"$(shell hg tip --template '{node|short}')\"" > $@
	echo "#define HG_TAG \"$(shell hg tip --template '{tags}')\"" >> $@

