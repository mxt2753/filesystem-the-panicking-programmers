CC=		gcc

all:	test mfs

test: mfs.o
	gcc -o test mfs.o -g --std=c99

mfs: mfs.o
	gcc -o mfs mfs.o -g --std=c99

clean:
	rm -f *.o *.a test mfs

.PHONY: all clean