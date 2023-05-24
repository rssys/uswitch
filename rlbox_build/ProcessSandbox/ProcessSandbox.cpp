#include "ProcessSandbox.h"
#include "ProcessSandbox_sharedmem.h"
#include "timing.h"
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <limits.h>  // INT_MAX
#include <new>  // placement new
#include <signal.h>  // SIGCHLD
#include <errno.h>
#include <string.h>  // strerror()
#include <fcntl.h>
#include "myHelpers.h"  // ERROR()
#include <vector>
#include <map>
#include <algorithm>
#include <string>
#include <mutex>
#include <vector>
#include <pthread.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <chrono>

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

std::map<std::string, pair<int, int>> PROCESS_SANDBOX_CLASSNAME::fork_servers;
std::mutex PROCESS_SANDBOX_CLASSNAME::fork_server_mutex;
size_t PROCESS_SANDBOX_CLASSNAME::sandboxCount;

inline std::pair<int, int> new_fork_server(const char* othersidepath) {
  int pipe_fd_read[2], pipe_fd_write[2];
  if (pipe(pipe_fd_read) == -1 || pipe(pipe_fd_write) == -1) {
    return std::make_pair(-1, -1);
  }
  char otherside_fd_read[16];
  char otherside_fd_write[16];
  snprintf(otherside_fd_read, 16, "%d", pipe_fd_write[0]);
  snprintf(otherside_fd_write, 16, "%d", pipe_fd_read[1]);
  pid_t pid = fork();
  if (pid == -1) {
    return std::make_pair(-1, -1);
  }
  if (pid == 0) {
    close(pipe_fd_read[0]);
    close(pipe_fd_write[1]);
    //printf("New forkserver\n");
    execl(othersidepath, othersidepath, "--forkserver", otherside_fd_read, otherside_fd_write, (char*)NULL);
  }
  close(pipe_fd_read[1]);
  close(pipe_fd_write[0]);
  int fd_read = pipe_fd_read[0];
  int fd_write = pipe_fd_write[1];
  return std::make_pair(fd_read, fd_write);
}

pid_t PROCESS_SANDBOX_CLASSNAME::createSandboxProcessForkServer(const char* othersidepath, unsigned sbcore) {
  auto it = fork_servers.find(othersidepath);
  int fd_read, fd_write;

  //printf("Making message for forkserver. sbcore %d, sandboxSharedState %p, shmempath %s.\n", sbcore, sandboxSharedState, shmempath);
  size_t shmempath_len = strlen(shmempath);
  uint32_t msg_len = 4 + sizeof(sbcore) + sizeof(sandboxSharedState) + shmempath_len;
  uint32_t command = 0; // new process
  vector<char> msg_buf(msg_len + 4);
  // make msg
  memcpy(msg_buf.data(), &msg_len, 4);
  memcpy(msg_buf.data() + 4, &command, 4);
  memcpy(msg_buf.data() + 8, &sbcore, sizeof(sbcore));
  memcpy(msg_buf.data() + 8 + sizeof(sbcore), &sandboxSharedState, sizeof(sandboxSharedState));
  memcpy(msg_buf.data() + 8 + sizeof(sbcore) + sizeof(sandboxSharedState), shmempath, shmempath_len);
  pid_t pid;
  
  auto try_to_create_process = [&] {
    std::lock_guard<std::mutex> lock(fork_server_mutex);
    //printf("Sending message to forkserver\n");
    if (write_all(fd_write, msg_buf.data(), msg_len + 4) != msg_len + 4) {
      return false;
    }
    if (read_all(fd_read, &pid, sizeof(pid_t)) != sizeof(pid_t)) {
      return false;
    }
    return true;
  };
  
  if (it == fork_servers.end()) {
    auto p = new_fork_server(othersidepath);
    fd_read = p.first;
    fd_write = p.second;
    if (fd_read == -1 || fd_write == -1) {
      return -1;
    }
    if (!try_to_create_process()) {
      return -1;
    }
    fork_servers[othersidepath] = p;
  } else {
    std::tie(fd_read, fd_write) = it->second;
    if (!try_to_create_process()) {
      close(fd_read);
      close(fd_write);
      auto p = new_fork_server(othersidepath);
      fd_read = p.first;
      fd_write = p.second;
      if (fd_read == -1 || fd_write == -1) {
        return -1;
      }
      if (!try_to_create_process()) {
        return -1;
      }
      fork_servers[othersidepath] = p;
    }
  }

  //printf("Received pid %d from forkserver.\n", pid);
  fork_server_writefd = fd_write;
  fork_server_readfd = fd_read;
  return pid;
}

pid_t PROCESS_SANDBOX_CLASSNAME::createSandboxProcess(const char* othersidepath, unsigned sbcore) {
  bool use_vfork = getenv("MOZ_RLBOX_SANDBOX_USE_VFORK") != NULL;
  use_fork_server = getenv("MOZ_RLBOX_SANDBOX_USE_FORKSERVER") != NULL;
  pid_t pid;
  ++sandboxCount;
  //printf("Sandbox %s count: %lu\n", STRINGIFY(PROCESS_SANDBOX_CLASSNAME), sandboxCount);
  if (use_fork_server) {
    return createSandboxProcessForkServer(othersidepath, sbcore);
  }
  if (use_vfork) {
    //printf("Using vfork()\n");
  }
  char sbcore_as_str[8];
  snprintf(sbcore_as_str, 8, "%u", sbcore);
  char sharedState_as_str[64];
  snprintf(sharedState_as_str, 64, "%p", sandboxSharedState);
  if (use_vfork) {
    pid = vfork();
  } else {
    pid = fork();
  }
  if (pid == -1) {
    ERROR("fork() failed: %s\n", strerror(errno));
  }
  if (pid == 0) {
    if (!execl(othersidepath, othersidepath, sbcore_as_str, sharedState_as_str, shmempath, (char*)NULL)) {
      _exit(1);
    }
  }
  return pid;
}

/*
pid_t PROCESS_SANDBOX_CLASSNAME::createSandboxProcess(const char* othersidepath, unsigned sbcore) {
  std::chrono::steady_clock::time_point start;
  start = startTimer();
  pid_t pid;
  char* pPath;
  pPath = getenv ("MOZ_RLBOX_SANDBOX_USE_VFORK");
  if (pPath != NULL) {
    pid = vfork();
    printf("Using vfork()\n");
  } else if (getenv ("MOZ_RLBOX_SANDBOX_USE_ODF") != NULL) {
    pid = syscall(439);
    printf("Using ODF\n");
  } else {
    pid = fork();
    printf("Using fork()\n");
  }
  if(pid == -1) ERROR("fork() failed: %s\n", strerror(errno));
  if(pid == 0) {
    printf("Fork Time in nanoseconds in child %ld\n", diffInNanoseconds(start, stopTimer()));
    fflush(stdout);
    // sandbox process
    char sbcore_as_str[8];
    snprintf(sbcore_as_str, 8, "%u", sbcore);
    char sharedState_as_str[64];
    snprintf(sharedState_as_str, 64, "%p", sandboxSharedState);
    printf("PID: %d\tCurrent Time %ld\n", getpid(), std::chrono::duration_cast<std::chrono::nanoseconds>
              (std::chrono::high_resolution_clock::now().time_since_epoch()).count());
    printf("OthersidePath: %s\n", othersidepath);
    if(!execl(othersidepath, othersidepath, sbcore_as_str, sharedState_as_str, shmempath, (char*)NULL)) {
      printf("execl failed with code %d (%s)\n", errno, strerror(errno));
      exit(1);
    }
    exit(0);
  }

  // pid_t pid = fork();
  // if(pid == -1) ERROR("fork() failed: %s\n", strerror(errno));
  // if(pid == 0) {
  //   // sandbox process
  //   char sbcore_as_str[8];
  //   snprintf(sbcore_as_str, 8, "%u", sbcore);
  //   char sharedState_as_str[64];
  //   snprintf(sharedState_as_str, 64, "%p", sandboxSharedState);
  //   if(!execl(othersidepath, othersidepath, sbcore_as_str, sharedState_as_str, shmempath, (char*)NULL)) {
  //     printf("execl failed with code %d (%s)\n", errno, strerror(errno));
  //     exit(1);
  //   }
  //   exit(0);
  // }
  // main process
  return pid;
}
*/

// returns the file descriptor
static int createSharedMemOfSize(const char* path, unsigned long long bytes) {
  int fd = shm_open(path, O_CREAT|O_RDWR|O_EXCL|O_TRUNC, S_IRUSR|S_IWUSR);
  if(fd < 0) return fd;  // failure
  off_t offt = lseek(fd, bytes-1, SEEK_SET);
  if(offt == (off_t)-1) {
    printf("lseek returned error %d (%s)\n", errno, strerror(errno));
    exit(1);
  }
  ssize_t ret = write(fd, "", 1);  // must write to end of file to keep file from being truncated
  if(ret != 1) {
    printf("write returned error %d (%s)\n", errno, strerror(errno));
    exit(1);
  }
  return fd;
}

// initializes the "shmempath" field
void* PROCESS_SANDBOX_CLASSNAME::createSharedMem() {
  for(unsigned i = 0; i<INT_MAX; i++) {
    char* path = new char[64];
    if(path == NULL) ERROR("createSharedMem: malloc for path failed with errno %u (%s)\n", errno, strerror(errno))
    snprintf(path, 64, "/sharedmem%i", i);
    int fd = createSharedMemOfSize(path, SHAREDMEM_SIZE);
    if(fd < 0) { delete path; continue; }  // that path already in use (by another sandbox)
    shmempath = path;
    void* shmemaddr = (void*) (0x200000000000+SHAREDMEM_SIZE*i);
    return mmap(shmemaddr, SHAREDMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  }
  ERROR("createSharedMem: ran out of candidate paths")
}

PROCESS_SANDBOX_CLASSNAME::PROCESS_SANDBOX_CLASSNAME(const char* othersidepath, unsigned maincore, unsigned sbcore) {
  //printf("Main process: started " STRINGIFY(PROCESS_SANDBOX_CLASSNAME) " constructor\n");
  {
    /*
    printf("  Attach gdb with command --- sudo gdb -p %d\n", getpid());
    fflush(stdout);
    volatile int gdb = 0;
    while (gdb == 0) {  // to continue, use gdb to set 'gdb' to something nonzero
      usleep(100000);  // sleep for 0.1 seconds
      fflush(stderr);
    }
    */
  }
  threadStates = new std::map<pid_t, perthread_shared_state_t>();
  void* sharedmem = createSharedMem();
  sandboxSharedState = new (sharedmem) persandbox_shared_state_t;  // "placement new" runs constructor for persandbox_shared_state_t
  //printf("Main process: initialized per-sandbox shared memory at %p\n", sandboxSharedState);
  sandboxSharedState->invoker.initProcess(Invoker::MAIN_PROCESS);  // init the main process (sandbox will init in its own main())
  for(unsigned i = 0; i < CALLBACK_TYPES; i++)
    for(unsigned j = 0; j < CALLBACKS_PER_TYPE; j++)
      callbacks[i][j].first = NULL;  // mark unregistered
  initSandboxMutex();
  sandboxPID = createSandboxProcess(othersidepath, sbcore);

  cpu_set_t maincpuset;
  if (maincore != 9999 /* special marker for don't set */) {
    CPU_ZERO(&maincpuset);
    CPU_SET(maincore, &maincpuset);
    if(pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &maincpuset) != 0) ERROR("Error setting affinity to %u\n", maincore)
  }

  perthread_shared_state_t* firstThreadState = getActiveThreadState();
  firstThreadState->funcnum = FUNCNUM_INIT;
  invokeFunctionCall(firstThreadState);  // ensure sandbox is initialized before returning
}

void PROCESS_SANDBOX_CLASSNAME::destroySandbox() {
  sandboxSharedState->invoker.deInitProcess(Invoker::MAIN_PROCESS);
  if(kill(sandboxPID, SIGKILL)) {
    printf("Failed to kill %u, error %d (%s)\n", sandboxPID, errno, strerror(errno));
  }
  int retCode;
  if (use_fork_server) {
    char buf[8 + sizeof(pid_t)];
    uint32_t len = 4 + sizeof(pid_t);
    uint32_t command = 1;
    memcpy(buf, &len, 4);
    memcpy(buf + 4, &command, 4);
    memcpy(buf + 8, &sandboxPID, sizeof(pid_t));
    //printf("Wait PID %d\n", sandboxPID);
    {
      std::lock_guard<std::mutex> lock(fork_server_mutex);
      write_all(fork_server_writefd, buf, 8 + sizeof(pid_t));
      read_all(fork_server_readfd, &retCode, sizeof(int));
    }
    //printf("PID %d exited\n", sandboxPID);
  } else {
    waitpid(sandboxPID, &retCode, 0);
  }
  munmap(sandboxSharedState, SHAREDMEM_SIZE);
  shm_unlink(shmempath);
  remove(shmempath);
  delete shmempath;
  delete (std::map<pid_t, perthread_shared_state_t*>*) threadStates;
  //printf(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) ": destroyed the sandbox at %p\n", sandboxSharedState);
}

void* PROCESS_SANDBOX_CLASSNAME::getSandboxMemoryBase() const {
  return (void*) sandboxSharedState;
}

void PROCESS_SANDBOX_CLASSNAME::initSandboxMutex() {
  // for more comments here see synch.cpp
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);  // we need recursive mutex for the callback case
                                                              // note: must unlock the same number of times we locked it
  pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_STALLED);
  pthread_mutex_init(&sandboxMutex, &attr);
  pthread_mutexattr_destroy(&attr);
}

void* PROCESS_SANDBOX_CLASSNAME::mallocInSandbox(uint32_t size) {
  pthread_mutex_lock(&sandboxMutex);
  perthread_shared_state_t* threadState = getActiveThreadState();
  threadState->funcnum = FUNCNUM_MALLOC;
  PUSH_VAL_TO_STACK<uint32_t>(threadState->stackPtr, size);
  invokeFunctionCall(threadState);
  pthread_mutex_unlock(&sandboxMutex);
  return POP_VAL_FROM_STACK<void*>(threadState->stackPtr);
}

void* PROCESS_SANDBOX_CLASSNAME::mallocInSandboxWithThreadState(uint32_t size, perthread_shared_state_t* threadState) {
  pthread_mutex_lock(&sandboxMutex);
  sandboxSharedState->activeThreadState = threadState;
  threadState->funcnum = FUNCNUM_MALLOC;
  PUSH_VAL_TO_STACK<uint32_t>(threadState->stackPtr, size);
  invokeFunctionCall(threadState);
  pthread_mutex_unlock(&sandboxMutex);
  return POP_VAL_FROM_STACK<void*>(threadState->stackPtr);
}

void PROCESS_SANDBOX_CLASSNAME::freeInSandbox(void* ptr) {
  pthread_mutex_lock(&sandboxMutex);
  perthread_shared_state_t* threadState = getActiveThreadState();
  threadState->funcnum = FUNCNUM_FREE;
  PUSH_VAL_TO_STACK<void*>(threadState->stackPtr, ptr);
  invokeFunctionCall(threadState);
  pthread_mutex_unlock(&sandboxMutex);
}

void PROCESS_SANDBOX_CLASSNAME::makeActiveSandbox() {
  // no one else is allowed to invoke or make active/inactive while this happens
  pthread_mutex_lock(&sandboxMutex);
  sandboxSharedState->invoker.switchToAtomicSpinlock();
  pthread_mutex_unlock(&sandboxMutex);
}

void PROCESS_SANDBOX_CLASSNAME::makeInactiveSandbox() {
  // no one else is allowed to invoke or make active/inactive while this happens
  pthread_mutex_lock(&sandboxMutex);
  sandboxSharedState->invoker.switchToMutexCond();
  pthread_mutex_unlock(&sandboxMutex);
}

#define WRAPPED_FUNCTION_0ARG_VOID(fname, rettype) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) () { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr) { \
    thisptr->inv_##fname(); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr) { \
    thisptr->inv_##fname(); \
  }

#define WRAPPED_FUNCTION_1ARG_VOID(fname, rettype, arg1type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1) { \
    thisptr->inv_##fname(arg1); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1) { \
    thisptr->inv_##fname(arg1); \
  }

#define WRAPPED_FUNCTION_2ARG_VOID(fname, rettype, arg1type, arg2type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2) { \
    thisptr->inv_##fname(arg1, arg2); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2) { \
    thisptr->inv_##fname(arg1, arg2); \
  }

#define WRAPPED_FUNCTION_3ARG_VOID(fname, rettype, arg1type, arg2type, arg3type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3) { \
    thisptr->inv_##fname(arg1, arg2, arg3); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3) { \
    thisptr->inv_##fname(arg1, arg2, arg3); \
  }

#define WRAPPED_FUNCTION_4ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4); \
  }

#define WRAPPED_FUNCTION_5ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5); \
  }

#define WRAPPED_FUNCTION_6ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6); \
  }

#define WRAPPED_FUNCTION_7ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
  }

#define WRAPPED_FUNCTION_8ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
  }

#define WRAPPED_FUNCTION_9ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); \
  }

#define WRAPPED_FUNCTION_10ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    PUSH_VAL_TO_STACK<arg10type>(threadState->stackPtr, arg10); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
  }

#define WRAPPED_FUNCTION_11ARG_VOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type, arg11type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    PUSH_VAL_TO_STACK<arg10type>(threadState->stackPtr, arg10); \
    PUSH_VAL_TO_STACK<arg11type>(threadState->stackPtr, arg11); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); \
  }

#define WRAPPED_FUNCTION_0ARG_NONVOID(fname, rettype) \
  rettype(PROCESS_SANDBOX_CLASSNAME::inv_##fname) () { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype(PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr) { \
    return thisptr->inv_##fname(); \
  } \
  extern "C" rettype(ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr) { \
    return thisptr->inv_##fname(); \
  }

#define WRAPPED_FUNCTION_1ARG_NONVOID_NOGL(fname, rettype, arg1type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1) { \
    return thisptr->inv_##fname(arg1); \
  }

#define WRAPPED_FUNCTION_1ARG_NONVOID(fname, rettype, arg1type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1) { \
    return thisptr->inv_##fname(arg1); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1) { \
    return thisptr->inv_##fname(arg1); \
  }

#define WRAPPED_FUNCTION_2ARG_NONVOID(fname, rettype, arg1type, arg2type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2) { \
    return thisptr->inv_##fname(arg1, arg2); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2) { \
    return thisptr->inv_##fname(arg1, arg2); \
  }

#define WRAPPED_FUNCTION_3ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3) { \
    return thisptr->inv_##fname(arg1, arg2, arg3); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3) { \
    return thisptr->inv_##fname(arg1, arg2, arg3); \
  }

#define WRAPPED_FUNCTION_4ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4); \
  }

#define WRAPPED_FUNCTION_5ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5); \
  }

#define WRAPPED_FUNCTION_6ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6); \
  }

#define WRAPPED_FUNCTION_7ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7); \
  }

#define WRAPPED_FUNCTION_8ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8); \
  }

#define WRAPPED_FUNCTION_9ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9); \
  }

#define WRAPPED_FUNCTION_10ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    PUSH_VAL_TO_STACK<arg10type>(threadState->stackPtr, arg10); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10); \
  }

#define WRAPPED_FUNCTION_11ARG_NONVOID(fname, rettype, arg1type, arg2type, arg3type, arg4type, arg5type, arg6type, arg7type, arg8type, arg9type, arg10type, arg11type) \
  rettype (PROCESS_SANDBOX_CLASSNAME::inv_##fname) (arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    pthread_mutex_lock(&sandboxMutex);  /* only one thread is allowed in the sandbox at a time */ \
    perthread_shared_state_t* threadState = getActiveThreadState(); \
    threadState->funcnum = FUNCNUM_OF(fname); \
    PUSH_VAL_TO_STACK<arg1type>(threadState->stackPtr, arg1); \
    PUSH_VAL_TO_STACK<arg2type>(threadState->stackPtr, arg2); \
    PUSH_VAL_TO_STACK<arg3type>(threadState->stackPtr, arg3); \
    PUSH_VAL_TO_STACK<arg4type>(threadState->stackPtr, arg4); \
    PUSH_VAL_TO_STACK<arg5type>(threadState->stackPtr, arg5); \
    PUSH_VAL_TO_STACK<arg6type>(threadState->stackPtr, arg6); \
    PUSH_VAL_TO_STACK<arg7type>(threadState->stackPtr, arg7); \
    PUSH_VAL_TO_STACK<arg8type>(threadState->stackPtr, arg8); \
    PUSH_VAL_TO_STACK<arg9type>(threadState->stackPtr, arg9); \
    PUSH_VAL_TO_STACK<arg10type>(threadState->stackPtr, arg10); \
    PUSH_VAL_TO_STACK<arg11type>(threadState->stackPtr, arg11); \
    invokeFunctionCall(threadState); \
    /* Currently, due to limitations on the otherside, we don't unlock until the toplevel call \
         has finished (including all callbacks).  Yes this sucks for other threads waiting. \
         Yes we should fix this to a more sane behavior, perhaps a multithreaded otherside. */ \
    pthread_mutex_unlock(&sandboxMutex); \
    return POP_VAL_FROM_STACK<rettype>(threadState->stackPtr); \
  } \
  rettype (PROCESS_SANDBOX_CLASSNAME::fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); \
  } \
  extern "C" rettype (ProcessSandbox_##fname) (PROCESS_SANDBOX_CLASSNAME* thisptr, arg1type arg1, arg2type arg2, arg3type arg3, arg4type arg4, arg5type arg5, arg6type arg6, arg7type arg7, arg8type arg8, arg9type arg9, arg10type arg10, arg11type arg11) { \
    return thisptr->inv_##fname(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11); \
  }

WRAPPED_FUNCTION_1ARG_NONVOID_NOGL(invokeDlSym, void*, const char*)
FOR_EACH_0ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_0ARG_VOID)
FOR_EACH_1ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_1ARG_VOID)
FOR_EACH_2ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_2ARG_VOID)
FOR_EACH_3ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_3ARG_VOID)
FOR_EACH_4ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_4ARG_VOID)
FOR_EACH_5ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_5ARG_VOID)
FOR_EACH_6ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_6ARG_VOID)
FOR_EACH_7ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_7ARG_VOID)
FOR_EACH_8ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_8ARG_VOID)
FOR_EACH_9ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_9ARG_VOID)
FOR_EACH_10ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_10ARG_VOID)
FOR_EACH_11ARG_VOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_11ARG_VOID)
FOR_EACH_0ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_0ARG_NONVOID)
FOR_EACH_1ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_1ARG_NONVOID)
FOR_EACH_2ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_2ARG_NONVOID)
FOR_EACH_3ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_3ARG_NONVOID)
FOR_EACH_4ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_4ARG_NONVOID)
FOR_EACH_5ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_5ARG_NONVOID)
FOR_EACH_6ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_6ARG_NONVOID)
FOR_EACH_7ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_7ARG_NONVOID)
FOR_EACH_8ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_8ARG_NONVOID)
FOR_EACH_9ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_9ARG_NONVOID)
FOR_EACH_10ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_10ARG_NONVOID)
FOR_EACH_11ARG_NONVOID_LIBRARY_FUNCTION(WRAPPED_FUNCTION_11ARG_NONVOID)

#undef WRAPPED_FUNCTION_1ARG_NONVOID_NOGL
#undef WRAPPED_FUNCTION_0ARG_VOID
#undef WRAPPED_FUNCTION_1ARG_VOID
#undef WRAPPED_FUNCTION_2ARG_VOID
#undef WRAPPED_FUNCTION_3ARG_VOID
#undef WRAPPED_FUNCTION_4ARG_VOID
#undef WRAPPED_FUNCTION_5ARG_VOID
#undef WRAPPED_FUNCTION_6ARG_VOID
#undef WRAPPED_FUNCTION_7ARG_VOID
#undef WRAPPED_FUNCTION_8ARG_VOID
#undef WRAPPED_FUNCTION_9ARG_VOID
#undef WRAPPED_FUNCTION_10ARG_VOID
#undef WRAPPED_FUNCTION_11ARG_VOID
#undef WRAPPED_FUNCTION_0ARG_NONVOID
#undef WRAPPED_FUNCTION_1ARG_NONVOID
#undef WRAPPED_FUNCTION_2ARG_NONVOID
#undef WRAPPED_FUNCTION_3ARG_NONVOID
#undef WRAPPED_FUNCTION_4ARG_NONVOID
#undef WRAPPED_FUNCTION_5ARG_NONVOID
#undef WRAPPED_FUNCTION_6ARG_NONVOID
#undef WRAPPED_FUNCTION_7ARG_NONVOID
#undef WRAPPED_FUNCTION_8ARG_NONVOID
#undef WRAPPED_FUNCTION_9ARG_NONVOID
#undef WRAPPED_FUNCTION_10ARG_NONVOID
#undef WRAPPED_FUNCTION_11ARG_NONVOID

template<typename argtype>
static void processVoid_1arg_Callback(void (*cb)(argtype, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg = POP_VAL_FROM_STACK<argtype>(stackPtr);
  cb(arg, extraArg);
}

template<typename arg1type, typename arg2type>
static void processVoid_2arg_Callback(void (*cb)(arg1type, arg2type, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg2 = POP_VAL_FROM_STACK<arg2type>(stackPtr);
  auto arg1 = POP_VAL_FROM_STACK<arg1type>(stackPtr);
  cb(arg1, arg2, extraArg);
}

template<typename arg1type, typename arg2type, typename arg3type, typename arg4type>
static void processVoid_4arg_Callback(void (*cb)(arg1type, arg2type, arg3type, arg4type, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg4 = POP_VAL_FROM_STACK<arg4type>(stackPtr);
  auto arg3 = POP_VAL_FROM_STACK<arg3type>(stackPtr);
  auto arg2 = POP_VAL_FROM_STACK<arg2type>(stackPtr);
  auto arg1 = POP_VAL_FROM_STACK<arg1type>(stackPtr);
  cb(arg1, arg2, arg3, arg4, extraArg);
}

template<typename rettype, typename argtype>
static void processNonvoid_1arg_Callback(rettype (*cb)(argtype, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg = POP_VAL_FROM_STACK<argtype>(stackPtr);
  PUSH_VAL_TO_STACK<rettype>(stackPtr, cb(arg, extraArg));
}

template<typename rettype, typename arg1type, typename arg2type>
static void processNonvoid_2arg_Callback(rettype (*cb)(arg1type, arg2type, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg2 = POP_VAL_FROM_STACK<arg2type>(stackPtr);
  auto arg1 = POP_VAL_FROM_STACK<arg1type>(stackPtr);
  PUSH_VAL_TO_STACK<rettype>(stackPtr, cb(arg1, arg2, extraArg));
}

template<typename rettype, typename arg1type, typename arg2type, typename arg3type>
static void processNonvoid_3arg_Callback(rettype (*cb)(arg1type, arg2type, arg3type, void*), uint8_t*& stackPtr, void* extraArg) {
  if(cb == NULL) ERROR("cb should not be NULL\n")
  auto arg3 = POP_VAL_FROM_STACK<arg3type>(stackPtr);
  auto arg2 = POP_VAL_FROM_STACK<arg2type>(stackPtr);
  auto arg1 = POP_VAL_FROM_STACK<arg1type>(stackPtr);
  PUSH_VAL_TO_STACK<rettype>(stackPtr, cb(arg1, arg2, arg3, extraArg));
}

// Gets the threadState for the current thread, and also sets activeThreadState appropriately
perthread_shared_state_t* PROCESS_SANDBOX_CLASSNAME::getActiveThreadState() {
  pid_t tid = syscall(SYS_gettid);
  perthread_shared_state_t* threadState;
  auto& threadStatesMap = *((std::map<pid_t, perthread_shared_state_t*>*) threadStates);
  if(threadStatesMap.count(tid)) threadState = threadStatesMap[tid];
  else {
    void* mem = mallocInSandboxWithThreadState(sizeof(perthread_shared_state_t), &sandboxSharedState->threadStateForInitializing);
    threadState = new (mem) perthread_shared_state_t;
    threadStatesMap[tid] = threadState;
  }
  sandboxSharedState->activeThreadState = threadState;
  return threadState;
}

// helper for addVoidStarArg below
template<typename rettype, typename... argtypes>
rettype (*addVoidStarArgHelper(rettype(*)(argtypes...)))(argtypes..., void*);

// Given a function type, get the function type that takes an extra void* argument
template<typename func>
using addVoidStarArg = decltype(addVoidStarArgHelper(std::declval<func>()));

// Assumes that funcnum, args, and activeThreadState have already been set appropriately.
// Argument: threadState for the current thread
void PROCESS_SANDBOX_CLASSNAME::invokeFunctionCall(perthread_shared_state_t* threadState) {
  threadState->returning = false;
  //printf(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) ": invokeFunctionCall: threadState %p, funcnum %u\n", threadState, threadState->funcnum);
  while(true) {
    sandboxSharedState->invoker.invoke(Invoker::MAIN_PROCESS);
    if(threadState->returning) break;
    //printf(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) ": threadState %p callback requested by otherside\n", threadState);
    // else this is a callback

    // see notes in ProcessSandbox_sharedmem.h
    unsigned type = threadState->callbacknum / CALLBACK_TYPES;
    unsigned index = threadState->callbacknum % CALLBACK_TYPES;
    //printf(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) ": callback requested was %u, i.e. type %u, index %u\n", threadState->callbacknum, type, index);
    auto pair = callbacks[type][index];
    void* extraState = pair.second;
    switch(type) {
      case 0: {
        addVoidStarArg<LIB::CB_TYPE_0> cb = (addVoidStarArg<LIB::CB_TYPE_0>) pair.first;
        // having to use an #ifdef switch here (and below) is definitely ugly
        //   and shouldn't be necessary, just using it here temporarily
        #ifdef USE_DUMMY_LIB
        processNonvoid_1arg_Callback<int, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBJPEG)
        processVoid_1arg_Callback<j_common_ptr>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_2arg_Callback<png_structp, png_const_charp>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        processNonvoid_3arg_Callback<voidpf, voidpf, uInt, uInt>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_0\n")
        #elif defined(USE_LIBTHEORA)
        processVoid_4arg_Callback<VOIDSTAR, th_ycbcr_buffer, int, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVPX)
        processVoid_4arg_Callback<void*, const unsigned char*, unsigned char*, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_0\n")
        #elif defined(USE_RLBOXTEST)
        processNonvoid_3arg_Callback<int, unsigned, const char*, unsigned[1]>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_TEST)
        processNonvoid_3arg_Callback<int, unsigned, char*, unsigned[1]>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBEVENT)
        processVoid_2arg_Callback<struct evhttp_request *, void *>(cb, threadState->stackPtr, extraState);
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 1: {
        addVoidStarArg<LIB::CB_TYPE_1> cb = (addVoidStarArg<LIB::CB_TYPE_1>) pair.first;
        #ifdef USE_DUMMY_LIB
        processVoid_2arg_Callback<char, char>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBJPEG)
        processVoid_1arg_Callback<j_decompress_ptr>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_2arg_Callback<png_structp, png_infop>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        processVoid_2arg_Callback<voidpf, voidpf>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_1\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_1\n")
        #elif defined(USE_LIBVPX)
        processVoid_2arg_Callback<void*, const vpx_image_t*>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_1\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_1\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_1\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_1\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 2: {
        addVoidStarArg<LIB::CB_TYPE_2> cb = (addVoidStarArg<LIB::CB_TYPE_2>) pair.first;
        #ifdef USE_DUMMY_LIB
        processNonvoid_1arg_Callback<VOIDSTAR, INTSTAR>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBJPEG)
        processVoid_2arg_Callback<j_decompress_ptr, long>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_4arg_Callback<png_structp, png_bytep, png_uint_32, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        processNonvoid_2arg_Callback<unsigned, VOIDFARSTAR, z_const unsigned char FAR * FAR *>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_2\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_2\n")
        #elif defined(USE_LIBVPX)
        processVoid_4arg_Callback<void*, const vpx_image_t*, const vpx_image_rect_t*, const vpx_image_rect_t*>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_2\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_2\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_2\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_2\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 3: {
        addVoidStarArg<LIB::CB_TYPE_3> cb = (addVoidStarArg<LIB::CB_TYPE_3>) pair.first;
        #ifdef USE_DUMMY_LIB
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): DUMMY_LIB doesn't have CB_TYPE_3\n")
        #elif defined(USE_LIBJPEG)
        processNonvoid_1arg_Callback<boolean, j_decompress_ptr>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_2arg_Callback<png_structp, png_infop>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        processNonvoid_3arg_Callback<int, VOIDFARSTAR, UCHARFSTAR, unsigned>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_3\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_3\n")
        #elif defined(USE_LIBVPX)
        processNonvoid_3arg_Callback<int, void*, size_t, vpx_codec_frame_buffer_t*>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_3\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_3\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_3\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_3\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 4: {
        addVoidStarArg<LIB::CB_TYPE_4> cb = (addVoidStarArg<LIB::CB_TYPE_4>) pair.first;
        #ifdef USE_DUMMY_LIB
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): DUMMY_LIB doesn't have CB_TYPE_4\n")
        #elif defined(USE_LIBJPEG)
        processVoid_1arg_Callback<j_decompress_ptr>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_2arg_Callback<png_structp, png_uint_32>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): ZLIB doesn't have CB_TYPE_4\n")
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_4\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_4\n")
        #elif defined(USE_LIBVPX)
        processNonvoid_2arg_Callback<int, void*, vpx_codec_frame_buffer_t*>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_4\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_4\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_4\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_4\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 5: {
        addVoidStarArg<LIB::CB_TYPE_5> cb = (addVoidStarArg<LIB::CB_TYPE_5>) pair.first;
        #ifdef USE_DUMMY_LIB
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): DUMMY_LIB doesn't have CB_TYPE_5\n")
        #elif defined(USE_LIBJPEG)
        processNonvoid_2arg_Callback<boolean, j_decompress_ptr, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_LIBPNG)
        processVoid_2arg_Callback<jmp_buf, int>(cb, threadState->stackPtr, extraState);
        #elif defined(USE_ZLIB)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): ZLIB doesn't have CB_TYPE_5\n")
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_5\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_5\n")
        #elif defined(USE_LIBVPX)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBVPX doesn't have CB_TYPE_5\n")
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_5\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_5\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_5\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_5\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 6: {
        addVoidStarArg<LIB::CB_TYPE_6> cb = (addVoidStarArg<LIB::CB_TYPE_6>) pair.first;
        #ifdef USE_DUMMY_LIB
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): DUMMY_LIB doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBJPEG)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBJPEG doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBPNG)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBPNG doesn't have CB_TYPE_6\n")
        #elif defined(USE_ZLIB)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): ZLIB doesn't have CB_TYPE_6\n")
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBVPX)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBVPX doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_6\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_6\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_6\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_6\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } case 7: {
        addVoidStarArg<LIB::CB_TYPE_7> cb = (addVoidStarArg<LIB::CB_TYPE_7>) pair.first;
        #ifdef USE_DUMMY_LIB
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): DUMMY_LIB doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBJPEG)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBJPEG doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBPNG)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBPNG doesn't have CB_TYPE_7\n")
        #elif defined(USE_ZLIB)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): ZLIB doesn't have CB_TYPE_7\n")
        #elif defined(USE_PNGDEC)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): PNGDEC doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBTHEORA)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBTHEORA doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBVPX)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): LIBVPX doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBVORBIS)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): VORBIS doesn't have CB_TYPE_7\n")
        #elif defined(USE_RLBOXTEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): RLBOXTEST doesn't have CB_TYPE_7\n")
        #elif defined(USE_TEST)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): TEST doesn't have CB_TYPE_7\n")
        #elif defined(USE_LIBEVENT)
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): EVENT doesn't have CB_TYPE_7\n")
        #else
        #error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
        #endif
        break;
      } default: {
        ERROR(STRINGIFY(PROCESS_SANDBOX_CLASSNAME) "::invokeFunctionCall(): invalid cbnum (%u) or callback type (%u)\n", threadState->callbacknum, type)
        break;
      }
    }
    threadState->returning = true;
    // return to top of while loop
  }
}
