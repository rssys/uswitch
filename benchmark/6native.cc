#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "vpx/vp8dx.h"
#include "vpx/vpx_codec.h"
#include "vpx/vpx_decoder.h"
#ifdef ENABLE_SECCOMP
#include <sys/syscall.h>
#include "seccomp-bpf.h"
bool init_seccomp(const std::vector<unsigned int> &allowed_syscalls) {
    std::vector<sock_filter> filter {
        VALIDATE_ARCHITECTURE,
        EXAMINE_SYSCALL
    };
    for (unsigned int s : allowed_syscalls) {
        sock_filter filter_allow[] = {ALLOW_SYSCALL_NUM(s)};
        for (int i = 0; i < sizeof(filter_allow) / sizeof(filter_allow[0]); ++i) {
            filter.push_back(filter_allow[i]);
        }
    }
    sock_filter filter_default = BPF_STMT(BPF_RET+BPF_K, SECCOMP_RET_KILL);
    filter.push_back(filter_default);
    struct sock_fprog prog = {
        .len = (unsigned short)filter.size(),
        .filter = filter.data(),
    };
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
        return false;
    }
    if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
        return false;
    }
    return true;
}
#endif

#define IVF_FILE_HDR_SZ 32
#define IVF_FRAME_HDR_SZ 12

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_vp9_file(int threads, uint8_t *input, size_t size) {
    vpx_codec_ctx_t codec;
    vpx_codec_dec_cfg_t cfg;
    vpx_codec_iface_t *iface = vpx_codec_vp9_dx();
    cfg.w = 0;
    cfg.h = 0;
    cfg.threads = threads;
    vpx_codec_dec_init(&codec, iface, &cfg, 0);
    for (size_t i = IVF_FILE_HDR_SZ; i + IVF_FRAME_HDR_SZ < size; ) {
        int frame_size = *(int *)(input + i);
        i += IVF_FRAME_HDR_SZ;
        vpx_codec_decode(&codec, input + i, frame_size, NULL, 0);
        vpx_image_t *img;
        vpx_codec_iter_t iter = NULL;
        while ((img = vpx_codec_get_frame(&codec, &iter)));
        i += frame_size;
    }
    vpx_codec_destroy(&codec);
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        std::cerr << "Usage: ./test6-native <filename> <times> <threads> [print]\n";
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
#ifdef ENABLE_SECCOMP
    static const std::vector<unsigned int> AllowedSyscalls {
        __NR_futex, __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_mprotect,
        __NR_clone, __NR_set_robust_list, __NR_madvise, __NR_exit,
        __NR_newfstatat, __NR_rt_sigaction, __NR_rt_sigprocmask,
        __NR_rt_sigreturn, __NR_clone3
    };
    init_seccomp(AllowedSyscalls);
#endif
    if (print) {
        std::vector<uint64_t> times(n);
        for (int i= 0; i < n; ++i) {
            uint64_t t1 = time_nanosec();
            load_vp9_file(threads, input, size);
            uint64_t t2 = time_nanosec();
            times[i] = t2 - t1;
        }
        for (int i = 0; i < n; ++i) {
            std::cout << times[i] << std::endl;
        }
    } else {
        for (int i= 0; i < n; ++i) {
            load_vp9_file(threads, input, size);
        }
    }
    return 0;
}