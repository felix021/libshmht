
CC=gcc
AR=ar
CFLAGS=-O2
INCLUDE=-I.

all: shmht.o
	$(CC) -o libshmht.so $(CFLAGS) -shared $^
	$(AR) rcs libshmht.a  $^

shmht.o: shmht.c shmht.h
	$(CC) $(CFLAGS) $(INCLUDE) -fPIC -c shmht.c

test: shmht_tests
	./shmht_tests

shmht_tests: shmht.o shmht_tests.o
	$(CC) $(CFLAGS) $(INCLUDE) -o $< $^

shmht_tests.o: shmht.h
	$(CC) $(CFLAGS) $(INCLUDE) -fPIC -c shmht_tests.c

.PHONY: clean
clean:
	rm -f *.so *.a *.o
