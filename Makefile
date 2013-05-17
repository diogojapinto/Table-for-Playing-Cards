# macros definitions
CC = gcc
CFLAGS = -Wall -pthread -lrt
XHDRS = headers.h

all: tpc

tpc: tpc.o
	$(CC) $(CFLAGS) tpc.o -o $@

tpc.o: tpc.c tpc.h $(XHDRS)
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o
