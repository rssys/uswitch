#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#define USE_LIBPNG
#include "ProcessSandbox.h"
#include "png.h"

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

void info_callback(png_structp png, png_infop info, void *data) {
    PNGProcessSandbox *sandbox = (PNGProcessSandbox *)data;
    sandbox->inv_png_read_update_info(png, info);
}

void row_callback(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass, void *data) {
}

void end_callback(png_structp png, png_infop info, void *data) {
}

void (*info_callback_ps)(png_structp png, png_infop info);

void (*row_callback_ps)(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass);

void (*end_callback_ps)(png_structp png, png_infop info);


static void load_png_file(PNGProcessSandbox *sandbox, uint8_t *input, size_t size) {
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->mallocInSandbox(size);
    memcpy(sandbox_buffer, input, size);
    png_structp *png = (png_structp *)sandbox->mallocInSandbox(sizeof(png_structp));
    size_t len = strlen(PNG_LIBPNG_VER_STRING);
    char *ver_str = (char *)sandbox->mallocInSandbox(len + 1);
    memcpy(ver_str, PNG_LIBPNG_VER_STRING, len + 1);
    *png = sandbox->inv_png_create_read_struct(ver_str, nullptr, nullptr, nullptr);
    png_infop *info = (png_infop *)sandbox->mallocInSandbox(sizeof(png_infop));
    *info = sandbox->inv_png_create_info_struct(*png);
    sandbox->inv_png_set_progressive_read_fn(*png, nullptr, info_callback_ps, row_callback_ps, end_callback_ps);
    sandbox->inv_png_process_data(*png, *info, sandbox_buffer, size);
    sandbox->inv_png_destroy_read_struct(png, info, nullptr);
    sandbox->freeInSandbox(png);
    sandbox->freeInSandbox(info);
    sandbox->freeInSandbox(sandbox_buffer);
    sandbox->freeInSandbox(ver_str);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test2-ps <filename> <times>\n";
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
    PNGProcessSandbox sandbox("./ProcessSandbox/ProcessSandbox_otherside_png64", 9999, 0);
    info_callback_ps = sandbox.registerCallback((decltype(info_callback_ps))info_callback, &sandbox);
    row_callback_ps = sandbox.registerCallback((decltype(row_callback_ps))row_callback, &sandbox);
    end_callback_ps = sandbox.registerCallback((decltype(end_callback_ps))end_callback, &sandbox);
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
    sandbox.destroySandbox();
    return 0;
}