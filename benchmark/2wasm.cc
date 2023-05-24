#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "png.h"
#include "wasmsandbox.h"
#include "wasm.hpp"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

void info_callback(png_structp png, png_infop info, void *data) {
    WasmSandbox *sandbox = (WasmSandbox *)data;
    wasm_call(sandbox, decltype(png_read_update_info) *, sandbox->get_symbol_addr("png_read_update_info"), png, info);
}

void row_callback(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass, void *data) {
}

void end_callback(png_structp png, png_infop info, void *data) {
}

void (*info_callback_wasm)(png_structp png, png_infop info);

void (*row_callback_wasm)(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass);

void (*end_callback_wasm)(png_structp png, png_infop info);


static void load_png_file(WasmSandbox *sandbox, uint8_t *input, size_t size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(png_create_read_struct);
    GET_FUNC_PTR(png_create_info_struct);
    GET_FUNC_PTR(png_set_progressive_read_fn);
    GET_FUNC_PTR(png_process_data);
    GET_FUNC_PTR(png_destroy_read_struct);
#undef GET_FUNC_PTR
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->malloc_in_sandbox_unsandboxed(size);
    memcpy(sandbox_buffer, input, size);
    png_structp *png = (png_structp *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(png_structp));
    size_t len = strlen(PNG_LIBPNG_VER_STRING);
    char *ver_str = (char *)sandbox->malloc_in_sandbox_unsandboxed(len + 1);
    memcpy(ver_str, PNG_LIBPNG_VER_STRING, len + 1);
    *png = wasm_call(sandbox, decltype(png_create_read_struct_s), (void *)png_create_read_struct_s, ver_str, nullptr, nullptr, nullptr);
    png_infop *info = (png_infop *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(png_infop));
    *info = wasm_call(sandbox, decltype(png_create_info_struct_s), (void *)png_create_info_struct_s, *png);
    wasm_call(sandbox, decltype(png_set_progressive_read_fn_s), (void *)png_set_progressive_read_fn_s, *png, nullptr, info_callback_wasm, row_callback_wasm, end_callback_wasm);
    wasm_call(sandbox, decltype(png_process_data_s), (void *)png_process_data_s, *png, *info, sandbox_buffer, size);
    wasm_call(sandbox, decltype(png_destroy_read_struct_s), (void *)png_destroy_read_struct_s, png, info, nullptr);
    sandbox->free_in_sandbox_unsandboxed(png);
    sandbox->free_in_sandbox_unsandboxed(info);
    sandbox->free_in_sandbox_unsandboxed(sandbox_buffer);
    sandbox->free_in_sandbox_unsandboxed(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test2-wasm <filename> <times>\n";
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
    WasmSandbox sandbox("../libraries_wasm/libpng/libpng.so", "libpng_");
    sandbox.init();
    uint32_t cb;
    info_callback_wasm;
    rlbox_wasm::Callback<16, decltype(info_callback_wasm)>::register_callback(&sandbox, info_callback, (void *)&sandbox, cb);
    info_callback_wasm = (decltype(info_callback_wasm))(uintptr_t)cb;
    rlbox_wasm::Callback<16, decltype(row_callback_wasm)>::register_callback(&sandbox, row_callback, nullptr, cb);
    row_callback_wasm = (decltype(row_callback_wasm))(uintptr_t)cb;
    rlbox_wasm::Callback<16, decltype(end_callback_wasm)>::register_callback(&sandbox, end_callback, nullptr, cb);
    end_callback_wasm = (decltype(end_callback_wasm))(uintptr_t)cb;
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