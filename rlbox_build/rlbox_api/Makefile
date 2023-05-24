.PHONY: build mkdir_out run32 run64

.DEFAULT_GOAL = build64

# Note - Use NO_NACL=1 to disable nacl build
# Note - Use NO_WASM=1 to disable wasm build
ifeq ($(origin CC),default)
	CC = gcc
endif
ifeq ($(origin CXX),default)
	CXX = g++
endif
CFLAGS ?= -g3
PROCESS_SANDBOX_DIR=$(shell realpath ../ProcessSandbox)
SANDBOXING_NACL_DIR=$(shell realpath ../Sandboxing_NaCl)
WASM_SANDBOX_DIR?=$(shell realpath ../wasm-sandboxing)
EMSDK_DIR?=$(shell realpath ../emsdk)
LUCET_SANDBOX_DIR=$(shell realpath ../lucet-playground)

ifeq ($(NO_PROCESS),1)
	PROCESS_INCLUDES=-DNO_PROCESS
	PROCESS_LIBS32=
	PROCESS_LIBS64=
else
	PROCESS_INCLUDES=-I$(PROCESS_SANDBOX_DIR) -I.
	PROCESS_LIBS32=-lpthread -lrt -lseccomp -L$(PROCESS_SANDBOX_DIR) -lProcessSandbox_rlboxtest32
	PROCESS_LIBS64=-lpthread -lrt -lseccomp -L$(PROCESS_SANDBOX_DIR) -lProcessSandbox_rlboxtest64
endif
ifeq ($(NO_NACL),1)
	NACL_INCLUDES=-DNO_NACL
	NACL_LIBS_32=
	NACL_LIBS_64=
else
	NACL_INCLUDES=-I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr
	NACL_LIBS_32=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-32/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_32 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_32 -lrt -lpthread
	NACL_LIBS_64=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread
endif
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
ifeq ($(NO_WASM),1)
	WASM_INCLUDES=-DNO_WASM
	WASM_LIBS_64=
else
	WASM_INCLUDES=-I$(WASM_SANDBOX_DIR)/wasm2c
	WASM_LIBS_64=$(WASM_SANDBOX_DIR)/bin/libwasm_sandbox.o
endif

-include $(WASM_SANDBOX_DIR)/builds/Makefile.inc
-include $(LUCET_SANDBOX_DIR)/Makefile.inc

mkdir_out:
	mkdir -p ./out/x32
	mkdir -p ./out/x64

out/x32/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(CXX) -m32 $(PROCESS_INCLUDES) $(NACL_INCLUDES) $(WASM_INCLUDES) -std=c++14 $(CFLAGS) -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.c -Wl,--export-dynamic $(PROCESS_LIBS32) $(NACL_LIBS_32)  -ldl -lpthread -o $@

out/x32/libtest.so: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(CXX) -m32 -std=c++11 $(CFLAGS) -shared -fPIC $(CURDIR)/libtest.c -o $@

ifeq ($(NO_NACL),1)
out/x32/libtest.nexe:
else
out/x32/libtest.nexe: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
		$(NACL_CLANG++32) -O3 -m32 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib $(CURDIR)/libtest.c -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init -o $@
endif

out/x64/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(CXX) $(PROCESS_INCLUDES) $(NACL_INCLUDES) $(WASM_INCLUDES) -std=c++14 $(CFLAGS) -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.c -Wl,--export-dynamic $(PROCESS_LIBS64) $(NACL_LIBS_64) $(WASM_LIBS_64) -ldl -lpthread -o $@

out/x64/libtest.so: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(CXX) -std=c++11 $(CFLAGS) -shared -fPIC $(CURDIR)/libtest.c -o $@

ifeq ($(NO_NACL),1)
out/x64/libtest.nexe:
else
out/x64/libtest.nexe: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(NACL_CLANG++64) -O3 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib $(CURDIR)/libtest.c -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o $@
endif

ifeq ($(NO_WASM),1)
out/x64/libwasm_test.so:
out/x64/liblucetwasm_test.so:
else
.ONESHELL:
SHELL=/bin/bash
out/x64/libwasm_test.so: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	source $(EMSDK_DIR)/emsdk_env.sh && \
	emcc -std=c++11 $(CFLAGS) -O0 $(CURDIR)/libtest.c -c -o ./out/x64/libwasm_test.o && \
	$(call convert_to_wasm,$(abspath ./out/x64/libwasm_test.o),$(abspath ./out/x64/libwasm_test.js),$(CFLAGS))

out/x64/liblucetwasm_test.so: mkdir_out $(CURDIR)/libtest.c $(CURDIR)/libtest.h
	$(call lucet_produce_wasm, $(CURDIR)/libtest.c $(CFLAGS) -o $(CURDIR)/out/x64/liblucetwasm_test.wasm) && \
	$(call lucet_produce_so, $(CURDIR)/out/x64/liblucetwasm_test.wasm -o $(CURDIR)/out/x64/liblucetwasm_test.so)

endif

build32: out/x32/test out/x32/libtest.so out/x32/libtest.nexe
build64: out/x64/test out/x64/libtest.so out/x64/libtest.nexe out/x64/libwasm_test.so
build:  build32 build64

run32:
	cd ./out/x32 && ./test

run64:
	cd ./out/x64 && ./test

clean:
	rm -rf ./out
