CXXFLAGS=-std=c++17 -O3 -g -fPIC
all: librlbox_wasm.a
wasmsandbox.o: wasmsandbox.h wasmsandbox.cc
	g++ wasmsandbox.cc -c $(CXXFLAGS)
librlbox_wasm.a: wasmsandbox.o
	ar rcs librlbox_wasm.a wasmsandbox.o
clean:
	rm -f librlbox_wasm.a wasmsandbox.o
