CC=gcc
CFLAGS=-std=c99 -g -pedantic -Wall -I/usr/local/include -I../
LDFLAGS=-L../ -lglew32 -lglfw -lopengl32 -lbsm -lm
OBJS=viewer.o
BINARYNAME=viewer

all: viewer

clean:
	rm -f $(BINARYNAME) $(OBJS)

viewer: $(OBJS)
	$(CC) $(CFLAGS) -o $(BINARYNAME) $(OBJS) $(LDFLAGS)

.o:
	$(CC) $(CFLAGS) $(INCLUDES) -c $*.c
