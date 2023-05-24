#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include <sys/syscall.h>
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#include "wasmsandbox.h"
#include "wasm.hpp"

#define IVF_FILE_HDR_SZ 32
#define IVF_FRAME_HDR_SZ 12

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_vp9_file(WasmSandbox *sandbox, int threads, uint8_t *input, size_t size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(vpx_codec_vp9_dx);
    GET_FUNC_PTR(vpx_codec_dec_init_ver);
    GET_FUNC_PTR(vpx_codec_decode);
    GET_FUNC_PTR(vpx_codec_get_frame);
    GET_FUNC_PTR(vpx_codec_destroy);
#undef GET_FUNC_PTR
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->malloc_in_sandbox_unsandboxed(size);
    memcpy(sandbox_buffer, input, size);
    vpx_codec_ctx_t *codec = (vpx_codec_ctx_t *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(vpx_codec_ctx_t));
    vpx_codec_iter_t *iter = (vpx_codec_iter_t *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(vpx_codec_iter_t));
    vpx_codec_dec_cfg_t *cfg = (vpx_codec_dec_cfg_t *)sandbox->malloc_in_sandbox_unsandboxed(sizeof(vpx_codec_dec_cfg_t));
    cfg->w = 0;
    cfg->h = 0;
    cfg->threads = threads;
    vpx_codec_iface_t *iface = wasm_call(sandbox, decltype(vpx_codec_vp9_dx_s), (void *)vpx_codec_vp9_dx_s);
    vpx_codec_err_t res = wasm_call(sandbox, decltype(vpx_codec_dec_init_ver_s),
        (void *)vpx_codec_dec_init_ver_s, codec, iface, cfg, 0, VPX_DECODER_ABI_VERSION);
    for (size_t i = IVF_FILE_HDR_SZ; i + IVF_FRAME_HDR_SZ < size; ) {
        int frame_size = *(int *)(sandbox_buffer + i);
        i += IVF_FRAME_HDR_SZ;
        wasm_call(sandbox, decltype(vpx_codec_decode_s), (void *)vpx_codec_decode_s, codec, sandbox_buffer + i, frame_size, nullptr, 0);
        vpx_image_t *img;
        while ((img = wasm_call(sandbox, decltype(vpx_codec_get_frame_s), (void *)vpx_codec_get_frame_s, codec, iter)));
        i += frame_size;
    }
    wasm_call(sandbox, decltype(vpx_codec_destroy_s), (void *)vpx_codec_destroy_s, codec);
    sandbox->free_in_sandbox_unsandboxed(codec);
    sandbox->free_in_sandbox_unsandboxed(iter);
    sandbox->free_in_sandbox_unsandboxed(cfg);
    sandbox->free_in_sandbox_unsandboxed(sandbox_buffer);
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "Usage: ./test6-uswitch <filename> <times> <threads> [print]\n";
        return 1;
    }
    const char *filename = argv[1];
    int n = atoi(argv[2]);
    int threads = atoi(argv[3]);
    bool print = !argv[4] || atoi(argv[4]);
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
    WasmSandbox sandbox("../libraries_wasm/libvpx/libvpx.so", "libvpx_");
    sandbox.init();
    if (print) {
        std::vector<uint64_t> times(n);
        for (int i= 0; i < n; ++i) {
            uint64_t t1 = time_nanosec();
            load_vp9_file(&sandbox, threads, input, size);
            uint64_t t2 = time_nanosec();
            times[i] = t2 - t1;
        }
        for (int i = 0; i < n; ++i) {
            std::cout << times[i] << std::endl;
        }
    } else {
        for (int i= 0; i < n; ++i) {
            load_vp9_file(&sandbox, threads, input, size);
        }
    }
    return 0;
}