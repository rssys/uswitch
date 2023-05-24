#include <iostream>
#include <fstream>
#include <vector>
#include <ctime>
#include <cstdlib>
#include <cinttypes>
#include <cstring>
#include "zlib.h"
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

static void load_gzip_file(uint8_t *input, size_t in_size, uint8_t *output, size_t out_size) {
    z_stream stream;
    memset(&stream, 0, sizeof(z_stream));
    if (inflateInit2(&stream, 16 + MAX_WBITS) != Z_OK) {
        return;
    }
    stream.avail_in = in_size;
    stream.next_in = input;
    stream.avail_out = out_size;
    stream.next_out = output;
    if (inflate(&stream, Z_NO_FLUSH) == Z_STREAM_ERROR) {
        return;
    }
    inflateEnd(&stream);
}

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./test3-native <filename> <times>\n";
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
    uint8_t *output = new uint8_t[size * 2];
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
        load_gzip_file(input, size, output, size * 2);
        uint64_t t2 = time_nanosec();
        times[i] = t2 - t1;
    }
    for (int i = 0; i < n; ++i) {
        std::cout << times[i] << std::endl;
    }
    //printf("%lu\n", (t2 - t1) / n);
    return 0;
}