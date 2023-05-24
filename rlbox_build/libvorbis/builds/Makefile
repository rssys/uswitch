.PHONY : build clean

.DEFAULT_GOAL := build
SHELL := /bin/bash

SANDBOXING_NACL_DIR=$(shell realpath ../../Sandboxing_NaCl)
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_AR32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-ar
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG++64=$(NACL_CLANG64)++

x64/non_nacl_build_debug:
	mkdir -p $@
	cd $@ && cmake -DCMAKE_BUILD_TYPE=Debug ../../.. -G "Unix Makefiles"
	$(MAKE) -C $@ -j8

x64/non_nacl_build:
	mkdir -p $@
	cd $@ && cmake -DCMAKE_BUILD_TYPE=Release ../../.. -G "Unix Makefiles"
	$(MAKE) -C $@ -j8

x64/nacl_build_debug:
	mkdir -p $@
	cd $@ && CC=$(NACL_CLANG64) AR=$(NACL_AR64) CFLAGS='-fPIC' LDFLAGS='-fPIC' cmake -DCMAKE_BUILD_TYPE=Debug ../../.. -G "Unix Makefiles"
	$(MAKE) -C $@ -j8
	mkdir -p $@/mainCombine
	cd $@/mainCombine && $(NACL_AR64) -x ../libvorbis.a
	cd $@/mainCombine && $(NACL_CLANG++64) -fPIC -m64 -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o ./libvorbis.nexe

x64/nacl_build:
	mkdir -p $@
	cd $@ && CC=$(NACL_CLANG64) AR=$(NACL_AR64) CFLAGS='-fPIC' LDFLAGS='-fPIC' cmake -DCMAKE_BUILD_TYPE=Release ../../.. -G "Unix Makefiles"
	$(MAKE) -C $@ -j8
	mkdir -p $@/mainCombine
	cd $@/mainCombine && $(NACL_AR64) -x ../libvorbis.a
	cd $@/mainCombine && $(NACL_CLANG++64) -fPIC -m64 -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o ./libvorbis.nexe

build: x64/non_nacl_build_debug x64/non_nacl_build x64/nacl_build_debug x64/nacl_build

clean:
	-rm -rf ./x32
	-rm -rf ./x64
