CC=../../wasi-sdk-14.0/bin/clang
NATIVE_CC=gcc
WASM2C_DIR=../../wasm2c_sandbox_compiler/wasm2c/
WASM2C=../../wasm2c_sandbox_compiler/bin/wasm2c
OBJECTS = png.o pngerror.o pngget.o pngmem.o pngpread.o pngread.o pngrio.o pngrtran.o pngrutil.o pngset.o pngtrans.o \
	pngwio.o pngwrite.o pngwtran.o pngwutil.o main.o \
	zlib/adler32.o zlib/crc32.o zlib/gzclose.o zlib/gzread.o zlib/infback.o zlib/inflate.o zlib/trees.o zlib/zutil.o \
	zlib/compress.o zlib/deflate.o zlib/gzlib.o zlib/gzwrite.o zlib/inffast.o zlib/inftrees.o zlib/uncompr.o
all: libpng.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -DFT_CONFIG_OPTION_USE_PNG -fPIC -O3 -I. -I./zlib -g -c
libpng.wasm: $(OBJECTS)
	$(CC) $(OBJECTS) -Wl,--export-all -Wl,--no-entry -Wl,--growable-table -o libpng.wasm
libpng.c: libpng.wasm
	$(WASM2C) libpng.wasm -o libpng.c -n libpng_
libpng.so: libpng.c
	$(NATIVE_CC) libpng.c $(WASM2C_DIR)/wasm-rt-wasi.c $(WASM2C_DIR)/wasm-rt-impl.c \
		$(WASM2C_DIR)/wasm-rt-os-unix.c -I$(WASM2C_DIR) -O3 -g -fPIC -shared -o libpng.so
clean:
	rm -rf libpng.so libpng.c libpng.wasm $(OBJECTS)
