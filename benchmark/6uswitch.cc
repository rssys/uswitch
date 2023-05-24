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
#include "uswitchsandbox.h"
#include "uswitch.hpp"

#define IVF_FILE_HDR_SZ 32
#define IVF_FRAME_HDR_SZ 12

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_vp9_file(USwitchSandbox *sandbox, int threads, uint8_t *input, size_t size) {
#define GET_FUNC_PTR(name) decltype(name) *name##_s = (decltype(name) *)sandbox->get_symbol_addr(#name)
    GET_FUNC_PTR(vpx_codec_vp9_dx);
    GET_FUNC_PTR(vpx_codec_dec_init_ver);
    GET_FUNC_PTR(vpx_codec_decode);
    GET_FUNC_PTR(vpx_codec_get_frame);
    GET_FUNC_PTR(vpx_codec_destroy);
#undef GET_FUNC_PTR
    uint8_t *sandbox_buffer = (uint8_t *)sandbox->malloc_in_sandbox(size);
    uswctx_t ctx = sandbox->get_context();
    memcpy(sandbox_buffer, input, size);
    vpx_codec_ctx_t *codec = (vpx_codec_ctx_t *)sandbox->malloc_in_sandbox(sizeof(vpx_codec_ctx_t));
    vpx_codec_iter_t *iter = (vpx_codec_iter_t *)sandbox->malloc_in_sandbox(sizeof(vpx_codec_iter_t));
    vpx_codec_dec_cfg_t *cfg = (vpx_codec_dec_cfg_t *)sandbox->malloc_in_sandbox(sizeof(vpx_codec_dec_cfg_t));
    cfg->w = 0;
    cfg->h = 0;
    cfg->threads = threads;
    vpx_codec_iface_t *iface;
    uswitch_call_dynamic(ctx, vpx_codec_vp9_dx_s, iface);
    vpx_codec_err_t res;
    uswitch_call_dynamic(ctx, vpx_codec_dec_init_ver_s, res, codec, iface, cfg, 0, VPX_DECODER_ABI_VERSION);
    for (size_t i = IVF_FILE_HDR_SZ; i + IVF_FRAME_HDR_SZ < size; ) {
        int frame_size = *(int *)(sandbox_buffer + i);
        i += IVF_FRAME_HDR_SZ;
        uswitch_call_dynamic(ctx, vpx_codec_decode_s, res, codec, sandbox_buffer + i, frame_size, nullptr, 0);
        vpx_image_t *img;
        while (!uswitch_call_dynamic(ctx, vpx_codec_get_frame_s, img, codec, iter) && img);
        i += frame_size;
    }
    uswitch_call_dynamic(ctx, vpx_codec_destroy_s, res, codec);
    sandbox->free_in_sandbox(codec);
    sandbox->free_in_sandbox(iter);
    sandbox->free_in_sandbox(cfg);
    sandbox->free_in_sandbox(sandbox_buffer);
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
    USwitchSandbox sandbox("../libraries_uswitch/libvpx/libvpx.so.4.1.0", 4096l << 20, 2l << 20);
    sandbox.init();
    static const std::vector<unsigned int> AllowedSyscalls {__NR_exit, __NR_futex, __NR_sched_yield, 451,
#ifdef ONLYMEMPROT
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_mprotect,
        __NR_clone, __NR_set_robust_list, __NR_madvise,
        __NR_pkey_mprotect, __NR_sigaltstack, __NR_gettid,
        __NR_newfstatat, __NR_rt_sigaction, __NR_rt_sigprocmask,
        __NR_rt_sigreturn, __NR_clone3
#endif
    };
    sandbox.init_seccomp(AllowedSyscalls);
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
