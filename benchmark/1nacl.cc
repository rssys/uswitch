#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "RLBox_NaCl.h"
#include "jpeglib.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_jpeg_file(RLBox_NaCl *sandbox, uint8_t *input, size_t size) {
    struct jpeg_decompress_struct *cinfo = (struct jpeg_decompress_struct *)sandbox->impl_mallocInSandbox(sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr *jerr = (struct jpeg_error_mgr *)sandbox->impl_mallocInSandbox(sizeof(struct jpeg_error_mgr));
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->impl_mallocInSandbox(size);
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->impl_LookupSymbol(#name, false)
    GET_FUNC_PTR(jpeg_CreateDecompress);
    GET_FUNC_PTR(jpeg_std_error);
    GET_FUNC_PTR(jpeg_mem_src);
    GET_FUNC_PTR(jpeg_read_header);
    GET_FUNC_PTR(jpeg_start_decompress);
    GET_FUNC_PTR(jpeg_read_scanlines);
    GET_FUNC_PTR(jpeg_finish_decompress);
    GET_FUNC_PTR(jpeg_destroy_decompress);
#undef GET_FUNC_PTR
    JSAMPARRAY (*alloc_sarray)(j_common_ptr cinfo, int pool_id, JDIMENSION samplesperrow, JDIMENSION numrows);
    alloc_sarray = (decltype(alloc_sarray))sandbox->impl_LookupSymbol("alloc_sarray", false);

    memcpy(sandbox_buffer, input, size);
    cinfo->err = sandbox->impl_InvokeFunction(jpeg_std_error_s, jerr);
    sandbox->impl_InvokeFunction(jpeg_CreateDecompress_s, cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));
    sandbox->impl_InvokeFunction(jpeg_mem_src_s, cinfo, sandbox_buffer, size);
    sandbox->impl_InvokeFunction(jpeg_read_header_s, cinfo, TRUE);
    sandbox->impl_InvokeFunction(jpeg_start_decompress_s, cinfo);
    int row_stride = cinfo->output_width * cinfo->output_components;
    JSAMPARRAY buffer = sandbox->impl_InvokeFunction(alloc_sarray, (j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);
    while (cinfo->output_scanline < cinfo->output_height) {
        sandbox->impl_InvokeFunction(jpeg_read_scanlines_s, cinfo, buffer, 1);
    }
    sandbox->impl_InvokeFunction(jpeg_finish_decompress_s, cinfo);
    sandbox->impl_InvokeFunction(jpeg_destroy_decompress_s, cinfo);
    sandbox->impl_freeInSandbox(sandbox_buffer);
    sandbox->impl_freeInSandbox(cinfo);
    sandbox->impl_freeInSandbox(jerr);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test1-nacl <filename> <times>\n";
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
    sandbox.impl_CreateSandbox("Sandboxing_NaCl/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe", "../rlbox_build/libjpeg-turbo/builds/x64/nacl_build_simd/mainCombine/libjpeg.nexe");
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
    //printf("%lu\n", (t2 - t1) / n);
    sandbox.impl_DestroySandbox();
    return 0;
}