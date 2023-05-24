CC=../../musl/bin/musl-gcc
AS=yasm
OBJECTS = test.o jcomapi.o jdapimin.o jdapistd.o jdatadst.o jdatasrc.o jdcoefct.o jdcolor.o jddctmgr.o jdhuff.o \
	jdinput.o jdmainct.o jdmarker.o jdmaster.o jdmerge.o jdphuff.o jdpostct.o jdsample.o jdtrans.o jerror.o \
	jfdctflt.o jfdctfst.o jfdctint.o jidctflt.o jidctfst.o jidctint.o jidctred.o jmemmgr.o jmemnobs.o \
	jquant1.o jquant2.o jutils.o jcapimin.o jcapistd.o jccoefct.o jccolor.o jcdctmgr.o jchuff.o jcinit.o \
	jcmainct.o jcmarker.o jcmaster.o jcparam.o jcphuff.o jcprepct.o jcsample.o jctrans.o simd/jsimd_x86_64.o \
	simd/jccolor-sse2-64.o simd/jcgray-sse2-64.o simd/jchuff-sse2-64.o simd/jcsample-sse2-64.o \
	simd/jdcolor-sse2-64.o simd/jdmerge-sse2-64.o simd/jdsample-sse2-64.o simd/jfdctflt-sse-64.o \
	simd/jfdctfst-sse2-64.o simd/jfdctint-sse2-64.o simd/jidctflt-sse2-64.o simd/jidctfst-sse2-64.o \
	simd/jidctint-sse2-64.o simd/jidctred-sse2-64.o simd/jquantf-sse2-64.o simd/jquanti-sse2-64.o
all: libjpeg.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -fPIC -O3 -g -c -fno-stack-protector
%.o: %.asm
	$(AS) $< -o $@ -f elf64 -I./simd/ -D__x86_64__ -DPIC -DELF
libjpeg.so: $(OBJECTS)
	$(CC) $(OBJECTS) ../fast_memcpy/libfast_memcpy.a -Wl,-z,relro,-z,now -shared -o libjpeg.so
clean:
	rm -rf libjpeg.so $(OBJECTS)
