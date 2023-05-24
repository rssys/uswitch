#include "ProcessSandbox_sharedmem.h"
#include "synch.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>  // strerror()
#include <stdlib.h>  // exit()
#include "myHelpers.h"
#include <signal.h>
#include <seccomp.h>
#include "seccomp-bpf.h"
#include <sys/prctl.h>
#include <malloc.h>  // malloc hooks
#include <dlfcn.h> //dlsym
#include "MyMalloc.h"

#include <stdio.h>  // fopen()
#include <string.h>  // strcmp()
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <chrono>
#include <string>

#if defined(__GLIBC__) && __GLIBC_MINOR__ >= 34

extern "C" void *__libc_malloc(size_t size);
extern "C" void *__libc_realloc(void *ptr, size_t size);
extern "C" void __libc_free(void *ptr);

void *(*volatile __malloc_hook)(size_t size, const void *caller);
void *(*volatile __realloc_hook)(void *ptr, size_t size, const void *caller);
void (*volatile __free_hook)(void *ptr, const void *caller);

extern "C" void *malloc(size_t size) {
  if (__malloc_hook) {
    return __malloc_hook(size, NULL);
  } else {
    return __libc_malloc(size);
  }
}

extern "C" void *realloc(void *ptr, size_t size) {
  if (__realloc_hook) {
    return __realloc_hook(ptr, size, NULL);
  } else {
    return __libc_realloc(ptr, size);
  }
}

extern "C" void free(void *ptr) {
  if (__free_hook) {
    __free_hook(ptr, NULL);
  } else {
    __libc_free(ptr);
  }
}

#endif

// WLOG this file only has one persandbox_shared_state_t object.
// If we have multiple sandboxes, they will be multiple processes, each with
//   their own independent execution of this file (and potentially different
//   values of sm)
static volatile persandbox_shared_state_t* sm = NULL;

static MyMalloc* myMalloc = NULL;

static void* libHandle = nullptr;

static void gotAbort(int signum, siginfo_t* siginfo, void* ucontext) {
  fprintf(stderr, "Sandbox thread got abort. Attach gdb with command --- sudo gdb -p %d\n", getpid());
  fflush(stderr);

  int i = 0;
  while (i == 0)
  {
      usleep(100000);  // sleep for 0.1 seconds
      fflush(stderr);
  }
  exit(1);
}

static void gotSegfault(int signum, siginfo_t* siginfo, void* ucontext) {
  fprintf(stderr, "Sandbox thread got segfault. Attach gdb with command --- sudo gdb -p %d\n", getpid());
  fflush(stderr);

  int i = 0;
  while (i == 0)
  {
      usleep(100000);  // sleep for 0.1 seconds
      fflush(stderr);
  }
  exit(1);
}

static void gotSIGSYS(int signum, siginfo_t* siginfo, void* ucontext) {
  fprintf(stderr, "Sandbox thread apparently committed seccomp violation. Attach gdb with commond --- sudo gdb -p %d\n", getpid());
  fflush(stderr);

  int i = 0;
  while (i == 0)
  {
      usleep(100000);  // sleep for 0.1 seconds
      fflush(stderr);
  }
  exit(1);
}

static int setSeccompBPFFilter(void)
{
  struct sock_filter filter[] = {
    /* Validate architecture. */
    VALIDATE_ARCHITECTURE,
    /* Grab the system call number. */
    EXAMINE_SYSCALL,
    /* List allowed syscalls. */
    ALLOW_SYSCALL(clock_gettime),
    ALLOW_SYSCALL(rt_sigreturn),
    // ALLOW_SYSCALL(sigreturn),
    ALLOW_SYSCALL(exit_group),
    ALLOW_SYSCALL(exit),
    ALLOW_SYSCALL(read),
    ALLOW_SYSCALL(write),

    ALLOW_SYSCALL(futex),
    ALLOW_SYSCALL(getpid),
    ALLOW_SYSCALL(gettid),
    ALLOW_SYSCALL(nanosleep),
    ALLOW_SYSCALL(tgkill),
    ALLOW_SYSCALL(sysinfo),
    // ALLOW_SYSCALL(sigprocmask),
    ALLOW_SYSCALL(rt_sigprocmask),
    ALLOW_SYSCALL(set_robust_list),
    ALLOW_SYSCALL(madvise),
    ALLOW_SYSCALL(clone), // resulting threads and procs inherit seccomp, so this is fine
#ifdef USE_LIBVPX
    ALLOW_SYSCALL(munmap),
#endif
    ALLOW_SYSCALL(readv),
    ALLOW_SYSCALL(writev),
    ALLOW_SYSCALL(clock_nanosleep),
    ALLOW_SYSCALL(rt_sigaction),
    ALLOW_SYSCALL(clone3),
#ifdef USE_LIBEVENT
    ALLOW_SYSCALL(epoll_pwait),
    ALLOW_SYSCALL(epoll_wait),
    ALLOW_SYSCALL(ioctl),
    ALLOW_SYSCALL(socket),
    ALLOW_SYSCALL(connect),
    ALLOW_SYSCALL(shutdown),
    ALLOW_SYSCALL(bind),
    ALLOW_SYSCALL(listen),
    ALLOW_SYSCALL(getsockname),
    ALLOW_SYSCALL(setsockopt),
    ALLOW_SYSCALL(sched_yield),
    ALLOW_SYSCALL(epoll_create1),
    ALLOW_SYSCALL(epoll_ctl),
    ALLOW_SYSCALL(accept4),
    ALLOW_SYSCALL(pipe2),
    ALLOW_SYSCALL(getuid),
    ALLOW_SYSCALL(geteuid),
    ALLOW_SYSCALL(getgid),
    ALLOW_SYSCALL(getegid),
    ALLOW_SYSCALL(sendto),
    ALLOW_SYSCALL(recvmsg),
    ALLOW_SYSCALL(close),
    ALLOW_SYSCALL(openat),
    ALLOW_SYSCALL(fstat),
    ALLOW_SYSCALL(lseek),
#endif

    // ALLOW_SYSCALL(mmap),
    IF_SYSCALL(mmap, 10)
      GET_ARG(4), /* get the fd value*/
      ALLOW_VALUE((unsigned int)-1), /* its fine as long as we don't access the file system*/

      GET_ARG(3), /* any mmap with a private flag is fine */
      ALLOW_VALUE(MAP_PRIVATE|MAP_DENYWRITE),
      ALLOW_VALUE(MAP_PRIVATE|MAP_FIXED|MAP_DENYWRITE),
      ALLOW_VALUE(MAP_PRIVATE),

    EXAMINE_SYSCALL,

    // ALLOW_SYSCALL(mprotect),
    IF_SYSCALL(mprotect, 9)
      GET_ARG(2), /* get the flags value*/
      // This should be fine in general, but let's prevent the exec flag just in case
      ALLOW_VALUE(PROT_NONE),
      ALLOW_VALUE(PROT_READ),
      ALLOW_VALUE(PROT_WRITE),
      ALLOW_VALUE(PROT_READ|PROT_WRITE),

    KILL_PROCESS,
  };

  struct sock_fprog prog = {
    .len = (unsigned short)(sizeof(filter)/sizeof(filter[0])),
    .filter = filter,
  };

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) ERROR("seccomp bpf failed : prctl(NO_NEW_PRIVS)\n")
  if (prctl(PR_SET_SECCOMP, SECCOMP_MODE_FILTER, &prog)) ERROR("seccomp bpf failed : prctl(SECCOMP)\n")

  return 0;
}

// // blog.yadutaf.fr/2014/05/29/introduction-to-seccomp-bpf-linux-syscall-filter
// // and just the man pages for these seccomp commands
// static void setSeccompFilter() {
//   // for security: disallow adding any new privileges from here on out
//   //prctl(PR_SET_NO_NEW_PRIVS, 1);
//   //printf("Sandbox: finished first prctl\n");
//   // and disallow ptrace, because otherwise when SECCOMP_RET_TRACE consults ptrace,
//   //   the attacker could provide malicious ptrace that allows things
//   //prctl(PR_SET_DUMPABLE, 0);
//   //printf("Sandbox: disabled ptrace\n");
//   // now initialize the filter
//   scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_TRAP);  // default to sending SIGSYS
//   if(ctx == NULL) ERROR("seccomp_init failed\n")

//   auto syscalls_to_allow = {
//     SCMP_SYS(rt_sigreturn),
//     SCMP_SYS(exit),
//     SCMP_SYS(read),
//     SCMP_SYS(write),
//     SCMP_SYS(futex),
//     SCMP_SYS(getpid),
//     SCMP_SYS(gettid),
//     SCMP_SYS(nanosleep),
//     SCMP_SYS(tgkill),
//     SCMP_SYS(sysinfo),
//     SCMP_SYS(sigprocmask),
//     SCMP_SYS(rt_sigprocmask),
//     SCMP_SYS(set_robust_list),
//     SCMP_SYS(madvise),
//     SCMP_SYS(mmap),
//     SCMP_SYS(mprotect),
//     SCMP_SYS(clone),
//   };

//   for(auto syscall : syscalls_to_allow) {
//     int status = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall, 0);
//     if(status) ERROR("seccomp_rule_add failed with %i\n", status)
//   }

//   int status = seccomp_load(ctx);
//   if(status) ERROR("seccomp_rule_add failed with %i\n", status)
//   seccomp_release(ctx);
// }

// see gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Hooks-for-Malloc.html
//   for more on malloc hooks
typedef void* (*malloc_hook_t)(size_t, const void*);
typedef void* (*realloc_hook_t)(void*, size_t, const void*);
typedef void (*free_hook_t)(void*, const void*);
static malloc_hook_t oldMallocHook;
static realloc_hook_t oldReallocHook;
static free_hook_t oldFreeHook;

static void* myMallocHook(size_t size, const void* caller) {
  return myMalloc->malloc(size);
}

static void* myReallocHook(void* oldmem, size_t bytes, const void* caller) {
  return myMalloc->realloc(oldmem, bytes);
}

static void myFreeHook(void* ptr, const void* caller) {
  if(myMalloc->isValid(ptr)) {
    myMalloc->free(ptr);
  } else {
    __free_hook = oldFreeHook;
    free(ptr);
    __free_hook = myFreeHook;
  }
}


static void initMallocHooks() {
  oldMallocHook = __malloc_hook;
  oldFreeHook = __free_hook;
  oldReallocHook = __realloc_hook;
  __malloc_hook = myMallocHook;
  __free_hook = myFreeHook;
  __realloc_hook = myReallocHook;
}

static void dispatchLibraryFunction(perthread_shared_state_t* threadState);

// This stub needs to have the same signature as the original callback,
//   so that we can pass it to the actual library
// With a template, it will automatically have the right signature
template<unsigned cbnum, typename argtype>
static void invokeVoid_1arg_Callback(argtype arg) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<argtype>(threadState->stackPtr, arg);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) return;
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

void* invokeDlSym(const char* functionName) {
  return dlsym(libHandle, functionName);
}

// Versions of the above for other 'families' of callback types
template<unsigned cbnum, typename arg1type, typename arg2type>
static void invokeVoid_2arg_Callback(arg1type arg1, arg2type arg2) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1);
  PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) return;
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

template<unsigned cbnum, typename arg1type, typename arg2type, typename arg3type, typename arg4type>
static void invokeVoid_4arg_Callback(arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1);
  PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2);
  PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3);
  PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) return;
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

template<unsigned cbnum, typename rettype, typename argtype>
static rettype invokeNonvoid_1arg_Callback(argtype arg) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<argtype>(threadState->stackPtr, arg);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) {
      return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr);
    }
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

template<unsigned cbnum, typename rettype, typename arg1type, typename arg2type>
static rettype invokeNonvoid_2arg_Callback(arg1type arg1, arg2type arg2) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1);
  PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) {
      return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr);
    }
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

template<unsigned cbnum, typename rettype, typename arg1type, typename arg2type, typename arg3type>
static rettype invokeNonvoid_3arg_Callback(arg1type arg1, arg2type arg2, arg3type arg3) {
  perthread_shared_state_t* threadState = sm->activeThreadState;
  threadState->callbacknum = cbnum;
  threadState->returning = false;
  PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1);
  PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2);
  PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3);
  while(true) {
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    if(threadState->returning) {
      return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr);
    }
    // else invoking another library function, perhaps recursively
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
  }
}

static void dispatchLibraryFunction(perthread_shared_state_t* threadState) {
  //printf("Sandbox: threadState %p dispatching on " STRINGIFY(LIB) " func %u\n", threadState, threadState->funcnum);
  switch(threadState->funcnum) {
    case FUNCNUM_INIT: {
      // By the time we get here, we've completed our init
      break;
    }
    case FUNCNUM_MALLOC: {
      uint32_t size = POP_VAL_FROM_STACK<uint32_t>(threadState->stackPtr);
      PUSH_VAL_TO_STACK<void*>(threadState->stackPtr, myMalloc->malloc(size));
      break;
    }
    case FUNCNUM_FREE: {
      void* ptr = POP_VAL_FROM_STACK<void*>(threadState->stackPtr);
      myMalloc->free(ptr);
      break;
    }

#define DISPATCH_0ARG_NONVOID(fname, rettype) \
  case FUNCNUM_OF(fname): { \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname()); \
    break; \
  }

#define DISPATCH_1ARG_NONVOID(fname, rettype, arg1type) \
  case FUNCNUM_OF(fname): { \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1)); \
    break; \
  }

#define DISPATCH_2ARG_NONVOID(fname, rettype, arg1type, arg2type) \
  case FUNCNUM_OF(fname): { \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2)); \
    break; \
  }

#define DISPATCH_3ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type) \
  case FUNCNUM_OF(fname): { \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3)); \
    break; \
  }

#define DISPATCH_4ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type) \
  case FUNCNUM_OF(fname): { \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4)); \
    break; \
  }

#define DISPATCH_5ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type) \
  case FUNCNUM_OF(fname): { \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5)); \
    break; \
  }

#define DISPATCH_6ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type) \
  case FUNCNUM_OF(fname): { \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6)); \
    break; \
  }

#define DISPATCH_7ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type) \
  case FUNCNUM_OF(fname): { \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7)); \
    break; \
  }

#define DISPATCH_8ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type) \
  case FUNCNUM_OF(fname): { \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8)); \
    break; \
  }

#define DISPATCH_9ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type) \
  case FUNCNUM_OF(fname): { \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9)); \
    break; \
  }

#define DISPATCH_10ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type) \
  case FUNCNUM_OF(fname): { \
    arg10type arg10 = POP_VAL_FROM_STACK<arg10type>(threadState->stackPtr); \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10)); \
    break; \
  }

#define DISPATCH_11ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type, arg11type) \
  case FUNCNUM_OF(fname): { \
    arg11type arg11 = POP_VAL_FROM_STACK<arg11type>(threadState->stackPtr); \
    arg10type arg10 = POP_VAL_FROM_STACK<arg10type>(threadState->stackPtr); \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    PUSH_VAL_TO_STACK<rettype>(threadState->stackPtr, fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11)); \
    break; \
  }

#define DISPATCH_0ARG_VOID(fname, rettype) \
  case FUNCNUM_OF(fname): { \
    fname(); \
    break; \
  }

#define DISPATCH_1ARG_VOID(fname, rettype, arg1type) \
  case FUNCNUM_OF(fname): { \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1); \
    break; \
  }

#define DISPATCH_2ARG_VOID(fname, rettype, arg1type, arg2type) \
  case FUNCNUM_OF(fname): { \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2); \
    break; \
  }

#define DISPATCH_3ARG_VOID(fname, rettype, arg1type, arg2type, arg3type) \
  case FUNCNUM_OF(fname): { \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3); \
    break; \
  }

#define DISPATCH_4ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type) \
  case FUNCNUM_OF(fname): { \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4); \
    break; \
  }

#define DISPATCH_5ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type) \
  case FUNCNUM_OF(fname): { \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5); \
    break; \
  }

#define DISPATCH_6ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type) \
  case FUNCNUM_OF(fname): { \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6); \
    break; \
  }

#define DISPATCH_7ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type) \
  case FUNCNUM_OF(fname): { \
    arg6type arg7 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
    break; \
  }

#define DISPATCH_8ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type) \
  case FUNCNUM_OF(fname): { \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
    break; \
  }

#define DISPATCH_9ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type) \
  case FUNCNUM_OF(fname): { \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); \
    break; \
  }

#define DISPATCH_10ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type) \
  case FUNCNUM_OF(fname): { \
    arg10type arg10 = POP_VAL_FROM_STACK<arg10type>(threadState->stackPtr); \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
    break; \
  }

#define DISPATCH_11ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type, arg11type) \
  case FUNCNUM_OF(fname): { \
    arg11type arg11 = POP_VAL_FROM_STACK<arg11type>(threadState->stackPtr); \
    arg10type arg10 = POP_VAL_FROM_STACK<arg10type>(threadState->stackPtr); \
    arg9type arg9 = POP_VAL_FROM_STACK<arg9type>(threadState->stackPtr); \
    arg8type arg8 = POP_VAL_FROM_STACK<arg8type>(threadState->stackPtr); \
    arg7type arg7 = POP_VAL_FROM_STACK<arg7type>(threadState->stackPtr); \
    arg6type arg6 = POP_VAL_FROM_STACK<arg6type>(threadState->stackPtr); \
    arg5type arg5 = POP_VAL_FROM_STACK<arg5type>(threadState->stackPtr); \
    arg4type arg4 = POP_VAL_FROM_STACK<arg4type>(threadState->stackPtr); \
    arg3type arg3 = POP_VAL_FROM_STACK<arg3type>(threadState->stackPtr); \
    arg2type arg2 = POP_VAL_FROM_STACK<arg2type>(threadState->stackPtr); \
    arg1type arg1 = POP_VAL_FROM_STACK<arg1type>(threadState->stackPtr); \
    fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
    break; \
  }

      DISPATCH_1ARG_NONVOID(invokeDlSym, void*, const char*)
      FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(DISPATCH_0ARG_VOID)
      FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(DISPATCH_1ARG_VOID)
      FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(DISPATCH_2ARG_VOID)
      FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(DISPATCH_3ARG_VOID)
      FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(DISPATCH_4ARG_VOID)
      FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(DISPATCH_5ARG_VOID)
      FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(DISPATCH_6ARG_VOID)
      FOR_EACH_7ARG_VOID_LIBRARY_FUNCTION(DISPATCH_7ARG_VOID)
      FOR_EACH_8ARG_VOID_LIBRARY_FUNCTION(DISPATCH_8ARG_VOID)
      FOR_EACH_9ARG_VOID_LIBRARY_FUNCTION(DISPATCH_9ARG_VOID)
      FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(DISPATCH_10ARG_VOID)
      FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(DISPATCH_11ARG_VOID)
      FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_0ARG_NONVOID)
      FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_1ARG_NONVOID)
      FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_2ARG_NONVOID)
      FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_3ARG_NONVOID)
      FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_4ARG_NONVOID)
      FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_5ARG_NONVOID)
      FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_6ARG_NONVOID)
      FOR_EACH_7ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_7ARG_NONVOID)
      FOR_EACH_8ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_8ARG_NONVOID)
      FOR_EACH_9ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_9ARG_NONVOID)
      FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_10ARG_NONVOID)
      FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(DISPATCH_11ARG_NONVOID)

#undef DISPATCH_0ARG_VOID
#undef DISPATCH_1ARG_VOID
#undef DISPATCH_2ARG_VOID
#undef DISPATCH_3ARG_VOID
#undef DISPATCH_4ARG_VOID
#undef DISPATCH_5ARG_VOID
#undef DISPATCH_6ARG_VOID
#undef DISPATCH_7ARG_VOID
#undef DISPATCH_8ARG_VOID
#undef DISPATCH_9ARG_VOID
#undef DISPATCH_10ARG_VOID
#undef DISPATCH_11ARG_VOID
#undef DISPATCH_0ARG_NONVOID
#undef DISPATCH_1ARG_NONVOID
#undef DISPATCH_2ARG_NONVOID
#undef DISPATCH_3ARG_NONVOID
#undef DISPATCH_4ARG_NONVOID
#undef DISPATCH_5ARG_NONVOID
#undef DISPATCH_6ARG_NONVOID
#undef DISPATCH_7ARG_NONVOID
#undef DISPATCH_8ARG_NONVOID
#undef DISPATCH_9ARG_NONVOID
#undef DISPATCH_10ARG_NONVOID
#undef DISPATCH_11ARG_NONVOID

      default: {
        printf("Unknown function number %u\n", threadState->funcnum);
        break;
      }
  }
}

inline int read_all(int fd, void* buf, unsigned int bytes) {
  int n = 0;
  while (n < bytes) {
    int len = read(fd, (char*)buf + n, bytes - n);
    if (len == -1) {
      return len;
    }
    n += len;
  }
  return n;
}

inline int write_all(int fd, const void* buf, unsigned int bytes) {
  int n = 0;
  while (n < bytes) {
    int len = write(fd, (char*)buf + n, bytes - n);
    if (len == -1) {
      return len;
    }
    n += len;
  }
  return n;
}

int main(int argc, char* argv[]) {
  unsigned sbcore;
  void* shbase;
  std::string mmap_path;
  if (argc == 4 && strcmp(argv[1], "--forkserver") == 0) {
    int read_fd = atoi(argv[2]);
    int write_fd = atoi(argv[3]);
    while (true) {
      uint32_t msg_len;
      uint32_t command;
      //printf("Fork server receiving message from firefox.\n");
      fflush(stdout);
      if (read_all(read_fd, &msg_len, 4) != 4) {
        return 0;
      }
      //printf("Fork server receiving message len %u.\n", msg_len);
      fflush(stdout);
      char* buffer = new char[msg_len];
      if (read_all(read_fd, buffer, msg_len) != msg_len) {
        return 0;
      }
      memcpy(&command, buffer, 4);
      //printf("Fork server received message from firefox, command: %d.\n", command);
      fflush(stdout);
      if (command == 0) {
        // new process
        //printf("Fork server received command: new process\n");
        fflush(stdout);
        memcpy(&sbcore, buffer + 4, sizeof(unsigned));
        memcpy(&shbase, buffer + 4 + sizeof(unsigned), sizeof(void*));
        mmap_path = string(buffer + 4 + sizeof(unsigned) + sizeof(void*), msg_len - 4 - sizeof(unsigned) - sizeof(void*));
        //printf("Fork server received sbcore %d, shbase %p, mmap_path %s\n", sbcore, shbase, mmap_path.c_str());
        fflush(stdout);
        delete[] buffer;
        pid_t pid = fork();
        if (pid == -1) {
          return 0;
        }
        if (pid == 0) {
          //printf("Sandbox: initializing with fork server. PID=%d\n", getpid());
          //printf("PID: %d\tCurrent Time %ld\n", getpid(), std::chrono::duration_cast<std::chrono::nanoseconds>
          //  (std::chrono::high_resolution_clock::now().time_since_epoch()).count());
          fflush(stdout);
          close(read_fd);
          close(write_fd);
          break;
        }
        //printf("Fork server writing message to firefox.\n");
        fflush(stdout);
        if (write_all(write_fd, &pid, sizeof(pid_t)) != sizeof(pid_t)) {
          return 0;
        }
        //printf("Fork server wrote pid to firefox. PID: %d\n", pid);
        fflush(stdout);
      } else if (command == 1) {
        // wait
        //printf("Fork server received command: wait\n");
        fflush(stdout);
        pid_t pid;
        memcpy(&pid, buffer + 4, sizeof(pid_t));
        int status;
        //printf("Fork server waiting PID %d\n", pid);
        fflush(stdout);
        waitpid(pid, &status, 0);
        write_all(write_fd, &status, sizeof(int));
        //printf("Fork server wrote status %d to firefox.\n", status);
        fflush(stdout);
        delete[] buffer;
      }
    }
  } else {
    if(argc < 4) ERROR("Too few arguments. Expected:\n  (1) core number to pin sandbox process to\n  (2) base addr of shared memory block\n  (3) filename for shm_open\n")
    if(argc > 4) ERROR("Too many arguments\n")
    mmap_path = argv[3];
    //printf("Sandbox: initializing. PID=%d\n", getpid());
    //printf("PID: %d\tCurrent Time %ld\n", getpid(), std::chrono::duration_cast<std::chrono::nanoseconds>
    //            (std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    fflush(stdout);
    if(!sscanf(argv[1], "%u", &sbcore)) ERROR("Expected an unsigned number for core\n")
    if(!sscanf(argv[2], "%p", &shbase)) ERROR("Error reading the shared memory base address\n")
  }
  /* instrument code
  while(1) {
    cmd = recv()
    if (cmd == newProcesSandbox){
      pid = fork()
      if (pid == 0) {
        break;
      }
      send(pid)
    }
  }

  */
  
  //cpu_set_t sbcpuset;
  //CPU_ZERO(&sbcpuset);
  //CPU_SET(sbcore, &sbcpuset);
  //if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &sbcpuset) != 0) ERROR("Error setting affinity to %u\n", sbcore)

  int fd = shm_open(mmap_path.c_str(), O_RDWR, 0);
  if(fd == -1) ERROR("shm_open(%s) failed with %u\n", mmap_path.c_str(), errno)
  void* sharedmem = mmap(shbase, SHAREDMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, fd, 0);
  if(sharedmem == MAP_FAILED) ERROR("mmap failed with %u\n", errno)
  if(sharedmem != shbase) ERROR("mmap didn't return the address we wanted: wanted %p, got %p\n", shbase, sharedmem)
  //printf("Sandbox: using shared memory at %p\n", sharedmem);
  sm = (persandbox_shared_state_t*) sharedmem;

  libHandle = dlopen(nullptr, RTLD_LAZY);

  // This is allocated before malloc hooks are in place, and that's fine
  // We don't need to worry about synchronizing operations with the other MyMalloc
  //   instance (in main process) (see MyMalloc.h) because the synch scheme (see
  //   synch.h) guarantees that we and the main process can't call malloc() or free()
  //   simultaneously
  myMalloc = new MyMalloc((void*)(sm->extraSpace), SHAREDMEM_SIZE-sizeof(persandbox_shared_state_t), false);

  {
    struct sigaction segvsa;
    memset(&segvsa, 0, sizeof(segvsa));
    segvsa.sa_sigaction = gotAbort;
    segvsa.sa_flags = SA_SIGINFO;
    sigaction(SIGABRT, &segvsa, NULL);
  }

  {
    struct sigaction segvsa;
    memset(&segvsa, 0, sizeof(segvsa));
    segvsa.sa_sigaction = gotSegfault;
    segvsa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &segvsa, NULL);
  }

  {
    struct sigaction syssa;
    memset(&syssa, 0, sizeof(syssa));
    syssa.sa_sigaction = gotSIGSYS;
    syssa.sa_flags = SA_SIGINFO;
    sigaction(SIGSYS, &syssa, NULL);
  }

#define FOR_0_TO_7(macro) \
  macro(0); macro(1); macro(2); macro(3); macro(4); macro(5); macro(6); macro(7);

  // Matching callback types to wrappers is done manually per-library, for now
#ifdef USE_DUMMY_LIB
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeNonvoid_1arg_Callback<i+0*CALLBACKS_PER_TYPE, int, int>;
#define TYPE_1(i) sm->cbWrappers[1][i] = (void*)invokeVoid_2arg_Callback<i+1*CALLBACKS_PER_TYPE, char, char>;
#define TYPE_2(i) sm->cbWrappers[2][i] = (void*)invokeNonvoid_1arg_Callback<i+2*CALLBACKS_PER_TYPE, void*, int*>;
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBJPEG)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeVoid_1arg_Callback<i+0*CALLBACKS_PER_TYPE, j_common_ptr>;
#define TYPE_1(i) sm->cbWrappers[1][i] = (void*)invokeVoid_1arg_Callback<i+1*CALLBACKS_PER_TYPE, j_decompress_ptr>;
#define TYPE_2(i) sm->cbWrappers[2][i] = (void*)invokeVoid_2arg_Callback<i+2*CALLBACKS_PER_TYPE, j_decompress_ptr, long>;
#define TYPE_3(i) sm->cbWrappers[3][i] = (void*)invokeNonvoid_1arg_Callback<i+3*CALLBACKS_PER_TYPE, boolean, j_decompress_ptr>;
#define TYPE_4(i) sm->cbWrappers[4][i] = (void*)invokeVoid_1arg_Callback<i+4*CALLBACKS_PER_TYPE, j_decompress_ptr>;
#define TYPE_5(i) sm->cbWrappers[5][i] = (void*)invokeNonvoid_2arg_Callback<i+5*CALLBACKS_PER_TYPE, boolean, j_decompress_ptr, int>;
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBPNG)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeVoid_2arg_Callback<i+0*CALLBACKS_PER_TYPE, png_structp, png_const_charp>;
#define TYPE_1(i) sm->cbWrappers[1][i] = (void*)invokeVoid_2arg_Callback<i+1*CALLBACKS_PER_TYPE, png_structp, png_infop>;
#define TYPE_2(i) sm->cbWrappers[2][i] = (void*)invokeVoid_4arg_Callback<i+2*CALLBACKS_PER_TYPE, png_structp, png_bytep, png_uint_32, int>;
#define TYPE_3(i) sm->cbWrappers[3][i] = (void*)invokeVoid_2arg_Callback<i+3*CALLBACKS_PER_TYPE, png_structp, png_infop>;
#define TYPE_4(i) sm->cbWrappers[4][i] = (void*)invokeVoid_2arg_Callback<i+4*CALLBACKS_PER_TYPE, png_structp, png_uint_32>;
#define TYPE_5(i) sm->cbWrappers[5][i] = (void*)invokeVoid_2arg_Callback<i+5*CALLBACKS_PER_TYPE, jmp_buf, int>;
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_ZLIB)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeNonvoid_3arg_Callback<i+0*CALLBACKS_PER_TYPE, voidpf, voidpf, uInt, uInt>;
#define TYPE_1(i) sm->cbWrappers[1][i] = (void*)invokeVoid_2arg_Callback<i+1*CALLBACKS_PER_TYPE, voidpf, voidpf>;
#define TYPE_2(i) sm->cbWrappers[2][i] = (void*)invokeNonvoid_2arg_Callback<i+2*CALLBACKS_PER_TYPE, unsigned, VOIDFARSTAR, z_const unsigned char FAR * FAR *>;
#define TYPE_3(i) sm->cbWrappers[3][i] = (void*)invokeNonvoid_3arg_Callback<i+3*CALLBACKS_PER_TYPE, int, VOIDFARSTAR, UCHARFSTAR, unsigned>;
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_PNGDEC)
#define TYPE_0(i)
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBTHEORA)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeVoid_4arg_Callback<i+0*CALLBACKS_PER_TYPE, VOIDSTAR, th_ycbcr_buffer, int, int>;
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBVPX)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeVoid_4arg_Callback<i+0*CALLBACKS_PER_TYPE, VOIDSTAR, UCHARSTAR, UCHARSTAR, int>;
#define TYPE_1(i) sm->cbWrappers[1][i] = (void*)invokeVoid_2arg_Callback<i+1*CALLBACKS_PER_TYPE, void*, const vpx_image_t*>;
#define TYPE_2(i) sm->cbWrappers[2][i] = (void*)invokeVoid_4arg_Callback<i+2*CALLBACKS_PER_TYPE, void*, const vpx_image_t*, const vpx_image_rect_t*, const vpx_image_rect_t*>;
#define TYPE_3(i) sm->cbWrappers[3][i] = (void*)invokeNonvoid_3arg_Callback<i+3*CALLBACKS_PER_TYPE, int, void*, size_t, vpx_codec_frame_buffer_t*>;
#define TYPE_4(i) sm->cbWrappers[4][i] = (void*)invokeNonvoid_2arg_Callback<i+4*CALLBACKS_PER_TYPE, int, void*, vpx_codec_frame_buffer_t*>;
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBVORBIS)
#define TYPE_0(i)
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_TEST)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeNonvoid_3arg_Callback<i+0*CALLBACKS_PER_TYPE, int, unsigned, char*, unsigned[1]>;
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_RLBOXTEST)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeNonvoid_3arg_Callback<i+0*CALLBACKS_PER_TYPE, int, unsigned, char*, unsigned[1]>;
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#elif defined(USE_LIBEVENT)
#define TYPE_0(i) sm->cbWrappers[0][i] = (void*)invokeVoid_2arg_Callback<i+0*CALLBACKS_PER_TYPE, struct evhttp_request *, void *>;
#define TYPE_1(i)
#define TYPE_2(i)
#define TYPE_3(i)
#define TYPE_4(i)
#define TYPE_5(i)
#define TYPE_6(i)
#define TYPE_7(i)
#else
#error Please define one of USE_DUMMY_LIB, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, USE_LIBVORBIS, USE_TEST, or USE_RLBOXTEST while compiling.
#endif

  FOR_0_TO_7(TYPE_0)
  FOR_0_TO_7(TYPE_1)
  FOR_0_TO_7(TYPE_2)
  FOR_0_TO_7(TYPE_3)
  FOR_0_TO_7(TYPE_4)
  FOR_0_TO_7(TYPE_5)
  FOR_0_TO_7(TYPE_6)
  FOR_0_TO_7(TYPE_7)

  /*
  fprintf(stderr, "Sandbox: Attach gdb with command --- sudo gdb -p %d\n", getpid());
  fflush(stderr);
  volatile int gdb = 0;
  while (gdb == 0) {  // to continue, use gdb to set 'gdb' to something nonzero
    usleep(100000);  // sleep for 0.1 seconds
    fflush(stderr);
  }
  */

  if (getenv("RLBOX_PROCESS_NO_SECCOMP")) {
    //printf("Sandbox: Not setting seccomp filter\n");
  } else {
    //printf("Sandbox: setting seccomp filter\n");
    setSeccompBPFFilter();
    //printf("Sandbox: seccomp filter in place\n");
  }

  initMallocHooks();

  Invoker* invoker = (Invoker*) &sm->invoker;
  invoker->initProcess(Invoker::OTHERSIDE);
  perthread_shared_state_t* threadState = sm->activeThreadState;
  //printf("Sandbox: completed init\n");

  while(true) {
    dispatchLibraryFunction(threadState);
    threadState->returning = true;
    Invoker* invoker = (Invoker*) &sm->invoker;
    invoker->invoke(Invoker::OTHERSIDE);
    threadState = sm->activeThreadState;
    // now that invoke has returned, we have been invoked again
  }

  invoker->deInitProcess(Invoker::OTHERSIDE);
  return 0;
}
