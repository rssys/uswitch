#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <sys/syscall.h>
#include "png.h"
#include "uswitchsandbox.h"
#include "uswitch.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

void info_callback(uswctx_t ctx, void *data, png_structp png, png_infop info) {
    uswitch_call_dynamic(ctx, (decltype(png_read_update_info) *)data, png, info);
}

void row_callback(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass) {
}

void end_callback(png_structp png, png_infop info) {
}

void (*info_callback_uswitch)(png_structp png, png_infop info);

void (*row_callback_uswitch)(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass);

void (*end_callback_uswitch)(png_structp png, png_infop info);


static void load_png_file(USwitchSandbox *sandbox, uint8_t *input, size_t size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(png_create_read_struct);
    GET_FUNC_PTR(png_create_info_struct);
    GET_FUNC_PTR(png_set_progressive_read_fn);
    GET_FUNC_PTR(png_process_data);
    GET_FUNC_PTR(png_destroy_read_struct);
#undef GET_FUNC_PTR
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->malloc_in_sandbox(size);
    uswctx_t ctx = sandbox->get_context();
    memcpy(sandbox_buffer, input, size);
    png_structp *png = (png_structp *)sandbox->malloc_in_sandbox(sizeof(png_structp));
    size_t len = strlen(PNG_LIBPNG_VER_STRING);
    char *ver_str = (char *)sandbox->malloc_in_sandbox(len + 1);
    memcpy(ver_str, PNG_LIBPNG_VER_STRING, len + 1);
    uswitch_call_dynamic(ctx, png_create_read_struct_s, png, ver_str, nullptr, nullptr, nullptr);
    png_infop *info = (png_infop *)sandbox->malloc_in_sandbox(sizeof(png_infop));
    uswitch_call_dynamic(ctx, png_create_info_struct_s, info, *png);
    uswitch_call_dynamic(ctx, png_set_progressive_read_fn_s, *png, nullptr, info_callback_uswitch, row_callback_uswitch, end_callback_uswitch);
    uswitch_call_dynamic(ctx, png_process_data_s, *png, *info, sandbox_buffer, size);
    uswitch_call_dynamic(ctx, png_destroy_read_struct_s, png, info, nullptr);
    sandbox->free_in_sandbox(png);
    sandbox->free_in_sandbox(info);
    sandbox->free_in_sandbox(sandbox_buffer);
    sandbox->free_in_sandbox(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test2-uswitch <filename> <times>\n";
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
    USwitchSandbox sandbox("../libraries_uswitch/libpng/libpng.so", 1024l << 20, 2l << 20);
    sandbox.init();
    static const std::vector<unsigned int> AllowedSyscalls {
#ifdef ONLYMEMPROT
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_newfstatat,
#endif
        __NR_exit, __NR_futex, __NR_sched_yield, 451};
    sandbox.init_seccomp(AllowedSyscalls);
    uswctx_t ctx = sandbox.get_context();
    info_callback_uswitch = uswitch_register_callback_get_fp(16, ctx, sandbox.get_symbol_addr("png_read_update_info"), info_callback);
    row_callback_uswitch = uswitch_register_callback_get_fp(16, ctx, row_callback);
    end_callback_uswitch = uswitch_register_callback_get_fp(16, ctx, end_callback);
    std::vector<uint64_t> times(n);
    for (int i= 0; i < n; ++i) {
        uint64_t t1 = time_nanosec();
        load_png_file(&sandbox, input, size);
        uint64_t t2 = time_nanosec();
        times[i] = t2 - t1;
    }
    for (int i = 0; i < n; ++i) {
        std::cout << times[i] << std::endl;
    }
    //printf("%lu\n", (t2 - t1) / n);
    return 0;
}