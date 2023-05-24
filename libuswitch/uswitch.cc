#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <atomic>
#include <memory>
#include <mutex>
#include <vector>
#include <fstream>
#include <string_view>
#include <string>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cinttypes>
#include <ctime>
#include <x86intrin.h>
#include <unistd.h>
#include <ucontext.h>
#include <signal.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <linux/uswitch.h>
#include <pthread.h>
#include "uswitch.h"

#if defined(__GLIBC__) && __GLIBC_MINOR__ >= 34
#include <sys/rseq.h>
#endif

#ifdef USWITCH_DISABLE_MPK
#define pkey_mprotect(a, b, c, d) pkey_mprotect(a, b, c, -1)
#define pkey_alloc(a, b) (0)
#define pkey_free(a) (0)
#endif

#ifdef USWITCH_SINGLE_THREADED
#define USWITCH_MT(...)
#else
#define USWITCH_MT(...) __VA_ARGS__
#endif

#ifdef USWITCH_NODEFER
#define USWITCH_CNTL_SWITCH USWITCH_CNTL_SWITCH_PAGE_TABLE
#define USWITCH_SW(...) __VA_ARGS__
#else
#define USWITCH_SW(...)
#endif

#ifdef USWITCH_ONLYMEMPROT
#define USWITCH_SW(...)
#endif

struct RegisterContext {
    void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
};

struct MainContext {
    RegisterContext regs;
    USwitchContext *ctx;
};

struct CallbackArgument {
    int callback_id;
    size_t type;
    int typed_callback_id;
    int res;
    long ret;
    long arg1, arg2, arg3, arg4, arg5, arg6;
};

struct USwitchReadonlyState {
    int pkru;
    int cid;
    USwitchContext *ctx;
    bool has_init;
    CallbackArgument *callback_arg;
    void *userdata1;
    void *userdata2;
};

typedef long (*CallbackHandler)(USwitchContext *, void *, void *, long, long, long, long, long, long);

struct USwitchMemoryMap {
    void *addr;
    size_t size;
    int prot;
};

struct USwitchProcessContext {
    int cid;
    int vpkey;
    int pkey;
    int max_callback_id;
    int ref;
    std::mutex mutex;
    std::unordered_map<int, std::tuple<CallbackHandler, void *, void *>> callback_handlers;
    std::unordered_map<size_t, std::vector<int>> type_callback_id_map;
    std::unordered_map<void *, USwitchMemoryMap> maps;
};

struct USwitchContext {
    RegisterContext registers;
    int cid;
    void *userdata1;
    void *userdata2;
    std::vector<RegisterContext> registers_stack;
    USwitchProcessContext *process_context;
    uint8_t *stack_base;
    uint8_t *stack_ptr;
    size_t stack_size;
    void (*cleanup_routine)(uswctx_t, void *, void *);
    bool (*memory_check_routine)(uswctx_t, void *, void *, void *, size_t);
};

struct USwitchState {
    int *kernel_cid;
    MainContext main_context;
    uint8_t *signal_stack;
    int pkru; // for signal handling
    std::vector<MainContext> *main_context_stack;
    std::unordered_map<int, std::unique_ptr<USwitchContext>> *contexts;
    uint8_t *signal_stack_base;
    size_t signal_stack_size;
    uint8_t *signal_alt_stack;
    size_t signal_alt_stack_size;
    int signal_number;
    int unhandled_signal_number;
};

struct USwitchPkeyEntry {
    int vpkey;
    std::atomic_int ref;
};

struct USwitchSignalAction {
    struct sigaction priv_action;
    bool passthrough;
    bool has_set_handler;
    struct sigaction sandbox_action;
    struct sigaction real_action;
};

struct USwitchProcessState {
    bool has_init;
    int max_vpkey;
    USwitchPkeyEntry pkeys[16];
    std::unordered_map<int, USwitchProcessContext *> *vpkey_pctx_map;
    std::unordered_map<USwitchProcessContext *, std::unique_ptr<USwitchProcessContext>> *process_contexts;
    std::mutex mutex;
    bool enable_signal;
    std::shared_ptr<USwitchSignalAction> *sigactions;
};
union USwitchFunctionTable {
    struct {
        int (*uswitch_current)(USwitchContext **ctx, void **userdata1, void **userdata2);
        int (*uswitch_callback)(int id, long *ret, long arg1, long arg2, long arg3,
            long arg4, long arg5, long arg6);
        int (*uswitch_callback_typed)(size_t type, int tcbid, long *ret, long arg1, long arg2, long arg3,
            long arg4, long arg5, long arg6);
    };
    char padding[4096];
};

struct MemoryMap {
    std::pair<uintptr_t, uintptr_t> addr;
    bool prot_r;
    bool prot_w;
    bool prot_x;
    bool prot_p;
    std::string filename;
};

enum PkeyMapping {
    PkeyDefault,
    PkeyReadonly,
    PkeyShared,
    PkeySandbox,
};

thread_local USwitchState uswitch_state;
thread_local char uswitch_rostate_buf[8192]; // two pages
//thread_local USwitchCleanup uswitch_cleanup;
USwitchProcessState uswitch_prstate;
USwitchFunctionTable uswitch_function_table __attribute__((aligned(4096)));
extern uint8_t __start_uswitch_trusted_code;
extern uint8_t __stop_uswitch_trusted_code;

constexpr static int SignalNumber = 64;

static inline USwitchReadonlyState &get_uswitch_rostate() {
    uintptr_t addr;
    asm (R"(
        rdfsbase %0
        addq $uswitch_rostate_buf@tpoff+0xfff, %0
        andq $0xfffffffffffff000, %0
    )"
        : "=r" (addr)
    );
    addr = (addr + 0xffflu) & (~0xffflu);
    return *(USwitchReadonlyState *)addr;
}

static int uswitch_process_init();
static void uswitch_entry(void (*entry)(void *), void *data);
static int get_real_pkey(int vpkey, int old_pkey);
static int reset_pkey(int vpkey, int pkey);
static void get_memory_maps(std::vector<MemoryMap> &maps);
static bool inspect_code();
static bool check_sandbox_memory(uintptr_t ptr, size_t size);
static int *read_pkru_from_xsave(const struct _xstate *xstate);

extern "C" bool NOCANARY NOINLINE context_switch_start(RegisterContext *old_ctx,
    const RegisterContext *new_ctx, void (*entry)(void *), void *data);
extern "C" bool NOCANARY NOINLINE context_switch_resume(RegisterContext *old_ctx,
    const RegisterContext *new_ctx);
extern "C" bool rewrite_glibc_wrpkru(uint8_t *start, uintptr_t len, uint8_t *addr);
extern "C" bool rewrite_ld_so_xrstor(uint8_t *start, uintptr_t len, uint8_t *addr);
//extern "C" int uswitch_reset_pkru_signal();
extern "C" uintptr_t uswitch_signal_handler(int sig, siginfo_t *info, uintptr_t frame);
extern "C" void uswitch_signal_handler_trampoline(int sig, siginfo_t *info, void *ucontext);
extern "C" void uswitch_signal_handler_noinit(int sig, siginfo_t *info, void *ucontext);

static void print_map() {
    std::ifstream ifs("/proc/self/maps");
    std::cout << ifs.rdbuf();
}

static inline void set_context(USwitchReadonlyState &uswitch_rostate, USwitchContext *ctx) {
    uswitch_rostate.ctx = ctx;
    if (ctx) {
        uswitch_rostate.callback_arg = (CallbackArgument *)ctx->stack_base;
        uswitch_rostate.userdata1 = ctx->userdata1;
        uswitch_rostate.userdata2 = ctx->userdata2;
    } else {
        uswitch_rostate.callback_arg = nullptr;
        uswitch_rostate.userdata1 = nullptr;
        uswitch_rostate.userdata2 = nullptr;
    }
}

extern "C" NOCANARY int uswitch_start(USwitchContext *ctx,
    void (*entry)(void *), void *data) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init) {
        return -1;
    }
    int pkey = get_real_pkey(ctx->process_context->vpkey, ctx->process_context->pkey);
    ctx->process_context->pkey = pkey;
    if (pkey == -1) {
        return -1;
    }
    //int pkru = 0;
    int pkru = ~(0x34 | (1 << (2 * pkey)) | (1 << (2 * pkey + 1)));
    uswitch_rostate.pkru = pkru;
    uswitch_rostate.cid = ctx->cid;
    set_context(uswitch_rostate, ctx);
    uswitch_rostate.callback_arg->callback_id = 0;
    ctx->registers_stack.push_back(ctx->registers);
    ctx->registers.rip = (void *)uswitch_entry;
    ctx->registers.rsp = (void *)(((uintptr_t)ctx->stack_ptr & ~0xfl) - 8);
    // System V ABI requies 16byte alignment of rsp
    uswitch_state.main_context_stack->push_back(uswitch_state.main_context);
    uswitch_state.main_context.ctx = ctx;
    *uswitch_state.kernel_cid = ctx->cid;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    bool has_ret = context_switch_start(&uswitch_state.main_context.regs, &ctx->registers, entry, data);
    --uswitch_prstate.pkeys[ctx->process_context->pkey].ref;
    ctx->stack_ptr = (uint8_t *)ctx->registers.rsp - 128;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    if (has_ret) {
        ctx->registers = ctx->registers_stack.back();
        ctx->registers_stack.pop_back();
        uswitch_state.main_context = uswitch_state.main_context_stack->back();
        uswitch_state.main_context_stack->pop_back();
        if (ctx->registers_stack.empty()) {
            ctx->stack_ptr = ctx->stack_base + ctx->stack_size;
        }
        USwitchContext *old_ctx = uswitch_state.main_context.ctx;
        if (old_ctx != uswitch_rostate.ctx) {
            set_context(uswitch_rostate, old_ctx);
        }
        return 0;
    }
    return -2;
}

extern "C" NOCANARY int uswitch_resume(USwitchContext *ctx) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    if (!ctx->registers_stack.size()) {
        return -1;
    }
    int pkey = get_real_pkey(ctx->process_context->vpkey, ctx->process_context->pkey);
    ctx->process_context->pkey = pkey;
    if (pkey == -1) {
        return -1;
    }
    //int pkru = 0;
    int pkru = ~(0x34 | (1 << (2 * pkey)) | (1 << (2 * pkey + 1)));
    uswitch_rostate.pkru = pkru;
    uswitch_rostate.cid = ctx->cid;
    set_context(uswitch_rostate, ctx);
    *uswitch_state.kernel_cid = ctx->cid;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    bool has_ret = context_switch_resume(&uswitch_state.main_context.regs, &ctx->registers);
    --uswitch_prstate.pkeys[ctx->process_context->pkey].ref;
    ctx->stack_ptr = (uint8_t *)ctx->registers.rsp - 128;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    if (has_ret) {
        ctx->registers = ctx->registers_stack.back();
        ctx->registers_stack.pop_back();
        uswitch_state.main_context = uswitch_state.main_context_stack->back();
        uswitch_state.main_context_stack->pop_back();
        if (ctx->registers_stack.empty()) {
            ctx->stack_ptr = ctx->stack_base + ctx->stack_size;
        }
        USwitchContext *old_ctx = uswitch_state.main_context.ctx;
        if (old_ctx != uswitch_rostate.ctx) {
            set_context(uswitch_rostate, old_ctx);
        }
        return 0;
    }
    return -2;
}

constexpr static size_t SignalStackSize = 128 * 1024;
constexpr static size_t AltSignalStackSize = 4096 * 2;
// alt stack is only used to store the signal frame

extern "C" int uswitch_init(int flags) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (uswitch_rostate.has_init) {
        return -1;
    }
    if ((unsigned long)&uswitch_rostate & 0xfff) {
        return -1;
    }
    if (!uswitch_prstate.has_init) {
        if (uswitch_process_init() == -1) {
            return -1;
        }
    }
#ifdef __GLIBC_HAVE_KERNEL_RSEQ
    uintptr_t thread_pointer = (uintptr_t)__builtin_thread_pointer();
    uintptr_t rseq_addr = thread_pointer + __rseq_offset;
    struct rseq *rseq = (struct rseq *)rseq_addr;
    syscall(SYS_rseq, rseq, sizeof(struct rseq), RSEQ_FLAG_UNREGISTER, RSEQ_SIG);
#endif
    uswitch_rostate.cid = 0;
    uswitch_rostate.ctx = nullptr;
    if (pkey_mprotect((void *)&uswitch_rostate, 4096, PROT_READ | PROT_WRITE, PkeyReadonly) == -1) {
        return -1;
    }
#ifdef USWITCH_ONLYMEMPROT
    uswitch_state.kernel_cid = new int[2];
#else
    if (int ret = syscall(449, &uswitch_state.kernel_cid, flags); ret < 0) {
        if ((ret = syscall(451, USWITCH_CNTL_GET_CID, &uswitch_state.kernel_cid)) < 0) {
            return -1;
        }
    }
#endif
    USWITCH_SW(syscall(451, USWITCH_CNTL_DISABLE_DEFERRED_SWITCH));
    uswitch_state.contexts = new std::unordered_map<int, std::unique_ptr<USwitchContext>>;
    uswitch_state.main_context_stack = new std::vector<MainContext>;
    if (uswitch_prstate.enable_signal) {
        uswitch_state.signal_stack_base = (uint8_t *)mmap(nullptr, SignalStackSize, PROT_WRITE | PROT_READ,
            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        if (uswitch_state.signal_stack_base == MAP_FAILED) {
            uswitch_state.signal_stack_base = nullptr;
            return -1;
        }
        // the first page of the signal stack is readonly to the sandboxes
        // because sigreturn requires the stack frame be readable after
        // restoring pkru
        if (pkey_mprotect(uswitch_state.signal_stack_base, 4096, PROT_WRITE | PROT_READ, PkeyReadonly) == -1) {
            return -1;
        }
        uswitch_state.signal_stack_size = SignalStackSize;
        uswitch_state.signal_stack = uswitch_state.signal_stack_base + SignalStackSize;
        uswitch_state.signal_alt_stack = (uint8_t *)mmap(nullptr, AltSignalStackSize, PROT_WRITE | PROT_READ,
            MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
        uswitch_state.signal_alt_stack_size = AltSignalStackSize;
        if (uswitch_state.signal_alt_stack == MAP_FAILED) {
            uswitch_state.signal_alt_stack = nullptr;
            return -1;
        }
        // the alt signal stack should be writable from all contexts
        if (pkey_mprotect(uswitch_state.signal_alt_stack, AltSignalStackSize,
                PROT_WRITE | PROT_READ, PkeyShared) == -1) {
            return -1;
        }
        stack_t stack;
        stack.ss_flags = 0;
        stack.ss_sp = uswitch_state.signal_alt_stack;
        stack.ss_size = uswitch_state.signal_alt_stack_size;
        if (sigaltstack(&stack, nullptr) == -1) {
            return -1;
        }
    }
    asm volatile ("wrgsbase %0" :: "r" (&uswitch_function_table));
    uswitch_rostate.has_init = true;
    uswitch_state.signal_number = -1;
    uswitch_state.unhandled_signal_number = -1;
    return 0;
}

extern "C" int uswitch_new_context(USwitchContext **ctx_, void *stack, size_t stack_size,
    void *userdata1, void *userdata2) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    int new_vpkey;
    USwitchProcessContext *pctx = new USwitchProcessContext;
    pctx->ref = 1;
    pctx->max_callback_id = 1;
    {
        USWITCH_MT(std::lock_guard<std::mutex> lock(uswitch_prstate.mutex));
        new_vpkey = uswitch_prstate.max_vpkey;
        (*uswitch_prstate.process_contexts)[pctx] = std::unique_ptr<USwitchProcessContext>(pctx);
        (*uswitch_prstate.vpkey_pctx_map)[new_vpkey] = pctx;
        pctx->vpkey = new_vpkey;
        pctx->pkey = -1;
        ++uswitch_prstate.max_vpkey;
    }
#ifdef USWITCH_ONLYMEMPROT
    static int MaxCid = 1;
    int cid = ++MaxCid;
#else
    int cid = (int)syscall(450, USWITCH_CLONE_FD_COPY | USWITCH_CLONE_FS_COPY);
#endif
    if (cid < 0) {
        return -1;
    }
    USwitchContext *ctx = new USwitchContext;
    ctx->cid = cid;
    ctx->stack_base = (uint8_t *)stack;
    ctx->stack_size = stack_size;
    ctx->stack_ptr = (uint8_t *)stack + stack_size;
    ctx->registers.rsp = ctx->stack_ptr;
    ctx->userdata1 = userdata1;
    ctx->userdata2 = userdata2;
    ctx->cleanup_routine = nullptr;
    ctx->memory_check_routine = nullptr;
    ctx->process_context = pctx;
    pctx->cid = cid;
    (*uswitch_state.contexts)[cid] = std::unique_ptr<USwitchContext>(ctx);
    *ctx_ = ctx;
    return cid;
}

extern "C" int uswitch_share(void *addr, size_t size, int prot, int readonly) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    int pkey = readonly ? PkeyReadonly : PkeyShared;
    if (pkey_mprotect(addr, size, prot, pkey) == -1) {
        return -1;
    }
    return 0;
}

extern "C" int uswitch_mmap(USwitchContext *ctx, void *addr, size_t size, int prot) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USwitchMemoryMap memory_map;
    memory_map.addr = addr;
    memory_map.size = size;
    memory_map.prot = prot;
    {
        USWITCH_MT(std::lock_guard<std::mutex> l1(uswitch_prstate.mutex));
        USWITCH_MT(std::lock_guard<std::mutex> l2(ctx->process_context->mutex));
        ctx->process_context->maps[addr] = memory_map;
        int pkey = ctx->process_context->pkey;
        if (pkey >= PkeySandbox && pkey < 16 && uswitch_prstate.pkeys[pkey].vpkey == ctx->process_context->vpkey) {
            // if active, set mprotect right now
            if (pkey_mprotect(addr, size, prot, pkey) == -1) {
                return -1;
            }
        }
    }
    return 0;
}

extern "C" int uswitch_mprotect(uswctx_t ctx, void *addr, void *start, size_t size, int prot) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> l1(uswitch_prstate.mutex));
    USWITCH_MT(std::lock_guard<std::mutex> l2(ctx->process_context->mutex));
    auto it = ctx->process_context->maps.find(addr);
    if (it == ctx->process_context->maps.end()) {
        return -1;
    }
    USwitchMemoryMap oldm = it->second;
    if (!(start >= addr && (uintptr_t)start + size < (uintptr_t)addr + oldm.size && size)) {
        return -1;
    }
    int pkey = ctx->process_context->pkey;
    if (pkey >= PkeySandbox && pkey < 16 && uswitch_prstate.pkeys[pkey].vpkey == ctx->process_context->vpkey) {
        // if active, set mprotect right now
        if (pkey_mprotect(start, size, prot, pkey) == -1) {
            return -1;
        }
    }
    
    USwitchMemoryMap m1, m2, m3;
    m1.addr = oldm.addr;
    m1.size = (uintptr_t)start - (uintptr_t)addr;
    m1.prot = oldm.prot;
    m2.addr = (void *)((uintptr_t)start + size); 
    m2.size = oldm.size - m1.size - size;
    m2.prot = oldm.prot;
    m3.addr = start;
    m3.size = size;
    m3.prot = prot;
    ctx->process_context->maps.erase(it);
    if (m1.size) {
        ctx->process_context->maps[m1.addr] = m1;
    }
    if (m2.size) {
        ctx->process_context->maps[m2.addr] = m2;
    }
    if (m3.size) {
        ctx->process_context->maps[m3.addr] = m3;
    }
    return 0;
}

extern "C" int uswitch_munmap(uswctx_t ctx, void *addr, void *start, size_t size) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> l1(uswitch_prstate.mutex));
    USWITCH_MT(std::lock_guard<std::mutex> l2(ctx->process_context->mutex));
    auto it = ctx->process_context->maps.find(addr);
    if (it == ctx->process_context->maps.end()) {
        return -1;
    }
    USwitchMemoryMap oldm = it->second;
    if (!(start >= addr && (uintptr_t)start + size < (uintptr_t)addr + oldm.size)) {
        return -1;
    }
    
    USwitchMemoryMap m1, m2;
    m1.addr = oldm.addr;
    m1.size = (uintptr_t)start - (uintptr_t)addr;
    m1.prot = oldm.prot;
    m2.addr = (void *)((uintptr_t)start + size); 
    m2.size = oldm.size - m1.size - size;
    m2.prot = oldm.prot;
    ctx->process_context->maps.erase(it);
    if (m1.size) {
        ctx->process_context->maps[m1.addr] = m1;
    }
    if (m2.size) {
        ctx->process_context->maps[m2.addr] = m2;
    }
    return munmap(start, size);
}

extern "C" int uswitch_mremap(uswctx_t ctx, void *addr, size_t new_size) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    size_t old_size;
    int old_prot;
    {
        USWITCH_MT(std::lock_guard<std::mutex> l1(uswitch_prstate.mutex));
        USWITCH_MT(std::lock_guard<std::mutex> l2(ctx->process_context->mutex));
        auto it = ctx->process_context->maps.find(addr);
        if (it == ctx->process_context->maps.end()) {
            return -1;
        }
        USwitchMemoryMap &memory_map = it->second;
        old_size = memory_map.size;
        old_prot = memory_map.prot;
        memory_map.size = new_size;
        if (mremap(addr, old_size, new_size, 0) == MAP_FAILED) {
            return -1;
        }
        int pkey = ctx->process_context->pkey;
        if (pkey >= PkeySandbox && pkey < 16 && uswitch_prstate.pkeys[pkey].vpkey == ctx->process_context->vpkey) {
            // if active, set mprotect right now
            if (new_size > old_size) {
                if (pkey_mprotect(addr, new_size, old_prot, pkey) == -1) {
                    return -1;
                }
            }
        }
    }
    return 0;
}

extern "C" int uswitch_share_pages() {
    if (!uswitch_prstate.has_init) {
        return -1;
    }
    std::string line;
    std::ifstream ifs("/proc/self/maps");
    if (!ifs) {
        return -1;
    }
    bool fail = false;
    while (std::getline(ifs, line)) {
        std::string_view line_view(line);
        size_t p1, p2;
        p2 = line_view.find('-');
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }
        line[p2] = 0;
        std::string_view addr_start_s = line_view.substr(0, p2);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }
        line[p2] = 0;
        std::string_view addr_end_s = line_view.substr(p1, p2 - p1);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }
        std::string_view prot_s = line_view.substr(p1, p2 - p1);
        if (prot_s.length() != 4) {
            fail = true;
            continue;
        }
        bool prot_r = prot_s[0] == 'r';
        bool prot_w = prot_s[1] == 'w';
        bool prot_x = prot_s[2] == 'x';
        bool prot_p = prot_s[3] == 'p';
        if (!(prot_r && !prot_w)) {
            continue;
        }

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }
        std::string_view offset_s = line_view.substr(p1, p2 - p1);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            fail = true;
            continue;
        }
        for (p1 = p2 + 1; p1 < line_view.length() && line_view[p1] == ' '; ++p1);
        std::string filename;
        if (p1 != line_view.length()) {
            filename = line_view.substr(p1);
        }

        unsigned long addr_start = strtoul(addr_start_s.begin(), nullptr, 16);
        unsigned long addr_end = strtoul(addr_end_s.begin(), nullptr, 16);
        size_t size = addr_end - addr_start;
        void *addr = (void *)addr_start;
        int prot = 0;
        if (prot_r) {
            prot |= PROT_READ;
        }
        if (prot_w) {
            prot |= PROT_WRITE;
        }
        if (prot_x) {
            prot |= PROT_EXEC;
        }
        if (pkey_mprotect(addr, size, prot, PkeyReadonly) == -1) {
            fail = true;
            continue;
        }
    }
    if (fail) {
        return -1;
    }
    return 0;
}

extern "C" int uswitch_destroy_context(USwitchContext *ctx) {
    if (ctx->cid == -1) {
        return -1;
    }
    if (ctx->cleanup_routine) {
        ctx->cleanup_routine(ctx, ctx->userdata1, ctx->userdata2);
    }
    {
        USWITCH_MT(std::lock_guard<std::mutex> lock(ctx->process_context->mutex));
        --ctx->process_context->ref;
    }
    //uswitch_destroy_pcontext(ctx->process_context);
    //uswitch_state.contexts->erase(ctx->cid);
    ctx->cid = -1;
    return 0;
}

#ifdef USWITCH_STAT
static inline void stat_call() {
    static constexpr uint64_t Interval = 1000000;
    static uint64_t num;
    static uint64_t last_num;
    static uint64_t last_time;
    static uint64_t start_time;
    struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
    uint64_t time = t.tv_sec * 1000000000ull + t.tv_nsec;
    ++num;
    if (!start_time) {
        start_time = time;
    }
    if (num - last_num > Interval) {
        printf("Frequency: %lu\n", num * 1000000000 / (time - start_time));
        last_time = time;
        last_num = num;
    }
}
#endif

extern "C" int uswitch_call(USwitchContext *ctx, void (*entry)(void *), void *data) {
    int ret;
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
#ifdef USWITCH_STAT
    stat_call();
#endif
    if ((ret = uswitch_start(ctx, entry, data)) != -2) {
        if (uswitch_state.unhandled_signal_number != -1) {
            ret = -1;
        }
        return ret;
    }
    CallbackArgument *arg = uswitch_rostate.callback_arg;
    auto get_callback = [&] (std::tuple<CallbackHandler, void *, void *> &handler) {
        USWITCH_MT(std::lock_guard<std::mutex> mutex(ctx->process_context->mutex));
        int callback_id = 0;
        if (uswitch_rostate.callback_arg->typed_callback_id == -1) {
            callback_id = uswitch_rostate.callback_arg->callback_id;
        } else {
            auto it = ctx->process_context->type_callback_id_map.find(uswitch_rostate.callback_arg->type);
            if (it != ctx->process_context->type_callback_id_map.end()) {
                std::vector<int> &type_ids = it->second;
                if (uswitch_rostate.callback_arg->typed_callback_id < type_ids.size()) {
                    callback_id = type_ids[uswitch_rostate.callback_arg->typed_callback_id];
                }
            }
        }
        if (callback_id != 0) {
            auto it = ctx->process_context->callback_handlers.find(callback_id);
            if (it == ctx->process_context->callback_handlers.end()) {
                return false;
            } else {
                handler = it->second;
                return true;
            }
        }
        return false;
    };
    std::tuple<CallbackHandler, void *, void *> handler;
    while (ret == -2) {
        if (get_callback(handler)) {
            //printf("uswitch:cb\n");
            arg->res = 0;
            arg->ret = std::get<0>(handler)(ctx, std::get<1>(handler), std::get<2>(handler),
                arg->arg1, arg->arg2, arg->arg3, arg->arg4, arg->arg5, arg->arg6);
        } else {
            arg->res = -1;
            arg->ret = 0;
        }
#ifdef USWITCH_STAT
        stat_call();
#endif
        ret = uswitch_resume(ctx);
    }
    if (uswitch_state.unhandled_signal_number != -1) {
        ret = -1;
    }
    return ret;
}

static NOCANARY int uswitch_callback_(int id, long *ret, long arg1, long arg2, long arg3,
    long arg4, long arg5, long arg6) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid == 0) {
        return -1;
    }
    CallbackArgument *arg = uswitch_rostate.callback_arg;
    arg->callback_id = id;
    arg->typed_callback_id = -1;
    arg->arg1 = arg1;
    arg->arg2 = arg2;
    arg->arg3 = arg3;
    arg->arg4 = arg4;
    arg->arg5 = arg5;
    arg->arg6 = arg6;
    arg->ret = 0;
    uswitch_yield();
    arg->callback_id = 0;
    if (ret) {
        *ret = arg->ret;
    }
    return arg->res;
}

extern "C" int uswitch_register_callback(USwitchContext *ctx, CallbackHandler handler, void *data1, void *data2) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> lock(ctx->process_context->mutex));
    int callback_id = ctx->process_context->max_callback_id;
    ctx->process_context->callback_handlers[callback_id] = std::make_tuple(handler, data1, data2);
    ++ctx->process_context->max_callback_id;
    return callback_id;
}

extern "C" int uswitch_unregister_callback(USwitchContext *ctx, int callback_id) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> lock(ctx->process_context->mutex));
    auto it = ctx->process_context->callback_handlers.find(callback_id);
    if (it == ctx->process_context->callback_handlers.end()) {
        return -1;
    }
    ctx->process_context->callback_handlers.erase(it);
    return 0;
}

extern "C" void *uswitch_push_stack(USwitchContext *ctx, ssize_t size) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return nullptr;
    }
    if (ctx->stack_ptr < ctx->stack_base || ctx->stack_ptr > ctx->stack_base + ctx->stack_size ||
        ctx->stack_ptr - size < ctx->stack_base || ctx->stack_ptr - size > ctx->stack_base + ctx->stack_size) {
        return nullptr;
    }
    ctx->stack_ptr -= size;
    return ctx->stack_ptr;
}

extern "C" int uswitch_get_userdata(USwitchContext *ctx, void **userdata1, void **userdata2) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    if (userdata1) {
        *userdata1 = ctx->userdata1;
    }
    if (userdata2) {
        *userdata2 = ctx->userdata2;
    }
    return 0;
}

static NOCANARY int uswitch_current_(USwitchContext **ctx, void **userdata1, void **userdata2) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init) {
        return -1;
    }
    if (ctx) {
        *ctx = uswitch_rostate.ctx;
    }
    if (userdata1) {
        *userdata1 = uswitch_rostate.userdata1;
    }
    if (userdata2) {
        *userdata2 = uswitch_rostate.userdata2;
    }
    return 0;
}

extern "C" int uswitch_check_stack_pointer(USwitchContext *ctx, const void *ptr, size_t size) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    uintptr_t stack = (uintptr_t)ctx->stack_base;
    size_t stack_size = ctx->stack_size;
    uintptr_t ptr_u = (uintptr_t)ptr;
    if (ptr_u < stack) {
        return -1;
    }
    if (ptr_u + size >= stack + stack_size) {
        return -1;
    }
    return 0;
}

extern "C" int uswitch_get_cid(USwitchContext *ctx) {
    return ctx->cid;
}

extern "C" int uswitch_get_context(USwitchContext **ctx, int cid) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init) {
        return -1;
    }
    auto it = uswitch_state.contexts->find(cid);
    if (it == uswitch_state.contexts->end()) {
        return -1;
    }
    *ctx = it->second.get();
    return 0;
}

extern "C" int uswitch_get_typed_callback_id(USwitchContext *ctx, size_t type, int cbid) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> lock(ctx->process_context->mutex));
    if (!ctx->process_context->callback_handlers.count(cbid)) {
        return -1;
    }
    std::vector<int> &type_ids = ctx->process_context->type_callback_id_map[type];
    int typed_cbid = type_ids.size();
    type_ids.push_back(cbid);
    return typed_cbid;
}

static NOCANARY int uswitch_callback_typed_(size_t type, int tcbid, long *ret, long arg1, long arg2, long arg3,
    long arg4, long arg5, long arg6) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid == 0) {
        return -1;
    }
    CallbackArgument *arg = uswitch_rostate.callback_arg;
    arg->callback_id = 0;
    arg->type = type;
    arg->typed_callback_id = tcbid;
    arg->arg1 = arg1;
    arg->arg2 = arg2;
    arg->arg3 = arg3;
    arg->arg4 = arg4;
    arg->arg5 = arg5;
    arg->arg6 = arg6;
    arg->ret = 0;
    uswitch_yield();
    arg->callback_id = 0;
    if (ret) {
        *ret = arg->ret;
    }
    return arg->res;
}

extern "C" int uswitch_call_without_mprotect(USwitchContext *ctx, void (*entry)(void *), void *data) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    *uswitch_state.kernel_cid = ctx->cid;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    entry(data);
    *uswitch_state.kernel_cid = 0;
    USWITCH_SW(syscall(451, USWITCH_CNTL_SWITCH));
    return 0;
}

extern "C" int uswitch_dup_context(USwitchContext **new_ctx, USwitchContext *old_ctx) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USwitchContext *ctx = new USwitchContext;
    *ctx = *old_ctx;
    int cid = old_ctx->cid;
    (*uswitch_state.contexts)[cid] = std::unique_ptr<USwitchContext>(ctx);
    *new_ctx = ctx;
    {
        USWITCH_MT(std::lock_guard<std::mutex> lock(ctx->process_context->mutex));
        ++ctx->process_context->ref;
    }
    return 0;
}

extern "C" int uswitch_set_stack(USwitchContext *ctx, void *stack, size_t stack_size) {
    ctx->stack_base = (uint8_t *)stack;
    ctx->stack_size = stack_size;
    ctx->stack_ptr = (uint8_t *)stack + stack_size;
    ctx->registers_stack.clear();
    return 0;
}

extern "C" int uswitch_set_userdata(USwitchContext *ctx, void *userdata1, void *userdata2) {
    ctx->userdata1 = userdata1;
    ctx->userdata2 = userdata2;
    return 0;
}

extern "C" int uswitch_clean_thread() {
    if (uswitch_state.contexts) {
        delete uswitch_state.contexts;
    }
    if (uswitch_state.main_context_stack) {
        delete uswitch_state.main_context_stack;
    }
    if(uswitch_state.signal_stack_base) {
        munmap(uswitch_state.signal_stack_base, uswitch_state.signal_stack_size);
    }
    if (uswitch_state.signal_alt_stack) {
        munmap(uswitch_state.signal_alt_stack, uswitch_state.signal_alt_stack_size);
    }
    return 0;
}

extern "C" void uswitch_lock_mutex() {
    if (!uswitch_prstate.has_init) {
        return;
    }
    USWITCH_MT(uswitch_prstate.mutex.lock());
}

extern "C" void uswitch_unlock_mutex() {
    if (!uswitch_prstate.has_init) {
        return;
    }
    USWITCH_MT(uswitch_prstate.mutex.unlock());
}

extern "C" int uswitch_set_cleanup_routine(USwitchContext *ctx, void (*routine)(uswctx_t, void *, void *)) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init) {
        return -1;
    }
    ctx->cleanup_routine = routine;
    return 0;
}

extern "C" int uswitch_get_pcontext(USwitchContext *ctx, USwitchProcessContext **pctx) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    *pctx = ctx->process_context;
    return 0;
}

extern "C" int uswitch_dup_context2(USwitchContext **new_ctx, USwitchProcessContext *pctx) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    USwitchContext *ctx;
    int cid;
    {
        USWITCH_MT(std::scoped_lock<std::mutex, std::mutex> lock(uswitch_prstate.mutex, pctx->mutex));
        if (!uswitch_prstate.process_contexts->count(pctx)) {
            return -1;
        }
        ctx = new USwitchContext;
        ++pctx->ref;
        cid = pctx->cid;
        ctx->cid = cid;
        ctx->process_context = pctx;
    }
    ctx->cleanup_routine = nullptr;
    ctx->memory_check_routine = nullptr;
    (*uswitch_state.contexts)[cid] = std::unique_ptr<USwitchContext>(ctx);
    *new_ctx = ctx;
    return 0;
}

extern "C" int uswitch_destroy_pcontext(USwitchProcessContext *pctx) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    if (pctx->ref) {
        return -1;
    }
    int cid = pctx->cid;
    int pkey = pctx->pkey;
    int vpkey = pctx->vpkey;
    {
        USWITCH_MT(std::lock_guard<std::mutex> lock(uswitch_prstate.mutex));
        if (pkey >= PkeySandbox && pkey < 16 && uswitch_prstate.pkeys[pkey].vpkey == vpkey) {
            if (uswitch_prstate.pkeys[pkey].ref) {
                return -1;
            }
            uswitch_prstate.pkeys[pkey].vpkey = -1;
        }
        uswitch_prstate.vpkey_pctx_map->erase(vpkey);
        uswitch_prstate.process_contexts->erase(pctx);
    }
#ifndef USWITCH_ONLYMEMPROT
    if (syscall(451, USWITCH_CNTL_DESTROY_CONTEXT, cid) < 0) {
        return -1;
    }
#endif
    return 0;
}

extern "C" int uswitch_cleanup() {
    //printf("start cleanup 1 %d\n", syscall(__NR_gettid));
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0) {
        return -1;
    }
    //printf("start cleanup 2 %d\n", syscall(__NR_gettid));
    while (uswitch_state.contexts->size()) {
        auto it = uswitch_state.contexts->begin();
        USwitchContext *ctx = it->second.get();
        if (ctx->cid == -1) {
            uswitch_state.contexts->erase(it);
            continue;
        }
        uswitch_destroy_context(ctx);
        if (ctx->cid == -1) {
            uswitch_state.contexts->erase(it);
        }
    }
    delete uswitch_state.contexts;
    delete uswitch_state.main_context_stack;
    if (uswitch_prstate.enable_signal) {
        munmap(uswitch_state.signal_stack_base, uswitch_state.signal_stack_size);
        munmap(uswitch_state.signal_alt_stack, uswitch_state.signal_alt_stack_size);
    }
    //printf("finish cleanup %d\n", syscall(__NR_gettid));
    return 0;
}

extern "C" int uswitch_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0 || !uswitch_prstate.enable_signal) {
        return -1;
    }
    if (signum < 0 || signum >= SignalNumber) {
        return -1;
    }
    struct sigaction action, orig_action;
    std::shared_ptr<USwitchSignalAction> old_action_ptr = uswitch_prstate.sigactions[signum];
    USwitchSignalAction old_action;
    bool has_old_action = false;
    bool need_set_handler = false;
    if (old_action_ptr) {
        old_action = *old_action_ptr;
        has_old_action = true;
    }
    if (act) {
        orig_action = *act;
        action = orig_action;
        int flags = 0;
        if (action.sa_flags & SA_NOCLDSTOP) {
            flags |= SA_NOCLDSTOP;
        }
        if (action.sa_flags & SA_NOCLDWAIT) {
            flags |= SA_NOCLDWAIT;
        }
        if (action.sa_flags & SA_RESTART) {
            flags |= SA_RESTART;
        }
        if (action.sa_flags & SA_ONSTACK) {
            flags |= SA_ONSTACK;
        }
        if (action.sa_flags & SA_SIGINFO) {
            need_set_handler = true;
        } else {
            if (action.sa_handler == SIG_DFL || action.sa_handler == SIG_IGN) {
                if (has_old_action && !old_action.passthrough) {
                    struct sigaction &sa = old_action.sandbox_action;
                    if ((sa.sa_flags & SA_SIGINFO) || sa.sa_handler != action.sa_handler) {
                        need_set_handler = true;
                    }
                }
            } else {
                need_set_handler = true;
            }
        }
        if (need_set_handler) {
            flags |= SA_SIGINFO;
            action.sa_sigaction = uswitch_signal_handler_trampoline;
            sigfillset(&action.sa_mask);
        }
        action.sa_flags = flags;
        act = &action;
        old_action_ptr.reset(new USwitchSignalAction);
        old_action_ptr->passthrough = true;
        old_action_ptr->priv_action = orig_action;
        old_action_ptr->has_set_handler = need_set_handler;
        old_action_ptr->real_action = action;
        uswitch_prstate.sigactions[signum] = old_action_ptr;
    }
    
    if (!has_old_action) {
        return sigaction(signum, act, oldact);
    }
    if (sigaction(signum, act, nullptr) == -1) {
        return -1;
    }
    if (oldact) {
        *oldact = old_action.priv_action;
    }
    return 0;
}

extern "C" int uswitch_sandbox_sigaction(int signum, bool passthrough, const struct sigaction *act) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || uswitch_rostate.cid != 0 || !uswitch_prstate.enable_signal) {
        return -1;
    }
    if (signum < 0 || signum >= SignalNumber) {
        return -1;
    }
    std::shared_ptr<USwitchSignalAction> old_action_ptr = uswitch_prstate.sigactions[signum];
    USwitchSignalAction old_action;
    bool has_old_action = false;
    if (old_action_ptr) {
        has_old_action = true;
        old_action = *old_action_ptr;
    } else {
        if (sigaction(signum, nullptr, &old_action.priv_action) == -1) {
            return -1;
        }
        if (uswitch_sigaction(signum, &old_action.priv_action, nullptr) == -1) {
            return -1;
        }
        old_action_ptr = uswitch_prstate.sigactions[signum];
        if (!old_action_ptr) {
            return -1;
        }
        old_action = *old_action_ptr;
    }
    old_action.passthrough = passthrough;
    if (!passthrough) {
        if (!act) {
            return -1;
        }
        struct sigaction action = *act;
        struct sigaction &pa = old_action.priv_action;
        struct sigaction &sa = old_action.sandbox_action;
        struct sigaction &ra = old_action.real_action;
        sa = action;
        bool need_set_handler = false;
        if (sa.sa_flags & SA_SIGINFO) {
            need_set_handler = true;
        } else if (sa.sa_handler == SIG_DFL || sa.sa_handler == SIG_IGN) {
            if (sa.sa_handler != pa.sa_handler) {
                need_set_handler = true;
            }
        } else {
            need_set_handler = true;
        }
        int flags = ra.sa_flags & (~SA_SIGINFO);
        if (need_set_handler) {
            flags |= SA_SIGINFO;
            ra.sa_sigaction = uswitch_signal_handler_trampoline;
            sigfillset(&ra.sa_mask);
        } else {
            ra.sa_handler = sa.sa_handler;
        }
        ra.sa_flags = flags;
        old_action_ptr.reset(new USwitchSignalAction);
        *old_action_ptr = old_action;
        uswitch_prstate.sigactions[signum] = old_action_ptr;
        if (sigaction(signum, &ra, nullptr) == -1) {
            return -1;
        }
    } else if (has_old_action) {
        old_action_ptr.reset(new USwitchSignalAction);
        *old_action_ptr = old_action;
        uswitch_prstate.sigactions[signum] = old_action_ptr;
        return uswitch_sigaction(signum, &old_action.priv_action, nullptr);
    }
    return 0;
}

extern "C" int uswitch_get_signal() {
    return uswitch_state.unhandled_signal_number;
}

extern "C" int uswitch_ret_signal() {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init || !uswitch_prstate.enable_signal) {
        return -1;
    }
    if (uswitch_state.pkru == 0 || uswitch_state.signal_number == -1) {
        return -1;
    }
    uswitch_state.signal_number = -2;
    return 0;
}

extern "C" int uswitch_dup_fd(uswctx_t ctx, int fd) {
    return syscall(451, USWITCH_CNTL_DUP_FILE, ctx->cid, fd);
}

extern "C" int uswitch_set_memory_check_routine(uswctx_t ctx,
    bool (*routine)(uswctx_t, void *, void *, void *, size_t)) {
    USwitchReadonlyState &uswitch_rostate = get_uswitch_rostate();
    if (!uswitch_rostate.has_init) {
        return -1;
    }
    ctx->memory_check_routine = routine;
    return 0;
}

extern "C" int uswitch_inspect_code(const void *mem, size_t size) {
    const uint8_t *start = (const uint8_t *)mem;
    const uint8_t *end = (const uint8_t *)mem + size;
    for (const uint8_t *i = start; i < end; ++i) {
        if (*i == 0x0f && i + 2 < end) {
            if (*(i + 1) == 0x01 && *(i + 2) == 0xef) {
                return -1;
            } else if (*(i + 1) == 0xae) {
                int h = *(i + 2) & 0xf8;
                if (h == 0x28 || h == 0x68 || h == 0xa8) {
                    return -1;
                }
            }
        } else if (*i == 0xf3 && i + 2 < end) {
            if (*(i + 1) == 0x0f && *(i + 2) == 0xae) {
                return -1;
            }
            if (i + 3 < end && (*(i + 1) == 0x48 || *(i + 1) == 0x49) && *(i + 2) == 0x0f && *(i + 3) == 0xae) {
                if (i + 4 < end) {
                    int b = *(i + 4);
                    if (b < 0xd0 || b > 0xd7) {
                        continue;
                    }
                }
                return -1;
            }
        }
    }
    return 0;
}

extern "C" int uswitch_inspect_code2(const void *mem, size_t size) {
    if (uswitch_inspect_code(mem, size) == -1) {
        return -1;
    }
    int fd[2];
    char buf[3];
    if (pipe(fd) == -1) {
        return -1;
    }
    const uint8_t *start = (const uint8_t *)mem;
    auto read_mem = [&] (const uint8_t *m, size_t s) {
        int res = write(fd[1], m, s);
        if (res == -1 && errno == EFAULT) {
            return 0;
        } else if (res != s) {
            return 1;
        }
        return 2;
    };
    int res = read_mem(start - 1, 3);
    if (res == 1) {
        goto fail;
    } else if (res == 2) {
        if (uswitch_inspect_code(start - 1, 3) == -1) {
            goto fail;
        }
    }
    res = read_mem(start - 2, 3);
    if (res == 1) {
        goto fail;
    } else if (res == 2) {
        if (uswitch_inspect_code(start - 2, 3) == -1) {
            goto fail;
        }
    }
    res = read_mem(start + size - 2, 3);
    if (res == 1) {
        goto fail;
    } else if (res == 2) {
        if (uswitch_inspect_code(start + size - 2, 3) == -1) {
            goto fail;
        }
    }
    res = read_mem(start + size - 1, 3);
    if (res == 1) {
        goto fail;
    } else if (res == 2) {
        if (uswitch_inspect_code(start + size - 1, 3) == -1) {
            goto fail;
        }
    }
    close(fd[0]);
    close(fd[1]);
    return 0;
fail:
    close(fd[0]);
    close(fd[1]);
    return -1;
}

int uswitch_process_init() {
    if (uswitch_prstate.has_init) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> lock(uswitch_prstate.mutex));
    if (!inspect_code()) {
        return -1;
    }
    bool fail = false;
    int i;
    for (i = 1; i < 16; ++i) {
        if (pkey_alloc(0, 0) == -1) {
            fail = true;
            break;
        }
    }
    if (fail) {
        for (int j = 1; i < i; ++j) {
            pkey_free(j);
        }
        return -1;
    }
    for (int i = 0; i < 16; ++i) {
        uswitch_prstate.pkeys[i].vpkey = -1;
        uswitch_prstate.pkeys[i].ref = 0;
    }
    if (pkey_mprotect(&uswitch_function_table, 4096, PROT_WRITE | PROT_READ, PkeyReadonly) == -1) {
        return -1;
    }
    uswitch_function_table.uswitch_current = uswitch_current_;
    uswitch_function_table.uswitch_callback = uswitch_callback_;
    uswitch_function_table.uswitch_callback_typed = uswitch_callback_typed_;
    uswitch_prstate.vpkey_pctx_map = new std::unordered_map<int, USwitchProcessContext *>;
    uswitch_prstate.process_contexts =
        new std::unordered_map<USwitchProcessContext *, std::unique_ptr<USwitchProcessContext>>;
    uswitch_prstate.sigactions = new std::shared_ptr<USwitchSignalAction>[SignalNumber];
    uswitch_prstate.enable_signal = true;
    uswitch_prstate.has_init = true;
    uswitch_reset_pkru(0);
    //uswitch_share_pages();
    return 0;
}

int reset_pkey(int vpkey, int pkey) {
    //return 0;
    bool fail = false;
    USwitchProcessContext *pctx = (*uswitch_prstate.vpkey_pctx_map)[vpkey];
    if (!pctx) {
        return -1;
    }
    USWITCH_MT(std::lock_guard<std::mutex> lock(pctx->mutex));
    auto &&maps = pctx->maps;
    for (auto it = maps.begin(); it != maps.end(); ) {
        auto &&map = it->second;
        if (pkey_mprotect(map.addr, map.size, map.prot, pkey) == -1) {
            fail = true;
            printf("fail to remap: %p %p+%lx %d %d %s\n", map.addr, map.addr, map.size, map.prot, pkey, strerror(errno));
            it = maps.erase(it);
        } else {
            ++it;
        }
    }
    return fail ? -1 : 0;
}

int get_real_pkey(int vpkey, int old_pkey) {
    USWITCH_MT(std::lock_guard<std::mutex> lock(uswitch_prstate.mutex));
    if (old_pkey != -1 && uswitch_prstate.pkeys[old_pkey].vpkey == vpkey) {
        ++uswitch_prstate.pkeys[old_pkey].ref;
        return old_pkey;
    }
    for (int i = PkeySandbox; i < 16; ++i) {
        if (uswitch_prstate.pkeys[i].vpkey == -1) {
            if (reset_pkey(vpkey, i) == -1) {
                return -1;
            }
            uswitch_prstate.pkeys[i].vpkey = vpkey;
            ++uswitch_prstate.pkeys[i].ref;
            return i;
        }
    }
    int inactive_pkeys[16];
    int num_inactive_pkeys = 0;
    for (int i = PkeySandbox; i < 16; ++i) {
        if (!uswitch_prstate.pkeys[i].ref) {
            inactive_pkeys[num_inactive_pkeys++] = i;
        }
    }
    if (num_inactive_pkeys == 0) {
        return -1;
    }
    int evicted_pkey = inactive_pkeys[rand() % num_inactive_pkeys];
    int evicted_vpkey = uswitch_prstate.pkeys[evicted_pkey].vpkey;
    reset_pkey(evicted_vpkey, PkeyDefault);
    if (reset_pkey(vpkey, evicted_pkey) == -1) {
        return -1;
    }
    uswitch_prstate.pkeys[evicted_pkey].vpkey = vpkey;
    uswitch_prstate.pkeys[evicted_pkey].ref = 1;
    return evicted_pkey;
}

void get_memory_maps(std::vector<MemoryMap> &maps) {
    std::string line;
    std::ifstream ifs("/proc/self/maps");
    if (!ifs) {
        return;
    }
    while (std::getline(ifs, line)) {
        std::string_view line_view(line);
        size_t p1, p2;
        p2 = line_view.find('-');
        if (p2 == std::string_view::npos) {
            continue;
        }
        line[p2] = 0;
        std::string_view addr_start_s = line_view.substr(0, p2);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            continue;
        }
        line[p2] = 0;
        std::string_view addr_end_s = line_view.substr(p1, p2 - p1);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            continue;
        }
        std::string_view prot_s = line_view.substr(p1, p2 - p1);
        if (prot_s.length() != 4) {
            continue;
        }
        bool prot_r = prot_s[0] == 'r';
        bool prot_w = prot_s[1] == 'w';
        bool prot_x = prot_s[2] == 'x';
        bool prot_p = prot_s[3] == 'p';

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            continue;
        }
        std::string_view offset_s = line_view.substr(p1, p2 - p1);

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            continue;
        }

        p1 = p2 + 1;
        p2 = line_view.find(' ', p1);
        if (p2 == std::string_view::npos) {
            continue;
        }
        for (p1 = p2 + 1; p1 < line_view.length() && line_view[p1] == ' '; ++p1);
        std::string filename;
        if (p1 != line_view.length()) {
            filename = line_view.substr(p1);
        }
        unsigned long addr_start = strtoul(addr_start_s.begin(), nullptr, 16);
        unsigned long addr_end = strtoul(addr_end_s.begin(), nullptr, 16);
        MemoryMap map;
        map.addr = std::make_pair((uintptr_t)addr_start, (uintptr_t)addr_end);
        map.prot_r = prot_r;
        map.prot_w = prot_w;
        map.prot_x = prot_x;
        map.prot_p = prot_p;
        map.filename = filename;
        maps.push_back(map);
    }
}

bool inspect_code() {
    std::vector<MemoryMap> maps;
    get_memory_maps(maps);
    for (auto &&m : maps) {
        if (!m.prot_x || !m.prot_r) {
            continue;
        }
        bool is_glibc = m.filename.find("libc") != std::string::npos;
        bool is_ld_so = m.filename.find("/lib/x86_64-linux-gnu/ld") != std::string::npos;
        uint8_t *start = (uint8_t *)m.addr.first;
        uint8_t *end = (uint8_t *)m.addr.second;
        for (uint8_t *i = start; i < end; ++i) {
            if (*i == 0x0f && i + 2 < end) {
                if (*(i + 1) == 0x01 && *(i + 2) == 0xef) {
                    if (i > &__start_uswitch_trusted_code && i + 2 < &__stop_uswitch_trusted_code) {
                        i = &__stop_uswitch_trusted_code;
                        //printf("found wrpkru: uswitch\n");
                        continue;
                    } else if (is_glibc) {
                        // rewrite
                        //printf("found wrpkru: glibc\n");
                        uintptr_t page_start = (uintptr_t)i & (~0xfffl);
                        uintptr_t page_end = (uintptr_t)(i + 2) & (~0xfffl);
                        if (!rewrite_glibc_wrpkru((uint8_t *)page_start, page_end + 4096 - page_start, i)) {
                            return false;
                        }
                    } else {
                        printf("found wrpkru: %p\n", i);
                        return false;
                    }
                } else if (*(i + 1) == 0xae) {
                    int h = *(i + 2) & 0xf8;
                    if (h == 0x28 || h == 0x68 || h == 0xa8) {
                        if (i > &__start_uswitch_trusted_code && i + 2 < &__stop_uswitch_trusted_code) {
                            i = &__stop_uswitch_trusted_code;
                            continue;
                        }
                        if (i + 5 < end &&
                            *(i + 2) == 0x6c && *(i + 3) == 0x31 &&
                            *(i + 4) == 0x7c && *(i + 5) == 0xec) {
                            // in libm.so, there is a byte of sequence of
                            // 0f ae 6c 31 7c ec in a floating number table,
                            // which can be decoded as:
                            //    xrstor 0x7c(%rcx, %rsi, 1)
                            //    in (%dx), %al
                            // We assume that we don't have the permission to do raw io,
                            // so this shouldn't be exploited
                            continue;
                        }
                        if (is_ld_so && i - 7 >= start && i + 4 < end && 
                            *(i - 7) == 0xb8 && *(i - 6) == 0xee &&
                            *(i - 5) == 0x00 && *(i - 4) == 0x00 &&
                            *(i - 3) == 0x00 && *(i - 2) == 0x31 &&
                            *(i - 1) == 0xd2 && *(i + 2) == 0x6c &&
                            *(i + 3) == 0x24 && *(i + 4) == 0x40) {
                            uintptr_t page_start = (uintptr_t)(i - 7) & (~0xfffl);
                            uintptr_t page_end = (uintptr_t)(i + 4) & (~0xfffl);
                            if (rewrite_ld_so_xrstor((uint8_t *)page_start, page_end + 4096 - page_start, i - 7)) {
                                continue;
                            }
                        }
                        printf("found xrstor: %p\n", i);
                        return false;
                    }
                }
            } else if (*i == 0xf3 && i + 2 < end) {
                if (*(i + 1) == 0x0f && *(i + 2) == 0xae) {
                    printf("found wrfsbase32\n");
                    return false;
                }
                if (i + 3 < end && (*(i + 1) & 0xf0 == 0x40) && *(i + 2) == 0x0f && *(i + 3) == 0xae) {
                    if (i + 4 < end) {
                        int b = *(i + 4);
                        if (b < 0xd0 || b > 0xd7) {
                            continue;
                        }
                    }
                    printf("found wrfsbase\n");
                    return false;
                }
            }
        }
    }
    return true;
}

bool check_sandbox_memory(uintptr_t ptr, size_t size) {
    USwitchReadonlyState &rostate = get_uswitch_rostate();
    if (!rostate.has_init || !rostate.ctx) {
        return false;
    }
    uswctx_t ctx = rostate.ctx;
    if (uswitch_state.signal_alt_stack && ptr >= (uintptr_t)uswitch_state.signal_alt_stack &&
        ptr + size < (uintptr_t)uswitch_state.signal_alt_stack + uswitch_state.signal_alt_stack_size) {
        return true;
    }
    if (ctx->memory_check_routine) {
        return ctx->memory_check_routine(ctx, rostate.userdata1,
            rostate.userdata2, (void *)ptr, size);
    }
    uintptr_t start = (uintptr_t)ctx->stack_base;
    uintptr_t end = start + ctx->stack_size;
    return ptr >= start && ptr + size <= end;
};

int *read_pkru_from_xsave(const struct _xstate *xstate) {
    int offset;
    asm volatile (
        "movl $0x0d, %%eax\n"
        "movl $9, %%ecx\n"
        "cpuid\n"
        : "=b" (offset) :: "eax", "ecx", "edx"
    );

    return (int *)((uintptr_t)xstate + offset);
}

static void exit_sig(int sig) {

    struct sigaction action;
    action.sa_flags = 0;
    action.sa_handler = SIG_DFL;
    if (sigaction(sig, &action, nullptr) == -1) {
        exit(128 + sig);
    }
    sigset_t sigset;
    if (sigfillset(&sigset) == -1 || sigdelset(&sigset, sig) == -1) {
        exit(128 + sig);
    }
    if (sigprocmask(SIG_SETMASK, &sigset, nullptr) == -1) {
        exit(128 + sig);
    }
    raise(sig);
    // unreachable
    exit(128 + sig);
}

extern "C" int get_fpstate_size(uintptr_t ptr, uint32_t *size, uint64_t *xfeatures);
extern "C" int sanitize_fpstate(uintptr_t ptr, size_t size);
uintptr_t uswitch_signal_handler(int sig, siginfo_t *info, uintptr_t frame) {
    USwitchReadonlyState &rostate = get_uswitch_rostate();
    int cid = uswitch_state.kernel_cid[0];
    int old_rostate_cid = rostate.cid;
    rostate.cid = 0;
    uswitch_state.kernel_cid[0] = 0;
    if (sig < 0 || sig >= SignalNumber || !uswitch_prstate.sigactions) {
        exit_sig(sig);
    }
    std::shared_ptr<USwitchSignalAction> action_ptr = uswitch_prstate.sigactions[sig];
    if (!action_ptr) {
        exit_sig(sig);
    }
    USwitchSignalAction action = *action_ptr;
    action_ptr.reset();
    bool is_frame_protected = uswitch_state.pkru == 0;
    // it's very unlikely that uswitch_state.pkru == 0 but the stack
    // is not in the protected memory

    if (!is_frame_protected && !check_sandbox_memory(frame + 8, 8)) {
        exit_sig(sig);
    }
    ucontext_t *uc = (ucontext_t *)(frame + 8);
    if (!is_frame_protected && !check_sandbox_memory((uintptr_t)&uc->uc_mcontext.fpregs, 8)) {
        exit_sig(sig);
    }
    intptr_t siginfo_offset = (uintptr_t)info - frame;
    intptr_t fpstate_offset = (uintptr_t)uc->uc_mcontext.fpregs - frame;
    if (siginfo_offset < 0 || fpstate_offset < 0) {
        exit_sig(sig);
    }
    if (!is_frame_protected && !check_sandbox_memory(frame + fpstate_offset, sizeof(struct _xstate))) {
        exit_sig(sig);
    }
    if (!is_frame_protected && !check_sandbox_memory(frame + siginfo_offset, sizeof(siginfo_t))) {
        exit_sig(sig);
    }
    uint32_t fpstate_size;
    uint64_t xfeatures;
    bool has_extended_fpstate = get_fpstate_size(frame + fpstate_offset,
        &fpstate_size, &xfeatures);
    if (!has_extended_fpstate) {
        exit_sig(sig);
    }
    uintptr_t frame_start = frame;
    uintptr_t frame_size = fpstate_offset + fpstate_size;
    if (!is_frame_protected && !check_sandbox_memory(frame_start, frame_size)) {
        exit_sig(sig);
    }

    // for the signals from the sandboxes,
    // we need to put some fields of the new signal frame in the first page of the signal stack
    // (which is readonly for all unprivileged contexts)
    // because they need to be readable after setting pkru
    // to prevent from informatino leakage, we only store the uc_flags, uc_link and uc_stack fields
    // in the first page

    // for the signals from the privileged context,
    // we put the signal frame in the second page (which is not readable by sandboxes)

    int pkru = uswitch_state.pkru;
    uintptr_t new_frame;
    uintptr_t new_fpstate;
    uintptr_t new_siginfo;
    uintptr_t stack = (uintptr_t)uswitch_state.signal_stack_base;
    if (pkru == 0) {
        // new_frame + fpstate_offset should be 64-byte aligned
        // new_frame == -fpstate_offset (mod 64)
        // new_frame == -8 (mod 16)
        // stack base is 4k aligned
        intptr_t mod = (-fpstate_offset) % 64 + 64;
        new_frame = stack + 4096 + mod;
        new_fpstate = new_frame + fpstate_offset;
        memcpy((void *)new_frame, (void *)frame, frame_size);
    } else {
        // Page0                       ||Page1
        // uc_flags, uc_link, uc_stack ||Other fields
        constexpr int offset_mcontext = offsetof(ucontext_t, uc_mcontext) + 8; // 48
        new_frame = stack + 4096 - offset_mcontext;
        new_fpstate = new_frame + fpstate_offset + 64;
        new_fpstate = new_fpstate & (~0x3f);
        memcpy((void *)new_fpstate, (void *)(frame + fpstate_offset), fpstate_size);
        memcpy((void *)new_frame, (void *)frame, fpstate_offset);
    }
    new_siginfo = new_frame + siginfo_offset;

    // after copy we check again
    uint32_t fpstate_size_new;
    uint64_t xfeatures_new;
    has_extended_fpstate = get_fpstate_size(new_fpstate, &fpstate_size_new, &xfeatures_new);
    if (!has_extended_fpstate || fpstate_size_new != fpstate_size || xfeatures_new != xfeatures) {
        exit_sig(sig);
    }
    if (!sanitize_fpstate(new_fpstate, fpstate_size)) {
        exit_sig(sig);
    }

    ucontext_t *new_uc = (ucontext_t *)(new_frame + 8);
    new_uc->uc_mcontext.fpregs = (fpregset_t)new_fpstate;
    new_uc->uc_stack.ss_flags = SS_DISABLE;
    struct _xstate *xstate = (struct _xstate *)new_fpstate;
    int *pkru_ptr = read_pkru_from_xsave(xstate);

    if (!pkru_ptr || (uintptr_t)pkru_ptr + 4 > (uintptr_t)xstate + fpstate_size) {
        exit_sig(sig);
    }
    *pkru_ptr = pkru;

    // We ensure that if uswitch_state.pkru == 0, the signal frame
    // must be in the protected memory. So we ensure that only when
    // the signal frame is trusted will we set pkru to 0.

    // Also, we ensure that if uswitch_state.pkru != 0,
    // it's always safe to jump to uswitch_ret
    const struct sigaction &pa = action.priv_action;
    const struct sigaction &sa = action.sandbox_action;
    int old_sig = uswitch_state.signal_number;
    uswitch_state.signal_number = sig;
    if (pkru == 0 || action.passthrough) {
        if (pa.sa_flags & SA_SIGINFO) {
            pa.sa_sigaction(sig, (siginfo_t *)new_siginfo, (void *)new_uc);
        } else {
            if (pa.sa_handler == SIG_DFL) {
                exit_sig(sig);
            } else if (pa.sa_handler != SIG_IGN) {
                pa.sa_handler(sig);
            }
        }
    } else {
        // set stack_ptr in case the signal handler needs to invoke a sandboxcall
        get_uswitch_rostate().ctx->stack_ptr = (uint8_t *)new_uc->uc_mcontext.gregs[REG_RSP] - 128;
        if (sa.sa_flags & SA_SIGINFO) {
            sa.sa_sigaction(sig, (siginfo_t *)new_siginfo, (void *)new_uc);
        } else {
            if (sa.sa_handler == SIG_DFL) {
                exit_sig(sig);
            } else if (sa.sa_handler == USWITCH_SIG_RET_ERR) {
                uswitch_state.signal_number = -2;
                uswitch_state.unhandled_signal_number = sig;
                new_uc->uc_mcontext.gregs[REG_RIP] = (uintptr_t)uswitch_ret;
            } else if (sa.sa_handler != SIG_IGN) {
                sa.sa_handler(sig);
            }
        }
        
    }
    if (pkru != 0 && uswitch_state.signal_number == -2) {
        // handler called uswitch_ret_signal
        uswitch_state.unhandled_signal_number = sig;
        new_uc->uc_mcontext.gregs[REG_RIP] = (uintptr_t)uswitch_ret;
    } else {
        uswitch_state.unhandled_signal_number = -1;
    }
    uswitch_state.signal_number = old_sig;

    uswitch_state.kernel_cid[0] = 0;
    uswitch_state.kernel_cid[1] = cid;
    rostate.cid = old_rostate_cid;
    uswitch_state.pkru = pkru;

    return new_frame + 8;
}

void uswitch_signal_handler_noinit(int sig, siginfo_t *info, void *ucontext) {
    if (sig < 0 || sig >= SignalNumber || !uswitch_prstate.sigactions) {
        exit_sig(sig);
    }
    std::shared_ptr<USwitchSignalAction> action_ptr = uswitch_prstate.sigactions[sig];
    if (!action_ptr) {
        exit_sig(sig);
    }
    USwitchSignalAction action = *action_ptr;
    action_ptr.reset();

    const struct sigaction &pa = action.priv_action;
    if (pa.sa_flags & SA_SIGINFO) {
        pa.sa_sigaction(sig, info, ucontext);
    } else {
        if (pa.sa_handler == SIG_DFL) {
            exit_sig(sig);
        } else if (pa.sa_handler != SIG_IGN) {
            pa.sa_handler(sig);
        }
    }
}

NOCANARY void uswitch_entry(void (*entry)(void *), void *data) {
    entry(data);
    uswitch_ret();
}
