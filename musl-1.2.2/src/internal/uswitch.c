#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <pthread_impl.h>
#include <libc.h>

//void *__dso_handle = (void *)&__dso_handle;

extern void malloc_init(void *base, size_t size);
int (*pthread_create_hook)(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg);
int (*pthread_join_hook)(pthread_t thread, void **retval);
int (*pthread_detach_hook)(pthread_t thread);
void (*pthread_exit_hook)(void *retval);
void *(*mmap_hook)(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
int (*munmap_hook)(void *addr, size_t length);
void *(*mremap_hook)(void *old_addr, size_t old_size, size_t new_size, int flags, void *new_addr);
int (*mprotect_hook)(void *addr, size_t length, int prot);
uintptr_t (*uswitch_get_tp)();

int (*uswitch_callback)(int id, long *ret, long arg1, long arg2, long arg3,
    long arg4, long arg5, long arg6);

static void uswitch_init_tp(int tid) {
    struct pthread *tp = (struct pthread *)uswitch_get_tp();
    libc.can_do_threads = 1;
    libc.threaded = 1;
    memset(tp, 0, sizeof(struct pthread));
    tp->self = tp;
    tp->detach_state = DT_JOINABLE;
    tp->tid = tid;
    tp->next = tp->prev = tp;
    size_t modid = *(size_t *)((uint8_t *)tp + 2048 + sizeof(uintptr_t) * 2);
    tp->dtv = (uintptr_t *)((uint8_t *)tp + 2048 + sizeof(uintptr_t)) - modid;
    tp->robust_list.head = &tp->robust_list.head;
}

void uswitch_add_thread(int tid, uintptr_t prev_tp) {
    struct pthread *thread = (struct pthread *)uswitch_get_tp();
    struct pthread *prev_thread = (struct pthread *)prev_tp;
    memset(thread, 0, sizeof(struct pthread));
    thread->prev = prev_thread;
    thread->next = prev_thread->next;
    thread->self = thread;
    thread->detach_state = DT_JOINABLE;
    thread->tid = tid;
    thread->robust_list.head = &thread->robust_list.head;
    size_t modid = *(size_t *)((uint8_t *)thread + 2048 + sizeof(uintptr_t) * 2);
    thread->dtv = (uintptr_t *)((uint8_t *)thread + 2048 + sizeof(uintptr_t)) - modid;
    if (prev_thread->next) {
        prev_thread->next->prev = thread;
    }
    prev_thread->next = thread;
    libc.threaded = 1;
}

struct set_uswitch_functions_ret_t {
    void *(*malloc)(size_t);
    void (*free)(void *);
};

struct set_uswitch_functions_ret_t set_uswitch_functions(
    int tid,
    void *heap_base, size_t heap_size,
    void *pthread_create_hook_, void *pthread_join_hook_, void *pthread_detach_hook_, void *pthread_exit_hook_,
    void *mmap_hook_, void *munmap_hook_, void *mremap_hook_, void *mprotect_hook_,
    void *uswitch_callback_, void *uswitch_get_tp_) {
    pthread_create_hook = pthread_create_hook_;
    pthread_join_hook = pthread_join_hook_;
    pthread_detach_hook = pthread_detach_hook_;
    pthread_exit_hook = pthread_exit_hook_;
    mmap_hook = mmap_hook_;
    munmap_hook = munmap_hook_;
    mremap_hook = mremap_hook_;
    mprotect_hook = mprotect_hook_;
    uswitch_callback = uswitch_callback_;
    uswitch_get_tp = uswitch_get_tp_;
    uswitch_init_tp(tid);
    malloc_init(heap_base, heap_size);
    struct set_uswitch_functions_ret_t ret = {malloc, free};
    return ret;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void *), void *arg) {
    return pthread_create_hook(thread, NULL, start_routine, arg);
}

int pthread_join(pthread_t thread, void **retval) {
    return pthread_join_hook(thread, retval);
}

int pthread_detach(pthread_t thread) {
    return pthread_detach_hook(thread);
}

int pthread_cancel(pthread_t thread) {
    return -1;
}

void pthread_exit(void *retval) {

}

__asm__ (
    ".global uswitch_dip_trampoline\n"
    "uswitch_dip_trampoline:\n"
    "sub $8, %rsp\n"
    "callq %rdx\n"
    "add $8, %rsp\n"
    "retq\n"
);