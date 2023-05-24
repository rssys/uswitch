#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "png.h"
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

void info_callback(png_structp png, png_infop info) {
    png_read_update_info(png, info);
}

void row_callback(png_structp png, png_bytep new_row, png_uint_32 row_num, int pass) {
}

void end_callback(png_structp png, png_infop info) {
}

static void load_png_file(uint8_t *input, size_t size) {
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    png_set_progressive_read_fn(png, NULL, info_callback, row_callback, end_callback);
    png_process_data(png, info, input, size);
    png_destroy_read_struct(&png, &info, NULL);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test2-native <filename> <times>\n";
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
    std::vector<uint64_t> times(n);
#ifdef ENABLE_SECCOMP
    static const std::vector<unsigned int> AllowedSyscalls {
        __NR_brk, __NR_mmap, __NR_munmap,
        __NR_lseek, __NR_fstat, __NR_read, __NR_write,
        __NR_close, __NR_exit_group, __NR_newfstatat
    };
    init_seccomp(AllowedSyscalls);
#endif
    for (int i= 0; i < n; ++i) {
        uint64_t t1 = time_nanosec();
        load_png_file(input, size);
        uint64_t t2 = time_nanosec();
        times[i] = t2 - t1;
    }
    for (int i = 0; i < n; ++i) {
        std::cout << times[i] << std::endl;
    }
    //printf("%lu\n", (t2 - t1) / n);
    return 0;
}