# macros definitions
CC = gcc
CFLAGS = -Wall -pthread -lrt
XHDRS = headers.h

all: tpc

tpc: tpc.o
	$(CC) tpc.o -o $@ $(CFLAGS)

tpc.o: tpc.c tpc.h $(XHDRS)
	$(CC) -c $< $(CFLAGS)

clean:
	rm -f *.o
