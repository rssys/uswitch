CC=gcc
OBJECTS = png.o pngerror.o pngget.o pngmem.o pngpread.o pngread.o pngrio.o pngrtran.o pngrutil.o pngset.o pngtrans.o \
	pngwio.o pngwrite.o pngwtran.o pngwutil.o intel/filter_sse2_intrinsics.o intel/intel_init.o \
	zlib/adler32.o zlib/crc32.o zlib/gzclose.o zlib/gzread.o zlib/infback.o zlib/inflate.o zlib/trees.o zlib/zutil.o \
	zlib/compress.o zlib/deflate.o zlib/gzlib.o zlib/gzwrite.o zlib/inffast.o zlib/inftrees.o zlib/uncompr.o
all: libpng.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -DMOZ_PNG_USE_INTEL_SSE -DFT_CONFIG_OPTION_USE_PNG -fPIC -O3 -I./zlib -g -c -fno-stack-protector
libpng.so: $(OBJECTS)
	$(CC) $(OBJECTS) -Wl,-z,relro,-z,now -shared -o libpng.so
clean:
	rm -rf libpng.so $(OBJECTS)
