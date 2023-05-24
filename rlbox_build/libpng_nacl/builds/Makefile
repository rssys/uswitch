.PHONY : build clean build32 build64

.DEFAULT_GOAL := build64
SHELL := /bin/bash

SANDBOXING_NACL_DIR=$(shell realpath ../../Sandboxing_NaCl)
ZLIB_DIR=$(shell realpath ../../zlib_nacl)
RLBOX_API=$(shell realpath ../../rlbox_api)
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_LIBM32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32/libm.a
NACL_LIBM64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib/libm.a
NACL_AR32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-ar
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
# EMCC_DIR=$(shell realpath ../../emsdk)
# EMCC=emcc
# EMCC_LD=emcc
# EMAR=emar
# WASM_SANDBOX_DIR=$(shell realpath ../../wasm-sandboxing)
override CFLAGS += -fPIC -O3
CFLAGS32 = $(CFLAGS) -m32
CFLAGS64 = $(CFLAGS) -m64
# EMSCRIPTENCFLAGS32 = $(CFLAGS) -m32
#We still use wasm 32
# EMSCRIPTENCFLAGS64 = $(CFLAGS) -m32


# define load_emsdk_path = 
# 	source $(EMCC_DIR)/emsdk_env.sh
# endef

# include $(WASM_SANDBOX_DIR)/builds/Makefile.inc

x32/non_nacl_build:
	-mkdir -p ./x32/non_nacl_build
	cd ./x32/non_nacl_build && cmake -D CMAKE_C_COMPILER=clang -D CMAKE_C_FLAGS='$(CFLAGS32)' -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x32/non_nacl_build/libz.a ../../../
	$(MAKE) -C ./x32/non_nacl_build

x64/non_nacl_build:
	-mkdir -p ./x64/non_nacl_build
	cd ./x64/non_nacl_build && cmake -D CMAKE_C_COMPILER=clang -D CMAKE_C_FLAGS='$(CFLAGS64)' -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/non_nacl_build/libz.a ../../../
	$(MAKE) -C ./x64/non_nacl_build

x32/nacl_build:
	-mkdir -p ./x32/nacl_build
	cd ./x32/nacl_build && cmake -D CMAKE_C_COMPILER=$(NACL_CLANG32) -D CMAKE_C_FLAGS='$(CFLAGS32)' -D M_LIBRARY=$(NACL_LIBM32) -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x32/nacl_build/libz.a -D PNG_SHARED=0 ../../../
	$(MAKE) -C ./x32/nacl_build
	mkdir -p ./x32/nacl_build/mainCombine
	cd ./x32/nacl_build/mainCombine && $(NACL_AR32) -x ../libpng.a
	cd ./x32/nacl_build/mainCombine && $(NACL_CLANG++32) -fPIC -m32 -I$(ZLIB_DIR) -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init $(ZLIB_DIR)/builds/x32/nacl_build/libz.a -o ./libpng.nexe

x64/nacl_build:
	-mkdir -p ./x64/nacl_build
	cd ./x64/nacl_build && cmake -D CMAKE_C_COMPILER=$(NACL_CLANG64) -D CMAKE_C_FLAGS='$(CFLAGS64)' -D M_LIBRARY=$(NACL_LIBM64) -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/nacl_build/libz.a -D PNG_SHARED=0 ../../../
	$(MAKE) -C ./x64/nacl_build
	mkdir -p ./x64/nacl_build/mainCombine
	cd ./x64/nacl_build/mainCombine && $(NACL_AR64) -x ../libpng.a
	cd ./x64/nacl_build/mainCombine && $(NACL_CLANG++64) -fPIC -m64 -I$(ZLIB_DIR) -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init $(ZLIB_DIR)/builds/x64/nacl_build/libz.a -o ./libpng.nexe

# x64/wasm_build:
# 	-mkdir -p ./x64/wasm_build
# 	$(call load_emsdk_path) && \
# 	cd ./x64/wasm_build && cmake -D CMAKE_C_COMPILER=$(EMCC) -D CMAKE_C_FLAGS='$(EMSCRIPTENCFLAGS64)' -D M_LIBRARY= -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/wasm_build/libz.a -D PNG_SHARED=0 ../../../
# 	$(MAKE) -C ./x64/wasm_build
# 	$(call load_emsdk_path) && \
# 	$(call convert_to_wasm,$(join $(abspath $@),/libpng.a),$(join $(abspath $@),/libpng_wasm.js),-g,$(ZLIB_DIR)/builds/x64/wasm_build/libz.a)

###########################################

x32/non_nacl_build_debug:
	-mkdir -p ./x32/non_nacl_build_debug
	cd ./x32/non_nacl_build_debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=clang -D CMAKE_C_FLAGS='$(CFLAGS32)' -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x32/non_nacl_build_debug/libz.a ../../../
	$(MAKE) -C ./x32/non_nacl_build_debug

x64/non_nacl_build_debug:
	-mkdir -p ./x64/non_nacl_build_debug
	cd ./x64/non_nacl_build_debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=clang -D CMAKE_C_FLAGS='$(CFLAGS64)' -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/non_nacl_build_debug/libz.a ../../../
	$(MAKE) -C ./x64/non_nacl_build_debug

x32/nacl_build_debug:
	-mkdir -p ./x32/nacl_build_debug
	cd ./x32/nacl_build_debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=$(NACL_CLANG32) -D CMAKE_C_FLAGS='$(CFLAGS32)' -D M_LIBRARY=$(NACL_LIBM32) -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x32/nacl_build_debug/libz.a -D PNG_SHARED=0 ../../../
	$(MAKE) -C ./x32/nacl_build_debug
	mkdir -p ./x32/nacl_build_debug/mainCombine
	cd ./x32/nacl_build_debug/mainCombine && $(NACL_AR32) -x ../libpng.a
	cd ./x32/nacl_build_debug/mainCombine && $(NACL_CLANG++32) -fPIC -m32 -I$(ZLIB_DIR) -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init $(ZLIB_DIR)/builds/x32/nacl_build_debug/libz.a -o ./libpng.nexe

x64/nacl_build_debug:
	-mkdir -p ./x64/nacl_build_debug
	cd ./x64/nacl_build_debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=$(NACL_CLANG64) -D CMAKE_C_FLAGS='$(CFLAGS64)' -D M_LIBRARY=$(NACL_LIBM64) -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/nacl_build_debug/libz.a -D PNG_SHARED=0 ../../../
	$(MAKE) -C ./x64/nacl_build_debug
	mkdir -p ./x64/nacl_build_debug/mainCombine
	cd ./x64/nacl_build_debug/mainCombine && $(NACL_AR64) -x ../libpng.a
	cd ./x64/nacl_build_debug/mainCombine && $(NACL_CLANG++64) -fPIC -m64 -I$(ZLIB_DIR) -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init $(ZLIB_DIR)/builds/x64/nacl_build_debug/libz.a -o ./libpng.nexe

# x64/wasm_build_debug:
# 	-mkdir -p ./x64/wasm_build_debug
# 	$(call load_emsdk_path) && \
# 	cd ./x64/wasm_build_debug && cmake -D CMAKE_BUILD_TYPE=Debug -D CMAKE_C_COMPILER=$(EMCC) -D CMAKE_C_FLAGS='$(EMSCRIPTENCFLAGS64)' -D M_LIBRARY= -D ZLIB_INCLUDE_DIR=$(ZLIB_DIR) -D ZLIB_LIBRARY=$(ZLIB_DIR)/builds/x64/wasm_build_debug/libz.a -D PNG_SHARED=0 ../../../
# 	$(MAKE) -C ./x64/wasm_build_debug
# 	$(call load_emsdk_path) && \
# 	$(call convert_to_wasm,$(join $(abspath $@),/libpng.a),$(join $(abspath $@),/libpng_wasm.js),-g,$(ZLIB_DIR)/builds/x64/wasm_build_debug/libz.a)

FLAGS_WHEN_LINKING_NACL_EXAMPLES_64_DEBUG = -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -fPIC -Wl,-rpath=$(SANDBOXING_NACL_DIR)/native_client/scons-out/dbg-linux-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread

x64/example_static_debug: ../example_static.cpp
	g++ -g ../example_static.cpp -I$(RLBOX_API) -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ -L./x64/non_nacl_build_debug -l:libpng.a -L$(ZLIB_DIR)/builds/x64/non_nacl_build_debug -l:libz.a -ldl -o x64/example_static_debug $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64_DEBUG)


###########################################

build32: x32/non_nacl_build x32/nacl_build x32/non_nacl_build_debug x32/nacl_build_debug
build64: x64/non_nacl_build x64/nacl_build x64/non_nacl_build_debug x64/nacl_build_debug
build: build32 build64

clean:
	-rm -rf ./x32
	-rm -rf ./x64
