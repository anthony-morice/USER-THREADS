CC = g++
CFLAGS = -lrt -g
DEPS = TCB.h uthread.h
OBJ = TCB.o uthread.o main.o

%.o: %.cpp $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) 

uthread-test: TCB.o uthread.o uthread-test.o 
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY: clean

clean:
	rm -f uthread-test *.o
