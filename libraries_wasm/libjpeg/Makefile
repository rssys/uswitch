CC=../../wasi-sdk-14.0/bin/clang
NATIVE_CC=gcc
WASM2C_DIR=../../wasm2c_sandbox_compiler/wasm2c/
WASM2C=../../wasm2c_sandbox_compiler/bin/wasm2c
OBJECTS = jcomapi.o jdapimin.o jdapistd.o jdatadst.o jdatasrc.o jdcoefct.o jdcolor.o jddctmgr.o jdhuff.o \
	jdinput.o jdmainct.o jdmarker.o jdmaster.o jdmerge.o jdphuff.o jdpostct.o jdsample.o jdtrans.o jerror.o \
	jfdctflt.o jfdctfst.o jfdctint.o jidctflt.o jidctfst.o jidctint.o jidctred.o jmemmgr.o jmemnobs.o \
	jquant1.o jquant2.o jutils.o jcapimin.o jcapistd.o jccoefct.o jccolor.o jcdctmgr.o jchuff.o jcinit.o \
	jcmainct.o jcmarker.o jcmaster.o jcparam.o jcphuff.o jcprepct.o jcsample.o jctrans.o jsimd_none.o main.o
all: libjpeg.so
.PHONY: all clean
%.o: %.c
	$(CC) $< -o $@ -O3 -g -c
libjpeg.wasm: $(OBJECTS)
	$(CC) $(OBJECTS) -Wl,--export-all -Wl,--no-entry -Wl,--growable-table -o libjpeg.wasm
libjpeg.c: libjpeg.wasm
	$(WASM2C) libjpeg.wasm -o libjpeg.c -n libjpeg_
libjpeg.so: libjpeg.c
	$(NATIVE_CC) libjpeg.c $(WASM2C_DIR)/wasm-rt-wasi.c $(WASM2C_DIR)/wasm-rt-impl.c \
		$(WASM2C_DIR)/wasm-rt-os-unix.c -I$(WASM2C_DIR) -g -O3 -fPIC -shared -o libjpeg.so
clean:
	rm -rf libjpeg.wasm libjpeg.so libjpeg.c libjpeg.h $(OBJECTS)
