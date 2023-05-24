#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <sys/syscall.h>
#include "png.h"
#include "RLBox_NaCl.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

void info_callback(png_structp png, png_infop info, void *data) {
    RLBox_NaCl *sandbox = (RLBox_NaCl *)data;
    sandbox->impl_InvokeFunction((decltype(png_read_update_info) *)sandbox->impl_LookupSymbol("png_read_update_info", false), png, info);
}

void row_callback(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass, void *data) {
}

void end_callback(png_structp png, png_infop info, void *data) {
}

void (*info_callback_nacl)(png_structp png, png_infop info);

void (*row_callback_nacl)(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass);

void (*end_callback_nacl)(png_structp png, png_infop info);


static void load_png_file(RLBox_NaCl *sandbox, uint8_t *input, size_t size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->impl_LookupSymbol(#name, false)
    GET_FUNC_PTR(png_create_read_struct);
    GET_FUNC_PTR(png_create_info_struct);
    GET_FUNC_PTR(png_set_progressive_read_fn);
    GET_FUNC_PTR(png_process_data);
    GET_FUNC_PTR(png_destroy_read_struct);
#undef GET_FUNC_PTR
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->impl_mallocInSandbox(size);
    memcpy(sandbox_buffer, input, size);
    size_t len = strlen(PNG_LIBPNG_VER_STRING);
    char *ver_str = (char *)sandbox->impl_mallocInSandbox(len + 1);
    memcpy(ver_str, PNG_LIBPNG_VER_STRING, len + 1);
    png_structp png = sandbox->impl_InvokeFunction(png_create_read_struct_s, ver_str, nullptr, nullptr, nullptr);
    png_infop info = sandbox->impl_InvokeFunction(png_create_info_struct_s, png);
    sandbox->impl_InvokeFunction(png_set_progressive_read_fn_s, png, nullptr, info_callback_nacl, row_callback_nacl, end_callback_nacl);
    sandbox->impl_InvokeFunction(png_process_data_s, png, info, sandbox_buffer, size);
    sandbox->impl_InvokeFunction(png_destroy_read_struct_s, png, info, nullptr);
    sandbox->impl_freeInSandbox(sandbox_buffer);
    sandbox->impl_freeInSandbox(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test2-nacl <filename> <times>\n";
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
    RLBox_NaCl sandbox;
    sandbox.impl_CreateSandbox("Sandboxing_NaCl/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe", "../rlbox_build/libpng_nacl/builds/x64/nacl_build/mainCombine/libpng.nexe");
    info_callback_nacl = (decltype(info_callback_nacl))sandbox.impl_RegisterCallback<void, png_structp, png_infop>((void *)info_callback, (void *)info_callback, (void *)&sandbox);
    row_callback_nacl = (decltype(row_callback_nacl))sandbox.impl_RegisterCallback<void, png_structp, png_bytep, png_uint_32, int>((void *)row_callback, (void *)row_callback, (void *)&sandbox);
    end_callback_nacl = (decltype(end_callback_nacl))sandbox.impl_RegisterCallback<void, png_structp, png_infop>((void *)end_callback, (void *)end_callback, (void *)&sandbox);
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
    sandbox.impl_DestroySandbox();
    return 0;
}