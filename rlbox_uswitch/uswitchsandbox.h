#pragma once
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <string>
#include <signal.h>
#include <pthread.h>
#include <dlfcn.h>
#include "uswitch.h"

struct USwitchThread;
struct SharedTLSData;
struct ELFSegment {
    uintptr_t addr_start;
    uintptr_t addr_end;
    bool prot_x;
    bool prot_w;
    bool prot_r;
};
struct VirtualMemoryArea {
    uintptr_t start;
    uintptr_t end;
    int prot;
    int flags;
};
class RLBox_USwitch;
class USwitchSandbox {
public:
    using SyscallHandler = std::function<void (unsigned int, void *)>;
    using SimpleSyscallHandler = std::function<long (unsigned int, long, long, long, long, long, long)>;
    enum {
        CallbackIDGetSymbol = 1,
        CallbackIDPthreadCreate = 2,
        CallbackIDPthreadJoin = 3,
        CallbackIDPthreadDetach = 4,
        CallbackIDSigaction = 5,
        CallbackIDMmap = 6,
        CallbackIDMunmap = 7,
        CallbackIDMremap = 8,
        CallbackIDMprotect = 9,
        CallbackIDStart
    };
    enum class SeccompAction {
        SeccompKill,
        SeccompAllow,
        SeccompTrap
    };
    USWITCH_PUBLIC explicit USwitchSandbox(const char *library_, size_t memory_size_, size_t stack_size_,
        int max_threads_=16);
    USWITCH_PUBLIC ~USwitchSandbox();
    USWITCH_PUBLIC bool init();
    USWITCH_PUBLIC bool reset();
    USWITCH_PUBLIC inline uswctx_t get_context() {
        return get_context(nullptr);
    }
    USWITCH_PUBLIC inline void enable_mmap() {
        allow_mmap = true;
    }
    USWITCH_PUBLIC inline void disable_mmap() {
        allow_mmap = false;
    }
    USWITCH_PUBLIC void *get_symbol_addr(const char *symbol);
    USWITCH_PUBLIC void *malloc_in_sandbox(size_t size);
    USWITCH_PUBLIC void free_in_sandbox(void *ptr);
    USWITCH_PUBLIC bool init_seccomp(const std::vector<unsigned int> &allowed_syscalls,
        const std::vector<unsigned int> &trapped_syscalls = DefaultTrappedSyscalls,
        SeccompAction default_action = SeccompAction::SeccompKill);
    USWITCH_PUBLIC static void *get_symbol_addr_in_sandbox(const char *symbol);
    USWITCH_PUBLIC static bool trap_syscalls();
    USWITCH_PUBLIC static void sandbox_signal_handler(int sig, siginfo_t *info, void *context);
    USWITCH_PUBLIC static const struct sigaction *get_sandbox_signal_dispatcher();
    USWITCH_PUBLIC bool register_syscall_handler(unsigned int syscall_num, const SyscallHandler &handler);
    USWITCH_PUBLIC bool register_syscall_handler(unsigned int syscall_num, const SimpleSyscallHandler &handler);
    USWITCH_PUBLIC void allow_sandbox_signals(const std::unordered_set<int> &signals);
    USWITCH_PUBLIC bool dispatch_sandbox_signal(int sig, siginfo_t *info);
    void *custom_data;
private:
    USWITCH_PUBLIC static const std::vector<unsigned int> DefaultTrappedSyscalls;
    friend class RLBox_USwitch;
    USWITCH_PUBLIC uswctx_t get_context(USwitchThread **thread);
    USWITCH_PUBLIC void exit_thread(uswctx_t ctx, bool release_thread);
    bool init_hook();
    bool init_callback();
    USwitchThread *init_thread();
    static void *malloc_hook(size_t size);
    static void *realloc_hook(void *ptr, size_t size);
    static void free_hook(void *ptr);
    static void *aligned_alloc_hook(size_t align, size_t len);
    static int pthread_create_hook(pthread_t *thread, const pthread_attr_t *attr,
        void *(*start_routine)(void *), void *arg);
    static std::pair<int, pthread_t> pthread_create_callback(uswctx_t ctx, void *data,
        void *(*start_routine)(void *), void *arg);
    static int pthread_join_hook(pthread_t thread, void **retval);
    static std::pair<int, void *> pthread_join_callback(uswctx_t ctx, void *data, pthread_t thread);
    static int pthread_detach_hook(pthread_t thread);
    static int pthread_detach_callback(uswctx_t ctx, void *data, pthread_t thread);
    static void pthread_exit_hook(void *retval);
    static int sigaction_callback(uswctx_t ctx, void *data, int sig, const struct sigaction *act,
        struct sigaction *oldact);
    static void *mmap_hook(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
    static int munmap_hook(void *addr, size_t length);
    static void *mremap_hook(void *old_address, size_t old_size, size_t new_size,
        int flags, void *new_address);
    static int mprotect_hook(void *addr, size_t length, int prot);
    static void *mmap_callback(uswctx_t ctx, void *data, void *addr, size_t length, int prot,
        int flags, int fd, off_t offset);
    static int munmap_callback(uswctx_t ctx, void *data, void *addr, size_t length);
    static void *mremap_callback(uswctx_t ctx, void *data, void *old_address, size_t old_size,
        size_t new_size, int flags, void *new_address);
    static int mprotect_callback(uswctx_t ctx, void *data, void *addr, size_t length, int prot);
    static uintptr_t get_thread_pointer();
    static void *thread_routine(void *data);
    static void sigsys_handler(int sig, siginfo_t *info, void *ucontext);
    int max_threads;
    bool has_init;
    uswctx_t main_ctx;
    uswpctx_t pctx;
    std::unordered_map<uintptr_t, std::unique_ptr<USwitchThread>> contexts;
    std::unordered_map<uswctx_t, uintptr_t> ctx_tp_map;
    std::unordered_map<unsigned int, SyscallHandler> syscall_handlers;
    size_t memory_size;
    size_t total_stack_size;
    size_t stack_size;
    size_t tls_size;
    size_t tls_per_thread_size;
    size_t tls_modid;
    std::string library;
    Lmid_t lmid;
    void *handle;
    uint8_t *memory;
    uint8_t *tls;
    uint8_t *stack_start;
    int num_threads;
    std::vector<int> thread_occupied;
    SharedTLSData *main_tls_state;
    std::function<void (void)> on_create;
    std::unordered_set<pthread_t> threads;
    std::mutex mutex;
    ELFSegment tls_segment;
    std::vector<std::pair<ELFSegment, std::unique_ptr<uint8_t []>>> rw_segments;
    void *(*malloc_addr)(size_t);
    void (*free_addr)(void *);
    std::unordered_map<int, std::shared_ptr<struct sigaction>> sandbox_sigactions;
    std::unordered_set<int> allowed_sandbox_signals;
    bool allow_mmap;
    std::map<uintptr_t, VirtualMemoryArea> vmas;
};

class USwitchSandboxSemaphore {
public:
    USWITCH_PUBLIC explicit USwitchSandboxSemaphore(int count_);
    USWITCH_PUBLIC void signal();
    USWITCH_PUBLIC void wait();
private:
    std::mutex mutex;
    std::condition_variable cv;
    int count;
};

class USwitchSandboxResource {
public:
    virtual ~USwitchSandboxResource() {}
};

class USwitchSandboxManager {
public:
    using Factory = std::function<USwitchSandboxResource *()>;
    USWITCH_PUBLIC explicit USwitchSandboxManager(int limit_);
    USWITCH_PUBLIC std::shared_ptr<USwitchSandboxResource> create_sandbox(const Factory &factory);
    USWITCH_PUBLIC void reset();
private:
    std::unordered_set<std::shared_ptr<USwitchSandboxResource>> sandboxes;
    std::recursive_mutex mutex;
    int limit;
};

USWITCH_PUBLIC const char *get_cached_shared_object(const std::string &lib);

extern USWITCH_PUBLIC USwitchSandboxSemaphore uswitch_sandbox_semaphore;
extern USWITCH_PUBLIC USwitchSandboxManager uswitch_sandbox_manager;

#define uswitch_sandbox_call(x, ...) [&]{ \
    char symbol[] = #x; \
    return (((decltype(x) *)USwitchSandbox::get_symbol_addr_in_sandbox(symbol))(__VA_ARGS__)); }()
