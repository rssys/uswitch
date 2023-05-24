# For verbose build: make SCONS_FLAGS=--verbose [target]
DEPOT_TOOLS_PATH := $(shell realpath ../depot_tools)
export PATH := $(DEPOT_TOOLS_PATH):$(PATH)

CXX=clang++
CXXFLAGS=-std=c++11 -fPIC -O0 -I.
CPU = 0

.PHONY: buildopt32 buildopt64 runopt32 runopt64 builddebug32 builddebug64 rundebug32 rundebug64 runperftest32 runperftest64 clean init

.DEFAULT_GOAL = buildopt64

.NOTPARALLEL: buildopt32 buildopt64 builddebug32 builddebug64 init_complete

init_complete:
	# Dependency installation moved to the parent repo
	# sudo apt -y install flex bison git libc6-dev-i386 cmake texinfo gcc-arm-linux-gnueabihf texinfo
	# if [ ! -e "/usr/include/asm-generic" ]; then
	# 	sudo ln -s /usr/include/asm-generic /usr/include/asm
	# fi
	if [ ! -e "$(DEPOT_TOOLS_PATH)" ]; then git clone https://chromium.googlesource.com/chromium/tools/depot_tools "$(DEPOT_TOOLS_PATH)"; fi
	gclient runhooks
	#Build the modified compiler
	tools/clang/scripts/update.py
	native_client/toolchain_build/toolchain_build_pnacl.py --verbose --sync --clobber --install native_client/toolchain/linux_x86/pnacl_newlib_raw
	#Install the modified compiler
	rm -rf native_client/toolchain/linux_x86/pnacl_newlib
	ln -s pnacl_newlib_raw native_client/toolchain/linux_x86/pnacl_newlib
	touch init_complete

init:
	if [ ! -e "init_complete" ]; then make init_complete; fi

buildopt32 : init
	cd native_client && ./scons MODE=opt-linux,nacl werror=0 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

buildopt64 : init
	cd native_client && ./scons MODE=opt-linux,nacl werror=0 platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=opt-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

builddebug32 : init
	cd native_client && ./scons MODE=dbg-linux,nacl werror=0 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=dbg-linux,nacl $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

builddebug64 : init
	cd native_client && ./scons MODE=dbg-linux,nacl werror=0 platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-clean
	if [ -d ./native_client/scons-out-firefox ]; then mv ./native_client/scons-out-firefox ./native_client/scons-out; fi
	cd native_client && ./scons -f SConstruct_Firefox werror=0 MODE=dbg-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	mv ./native_client/scons-out ./native_client/scons-out-firefox
	mv ./native_client/scons-out-clean ./native_client/scons-out

runopt32 :
	./native_client/scons-out/opt-linux-x86-32/staging/dyn_ldr_test

runopt64 :
	./native_client/scons-out/opt-linux-x86-64/staging/dyn_ldr_test

runopt64_cpp :
	./native_client/scons-out/opt-linux-x86-64/staging/dyn_ldr_test_api

rundebug32 :
	./native_client/scons-out/dbg-linux-x86-32/staging/dyn_ldr_test

rundebug64 :
	./native_client/scons-out/dbg-linux-x86-64/staging/dyn_ldr_test

rundebug64_cpp :
	./native_client/scons-out/dbg-linux-x86-64/staging/dyn_ldr_test_api

runperftest32 :
	./native_client/scons-out/opt-linux-x86-32/staging/dyn_ldr_benchmark

runperftest64 :
	./native_client/scons-out/opt-linux-x86-64/staging/dyn_ldr_benchmark

clean :
	cd native_client && ./scons -c MODE=opt-linux,nacl $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=opt-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=dbg-linux,nacl $(SCONS_FLAGS)
	cd native_client && ./scons -c MODE=dbg-linux,nacl platform=x86-64 $(SCONS_FLAGS)
	rm -rf ./native_client/scons-out
	rm -rf ./native_client/scons-out-firefox
