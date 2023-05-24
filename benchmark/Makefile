CXX=g++
all: test1-ps test1-native test1-uswitch test1-wasm \
	test2-native test2-uswitch test2-wasm test2-ps \
	test3-native test3-uswitch test3-wasm test3-ps \
	test4-native test4-uswitch test4-wasm test4-ps \
	test5-native test5-uswitch test5-wasm test5-ps \
	test6-native test6-uswitch test6-wasm test6-ps \
	test8-native test8-uswitch test8-ps
test1-ps: 1ps.cc
	$(CXX) 1ps.cc -std=c++17 -O3 -g -o test1-ps -I../libraries_uswitch/libjpeg -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_jpeg64 -pthread -lrt
test1-native: 1native.cc
	$(CXX) 1native.cc -std=c++17 -O3 -g -o test1-native -I../libraries_native/libjpeg -L../libraries_native/libjpeg -ljpeg -DENABLE_SECCOMP
test1-wasm: 1wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test1-wasm -I../libraries_uswitch/libjpeg -I../rlbox_wasm/ \
		1wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test1-uswitch: 1uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test1-uswitch -I../libraries_uswitch/libjpeg -I../libuswitch -I../rlbox_uswitch/ \
		1uswitch.cc ../libuswitch/libuswitch.a ../rlbox_uswitch/librlbox_uswitch.a -ldl -pthread
test2-native: 2native.cc
	$(CXX) 2native.cc -std=c++17 -O3 -g -o test2-native -I../libraries_native/libpng -L../libraries_native/libpng -lpng -DENABLE_SECCOMP
test2-uswitch: 2uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test2-uswitch -I../libraries_uswitch/libpng -I../libuswitch -I../rlbox_uswitch/ \
		2uswitch.cc ../libuswitch/libuswitch.a ../rlbox_uswitch/librlbox_uswitch.a -ldl -pthread
test2-wasm: 2wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test2-wasm -I../libraries_uswitch/libpng -I../rlbox_wasm/ \
		2wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test2-ps: 2ps.cc
	$(CXX) 2ps.cc -std=c++17 -O3 -g -o test2-ps -I../libraries_uswitch/libpng -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_png64 -pthread -lrt
test3-native: 3native.cc
	$(CXX) 3native.cc -std=c++17 -O3 -g -o test3-native -I../libraries_native/zlib -L../libraries_native/libz -lz -DENABLE_SECCOMP
test3-uswitch: 3uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test3-uswitch -I../libraries_uswitch/zlib -I../libuswitch -I../rlbox_uswitch/ \
		3uswitch.cc ../libuswitch/libuswitch.a ../rlbox_uswitch/librlbox_uswitch.a -ldl -pthread
test3-wasm: 3wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test3-wasm -I../libraries_uswitch/zlib -I../rlbox_wasm/ \
		3wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test3-ps: 3ps.cc
	$(CXX) 3ps.cc -std=c++17 -O3 -g -o test3-ps -I../libraries_uswitch/zlib -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_zlib64 -pthread -lrt
test4-native: 4native.cc
	$(CXX) 4native.cc -std=c++17 -O3 -g -o test4-native -I../libraries_native/libjpeg -L../libraries_native/libjpeg -ljpeg
test4-uswitch: 4uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test4-uswitch -I../libuswitch -I../rlbox_uswitch/ \
		4uswitch.cc ../rlbox_uswitch/librlbox_uswitch.a ../libuswitch/libuswitch.a -ldl -pthread
test4-wasm: 4wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test4-wasm -I../rlbox_wasm/ 4wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test4-ps: 4ps.cc
	$(CXX) 4ps.cc -std=c++17 -O3 -g -o test4-ps -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_dummy64 -pthread -lrt
test5-native: 5native.cc
	$(CXX) 5native.cc -std=c++17 -O3 -g -o test5-native -I../libraries_native/libjpeg -L../libraries_native/libjpeg -ljpeg
test5-uswitch: 5uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test5-uswitch -I../libuswitch -I../rlbox_uswitch/ \
		5uswitch.cc ../rlbox_uswitch/librlbox_uswitch.a ../libuswitch/libuswitch.a -ldl -pthread
test5-wasm: 5wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test5-wasm -I../rlbox_wasm/ 5wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test5-ps: 5ps.cc
	$(CXX) 5ps.cc -std=c++17 -O3 -g -o test5-ps -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_dummy64 -pthread -lrt
test6-native: 6native.cc
	$(CXX) 6native.cc -std=c++17 -O3 -g -o test6-native -I../libraries_native/libvpx -L../libraries_native/libvpx -lvpx -DENABLE_SECCOMP
test6-uswitch: 6uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test6-uswitch -I../libraries_uswitch/libvpx -I../libuswitch -I../rlbox_uswitch/ \
		6uswitch.cc ../libuswitch/libuswitch.a ../rlbox_uswitch/librlbox_uswitch.a -ldl -pthread
test6-wasm: 6wasm.cc
	$(CXX) -std=c++17 -O3 -g -o test6-wasm -I../libraries_uswitch/libvpx -I../rlbox_wasm/ \
		6wasm.cc ../rlbox_wasm/librlbox_wasm.a -ldl -pthread
test6-ps: 6ps.cc
	$(CXX) 6ps.cc -std=c++17 -O3 -g -o test6-ps -I../libraries_uswitch/libvpx -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_vpx64 -pthread -lrt
test8-native: 8native.cc
	$(CXX) 8native.cc -std=c++17 -O3 -g -o test8-native -I../libraries_native/libevent/include -L../libraries_native/libevent -levent -DENABLE_SECCOMP
test8-uswitch: 8uswitch.cc
	$(CXX) -std=c++17 -O3 -g -o test8-uswitch -I../libraries_uswitch/libevent/include -I../libuswitch -I../rlbox_uswitch/ \
		8uswitch.cc ../libuswitch/libuswitch.a ../rlbox_uswitch/librlbox_uswitch.a -ldl -pthread
test8-ps: 8ps.cc
	$(CXX) 8ps.cc -std=c++17 -O3 -g -o test8-ps -I../libraries_uswitch/libevent/include -I./ProcessSandbox \
		-L./ProcessSandbox -lProcessSandbox_event64 -pthread -lrt
clean:
	rm -f test1-ps test1-native test1-uswitch test1-wasm \
		test2-native test2-uswitch test2-wasm test2-ps \
		test3-native test3-uswitch test3-wasm test3-ps \
		test4-native test4-uswitch test4-wasm test4-ps \
		test5-native test5-uswitch test5-wasm test5-ps \
		test6-native test6-uswitch test6-wasm test6-ps \
		test8-native test8-uswitch test8-ps
