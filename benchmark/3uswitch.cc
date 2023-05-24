#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <sys/syscall.h>
#include "zlib.h"
#include "uswitchsandbox.h"
#include "uswitch.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_gzip_file(USwitchSandbox *sandbox, uint8_t *input, size_t in_size, uint8_t *output, size_t out_size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(inflateInit2_);
    GET_FUNC_PTR(inflate);
    GET_FUNC_PTR(inflateEnd);
#undef GET_FUNC_PTR
    z_stream *stream = (z_stream *)sandbox->malloc_in_sandbox(sizeof(z_stream));
    size_t len = strlen(ZLIB_VERSION);
    char *ver_str = (char *)sandbox->malloc_in_sandbox(len + 1);
    
    memcpy(ver_str, ZLIB_VERSION, len + 1);
    memset(stream, 0, sizeof(z_stream));
    uswctx_t ctx = sandbox->get_context();
    int res;
    uswitch_call_dynamic(ctx, inflateInit2__s, res, stream, 16 + MAX_WBITS, ver_str, sizeof(z_stream));
    if (res != Z_OK) {
        return;
    }
    stream->avail_in = in_size;
    stream->next_in = input;
    stream->avail_out = out_size;
    stream->next_out = output;
    uswitch_call_dynamic(ctx, inflate_s, res, stream, Z_NO_FLUSH);
    if (res == Z_STREAM_ERROR) {
        return;
    }
    uswitch_call_dynamic(ctx, inflateEnd_s, nullptr, stream);
    sandbox->free_in_sandbox(stream);
    sandbox->free_in_sandbox(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test3-uswitch <filename> <times>\n";
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
    USwitchSandbox sandbox("../libraries_uswitch/zlib/libz.so", 1024l << 20, 2l << 20);
    sandbox.init();
    static const std::vector<unsigned int> AllowedSyscalls {
#ifdef ONLYMEMPROT
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_newfstatat,
#endif
        __NR_exit, __NR_futex, __NR_sched_yield, 451};
    sandbox.init_seccomp(AllowedSyscalls);
    std::vector<uint64_t> times(n);
    uint8_t *input_s = (uint8_t *)sandbox.malloc_in_sandbox(size);
    uint8_t *output = (uint8_t *)sandbox.malloc_in_sandbox(size * 2);
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