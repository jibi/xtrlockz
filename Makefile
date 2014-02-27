LDLIBS=-lX11 -lcrypt
CC=clang
CFLAGS=-Wall

xtrlockz:	xtrlockz.o
xtrlockz.o:	xtrlockz.c
clean:
	@rm -f xtrlockz *.o

install:	xtrlockz
	install -m 4775 -o root -g root xtrlockz /usr/bin/
