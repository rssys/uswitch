
CXXFLAGS=-std=c++17 -fno-stack-protector -O3 -g -I ../linux/usr/include
CFLAGS=-fno-stack-protector -O3 -g
all: libuswitch.a
libuswitch.a: uswitch.o uswitch.S.o signal.o
	ar rcs libuswitch.a uswitch.o uswitch.S.o signal.o
uswitch.o: uswitch.cc
	g++ uswitch.cc -c $(CXXFLAGS)
uswitch.S.o: uswitch.S
	gcc uswitch.S -c -o uswitch.S.o $(CFLAGS)
signal.o: signal.c
	gcc signal.c -c -I $(CFLAGS)
clean:
	rm -f libuswitch.a uswitch.o uswitch.S.o signal.o
