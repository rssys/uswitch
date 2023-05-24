CXXFLAGS=-std=c++17 -O3 -g -I../libuswitch -fPIC
CFLAGS=-O3 -g -I../libuswitch
all: shared static
shared: librlbox_uswitch.so
static: librlbox_uswitch.a
uswitchsandbox.o: uswitchsandbox.h uswitchsandbox.cc
	g++ uswitchsandbox.cc -c $(CXXFLAGS)
librlbox_uswitch.a: uswitchsandbox.o
	ar rcs librlbox_uswitch.a uswitchsandbox.o
librlbox_uswitch.so: uswitchsandbox.o
	g++ uswitchsandbox.o -shared -o librlbox_uswitch.so
clean:
	rm -f uswitchsandbox.o lib.o librlbox_uswitch.so librlbox_uswitch.a libhook.a
