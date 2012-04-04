CC=gcc
AR=ar
CFLAGS=-std=c99 -fPIC -pedantic -Wall -I/usr/local/include
LDFLAGS=-lm
OBJS=bsm.o
STATIC=libbsm.a
SHARED=libbsm.so

all: libbsm

clean:
	rm -f $(STATIC) $(SHARED) $(OBJS)

libbsm: $(OBJS)
	$(AR) rs $(STATIC) $(OBJS)
	$(CC) -shared -o $(SHARED) $(OBJS)

.o:
	$(CC) $(CFLAGS) $(LDFLAGS) -c $*.c
