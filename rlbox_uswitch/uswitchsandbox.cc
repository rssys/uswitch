#include <vector>
#include <fstream>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <map>
#include <set>
#include <new>
#include <unordered_set>
#include <cinttypes>
#include <cstring>
#include <alloca.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <link.h>
#include <ucontext.h>
#include <signal.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/sendfile.h>
#include <linux/limits.h>
#include "uswitchsandbox.h"
#include "uswitch.h"
#include "uswitch.hpp"
#include "seccomp-bpf.h"

USwitchSandboxSemaphore uswitch_sandbox_semaphore(13);
USwitchSandboxManager uswitch_sandbox_manager(13);
const std::vector<unsigned int> USwitchSandbox::DefaultTrappedSyscalls;

static void print_map() {
    std::ifstream ifs("/proc/self/maps");
    std::cout << ifs.rdbuf();
}

struct USwitchThread {
    uswctx_t ctx;
    uint8_t *stack;
    size_t stack_size;
    SharedTLSData *tls_state;
    int index;
};

struct SharedTLSData {
    uintptr_t thread_pointer;
    uintptr_t dtv;
    size_t tls_modid;
    size_t tls_size;
    size_t tcb_avail_size;
    bool has_retval;
    void *retval;
    std::mutex *mutex;
    std::mutex mutex_;
};

struct MemoryMap {
    std::pair<uintptr_t, uintptr_t> addr;
    bool prot_r;
    bool prot_w;
    bool prot_x;
    bool prot_p;
    std::string filename;
};

static inline std::string get_library_path(const std::string &lib) {
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
  if (len == -1) {
    return "";
  }
  std::string path(buf, len);
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    return "";
  }
  path.resize(pos + 1);
  return path + lib;
}

static inline uintptr_t get_tp() {
    uintptr_t tp;
    asm ("movq %%fs:0, %0\n" : "=r" (tp));
    return tp;
}

using PtrElfPhdr = const ElfW(Phdr) *;

static inline bool get_dl_map(void *handle, uintptr_t &base_addr, PtrElfPhdr &phdr, int &num_phdr) {
    struct link_map *map;
    // dl_phdr_info specifies namespace by return address, so we have to call it from the shared object
    // the trampoline helps us do that
    int (*trampoline)(int (*)(struct dl_phdr_info *, size_t, void *), void *, decltype(dl_iterate_phdr) *);
    trampoline = (decltype(trampoline))dlsym(handle, "uswitch_dip_trampoline");
    if (!trampoline) {
        return false;
    }
    // check it's not tampered
    if (memcmp((void *)trampoline, "\x48\x83\xec\x08\xff\xd2\x48\x83\xc4\x08\xc3", 11) != 0) {
        return false;
    }
    if (dlinfo(handle, RTLD_DI_LINKMAP, (void *)&map) == -1) {
        return false;
    }
    uintptr_t base = map->l_addr;
    base_addr = base;
    bool found = false;
    auto f = [&] (struct dl_phdr_info *info, size_t size) -> int {
        if (base == info->dlpi_addr) {
            phdr = info->dlpi_phdr;
            num_phdr = info->dlpi_phnum;
            found = true;
        }
        return 0;
    };
    using T = decltype(f);
    auto callback = [] (struct dl_phdr_info *info, size_t size, void *data) {
        T *f = (T *)data;
        return (*f)(info, size);
    };
    if (trampoline(callback, (void *)&f, dl_iterate_phdr) != 0) {
        return false;
    }
    return found;
}

static std::vector<ELFSegment> interval_difference(std::vector<ELFSegment> &segs1,
    std::vector<ELFSegment> &segs2) {
    std::vector<ELFSegment> res;
    std::sort(segs2.begin(), segs2.end(), [] (const ELFSegment s1, const ELFSegment &s2) {
        return s1.addr_start < s2.addr_start;
    });
    for (auto &&s1 : segs1) {
        uintptr_t start = s1.addr_start;
        uintptr_t end = s1.addr_end;
        for (auto && s2 : segs2) {
            end = s2.addr_start;
            if (end >= s1.addr_end) {
                break;
            }
            if (end > start) {
                ELFSegment s = s1;
                s.addr_start = start;
                s.addr_end = end;
                res.push_back(s);
            }
            start = s2.addr_end;
            if (start >= s1.addr_end) {
                break;
            }
        }
        if (start < s1.addr_end) {
            ELFSegment s = s1;
            s.addr_start = start;
            s.addr_end = s1.addr_end;
            res.push_back(s);
        }
    }
    return res;
}

static uint64_t time_nanosec() {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec * 1000000000ull + t.tv_nsec;
}

static bool get_dl_segments(void *handle, ELFSegment &tls_segment,
    std::vector<std::pair<ELFSegment, std::unique_ptr<uint8_t []>>> &rw_segments) {
    uintptr_t dl_base_addr, dl_map_start, dl_map_end;
    const ElfW(Phdr) *phdr;
    int num_phdr;
    if (!get_dl_map(handle, dl_base_addr, phdr, num_phdr)) {
        printf("failed to get dl map\n");
        return false;
    }
    //dl_map_end = (dl_map_end + 0xffflu) & ~0xffflu;
    std::vector<ELFSegment> loadable_segments;
    std::vector<ELFSegment> relro_segments;
    for (int i = 0; i < num_phdr; ++i) {
        auto p_type = phdr[i].p_type;
        auto p_vaddr = phdr[i].p_vaddr;
        auto p_memsz = phdr[i].p_memsz;
        auto flags = phdr[i].p_flags;
        uintptr_t start = dl_base_addr + p_vaddr;
        uintptr_t end = start + p_memsz;
        bool prot_x = flags & PF_X;
        bool prot_w = flags & PF_W;
        bool prot_r = flags & PF_R;
        ELFSegment seg;
        seg.prot_x = prot_x;
        seg.prot_w = prot_w;
        seg.prot_r = prot_r;
        seg.addr_start = start;
        seg.addr_end = end;
        if (p_type == PT_LOAD) {
            loadable_segments.push_back(seg);
        } else if (p_type == PT_GNU_RELRO) {
            relro_segments.push_back(seg);
        } else if (p_type == PT_TLS) {
            tls_segment = seg;
        }
    }
    const std::vector<ELFSegment> &s1 = interval_difference(loadable_segments, relro_segments);
    auto set_mprotect = [&] (const std::vector<ELFSegment> &segs) {
        for (auto &&s : segs) {
            if (!s.prot_r) {
                continue;
            }
            uintptr_t start = s.addr_start & ~0xfffl;
            uintptr_t end = (s.addr_end + 0xfffl) & ~0xfffl;
            //if (start < dl_map_start || end > dl_map_end) {
            //    continue;
            //}
            int prot = PROT_READ;
            if (s.prot_x) {
                prot |= PROT_EXEC;
            }
            if (s.prot_w) {
                prot |= PROT_WRITE;
            }
            if (s.prot_x) {
                uint8_t *start = (uint8_t *)s.addr_start;
                uint8_t *end = (uint8_t *)s.addr_end;
                for (uint8_t *i = start; i < end; ++i) {
                    if (*i == 0x0f && i + 2 < end) {
                        if (*(i + 1) == 0x01 && *(i + 2) == 0xef) {
                            printf("found wrpkru: %p\n", i);
                            abort();
                        } else if (*(i + 1) == 0xae) {
                            int h = *(i + 2) & 0xf8;
                            if (h == 0x28 || h == 0x68 || h == 0xa8) {
                                printf("found xrstor: %p\n", i);
                                abort();
                            }
                        }
                    } else if (*i == 0xf3 && i + 2 < end) {
                        if (*(i + 1) == 0x0f && *(i + 2) == 0xae) {
                            printf("found wrfsbase32\n");
                            abort();
                        }
                        if (i + 3 < end && *(i + 1) == 0x48 && *(i + 1) == 0x0f && *(i + 1) == 0xae) {
                            printf("found wrfsbase\n");
                            abort();
                        }
                    }
                }
            }
            if (s.prot_w) {
                uint8_t *data = new uint8_t[s.addr_end - s.addr_start];
                memcpy(data, (void *)s.addr_start, s.addr_end - s.addr_start);
                rw_segments.emplace_back(s, std::unique_ptr<uint8_t []>(data));
            } else {
                uswitch_share((void *)start, end - start, prot, 1);
            }
        }
    };
    set_mprotect(s1);
    set_mprotect(relro_segments);
    return true;
}

USwitchSandbox::USwitchSandbox(const char *library_, size_t memory_size_, size_t stack_size_,
    int max_threads_) :
    has_init(false), main_ctx(nullptr), memory_size(memory_size_), stack_size(stack_size_), num_threads(1),
    tls_per_thread_size(4096), max_threads(max_threads_),
    library(library_), handle(nullptr), memory(nullptr), stack_start(nullptr), main_tls_state(nullptr),
    allow_mmap(false) {}

USwitchSandbox::~USwitchSandbox() {
    if (handle) {
        dlclose(handle);
    }
    if (memory) {
        munmap(memory, memory_size);
    }
    for (auto &&it : contexts) {
        uswitch_set_cleanup_routine(it.second->ctx, nullptr);
        uswitch_destroy_context(it.second->ctx);
    }
    if (pctx) {
        uswitch_destroy_pcontext(pctx);
    }
}

static std::string get_shared_object_copy(const std::string &filename);

bool USwitchSandbox::init() {
    if (has_init) {
        return false;
    }
    //handle = dlopen(get_shared_object_copy(library).c_str(), RTLD_NOW | RTLD_DEEPBIND);
    //handle = dlopen(library.c_str(), RTLD_NOW | RTLD_DEEPBIND);
    handle = dlmopen(LM_ID_NEWLM, library.c_str(), RTLD_NOW | RTLD_DEEPBIND);
    if (!handle) {
        printf("dlmopen, error: %s\n", dlerror());
        return false;
    }
    if (dlinfo(handle, RTLD_DI_TLS_MODID, (void *)&tls_modid) == -1) {
        printf("dlinfo, error: %s\n", dlerror());
        return false;
    }
    tls_segment.addr_start = 0;
    tls_segment.addr_end = 0;
    uswitch_init(0);
    if (!get_dl_segments(handle, tls_segment, rw_segments)) {
        return false;
    }
    tls_per_thread_size = 4096 + tls_segment.addr_end - tls_segment.addr_start;
    tls_per_thread_size = (tls_per_thread_size + 0xfffl) & ~0xfffl;
    total_stack_size = stack_size * max_threads;
    tls_size = tls_per_thread_size * max_threads;
    if (memory_size < total_stack_size + tls_size) {
        return false;
    }
    memory = (uint8_t *)mmap(nullptr, memory_size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED) {
        return false;
    }
    // |heap|TLS data|stack|
    stack_start = memory + memory_size - total_stack_size;
    tls = memory + memory_size - total_stack_size - tls_size;
    main_tls_state = (SharedTLSData *)(tls + tls_per_thread_size - 2048);
    if (tls_segment.addr_start) {
        memcpy(tls, (void *)tls_segment.addr_start, tls_segment.addr_end - tls_segment.addr_start);
    }
    uint8_t *main_stack = stack_start;
    if (uswitch_new_context(&main_ctx, main_stack, stack_size, this, main_tls_state) == -1) {
        return false;
    }
    if (uswitch_get_pcontext(main_ctx, &pctx) == -1) {
        return false;
    }
    if (uswitch_mmap(main_ctx, memory, memory_size, PROT_READ | PROT_WRITE) == -1) {
        return false;
    }
    for (auto &&s : rw_segments) {
        uintptr_t start = s.first.addr_start & ~0xfffl;
        uintptr_t end = (s.first.addr_end + 0xfffl) & ~0xfffl;
        uswitch_mmap(main_ctx, (void *)start, end - start, PROT_READ | PROT_WRITE);
    }
    USwitchThread *th = new USwitchThread;
    th->ctx = main_ctx;
    th->stack = main_stack;
    th->stack_size = stack_size;
    th->tls_state = main_tls_state;
    th->index = 0;
    uintptr_t tp = get_tp();
    contexts[tp] = std::unique_ptr<USwitchThread>(th);
    ctx_tp_map[main_ctx] = tp;
    thread_occupied.resize(max_threads);
    thread_occupied[0] = 1;
    if (!init_hook() || !init_callback()) {
        return false;
    }
    uswitch_set_cleanup_routine(main_ctx, [] (uswctx_t ctx, void *userdata1, void *userdata2) {
        USwitchSandbox *sandbox = (USwitchSandbox *)userdata1;
        sandbox->exit_thread(ctx, true);
    });
    has_init = true;
    return true;
}

bool USwitchSandbox::reset() {
    if (!has_init) {
        return false;
    }
    if (madvise(memory, memory_size, MADV_DONTNEED) == -1) {
        return false;
    }
    if (tls_segment.addr_start) {
        memcpy(tls, (void *)tls_segment.addr_start, tls_segment.addr_end - tls_segment.addr_start);
    }
    for (auto &&s : rw_segments) {
        memcpy((void *)s.first.addr_start, s.second.get(), s.first.addr_end - s.first.addr_start);
        //std::ofstream ofs1("/home/ubuntu/tmp/1.txt");
        //std::ofstream ofs2("/home/ubuntu/tmp/2.txt");
        //ofs1.write((const char *)s.first.addr_start, s.first.addr_end - s.first.addr_start);
        //ofs2.write((const char *)s.second.get(), s.first.addr_end - s.first.addr_start);
        //memcpy((void *)s.first.addr_start, s.second.get(), 112);
    }
    uswitch_init(0);
    for (auto &&it : contexts) {
        uswitch_set_cleanup_routine(it.second->ctx, nullptr);
        uswitch_destroy_context(it.second->ctx);
    }
    if (uswitch_dup_context2(&main_ctx, pctx) == -1) {
        return false;
    }
    uswitch_set_stack(main_ctx, stack_start, stack_size);
    uswitch_set_userdata(main_ctx, this, main_tls_state);
    uswitch_set_cleanup_routine(main_ctx, [] (uswctx_t ctx, void *userdata1, void *userdata2) {
        USwitchSandbox *sandbox = (USwitchSandbox *)userdata1;
        sandbox->exit_thread(ctx, true);
    });
    USwitchThread *th = new USwitchThread;
    uintptr_t tp = get_tp();
    th->ctx = main_ctx;
    th->stack = stack_start;
    th->stack_size = stack_size;
    th->tls_state = main_tls_state;
    th->index = 0;
    contexts.clear();
    contexts[tp] = std::unique_ptr<USwitchThread>(th);
    num_threads = 1;
    threads.clear();
    thread_occupied.clear();
    thread_occupied.resize(max_threads);
    thread_occupied[0] = 1;
    ctx_tp_map.clear();
    ctx_tp_map[main_ctx] = tp;
    if (!init_hook()) {
        return false;
    }
    return true;
}

uswctx_t USwitchSandbox::get_context(USwitchThread **thread) {
    if (!has_init) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lock(mutex);
    uintptr_t tp = get_tp();
    //printf("get context %d %p\n", syscall(__NR_gettid), tp);
    auto it = contexts.find(tp);
    if (it != contexts.end()) {
        if (thread) {
            *thread = it->second.get();
        }
        return it->second->ctx;
    }
    USwitchThread *new_thread = init_thread();
    if (!new_thread) {
        printf("failed to init thread\n");
        return nullptr;
    }
    if (thread) {
        *thread = new_thread;
    }
    contexts[tp] = std::unique_ptr<USwitchThread>(new_thread);
    ctx_tp_map[new_thread->ctx] = tp;
    return new_thread->ctx;
}

void *USwitchSandbox::get_symbol_addr(const char *symbol) {
    if (!has_init || !handle) {
        return nullptr;
    }
    return dlsym(handle, symbol);
}

bool USwitchSandbox::init_hook() {
    struct FuncAddr {
        void *(*malloc)(size_t);
        void (*free)(void *);
    };
    typedef FuncAddr (*set_uswitch_functions_t)(int, void *, size_t,
        void *, void*, void *, void *, void *, void *, void *, void *, void *, void *);
    set_uswitch_functions_t set_uswitch_functions = (set_uswitch_functions_t)dlsym(handle, "set_uswitch_functions");
    if (!set_uswitch_functions) {
        return false;
    }
    size_t heap_size = memory_size - total_stack_size - tls_size;
    new (main_tls_state) SharedTLSData;
    // |TLS data|TCB (2048)|SharedTLSState(2048)|
    main_tls_state->thread_pointer = (uintptr_t)tls + tls_per_thread_size - 4096;
    main_tls_state->tls_size = tls_per_thread_size - 4096;
    main_tls_state->tcb_avail_size = 2048;
    main_tls_state->dtv = (uintptr_t)tls;
    main_tls_state->tls_modid = tls_modid;
    main_tls_state->has_retval = false;
    int tid = syscall(SYS_gettid);
    FuncAddr addr;
    if (uswitch_call_dynamic(main_ctx, set_uswitch_functions, addr,
        tid, memory, heap_size,
        (void *)pthread_create_hook, (void *)pthread_join_hook, (void *)pthread_detach_hook, (void *)pthread_exit_hook,
        (void *)mmap_hook, (void *)munmap_hook, (void *)mremap_hook, (void *)mprotect_hook,
        (void *)uswitch_callback, (void *)get_thread_pointer) == -1) {
        return false;
    }
    malloc_addr = addr.malloc;
    free_addr = addr.free;
    //printf("uswitch:init\n");
    return true;
}

bool USwitchSandbox::init_callback() {
    if (uswitch_register_callback_dynamic(main_ctx,
        [] (uswctx_t ctx, void *data, const char *sym, size_t len) -> void * {
            USwitchSandbox *self = (USwitchSandbox *)data;
            if (!self || !self->handle) {
                return nullptr;
            }
            if (uswitch_check_stack_pointer(ctx, sym, len) == -1 || sym[len] != 0) {
                return nullptr;
            }
            return dlsym(self->handle, sym);
        }, this
    ) != CallbackIDGetSymbol) {
        return false;
    }
    if (uswitch_register_callback_dynamic(main_ctx, pthread_create_callback, this) != CallbackIDPthreadCreate ||
        uswitch_register_callback_dynamic(main_ctx, pthread_join_callback, this) != CallbackIDPthreadJoin ||
        uswitch_register_callback_dynamic(main_ctx, pthread_detach_callback, this) != CallbackIDPthreadDetach ||
        uswitch_register_callback_dynamic(main_ctx, sigaction_callback, this) != CallbackIDSigaction ||
        uswitch_register_callback_dynamic(main_ctx, mmap_callback, this) != CallbackIDMmap ||
        uswitch_register_callback_dynamic(main_ctx, munmap_callback, this) != CallbackIDMunmap ||
        uswitch_register_callback_dynamic(main_ctx, mremap_callback, this) != CallbackIDMremap ||
        uswitch_register_callback_dynamic(main_ctx, mprotect_callback, this) != CallbackIDMprotect) {
        return false;
    }
    register_syscall_handler(__NR_rt_sigaction,
        [this] (unsigned int, long arg1, long arg2, long arg3, long, long, long) -> long {
            int sig = (int)arg1;
            const struct sigaction *act = (const struct sigaction *)arg2;
            struct sigaction *oldact = (struct sigaction *)arg3;
            return sigaction_callback(nullptr, (void *)this, sig, act, oldact);
        }
    );
    return true;
}

bool USwitchSandbox::init_seccomp(const std::vector<unsigned int> &allowed_syscalls,
    const std::vector<unsigned int> &trapped_syscalls, USwitchSandbox::SeccompAction default_action) {
    bool res = true;
    if (uswitch_call_without_mprotect(main_ctx, [&] {
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
        for (unsigned int s : trapped_syscalls) {
            sock_filter filter_trap[] = {TRAP_SYSCALL_NUM(s)};
            for (int i = 0; i < sizeof(filter_trap) / sizeof(filter_trap[0]); ++i) {
                filter.push_back(filter_trap[i]);
            }
        }
        unsigned int action;
        switch (default_action) {
        case SeccompAction::SeccompKill: action = SECCOMP_RET_KILL; break;
        case SeccompAction::SeccompAllow: action = SECCOMP_RET_ALLOW; break;
        case SeccompAction::SeccompTrap: action = SECCOMP_RET_TRAP; break;
        default: res = false;
        }
        sock_filter filter_default = BPF_STMT(BPF_RET+BPF_K, action);
        filter.push_back(filter_default);
        struct sock_fprog prog = {
            .len = (unsigned short)filter.size(),
            .filter = filter.data(),
        };
        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
            res = false;
            return;
        }
        if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) {
            res = false;
            return;
        }
    }) == -1) {
        return false;
    }
    return res;
}

void USwitchSandbox::exit_thread(uswctx_t ctx, bool release_thread) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = ctx_tp_map.find(ctx);
    if (it == ctx_tp_map.end()) {
        return;
    }
    uintptr_t tp = it->second;
    ctx_tp_map.erase(it);
    auto it2 = contexts.find(tp);
    if (it2 == contexts.end()) {
        return;
    }
    if (release_thread) {
        thread_occupied[it2->second->index] = 0;
        --num_threads;
    }
    contexts.erase(it2);
}

USwitchThread *USwitchSandbox::init_thread() {
    //printf("init thread\n");
    if (num_threads >= max_threads) {
        return nullptr;
    }
    int thread_index = -1;
    for (int i = 0; i < thread_occupied.size(); ++i) {
        if (!thread_occupied[i]) {
            thread_index = i;
            break;
        }
    }
    if (thread_index == -1) {
        return nullptr;
    }
    uswctx_t new_ctx;
    uswitch_init(0);
    if (uswitch_dup_context2(&new_ctx, pctx) == -1) {
        printf("fail to create uswitch context for current thread\n");
        return nullptr;
    }
    thread_occupied[thread_index] = 1;
    ++num_threads;
    USwitchThread *th = new USwitchThread;
    th->ctx = new_ctx;
    th->stack = stack_start + thread_index * stack_size;
    th->stack_size = stack_size;
    th->index = thread_index;
    SharedTLSData *tls_state = (SharedTLSData *)(tls + tls_per_thread_size * (thread_index + 1) - 2048);
    new (tls_state) SharedTLSData;
    // |TLS data|TCB (2048)|SharedTLSState(2048)|
    tls_state->thread_pointer = (uintptr_t)tls + tls_per_thread_size * (thread_index + 1) - 4096;
    tls_state->tls_size = tls_per_thread_size - 4096;
    tls_state->tcb_avail_size = 2048;
    tls_state->dtv = (uintptr_t)tls + tls_per_thread_size * thread_index;
    tls_state->tls_modid = tls_modid;
    if (tls_segment.addr_start) {
        memcpy(tls + tls_per_thread_size * thread_index, (void *)tls_segment.addr_start,
            tls_segment.addr_end - tls_segment.addr_start);
    }
    tls_state->has_retval = false;
    th->tls_state = tls_state;
    uswitch_set_stack(new_ctx, th->stack, th->stack_size);
    uswitch_set_userdata(new_ctx, this, tls_state);
    void (*uswitch_add_thread)(int, uintptr_t) = (void (*)(int, uintptr_t))dlsym(handle, "uswitch_add_thread");
    if (uswitch_add_thread) {
        uswitch_call_dynamic(new_ctx, uswitch_add_thread, syscall(SYS_gettid), tls_state->thread_pointer);
    }
    uswitch_set_cleanup_routine(new_ctx, [] (uswctx_t ctx, void *userdata1, void *userdata2) {
        USwitchSandbox *sandbox = (USwitchSandbox *)userdata1;
        sandbox->exit_thread(ctx, true);
    });
    //printf("finish init thread\n");
    return th;
}

void *USwitchSandbox::malloc_in_sandbox(size_t size) {
    void *ret = nullptr;
    uswctx_t ctx = get_context();
    if (!ctx) {
        return nullptr;
    }
    uswitch_call_dynamic(ctx, malloc_addr, ret, size);
    
    return ret;
}

void USwitchSandbox::free_in_sandbox(void *ptr) {
    uswctx_t ctx = get_context();
    if (!ctx) {
        return;
    }
   uswitch_call_dynamic(ctx, free_addr, ptr);
}

NOCANARY void *USwitchSandbox::get_symbol_addr_in_sandbox(const char *symbol) {
    size_t len;
    for (len = 0; symbol[len]; ++len);
    return uswitch_callback_static(CallbackIDGetSymbol, void *(*)(const char *, size_t), symbol, len);
}

bool USwitchSandbox::trap_syscalls() {
    struct sigaction action;
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = sigsys_handler;
    return uswitch_sandbox_sigaction(SIGSYS, false, &action) == 0;
}

void USwitchSandbox::sandbox_signal_handler(int sig, siginfo_t *info, void *context) {
    USwitchSandbox *sandbox;;
    if (uswitch_current(nullptr, (void **)&sandbox, nullptr) == -1) {
        return;
    }
    sandbox->dispatch_sandbox_signal(sig, info);
}

const struct sigaction *USwitchSandbox::get_sandbox_signal_dispatcher() {
    static struct sigaction act;
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = sandbox_signal_handler;
    return &act;
}

bool USwitchSandbox::register_syscall_handler(unsigned int syscall_num, const SyscallHandler &handler) {
    std::lock_guard<std::mutex> lock(mutex);
    syscall_handlers[syscall_num] = handler;
    return true;
}

bool USwitchSandbox::register_syscall_handler(unsigned int syscall_num, const SimpleSyscallHandler &handler) {
    return register_syscall_handler(syscall_num, [handler] (unsigned int syscall_num, void *ucontext) {
        ucontext_t *uc = (ucontext_t *)ucontext;
        long arg1 = (long)uc->uc_mcontext.gregs[REG_RDI];
        long arg2 = (long)uc->uc_mcontext.gregs[REG_RSI];
        long arg3 = (long)uc->uc_mcontext.gregs[REG_RDX];
        long arg4 = (long)uc->uc_mcontext.gregs[REG_R10];
        long arg5 = (long)uc->uc_mcontext.gregs[REG_R8];
        long arg6 = (long)uc->uc_mcontext.gregs[REG_R9];
        uc->uc_mcontext.gregs[REG_RAX] = handler(syscall_num, arg1, arg2, arg3, arg4, arg5, arg6);
    });
}

void USwitchSandbox::allow_sandbox_signals(const std::unordered_set<int> &signals) {
    std::lock_guard<std::mutex> lock(mutex);
    allowed_sandbox_signals = signals;
}

bool USwitchSandbox::dispatch_sandbox_signal(int sig, siginfo_t *info) {
    std::shared_ptr<struct sigaction> action;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!allowed_sandbox_signals.count(sig)) {
            return false;
        }
        auto it = sandbox_sigactions.find(sig);
        if (it == sandbox_sigactions.end()) {
            return false;
        }
        action = it->second;
    }
    if (!action) {
        return false;
    }
    uswctx_t ctx = get_context();
    if (!ctx) {
        return false;
    }
    if (action->sa_flags & SA_SIGINFO) {
        void *info_sandbox = uswitch_push_stack(ctx, sizeof(siginfo_t));
        if (!info_sandbox) {
            return false;
        }
        memcpy(info_sandbox, info, sizeof(siginfo_t));
        if (uswitch_call_dynamic(ctx, action->sa_sigaction, sig, (siginfo_t *)info_sandbox, nullptr) == -1) {
            return false;
        }
        uswitch_push_stack(ctx, -sizeof(siginfo_t));
    } else if (action->sa_handler == SIG_DFL) {
        uswitch_ret_signal();
        return true;
    } else if (action->sa_handler == SIG_IGN) {
        return true;
    }
    if (uswitch_call_dynamic(ctx, action->sa_handler, sig) == -1) {
        return false;
    }
    return true;
}

void USwitchSandbox::sigsys_handler(int sig, siginfo_t *info, void *ucontext) {
    if (sig != SIGSYS || info->si_code != SYS_SECCOMP) {
        return;
    }
    USwitchSandbox *sandbox;;
    if (uswitch_current(nullptr, (void **)&sandbox, nullptr) == -1) {
        return;
    }
    int syscall_num = info->si_syscall;
    SyscallHandler handler;
    {
        std::lock_guard<std::mutex> lock(sandbox->mutex);
        auto it = sandbox->syscall_handlers.find(syscall_num);
        if (it == sandbox->syscall_handlers.end()) {
            uswitch_ret_signal();
            return;
        }
        handler = it->second;
    }
    handler(syscall_num, ucontext);
}

NOCANARY int USwitchSandbox::pthread_create_hook(pthread_t *thread, const pthread_attr_t *attr,
    void *(*start_routine)(void *), void *arg) {
    using CallbackType = std::pair<int, pthread_t> (*)(void *(*)(void *), void *);
    std::pair<int, pthread_t> res = uswitch_callback_static(CallbackIDPthreadCreate, CallbackType, start_routine, arg);
    if (!res.first) {
        *thread = res.second;
    }
    return res.first;
}

std::pair<int, pthread_t> USwitchSandbox::pthread_create_callback(uswctx_t ctx, void *data,
    void *(*start_routine)(void *), void *arg) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    if (!self) {
        return std::make_pair<int, pthread_t>(-1, (pthread_t)0);
    }
    pthread_t thread;
    int res = pthread_create(&thread, nullptr, thread_routine,
        new std::tuple<USwitchSandbox *, void *(*)(void *), void *>(self, start_routine, arg));
    {
        std::lock_guard<std::mutex> lock(self->mutex);
        self->threads.insert(thread);
    }
    return std::make_pair(res, thread);
}

NOCANARY int USwitchSandbox::pthread_join_hook(pthread_t thread, void **retval) {
    using CallbackType = std::pair<int, void *> (*)(pthread_t);
    std::pair<int, void *> res = uswitch_callback_static(CallbackIDPthreadJoin, CallbackType, thread);
    if (!res.first && retval) {
        *retval = res.second;
    }
    return res.first;
}

std::pair<int, void *> USwitchSandbox::pthread_join_callback(uswctx_t ctx, void *data, pthread_t thread) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    if (!self) {
        return std::make_pair<int, void *>(-1, (pthread_t)0);
    }
    if (!self->threads.count(thread)) {
        return std::make_pair<int, void *>(-1, nullptr);
    }
    void *ret = nullptr;
    int res = pthread_join(thread, &ret);
    return std::make_pair(res, ret);
}

NOCANARY int USwitchSandbox::pthread_detach_hook(pthread_t thread) {
    return uswitch_callback_static(CallbackIDPthreadDetach, int (*)(pthread_t), thread);
}

int USwitchSandbox::pthread_detach_callback(uswctx_t ctx, void *data, pthread_t thread) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    if (!self) {
        return -1;
    }
    if (!self->threads.count(thread)) {
        return -1;
    }
    return pthread_detach(thread);
}

NOCANARY void USwitchSandbox::pthread_exit_hook(void *retval) {
    void *userdata;
    if (uswitch_current(nullptr, nullptr, &userdata) == -1 || !userdata) {
        uswitch_ret();
        return;
    }
    SharedTLSData *state = (SharedTLSData *)userdata;
    state->has_retval = true;
    state->retval = retval;
    uswitch_ret();
}

int USwitchSandbox::sigaction_callback(uswctx_t ctx, void *data, int sig, const struct sigaction *act,
        struct sigaction *oldact) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    if (!self) {
        return -1;
    }
    std::lock_guard<std::mutex> lock(self->mutex);
    if (!self->allowed_sandbox_signals.count(sig)) {
        return -1;
    }
    if (act && ((uint8_t *)act < self->memory ||
        (uint8_t *)act + sizeof(struct sigaction) > self->memory + self->memory_size)) {
        return -1;
    }
    if (oldact && ((uint8_t *)oldact < self->memory ||
        (uint8_t *)oldact + sizeof(struct sigaction) > self->memory + self->memory_size)) {
        return -1;
    }
    auto it = self->sandbox_sigactions.find(sig);
    std::shared_ptr<struct sigaction> action;
    if (it != self->sandbox_sigactions.end()) {
        action = it->second;
    } else {
        action.reset(new struct sigaction);
        action->sa_flags = 0;
        sigemptyset(&action->sa_mask);
        action->sa_handler = SIG_DFL;
        self->sandbox_sigactions[sig] = action;
    }
    if (oldact) {
        *oldact = *action;
    }
    if (act) {
        *action = *act;
    }
    return 0;
}

NOCANARY void *USwitchSandbox::mmap_hook(void *addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return uswitch_callback_static(CallbackIDMmap, decltype(mmap_hook) *,
        addr, length, prot, flags, fd, offset);
}

NOCANARY int USwitchSandbox::munmap_hook(void *addr, size_t length) {
    return uswitch_callback_static(CallbackIDMunmap, decltype(munmap_hook) *, addr, length);
}

NOCANARY void *USwitchSandbox::mremap_hook(void *old_address, size_t old_size, size_t new_size,
    int flags, void *new_address) {
    return uswitch_callback_static(CallbackIDMremap, decltype(mremap_hook) *,
        old_address, old_size, new_size, flags, new_address);
}

NOCANARY int USwitchSandbox::mprotect_hook(void *addr, size_t length, int prot) {
    return uswitch_callback_static(CallbackIDMprotect, decltype(mprotect_hook) *,
        addr, length, prot);
}

void *USwitchSandbox::mmap_callback(uswctx_t ctx, void *data, void *addr, size_t length, int prot,
    int flags, int fd, off_t offset) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    std::lock_guard<std::mutex> lock(self->mutex);
    if (!self->allow_mmap) {
        return (void *)(unsigned long)(-EPERM);
    }
    static constexpr int UnsupportedFlags = MAP_SHARED | MAP_32BIT | MAP_FIXED |
        MAP_GROWSDOWN | MAP_HUGETLB;
    if ((flags & UnsupportedFlags) || (prot & PROT_EXEC)) {
        return (void *)(unsigned long)(-EPERM);
    }
    int priv_fd = -1;
    if (fd >= 0) {
        priv_fd = uswitch_dup_fd(ctx, fd);
        if (priv_fd == -1) {
            return (void *)(unsigned long)(-EBADF);
        }
    }
    size_t alignment = 4096;
    length = (length + alignment - 1) & (~(alignment - 1));
    // addr is ignored
    void *res = mmap(nullptr, length, prot, flags, priv_fd, offset);
    if (priv_fd != -1) {
        close(priv_fd);
    }
    if (res == MAP_FAILED) {
        return (void *)(unsigned long)(-errno);
    }
    if (uswitch_mmap(ctx, res, length, prot) == -1) {
        return (void *)(unsigned long)(-1);
    }
    VirtualMemoryArea vma;
    vma.start = (uintptr_t)res;
    vma.end = (uintptr_t)res + length;
    vma.prot = prot;
    vma.flags = flags;
    self->vmas[vma.start] = vma;
    return res;
}

int USwitchSandbox::munmap_callback(uswctx_t ctx, void *data, void *addr, size_t length) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    std::lock_guard<std::mutex> lock(self->mutex);
    if (!self->allow_mmap) {
        return -EPERM;
    }
    size_t alignment = 4096;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + length;
    if ((start & (alignment - 1)) || end < start) {
        return -EINVAL;
    }
    end = (end + alignment - 1) & (~(alignment - 1));
    if (start == end) {
        return 0;
    }
    int res = 0;
    std::vector<VirtualMemoryArea> vma_to_add;
    bool remove;
    // find the last addr that's <= start
    auto beg = self->vmas.upper_bound(start);
    if (beg != self->vmas.begin()) {
        --beg;
    }
    for (auto it = beg; it != self->vmas.end();) {
        const VirtualMemoryArea &vma = it->second;
        remove = false;
        if (end > vma.start && start < vma.end) {
            remove = true;
            uintptr_t s = vma.start > start ? vma.start : start;
            uintptr_t e = vma.end < end ? vma.end : end;
            if (uswitch_munmap(ctx, (void *)vma.start, (void *)s, e - s) == -1) {
                res = -EINVAL;
            }
            VirtualMemoryArea m1, m2;
            m1.start = vma.start;
            m1.end = s;
            m1.prot = vma.prot;
            m1.flags = vma.flags;
            m2.start = e;
            m2.end = vma.end;
            m2.prot = vma.prot;
            m2.flags = vma.flags;
            if (m1.end > m1.start) {
                vma_to_add.push_back(m1);
            }
            if (m2.end > m2.start) {
                vma_to_add.push_back(m2);
            }
        }
        if (remove) {
            self->vmas.erase(it++);
        } else {
            ++it;
        }
    }
    for (auto &&vma : vma_to_add) {
        self->vmas[vma.start] = vma;
    }
    return res;
}

void *USwitchSandbox::mremap_callback(uswctx_t ctx, void *data, void *addr, size_t old_size,
    size_t new_size, int flags, void *new_address) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    std::lock_guard<std::mutex> lock(self->mutex);
    if (!self->allow_mmap) {
        return (void *)(unsigned long)(-EPERM);
    }
    if (flags & (~MREMAP_MAYMOVE)) {
        return (void *)(unsigned long)(-EPERM);
    }
    flags &= ~MREMAP_MAYMOVE;
    auto it = self->vmas.find((uintptr_t)addr);
    if (it == self->vmas.end()) {
        return (void *)(unsigned long)(-ENOMEM);
    }
    if (uswitch_mremap(ctx, addr, new_size) == -1) {
        return (void *)(unsigned long)(-ENOMEM);
    }
    it->second.end = it->second.start + new_size;
    return 0;
}

int USwitchSandbox::mprotect_callback(uswctx_t ctx, void *data, void *addr, size_t length, int prot) {
    USwitchSandbox *self = (USwitchSandbox *)data;
    std::lock_guard<std::mutex> lock(self->mutex);
    if (!self->allow_mmap || (prot & PROT_EXEC)) {
        return -EPERM;
    }
    size_t alignment = 4096;
    uintptr_t start = (uintptr_t)addr;
    uintptr_t end = start + length;
    if ((start & (alignment - 1)) || end < start) {
        return -EINVAL;
    }
    end = (end + alignment - 1) & (~(alignment - 1));
    if (start == end) {
        return -ENOMEM;
    }
    auto beg = self->vmas.upper_bound(start);
    if (beg != self->vmas.begin()) {
        --beg;
    }
    bool has_found_vma = false;
    uintptr_t last_end;
    for (auto it = beg; it != self->vmas.end(); ++it) {
        const VirtualMemoryArea &vma = it->second;
        if (end > vma.start && start < vma.end) {
            uintptr_t e = vma.end < end ? vma.end : end;
            if (!has_found_vma) {
                has_found_vma = true;
                last_end = e;
            } else {
                if (vma.start != last_end) {
                    // not contiguous
                    return -ENOMEM;
                }
                last_end = e;
            }
        }
    }
    if (!has_found_vma || last_end != end) {
        return -ENOMEM;
    }

    std::vector<VirtualMemoryArea> vma_to_add;
    bool remove;
    for (auto it = beg; it != self->vmas.end();) {
        const VirtualMemoryArea &vma = it->second;
        remove = false;
        if (end > vma.start && start < vma.end) {
            remove = true;
            uintptr_t s = vma.start > start ? vma.start : start;
            uintptr_t e = vma.end < end ? vma.end : end;
            if (uswitch_mprotect(ctx, (void *)vma.start, (void *)s, e - s, prot) == -1) {
                return -ENOMEM;
            }
            VirtualMemoryArea m1, m2, m3;
            m1.start = vma.start;
            m1.end = s;
            m1.prot = vma.prot;
            m1.flags = vma.flags;
            m2.start = e;
            m2.end = vma.end;
            m2.prot = vma.prot;
            m2.flags = vma.flags;
            m3.start = s;
            m3.end = e;
            m3.prot = prot;
            m3.flags = vma.flags;
            if (m1.end > m1.start) {
                vma_to_add.push_back(m1);
            }
            if (m2.end > m2.start) {
                vma_to_add.push_back(m2);
            }
            if (m3.end > m3.start) {
                vma_to_add.push_back(m3);
            }
        }
        if (remove) {
            self->vmas.erase(it++);
        } else {
            ++it;
        }
    }
    for (auto &&vma : vma_to_add) {
        self->vmas[vma.start] = vma;
    }
    return 0;
}

void *USwitchSandbox::thread_routine(void *data) {
    auto pdata = (std::tuple<USwitchSandbox *, void (*)(void *), void *> *)data;
    auto [self, routine, data_] = *pdata;
    delete pdata;
    void *ret;
    uintptr_t tp = get_tp();
    USwitchThread *thread = nullptr;
    //printf("thread routine tid %d\n", syscall(__NR_gettid));
    uswctx_t ctx = self->get_context(&thread);
    if (!ctx) {
        return nullptr;
    }
    uswitch_call_dynamic(ctx, routine, ret, data_);
    if (thread->tls_state->has_retval) {
        ret = thread->tls_state->retval;
    }
    {
        std::lock_guard<std::mutex> lock(self->mutex);
        self->threads.erase((pthread_t)tp);
        uswitch_set_cleanup_routine(thread->ctx, nullptr);
        uswitch_destroy_context(thread->ctx);
        self->thread_occupied[thread->index] = 0;
        self->contexts.erase(tp);
        --self->num_threads;
    }
    return ret;
}

NOCANARY uintptr_t USwitchSandbox::get_thread_pointer() {
    void *userdata;
    if (uswitch_current(nullptr, nullptr, &userdata) == -1 || !userdata) {
        return 0;
    }
    SharedTLSData *state = (SharedTLSData *)userdata;
    return state->thread_pointer;
}

USwitchSandboxSemaphore::USwitchSandboxSemaphore(int count_) : count(count_) {}

void USwitchSandboxSemaphore::signal() {
    std::lock_guard<std::mutex> lock(mutex);
    ++count;
    cv.notify_one();
}

void USwitchSandboxSemaphore::wait() {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] {
        return count > 0;
    });
    --count;
}

USwitchSandboxManager::USwitchSandboxManager(int limit_) : limit(limit_) {}

std::shared_ptr<USwitchSandboxResource> USwitchSandboxManager::create_sandbox(
    const USwitchSandboxManager::Factory &factory) {
    std::lock_guard<std::recursive_mutex> lock(mutex);
    while (sandboxes.size() >= limit) {
        size_t min_ref = -1;
        auto min_ref_it = sandboxes.end();
        for (auto it = sandboxes.begin(); it != sandboxes.end(); ++it) {
            size_t count = it->use_count();
            if (count < min_ref) {
                min_ref = count;
                min_ref_it = it;
            }
        }
        if (min_ref_it != sandboxes.end()) {
            sandboxes.erase(min_ref_it);
        }
    }
    uswitch_sandbox_semaphore.wait();
    std::shared_ptr<USwitchSandboxResource> sandbox(factory(), [this] (USwitchSandboxResource *s) {
        delete s;
        uswitch_sandbox_semaphore.signal();
    });
    sandboxes.insert(sandbox);
    return sandbox;
}

void USwitchSandboxManager::reset() {
    sandboxes.clear();
}

const char *get_cached_shared_object(const std::string &lib) {
    static std::unordered_map<std::string, std::string> so_cache;
    static std::mutex mutex;
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto it = so_cache.find(lib);
        if (it != so_cache.end()) {
            return it->second.c_str();
        }
        const std::string &filename = get_library_path(lib);
        int sofd = open(filename.c_str(), O_RDONLY);
        int memfd = memfd_create(lib.c_str(), 0);
        if (sofd == -1 || memfd == -1) {
            close(sofd);
            close(memfd);
            return nullptr;
        }
        constexpr size_t BufferSize = 4096 * 1024;
        ssize_t sent;
        do {
            sent = sendfile(memfd, sofd, 0, BufferSize);
        } while (sent > 0);
        close(sofd);
        if (sent < 0) {
            close(memfd);
            return nullptr;
        }
        so_cache[lib] = "/proc/self/fd/" + std::to_string(memfd);
        return so_cache[lib].c_str();
    }
}

std::string get_shared_object_copy(const std::string &filename) {
    int sofd = open(filename.c_str(), O_RDONLY);
    int memfd = memfd_create(filename.c_str(), 0);
    if (sofd == -1 || memfd == -1) {
        close(sofd);
        close(memfd);
        return "";
    }
    constexpr size_t BufferSize = 4096 * 1024;
    ssize_t sent;
    do {
        sent = sendfile(memfd, sofd, 0, BufferSize);
    } while (sent > 0);
    close(sofd);
    if (sent < 0) {
        close(memfd);
        return "";
    }
    return "/proc/" + std::to_string(getpid()) +"/fd/" + std::to_string(memfd);
}
