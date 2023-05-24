CC=../../wasi-sdk-14.0/bin/clang
NATIVE_CC=gcc
WASM2C_DIR=../../wasm2c_sandbox_compiler/wasm2c/
WASM2C=../../wasm2c_sandbox_compiler/bin/wasm2c
OBJECTS = adler32.o crc32.o gzclose.o gzread.o infback.o inflate.o trees.o zutil.o \
	compress.o deflate.o gzlib.o gzwrite.o inffast.o inftrees.o uncompr.o main.o
all: libz.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -fPIC -O3 -g -c
libz.wasm: $(OBJECTS)
	$(CC) $(OBJECTS) -Wl,--export-all -Wl,--no-entry -Wl,--growable-table -o libz.wasm
libz.c: libz.wasm
	$(WASM2C) libz.wasm -o libz.c -n libz_
libz.so: libz.c
	$(NATIVE_CC) libz.c $(WASM2C_DIR)/wasm-rt-wasi.c $(WASM2C_DIR)/wasm-rt-impl.c \
		$(WASM2C_DIR)/wasm-rt-os-unix.c -I$(WASM2C_DIR) -g -O3 -fPIC -shared -o libz.so
clean:
	rm -rf libz.so libz.wasm libz.c $(OBJECTS)
