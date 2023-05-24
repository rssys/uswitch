#pragma once

#define NOCANARY __attribute__((__optimize__("-fno-stack-protector")))
#define NOINLINE __attribute__((noinline))
#define USWITCH_PUBLIC __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif
#define USWITCH_SIG_RET_ERR ((void (*)(int))2)
struct USwitchContext;
typedef struct USwitchContext *uswctx_t;
typedef struct USwitchProcessContext *uswpctx_t;
struct sigaction;
USWITCH_PUBLIC int uswitch_start(uswctx_t ctx, void (*entry)(void *), void *data);
USWITCH_PUBLIC int uswitch_resume(uswctx_t ctx);
USWITCH_PUBLIC int uswitch_yield();
USWITCH_PUBLIC void uswitch_ret();
USWITCH_PUBLIC int uswitch_init(int flags);
USWITCH_PUBLIC int uswitch_new_context(uswctx_t *ctx, void *stack, size_t stack_size, void *userdata1, void *userdata2);
USWITCH_PUBLIC int uswitch_destroy_context(uswctx_t ctx);
USWITCH_PUBLIC int uswitch_share(void *addr, size_t size, int prot, int readonly);
USWITCH_PUBLIC int uswitch_mmap(uswctx_t ctx, void *addr, size_t size, int prot);
USWITCH_PUBLIC int uswitch_mprotect(uswctx_t ctx, void *addr, void *start, size_t size, int prot);
USWITCH_PUBLIC int uswitch_munmap(uswctx_t ctx, void *addr, void *start, size_t size);
USWITCH_PUBLIC int uswitch_mremap(uswctx_t ctx, void *addr, size_t new_size);
USWITCH_PUBLIC int uswitch_share_pages();
USWITCH_PUBLIC int uswitch_call(uswctx_t ctx, void (*entry)(void *), void *data);
USWITCH_PUBLIC int uswitch_register_callback(uswctx_t ctx,
    long (*handler)(uswctx_t, void *, void *, long, long, long, long, long, long),
    void *data1, void *data2);
USWITCH_PUBLIC int uswitch_unregister_callback(uswctx_t ctx, int callback_id);
USWITCH_PUBLIC void *uswitch_push_stack(uswctx_t ctx, ssize_t size);
USWITCH_PUBLIC int uswitch_get_userdata(uswctx_t ctx, void **userdata1, void **userdata2);
USWITCH_PUBLIC int uswitch_check_stack_pointer(uswctx_t ctx, const void *ptr, size_t size);
USWITCH_PUBLIC int uswitch_get_cid(uswctx_t ctx);
USWITCH_PUBLIC int uswitch_get_context(uswctx_t *ctx, int cid);
USWITCH_PUBLIC int uswitch_get_typed_callback_id(uswctx_t ctx, size_t type, int cbid);
USWITCH_PUBLIC int uswitch_call_without_mprotect(uswctx_t ctx, void (*entry)(void *), void *data);
USWITCH_PUBLIC int uswitch_dup_context(uswctx_t *new_ctx, uswctx_t old_ctx);
USWITCH_PUBLIC int uswitch_set_stack(uswctx_t ctx, void *stack, size_t stack_size);
USWITCH_PUBLIC int uswitch_set_userdata(uswctx_t ctx, void *userdata1, void *userdata2);
USWITCH_PUBLIC int uswitch_clean_thread();
USWITCH_PUBLIC void uswitch_lock_mutex();
USWITCH_PUBLIC void uswitch_unlock_mutex();
USWITCH_PUBLIC int uswitch_set_cleanup_routine(uswctx_t ctx, void (*routine)(uswctx_t, void *, void *));
USWITCH_PUBLIC int uswitch_set_memory_check_routine(uswctx_t ctx, bool (*routine)(uswctx_t, void *, void *, void *, size_t));
USWITCH_PUBLIC int uswitch_get_pcontext(uswctx_t ctx, uswpctx_t *pctx);
USWITCH_PUBLIC int uswitch_dup_context2(uswctx_t *new_ctx, uswpctx_t pctx);
USWITCH_PUBLIC int uswitch_destroy_pcontext(uswpctx_t pctx);
USWITCH_PUBLIC int uswitch_cleanup();
USWITCH_PUBLIC int uswitch_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
USWITCH_PUBLIC int uswitch_sandbox_sigaction(int signum, bool passthrough, const struct sigaction *act);
USWITCH_PUBLIC int uswitch_get_signal();
USWITCH_PUBLIC int uswitch_ret_signal();
USWITCH_PUBLIC int uswitch_dup_fd(uswctx_t ctx, int fd);
NOCANARY static inline int uswitch_current(uswctx_t *ctx, void **userdata1, void **userdata2) {
    int (*f)(uswctx_t *, void **, void **);
    asm ("movq %%gs:0, %0" : "=r"(f));
    return f(ctx, userdata1, userdata2);
}
NOCANARY static inline int uswitch_callback(int id, long *ret, long arg1, long arg2, long arg3,
    long arg4, long arg5, long arg6) {
    int (*f)(int, long *, long, long, long, long, long, long);
    asm ("movq %%gs:8, %0" : "=r"(f));
    return f(id, ret, arg1, arg2, arg3, arg4, arg5, arg6);
}
NOCANARY static inline int uswitch_callback_typed(size_t type, int tcbid, long *ret, long arg1, long arg2, long arg3,
    long arg4, long arg5, long arg6) {
    int (*f)(size_t, int, long *, long, long, long, long, long, long);
    asm ("movq %%gs:16, %0" : "=r"(f));
    return f(type, tcbid, ret, arg1, arg2, arg3, arg4, arg5, arg6);
}
USWITCH_PUBLIC int uswitch_reset_pkru(int force);
USWITCH_PUBLIC int uswitch_inspect_code(const void *mem, size_t size);
USWITCH_PUBLIC int uswitch_inspect_code2(const void *mem, size_t size);
//#include <unistd.h>
//#include <sys/syscall.h>
//inline void print_pkru(const char *s) {
//    int pkru;
//    asm (
//        "xor %%ecx, %%ecx\n"
//        "rdpkru\n"
//        : "=a" (pkru) :: "ecx", "edx"
//    );
//    printf("%s, pkru: %d, tid: %d, pid: %d\n", s, pkru, syscall(__NR_gettid), getpid());
//}
#ifdef __cplusplus
}
#endif