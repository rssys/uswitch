#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "zlib.h"
#include "wasmsandbox.h"
#include "wasm.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_gzip_file(WasmSandbox *sandbox, uint8_t *input, size_t in_size, uint8_t *output, size_t out_size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(inflateInit2_);
    GET_FUNC_PTR(inflate);
    GET_FUNC_PTR(inflateEnd);
#undef GET_FUNC_PTR
    z_stream *stream = (z_stream *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(z_stream));
    size_t len = strlen(ZLIB_VERSION);
    char *ver_str = (char *)sandbox->malloc_in_sandbox_unsandboxed(len + 1);
    memcpy(ver_str, ZLIB_VERSION, len + 1);
    memset(stream, 0, sizeof(z_stream));
    if (wasm_call(sandbox, decltype(inflateInit2__s), (void *)inflateInit2__s, stream, 16 + MAX_WBITS, ver_str, sizeof(z_stream)) != Z_OK) {
        return;
    }
    stream->avail_in = in_size;
    stream->next_in = (uint8_t *)(uintptr_t)sandbox->get_sandboxed_pointer<uint8_t>(input);
    stream->avail_out = out_size;
    stream->next_out = (uint8_t *)(uintptr_t)sandbox->get_sandboxed_pointer<uint8_t>(output);
    if (wasm_call(sandbox, decltype(inflate_s), (void *)inflate_s, stream, Z_NO_FLUSH) == Z_STREAM_ERROR) {
        return;
    }
    wasm_call(sandbox, decltype(inflateEnd_s), (void *)inflateEnd_s, stream);
    sandbox->free_in_sandbox_unsandboxed(stream);
    sandbox->free_in_sandbox_unsandboxed(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test3-wasm <filename> <times>\n";
        return 1;
    }
    const char *filename = argv[1];
    int n = atoi(argv[2]);
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs) {
        std::cerr << "Failed to open file: " << filename << std::endl;
        return 1;
    }
    ifs.seekg(0, std::ios_base::end);
    size_t size = ifs.tellg();
    ifs.seekg(0, std::ios_base::beg);
    uint8_t *input = new uint8_t[size];
    if (!ifs.read((char *)input, size)) {
        std::cerr << "Failed to read file\n";
        return 1;
    }
    WasmSandbox sandbox("../libraries_wasm/zlib/libz.so", "libz_");
    sandbox.init();
    std::vector<uint64_t> times(n);
    uint8_t *input_s = (uint8_t *)sandbox.malloc_in_sandbox_unsandboxed(size);
    uint8_t *output = (uint8_t *)sandbox.malloc_in_sandbox_unsandboxed(size * 2);
    memcpy(input_s, input, size);
    for (int i= 0; i < n; ++i) {
        uint64_t t1 = time_nanosec();
        load_gzip_file(&sandbox, input_s, size, output, size * 2);
        uint64_t t2 = time_nanosec();
        times[i] = t2 - t1;
    }
    for (int i = 0; i < n; ++i) {
        std::cout << times[i] << std::endl;
    }
    //printf("%lu\n", (t2 - t1) / n);
    return 0;
}