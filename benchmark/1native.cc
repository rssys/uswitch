#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "jpeglib.h"
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

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static void load_jpeg_file(uint8_t *input, size_t size) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, input, size);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);
    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer = cinfo.mem->alloc_sarray((j_common_ptr)&cinfo, JPOOL_IMAGE, row_stride, 1);
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

int main(int argc, char **argv) {
    if (argc != 3 && argc != 4) {
        std::cerr << "Usage: ./test1-native <filename> <times> [print]\n";
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
#ifdef ENABLE_SECCOMP
    static const std::vector<unsigned int> AllowedSyscalls {
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_newfstatat
    };
    init_seccomp(AllowedSyscalls);
#endif
    if (print) {
        std::vector<uint64_t> times(n);
        for (int i= 0; i < n; ++i) {
            uint64_t t1 = time_nanosec();
            load_jpeg_file(input, size);
            uint64_t t2 = time_nanosec();
            times[i] = t2 - t1;
        }
        for (int i = 0; i < n; ++i) {
            std::cout << times[i] << std::endl;
        }
    } else {
        uint64_t t1 = time_nanosec();
        for (int i= 0; i < n; ++i) {
            load_jpeg_file(input, size);
        }
        uint64_t t2 = time_nanosec();
        std::cout << t2 - t1 << std::endl;
    }
    return 0;
}