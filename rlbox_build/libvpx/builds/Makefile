.PHONY : build clean

.DEFAULT_GOAL := build
SHELL := /bin/bash

SANDBOXING_NACL_DIR=$(shell realpath ../../Sandboxing_NaCl)
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_AR32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-ar
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
NACL_STRIP32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-strip
NACL_STRIP64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-strip
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_VERIFER=$(SANDBOXING_NACL_DIR)/native_client/scons-out/opt-linux-x86-64/staging/ncval_new
NACL_NASM_DIR=$(shell realpath ../../NASM_NaCl/)
NACL_NASM=$(NACL_NASM_DIR)/nasm
NAFLAGS="-nacl -DCOMPILING_FOR_NACL"

x64/non_nacl_build_debug:
	mkdir -p $@
	cd $@ && ../../../configure \
	--enable-debug --disable-optimizations \
	--enable-shared \
	--enable-pic \
	--disable-examples --disable-tools --disable-docs --disable-unit-tests \
	--enable-multi-res-encoding --enable-postproc --enable-vp9-postproc \
	--as=nasm
	echo "AS=$(NACL_NASM)" >> $@/libs-x86_64-linux-gcc.mk
	cd $@ && make -j8

x64/non_nacl_build:
	mkdir -p $@
	cd $@ && ../../../configure \
	--enable-shared \
	--enable-pic \
	--disable-examples --disable-tools --disable-docs --disable-unit-tests \
	--enable-multi-res-encoding --enable-postproc --enable-vp9-postproc \
	--as=nasm \
	--enable-optimizations
	echo "AS=$(NACL_NASM)" >> $@/libs-x86_64-linux-gcc.mk
	cd $@ && make -j8

x64/nacl_build:
	mkdir -p $@
	cd $@ && ../../../configure \
	--enable-pic \
	--disable-examples --disable-tools --disable-docs --disable-unit-tests \
	--enable-multi-res-encoding --enable-postproc --enable-vp9-postproc \
	--as=nasm \
	--enable-optimizations \
	--disable-ssse3
	# higher simd stuff causing issues, so disabling for now. This level of simd sufficient for perf.
	echo "CC=$(NACL_CLANG64)" >> $@/libs-x86_64-linux-gcc.mk
	echo "CXX=$(NACL_CLANG++64)" >> $@/libs-x86_64-linux-gcc.mk
	echo "AR=$(NACL_AR64)" >> $@/libs-x86_64-linux-gcc.mk
	echo "STRIP=$(NACL_STRIP64)" >> $@/libs-x86_64-linux-gcc.mk
	echo "AS=$(NACL_NASM)" >> $@/libs-x86_64-linux-gcc.mk
	echo "ASFLAGS:=\$$(ASFLAGS) $(NAFLAGS)" >> $@/libs-x86_64-linux-gcc.mk
	cd $@ && make -j8
	mkdir -p $@/mainCombine
	cd $@/mainCombine && $(NACL_AR64) -x ../libvpx.a
	cd $@/mainCombine && $(NACL_CLANG++64) -fPIC -m64 -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o ./libvpx.nexe
	$(NACL_VERIFER) $@/mainCombine/libvpx.nexe

build: x64/non_nacl_build_debug x64/non_nacl_build x64/nacl_build # x64/nacl_build_debug

clean:
	-rm -rf ./x32
	-rm -rf ./x64
