CC=g++
override CFLAGS += -std=gnu++11 -O3 -fPIC -fdiagnostics-color=always -march=native
override LIBFLAGS += -L. -lpthread -lrt -lseccomp -ldl
CFLAGS32 = $(CFLAGS) -m32
CFLAGS64 = $(CFLAGS) -m64
SHARED_LIB_FLAGS=-shared
SHARED_LIB_LDFLAGS=-ldl
PRODUCTS32 = library32.so \
						 libProcessSandbox_dummy32.a \
						 libProcessSandbox_jpeg32.a \
						 libProcessSandbox_png32.a \
						 libProcessSandbox_zlib32.a \
						 libProcessSandbox_theora32.a \
						 libProcessSandbox_vpx32.a \
						 libProcessSandbox_vorbis32.a \
						 libProcessSandbox_rlboxtest32.a \
						 ProcessSandbox_otherside_dummy32 \
						 ProcessSandbox_otherside_jpeg32 \
						 ProcessSandbox_otherside_jpeg32_simd \
						 ProcessSandbox_otherside_png32 \
						 ProcessSandbox_otherside_zlib32 \
						 ProcessSandbox_otherside_theora32 \
						 ProcessSandbox_otherside_vpx32 \
						 ProcessSandbox_otherside_vorbis32 \
						 main32
PRODUCTS64 = library64.so \
						 libProcessSandbox_dummy64.a \
						 libProcessSandbox_jpeg64.a \
						 libProcessSandbox_png64.a \
						 libProcessSandbox_zlib64.a \
						 libProcessSandbox_theora64.a \
						 libProcessSandbox_vpx64.a \
						 libProcessSandbox_vorbis64.a \
						 libProcessSandbox_test64.a \
						 libProcessSandbox_rlboxtest64.a \
						 libProcessSandbox_event64.a \
						 ProcessSandbox_otherside_dummy64 \
						 ProcessSandbox_otherside_jpeg64 \
						 ProcessSandbox_otherside_jpeg64_simd \
						 ProcessSandbox_otherside_png64 \
						 ProcessSandbox_otherside_zlib64 \
						 ProcessSandbox_otherside_theora64 \
						 ProcessSandbox_otherside_vpx64 \
						 ProcessSandbox_otherside_vorbis64 \
						 ProcessSandbox_otherside_test64 \
						 ProcessSandbox_otherside_event64 \
						 main64 \
						 test_dyn_lib.so \
						 dyn_ldr_test_api \
						 dyn_ldr_benchmark

# paths of the ProcessSandbox_otherside executables for the purposes of the 'run' and 'debug' targets
OTHERSIDE32_PATH ?= ProcessSandbox_otherside_dummy32
OTHERSIDE64_PATH ?= ProcessSandbox_otherside_dummy64

# paths to libjpeg-turbo_nacltests, for the purposes of the libjpeg targets
LIBJPEG_TURBO_NACLTESTS_PATH ?= $(shell realpath ../libjpeg-turbo)
LIBJPEG_BUILD32_TARGETNAME ?= x86/non_nacl_build
LIBJPEG_BUILD64_TARGETNAME ?= x64/non_nacl_build
LIBJPEG_BUILD32_SIMD_TARGETNAME ?= x86/non_nacl_build_simd
LIBJPEG_BUILD64_SIMD_TARGETNAME ?= x64/non_nacl_build_simd
LIBJPEG_BUILD32_PATH ?= $(LIBJPEG_TURBO_NACLTESTS_PATH)/builds/$(LIBJPEG_BUILD32_TARGETNAME)
LIBJPEG_BUILD64_PATH ?= $(LIBJPEG_TURBO_NACLTESTS_PATH)/builds/$(LIBJPEG_BUILD64_TARGETNAME)
LIBJPEG_BUILD32_SIMD_PATH ?= $(LIBJPEG_TURBO_NACLTESTS_PATH)/builds/$(LIBJPEG_BUILD32_SIMD_TARGETNAME)
LIBJPEG_BUILD64_SIMD_PATH ?= $(LIBJPEG_TURBO_NACLTESTS_PATH)/builds/$(LIBJPEG_BUILD64_SIMD_TARGETNAME)
LIBJPEG_SO32_PATH ?= $(LIBJPEG_BUILD32_PATH)/.libs
LIBJPEG_SO64_PATH ?= $(LIBJPEG_BUILD64_PATH)/.libs
LIBJPEG_SO32_SIMD_PATH ?= $(LIBJPEG_BUILD32_SIMD_PATH)/.libs
LIBJPEG_SO64_SIMD_PATH ?= $(LIBJPEG_BUILD64_SIMD_PATH)/.libs

# paths to libpng, libz, libtheora, libvpx, libvorbis, and the rlbox api, for the purposes of the corresponding targets
LIBPNG_PATH ?= $(shell realpath ../libpng_nacl)
LIBPNG_SO32_PATH ?= $(LIBPNG_PATH)/builds/x32/non_nacl_build
LIBPNG_SO64_PATH ?= $(LIBPNG_PATH)/builds/x64/non_nacl_build
LIBZ_PATH ?= $(shell realpath ../zlib_nacl)
LIBZ_SO32_PATH ?= $(LIBZ_PATH)/builds/x32/non_nacl_build
LIBZ_SO64_PATH ?= $(LIBZ_PATH)/builds/x64/non_nacl_build
LIBTHEORA_PATH ?= $(shell realpath ../libtheora)
LIBTHEORA_SO32_PATH ?= $(LIBTHEORA_PATH)/builds/x32/non_nacl_build
LIBTHEORA_SO64_PATH ?= $(LIBTHEORA_PATH)/builds/x64/non_nacl_build
LIBVPX_PATH ?= $(shell realpath ../libvpx)
LIBVPX_SO32_PATH ?= $(LIBVPX_PATH)/builds/x32/non_nacl_build
LIBVPX_SO64_PATH ?= $(LIBVPX_PATH)/builds/x64/non_nacl_build
LIBVORBIS_PATH ?= $(shell realpath ../libvorbis)
LIBVORBIS_SO32_PATH ?= $(LIBVORBIS_PATH)/builds/x32/non_nacl_build
LIBVORBIS_SO64_PATH ?= $(LIBVORBIS_PATH)/builds/x64/non_nacl_build
RLBOX_PATH ?= $(shell realpath ../rlbox_api)
RLBOXLIB_SO32_PATH ?= $(RLBOX_PATH)/out/x32
RLBOXLIB_SO64_PATH ?= $(RLBOX_PATH)/out/x64
LIBEVENT_PATH ?= $(shell realpath ../../libraries_native/libevent)

# Core to pin main application process to, for the 'run' and 'debug' targets
MAINCORE=1
# Core to pin sandbox process to, for the 'run' and 'debug' targets
SANDBOXCORE=3
# Number of sandboxes, for the 'run' and 'debug' targets
NUM_SBOXES=2

.PHONY : all all32 all64 run32 run64 debug32 debug64 clean kill_old_processes

all : all32 all64

all32 : $(PRODUCTS32)

all64 : $(PRODUCTS64)
.DEFAULT_GOAL = all64

library32.so : library.cpp library.h
	$(CC) $(CFLAGS32) $(SHARED_LIB_FLAGS) $< -o $@ $(SHARED_LIB_LDFLAGS)

library64.so : library.cpp library.h
	$(CC) $(CFLAGS64) $(SHARED_LIB_FLAGS) $< -o $@ $(SHARED_LIB_LDFLAGS)

test_dyn_lib.so : test_dyn_lib.c test_dyn_lib.h
	$(CC) $(CFLAGS64) $(SHARED_LIB_FLAGS) $< -o $@ $(SHARED_LIB_LDFLAGS)

dyn_ldr_test_api : test_dyn_lib.so dyn_ldr_test_api.cpp libProcessSandbox_test64.a process_sandbox_cpp.h
	$(CC) $(CFLAGS64) -DUSE_TEST dyn_ldr_test_api.cpp libProcessSandbox_test64.a -o $@ $(LIBFLAGS) -l:test_dyn_lib.so

dyn_ldr_benchmark: test_dyn_lib.so dyn_ldr_benchmark.cpp libProcessSandbox_test64.a process_sandbox_cpp.h
	$(CC) $(CFLAGS64) -DUSE_TEST dyn_ldr_benchmark.cpp libProcessSandbox_test64.a -o $@ $(LIBFLAGS) -l:test_dyn_lib.so

libProcessSandbox_dummy32.a : ProcessSandbox_dummy32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_dummy64.a : ProcessSandbox_dummy64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_jpeg32.a : ProcessSandbox_jpeg32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_jpeg64.a : ProcessSandbox_jpeg64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_png32.a : ProcessSandbox_png32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_png64.a : ProcessSandbox_png64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_rlboxtest32.a : ProcessSandbox_rlboxtest32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_rlboxtest64.a : ProcessSandbox_rlboxtest64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_zlib32.a : ProcessSandbox_zlib32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_zlib64.a : ProcessSandbox_zlib64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_pngdec32.a : ProcessSandbox_pngdec32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_pngdec64.a : ProcessSandbox_pngdec64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_theora32.a : ProcessSandbox_theora32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_theora64.a : ProcessSandbox_theora64.o synch64.o timing64.o MyMalloc64.o
	ar crvs $@ $^

libProcessSandbox_vpx32.a : ProcessSandbox_vpx32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_vpx64.a : ProcessSandbox_vpx64.o synch64.o timing64.o MyMalloc64.o
	ar crvs $@ $^

libProcessSandbox_vorbis32.a : ProcessSandbox_vorbis32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_vorbis64.a : ProcessSandbox_vorbis64.o synch64.o timing64.o MyMalloc64.o
	ar crvs $@ $^

libProcessSandbox_test32.a : ProcessSandbox_test32.o synch32.o timing32.o MyMalloc32.o
	ar crvs $@ $^

libProcessSandbox_test64.a : ProcessSandbox_test64.o synch64.o timing64.o
	ar crvs $@ $^

libProcessSandbox_event64.a : ProcessSandbox_event64.o synch64.o timing64.o
	ar crvs $@ $^

ProcessSandbox_otherside_dummy32 : ProcessSandbox_otherside_dummy32.o synch32.o MyMalloc32.o library32.so
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS) -lrary32

ProcessSandbox_otherside_dummy64 : ProcessSandbox_otherside_dummy64.o synch64.o MyMalloc64.o library64.so
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS) -lrary64

ProcessSandbox_otherside_jpeg32 : ProcessSandbox_otherside_jpeg32.o synch32.o MyMalloc32.o $(LIBJPEG_SO32_PATH)/libjpeg.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg64 : ProcessSandbox_otherside_jpeg64.o synch64.o MyMalloc64.o $(LIBJPEG_SO64_PATH)/libjpeg.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg32_simd : ProcessSandbox_otherside_jpeg32_simd.o synch32.o MyMalloc32.o $(LIBJPEG_SO32_SIMD_PATH)/libjpeg.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg64_simd : ProcessSandbox_otherside_jpeg64_simd.o synch64.o MyMalloc64.o $(LIBJPEG_SO64_SIMD_PATH)/libjpeg.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_png32 : ProcessSandbox_otherside_png32.o synch32.o MyMalloc32.o $(LIBPNG_SO32_PATH)/libpng.a $(LIBZ_SO32_PATH)/libz.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_png64 : ProcessSandbox_otherside_png64.o synch64.o MyMalloc64.o $(LIBPNG_SO64_PATH)/libpng.a $(LIBZ_SO64_PATH)/libz.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_zlib32 : ProcessSandbox_otherside_zlib32.o synch32.o MyMalloc32.o $(LIBZ_SO32_PATH)/libz.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_zlib64 : ProcessSandbox_otherside_zlib64.o synch64.o MyMalloc64.o $(LIBZ_SO64_PATH)/libz.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_theora32 : ProcessSandbox_otherside_theora32.o synch32.o MyMalloc32.o $(LIBTHEORA_SO32_PATH)/libtheora.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_theora64 : ProcessSandbox_otherside_theora64.o synch64.o MyMalloc64.o $(LIBTHEORA_SO64_PATH)/libtheora.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_vpx32 : ProcessSandbox_otherside_vpx32.o synch32.o MyMalloc32.o $(LIBVPX_SO32_PATH)/libvpx.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_vpx64 : ProcessSandbox_otherside_vpx64.o synch64.o MyMalloc64.o $(LIBVPX_SO64_PATH)/libvpx.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_vorbis32 : ProcessSandbox_otherside_vorbis32.o synch32.o MyMalloc32.o $(LIBVORBIS_SO32_PATH)/libvorbis.a
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_vorbis64 : ProcessSandbox_otherside_vorbis64.o synch64.o MyMalloc64.o $(LIBVORBIS_SO64_PATH)/libvorbis.a
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_rlboxtest32 : ProcessSandbox_otherside_rlboxtest32.o synch32.o MyMalloc32.o $(RLBOXLIB_SO32_PATH)/libtest.so
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_rlboxtest64 : ProcessSandbox_otherside_rlboxtest64.o synch64.o MyMalloc64.o $(RLBOXLIB_SO64_PATH)/libtest.so
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_pngdec32 : ProcessSandbox_otherside_pngdec32.o synch32.o MyMalloc32.o
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS) -L$(LIBPNG_SO32_PATH)

ProcessSandbox_otherside_pngdec64 : ProcessSandbox_otherside_pngdec64.o synch64.o MyMalloc64.o
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS) -L$(LIBPNG_SO64_PATH)

ProcessSandbox_otherside_test32 : ProcessSandbox_otherside_test32.o synch32.o MyMalloc32.o
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS) -l:test_dyn_lib.so

ProcessSandbox_otherside_test64 : ProcessSandbox_otherside_test64.o synch64.o MyMalloc64.o test_dyn_lib.so
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS) -l:test_dyn_lib.so

ProcessSandbox_otherside_event64 : ProcessSandbox_otherside_event64.o synch64.o MyMalloc64.o
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS) -L$(LIBEVENT_PATH) -levent

ProcessSandbox_otherside_dummy32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h library_helpers.h library32.so
	$(CC) $(CFLAGS32) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS) -lrary32

ProcessSandbox_otherside_dummy64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h library_helpers.h library64.so
	$(CC) $(CFLAGS64) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS) -lrary64

ProcessSandbox_otherside_jpeg32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD32_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD64_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg32_simd.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD32_SIMD_PATH) $(LIBJPEG_SO32_SIMD_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD32_SIMD_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_jpeg64_simd.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD64_SIMD_PATH) $(LIBJPEG_SO64_SIMD_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD64_SIMD_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_png32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h $(LIBPNG_SO32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBPNG -I$(LIBPNG_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_png64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h $(LIBPNG_SO64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBPNG -I$(LIBPNG_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_zlib32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_ZLIB -I$(LIBZ_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_zlib64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_ZLIB -I$(LIBZ_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_pngdec32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h $(LIBPNG_SO32_PATH)
	$(CC) $(CFLAGS32) -DUSE_PNGDEC -c $< -o $@ $(LIBFLAGS) -L$(LIBPNG_SO32_PATH)

ProcessSandbox_otherside_pngdec64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h $(LIBPNG_SO64_PATH)
	$(CC) $(CFLAGS64) -DUSE_PNGDEC -c $< -o $@ $(LIBFLAGS) -L$(LIBPNG_SO64_PATH)

ProcessSandbox_otherside_theora32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libtheora_helpers.h $(LIBTHEORA_SO32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBTHEORA -I$(LIBTHEORA_PATH)/include -c $< -o $@ $(LIBFLAGS) -L$(LIBTHEORA_SO32_PATH)

ProcessSandbox_otherside_theora64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libtheora_helpers.h $(LIBTHEORA_SO64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBTHEORA -I$(LIBTHEORA_PATH)/include -c $< -o $@ $(LIBFLAGS) -L$(LIBTHEORA_SO64_PATH)

ProcessSandbox_otherside_vpx32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvpx_helpers.h $(LIBVPX_SO32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBVPX -I$(LIBVPX_PATH) -c $< -o $@ $(LIBFLAGS) -L$(LIBVPX_SO32_PATH)

ProcessSandbox_otherside_vpx64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvpx_helpers.h $(LIBVPX_SO64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBVPX -I$(LIBVPX_PATH) -c $< -o $@ $(LIBFLAGS) -L$(LIBVPX_SO64_PATH)

ProcessSandbox_otherside_vorbis32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvorbis_helpers.h $(LIBVORBIS_SO32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBVORBIS -I$(LIBVORBIS_PATH)/include -c $< -o $@ $(LIBFLAGS) -L$(LIBVORBIS_SO32_PATH)

ProcessSandbox_otherside_vorbis64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvorbis_helpers.h $(LIBVORBIS_SO64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBVORBIS -I$(LIBVORBIS_PATH)/include -c $< -o $@ $(LIBFLAGS) -L$(LIBVORBIS_SO64_PATH)

ProcessSandbox_otherside_test32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_TEST -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_test64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_TEST -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_rlboxtest32.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h rlboxtestlib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_RLBOXTEST -I$(RLBOX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_rlboxtest64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h rlboxtestlib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_RLBOXTEST -I$(RLBOX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_otherside_event64.o : ProcessSandbox_otherside.cpp ProcessSandbox_sharedmem.h synch.h MyMalloc.h libevent_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBEVENT -I$(LIBEVENT_PATH)/include -c $< -o $@ $(LIBFLAGS)

main32: main32.o libProcessSandbox_dummy32.a library32.so
	$(CC) $(CFLAGS32) $^ -o $@ $(LIBFLAGS) -lrary32

main64: main64.o libProcessSandbox_dummy64.a library64.so
	$(CC) $(CFLAGS64) $^ -o $@ $(LIBFLAGS) -lrary64

ProcessSandbox_dummy32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h library_helpers.h
	$(CC) $(CFLAGS32) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_dummy64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h library_helpers.h
	$(CC) $(CFLAGS64) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_jpeg32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD32_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD32_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_jpeg64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD64_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD64_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_jpeg32_simd.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD32_SIMD_PATH)
	$(CC) $(CFLAGS32) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD32_SIMD_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS) -L$(LIBJPEG_SO32_SIMD_PATH) -ljpeg

ProcessSandbox_jpeg64_simd.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h jpeglib_helpers.h $(LIBJPEG_BUILD64_SIMD_PATH)
	$(CC) $(CFLAGS64) -DUSE_LIBJPEG -I$(LIBJPEG_BUILD64_SIMD_PATH) -I$(LIBJPEG_TURBO_NACLTESTS_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_png32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_LIBPNG -I$(LIBPNG_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_png64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBPNG -I$(LIBPNG_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_rlboxtest32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h rlboxtestlib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_RLBOXTEST -I$(RLBOX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_rlboxtest64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h rlboxtestlib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_RLBOXTEST -I$(RLBOX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_zlib32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h zlib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_ZLIB -I$(LIBZ_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_zlib64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h zlib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_ZLIB -I$(LIBZ_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_pngdec32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_PNGDEC -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_pngdec64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h pnglib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_PNGDEC -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_theora32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libtheora_helpers.h
	$(CC) $(CFLAGS32) -DUSE_LIBTHEORA -I$(LIBTHEORA_PATH)/include -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_theora64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libtheora_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBTHEORA -I$(LIBTHEORA_PATH)/include -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_vpx32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvpx_helpers.h
	$(CC) $(CFLAGS32) -DUSE_LIBVPX -I$(LIBVPX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_vpx64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvpx_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBVPX -I$(LIBVPX_PATH) -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_vorbis32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvorbis_helpers.h
	$(CC) $(CFLAGS32) -DUSE_LIBVORBIS -I$(LIBVORBIS_PATH)/include -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_vorbis64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libvorbis_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBVORBIS -I$(LIBVORBIS_PATH)/include -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_test32.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h test_dyn_lib_helpers.h
	$(CC) $(CFLAGS32) -DUSE_TEST -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_test64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h test_dyn_lib_helpers.h
	$(CC) $(CFLAGS64) -DUSE_TEST -c $< -o $@ $(LIBFLAGS)

ProcessSandbox_event64.o : ProcessSandbox.cpp ProcessSandbox.h ProcessSandbox_sharedmem.h synch.h MyMalloc.h libevent_helpers.h
	$(CC) $(CFLAGS64) -DUSE_LIBEVENT -I$(LIBEVENT_PATH)/include -c $< -o $@ $(LIBFLAGS)

main32.o : main.cpp ProcessSandbox.h myHelpers.h timing.h library.h library32.so
	$(CC) $(CFLAGS32) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS) -lrary32

main64.o : main.cpp ProcessSandbox.h myHelpers.h timing.h library.h library64.so
	$(CC) $(CFLAGS64) -DUSE_DUMMY_LIB -c $< -o $@ $(LIBFLAGS) -lrary64

synch32.o : synch.cpp synch.h
	$(CC) $(CFLAGS32) -c $< -o $@ $(LIBFLAGS)

synch64.o : synch.cpp synch.h
	$(CC) $(CFLAGS64) -c $< -o $@ $(LIBFLAGS)

timing32.o : timing.cpp timing.h
	$(CC) $(CFLAGS32) -c $< -o $@ $(LIBFLAGS)

timing64.o : timing.cpp timing.h
	$(CC) $(CFLAGS64) -c $< -o $@ $(LIBFLAGS)

MyMalloc32.o : MyMalloc.cpp MyMalloc.h dlmalloc.c
	$(CC) $(CFLAGS32) -c $< -o $@ $(LIBFLAGS)

MyMalloc64.o : MyMalloc.cpp MyMalloc.h dlmalloc.c
	$(CC) $(CFLAGS64) -c $< -o $@ $(LIBFLAGS)

run32 : main32 $(OTHERSIDE32_PATH) kill_old_processes
	LD_LIBRARY_PATH=".:$$LD_LIBRARY_PATH" ./main32 $(OTHERSIDE32_PATH) $(MAINCORE) $(SANDBOXCORE) $(NUM_SBOXES)

run64 : main64 $(OTHERSIDE64_PATH) kill_old_processes
	LD_LIBRARY_PATH=".:$$LD_LIBRARY_PATH" ./main64 $(OTHERSIDE64_PATH) $(MAINCORE) $(SANDBOXCORE) $(NUM_SBOXES)

debug32 : main32 $(OTHERSIDE32_PATH) kill_old_processes
	LD_LIBRARY_PATH=".:$$LD_LIBRARY_PATH" gdb --args ./main32 $(OTHERSIDE32_PATH) $(MAINCORE) $(SANDBOXCORE) $(NUM_SBOXES)

debug64 : main64 $(OTHERSIDE64_PATH) kill_old_processes
	LD_LIBRARY_PATH=".:$$LD_LIBRARY_PATH" gdb --args ./main64 $(OTHERSIDE64_PATH) $(MAINCORE) $(SANDBOXCORE) $(NUM_SBOXES)

kill_old_processes :
	-pkill -f ProcessSandbox_otherside

clean:
	-rm -f $(PRODUCTS32) $(PRODUCTS64) *.o
