#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#define USE_LIBJPEG
#include "ProcessSandbox.h"
#include "jpeglib.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_jpeg_file(JPEGProcessSandbox *sandbox, uint8_t *input, size_t size) {
    struct jpeg_decompress_struct *cinfo = (struct jpeg_decompress_struct *)sandbox->mallocInSandbox(sizeof(struct jpeg_decompress_struct));
    struct jpeg_error_mgr *jerr = (struct jpeg_error_mgr *)sandbox->mallocInSandbox(sizeof(struct jpeg_error_mgr));
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->mallocInSandbox(size);
    memcpy(sandbox_buffer, input, size);
    cinfo->err = sandbox->inv_jpeg_std_error(jerr);
    sandbox->inv_jpeg_CreateDecompress(cinfo, JPEG_LIB_VERSION, sizeof(struct jpeg_decompress_struct));
    sandbox->inv_jpeg_mem_src(cinfo, sandbox_buffer, size);
    sandbox->inv_jpeg_read_header(cinfo, TRUE);
    sandbox->inv_jpeg_start_decompress(cinfo);
    int row_stride = cinfo->output_width * cinfo->output_components;
    JSAMPARRAY buffer = sandbox->inv_alloc_sarray_ps((j_common_ptr)cinfo, JPOOL_IMAGE, row_stride, 1);
    while (cinfo->output_scanline < cinfo->output_height) {
        sandbox->inv_jpeg_read_scanlines(cinfo, buffer, 1);
    }
    sandbox->inv_jpeg_finish_decompress(cinfo);
    sandbox->inv_jpeg_destroy_decompress(cinfo);
    sandbox->freeInSandbox(sandbox_buffer);
    sandbox->freeInSandbox(cinfo);
    sandbox->freeInSandbox(jerr);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test1-ps <filename> <times>\n";
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
    JPEGProcessSandbox sandbox("./ProcessSandbox/ProcessSandbox_otherside_jpeg64_simd", 9999, 0);
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
    sandbox.destroySandbox();
    return 0;
}