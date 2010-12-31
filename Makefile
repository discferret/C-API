.PHONY:	all clean

SONAME=libdiscferret.so.1

OBJS=discferret.o
OBJS_SO=$(addprefix obj_so/,$(OBJS))

CC=gcc
CFLAGS=-g -ggdb

all:	output/libdiscferret.so

output/$(SONAME).0:	$(OBJS_SO)
	$(LD) -shared -soname $(SONAME) -o $@ $<

#libdiscferret.a:	$(OBJS_SO)
#	ar -cr $@ $<

clean:
	-rm -f libdiscferret.a $(OBJS_SO)
	-mkdir -p obj_so output

obj_so/%.o:	src/%.c
	$(CC) -c -fPIC $(CFLAGS) -o $@ $^
