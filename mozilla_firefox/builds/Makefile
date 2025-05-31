.PHONY: build run clean

.DEFAULT_GOAL: build

initbootstrap:
	cd .. && ./mach bootstrap --no-interactive --application-choice browser
	touch initbootstrap

inithasrun: initbootstrap
	cd .. && ./mach configure
	#fix bugs that sometimes crops up in firefox's build system
	if [ ! -f ../ipc/chromium/src/third_party/libevent/evconfig-private.h ] && [ -f ../ipc/chromium/src/third_party/libevent/configure.ac ]; then \
		cd ../ipc/chromium/src/third_party/libevent && \
		autoreconf -i --force && \
		./configure; \
	fi
	if [ ! -f ../toolkit/crashreporter/google-breakpad/src/third_party/lss/linux_syscall_support.h ]; then \
		git clone https://chromium.googlesource.com/linux-syscall-support ../toolkit/crashreporter/google-breakpad/src/third_party/lss; \
	fi
	echo hi > "inithasrun"

build: inithasrun
	cd .. && ./mach build

run:
	cd .. && ./mach run ./testing/talos/talos/tests/jpeg_page_render/jpeg_render.html ./testing/talos/talos/tests/jpeg_page_render/png_render.html

clean:
	-rm -rf ../firefox_64bit_optdebug_static
