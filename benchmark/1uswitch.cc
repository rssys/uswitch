#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <sys/syscall.h>
#include "jpeglib.h"
#include "uswitchsandbox.h"
#include "uswitch.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_jpeg_file(USwitchSandbox *sandbox, uint8_t *input, size_t size) {
    struct jpeg_decompress_struct *cinfo = (struct jpeg_decompress_struct *)sandbox->malloc_in_sandbox(sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr *jerr = (struct jpeg_error_mgr *)sandbox->malloc_in_sandbox(sizeof(struct jpeg_error_mgr));
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->malloc_in_sandbox(size);
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(jpeg_CreateDecompress);
    GET_FUNC_PTR(jpeg_std_error);
    GET_FUNC_PTR(jpeg_mem_src);
    GET_FUNC_PTR(jpeg_read_header);
    GET_FUNC_PTR(jpeg_start_decompress);
    GET_FUNC_PTR(jpeg_read_scanlines);
    GET_FUNC_PTR(jpeg_finish_decompress);
    GET_FUNC_PTR(jpeg_destroy_decompress);
#undef GET_FUNC_PTR

    uswctx_t ctx = sandbox->get_context();
    memcpy(sandbox_buffer, input, size);
    uswitch_call_dynamic(ctx, jpeg_std_error_s, cinfo->err, jerr);
    uswitch_call_dynamic(ctx, jpeg_CreateDecompress_s, cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));
    uswitch_call_dynamic(ctx, jpeg_mem_src_s, cinfo, sandbox_buffer, size);
    uswitch_call_dynamic(ctx, jpeg_read_header_s, nullptr, cinfo, TRUE);
    uswitch_call_dynamic(ctx, jpeg_start_decompress_s, nullptr, cinfo);
    int row_stride = cinfo->output_width * cinfo->output_components;
    JSAMPARRAY buffer;
    uswitch_call_dynamic(ctx, cinfo->mem->alloc_sarray, buffer, (j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);
    while (cinfo->output_scanline < cinfo->output_height) {
        uswitch_call_dynamic(ctx, jpeg_read_scanlines_s, nullptr, cinfo, buffer, 1);
    }
    uswitch_call_dynamic(ctx, jpeg_finish_decompress_s, nullptr, cinfo);
    uswitch_call_dynamic(ctx, jpeg_destroy_decompress_s, cinfo);
    sandbox->free_in_sandbox(sandbox_buffer);
    sandbox->free_in_sandbox(cinfo);
    sandbox->free_in_sandbox(jerr);
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: ./test1-uswitch <filename> <times> [print]\n";
        return 1;
    }
    const char *filename = argv[1];
    int n = atoi(argv[2]);
    bool print = !argv[3] || atoi(argv[3]);
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
    USwitchSandbox sandbox("../libraries_uswitch/libjpeg/libjpeg.so", 1024l << 20, 2l << 20);
    sandbox.init();

    static const std::vector<unsigned int> AllowedSyscalls {
#ifdef ONLYMEMPROT
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_newfstatat,
#endif
        __NR_exit, __NR_futex, __NR_sched_yield, 451};
    sandbox.init_seccomp(AllowedSyscalls);
    if (print) {
        std::vector<uint64_t> times(n);
        for (int i= 0; i < n; ++i) {
            uint64_t t1 = time_nanosec();
            load_jpeg_file(&sandbox, input, size);
            uint64_t t2 = time_nanosec();
            times[i] = t2 - t1;
        }
        for (int i = 0; i < n; ++i) {
            std::cout << times[i] << std::endl;
        }
    } else {
        uint64_t t1 = time_nanosec();
        for (int i= 0; i < n; ++i) {
            load_jpeg_file(&sandbox, input, size);
        }
        uint64_t t2 = time_nanosec();
        std::cout << t2 - t1 << std::endl;
    }
    //printf("%lu\n", (t2 - t1) / n);
    return 0;
}