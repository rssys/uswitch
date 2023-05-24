#pragma once

#include "synch.h"
#include <stdint.h>
#include <type_traits>
#include <cstring>
#ifdef USE_DUMMY_LIB
#include "library_helpers.h"
#elif defined(USE_TEST)
#include "test_dyn_lib_helpers.h"
#elif defined(USE_RLBOXTEST)
#include "rlboxtestlib_helpers.h"
#elif defined(USE_LIBJPEG)
#include "jpeglib_helpers.h"
#elif defined(USE_LIBPNG)
#include "pnglib_helpers.h"
#elif defined(USE_ZLIB)
#include "zlib_helpers.h"
#elif defined(USE_PNGDEC)
#include "pngdec_helpers.h"
#elif defined(USE_LIBTHEORA)
#include "libtheora_helpers.h"
#elif defined(USE_LIBVPX)
#include "libvpx_helpers.h"
#elif defined(USE_LIBVORBIS)
#include "libvorbis_helpers.h"
#elif defined(USE_LIBEVENT)
#include "libevent_helpers.h"
#else
#error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
#endif

#define KBYTE 1024ull
#define MBYTE (KBYTE*KBYTE)
#define GBYTE (MBYTE*KBYTE)

#ifdef __amd64__  // 64-bit
#define SHAREDMEM_SIZE ((uintptr_t)(4*GBYTE))  /* allocated/paged-in on demand */
#else
#define SHAREDMEM_SIZE ((uintptr_t)(256*MBYTE))  /* allocated/paged-in on demand */
#endif

// How many different callback types (signatures) we support
#define CALLBACK_TYPES 8

// How many different functions can be registered as callbacks, per callback type
#define CALLBACKS_PER_TYPE 8

// Assign each library function a number to be used in 'funcnum' field below
#define FUNCNUM_OF(fname, ...) FUNCNUM_##fname
#define FUNCNUM_AND_COMMA(fname, ...) FUNCNUM_##fname,
typedef enum {
  FOR_EACH_LIBRARY_FUNCTION(FUNCNUM_AND_COMMA)

  // A special 'funcnum' which indicates just that the sandbox should initialize itself.
  //   When the sandbox returns from a call to this 'funcnum', it is guaranteed that the
  //   sandbox is fully initialized (and in particular, all of 'callbackWrappers' are valid)
  FUNCNUM_INIT,

  // A special 'funcnum' which requests the sandbox to perform a malloc (in shared memory)
  FUNCNUM_MALLOC,
  // Likewise, but for free
  FUNCNUM_FREE,

} funcnum_t;

typedef struct {
    // Just before transferring control, the active side sets this to
    //   TRUE if it is returning from a command from the other side
    //   or FALSE if it is making a new (possibly nested) request of the other side
    bool returning;

    // The function number or callback number of the request
    // By convention, callbacknums 0 through (CALLBACKS_PER_TYPE-1) indicate
    //   callbacks of type 1; callbacknums (CALLBACKS_PER_TYPE) through
    //   (CALLBACKS_PER_TYPE+CALLBACKS_PER_TYPE-1) indicate callbacks of type 2; etc
    union {
      funcnum_t funcnum;
      unsigned callbacknum;
    };

    // space for storing function and callback arguments and return values
#define STACK_SIZE_IN_BYTES (1*KBYTE)  /* way too much space, for now */
    uint8_t stack[STACK_SIZE_IN_BYTES];
    uint8_t* stackPtr = stack;  // stack grows UP
} perthread_shared_state_t;

typedef struct {
    // Used by each side to "invoke" the other
    // i.e. this abstracts our interprocess signaling
    Invoker invoker;

    // Pointer to the per-thread shared state for the currently active thread
    //   (i.e. the thread currently "using" the sandbox)
    // Should point into somewhere in extraSpace in this struct
    // The main process uses this to tell the sandbox process where to find the
    //   funcnum, arguments, etc
    perthread_shared_state_t* activeThreadState;

    // for use in getActiveThreadState, to avoid mutual recursion
    perthread_shared_state_t threadStateForInitializing;

    // Function pointers (to valid *sandbox* addresses) to be used by the main app
    //   as substitute callback pointers.
    // I.e., ProcessSandbox::registerCallback() returns values from these arrays.
    // cbWrappers[0][5] indicates the fifth callback registered of type 0
    void* cbWrappers[CALLBACKS_PER_TYPE][CALLBACK_TYPES];

    // Space to use for mallocInSandbox()
    uint8_t extraSpace[0];
} persandbox_shared_state_t;

// takes a reference to the stack pointer, so it can modify the stack pointer
template<typename T>
inline 
typename std::enable_if<!std::is_array<T>::value,
void>::type PUSH_VAL_TO_STACK(uint8_t*& stackPtr, const T value) {
  *(T*)stackPtr = value;
  stackPtr += sizeof(T);
}

template<typename T>
inline 
typename std::enable_if<std::is_array<T>::value,
void>::type PUSH_VAL_TO_STACK(uint8_t*& stackPtr, const T value) {
  memcpy(stackPtr, value, sizeof(T));
  stackPtr += sizeof(T);
}


// takes a reference to the stack pointer, so it can modify the stack pointer
template<typename T>
inline typename std::enable_if<!std::is_array<T>::value,
T>::type POP_VAL_FROM_STACK(uint8_t*& stackPtr) {
  stackPtr -= sizeof(T);
  T retval = *(T*)stackPtr;
  return retval;
}

template<typename T>
inline typename std::enable_if<std::is_array<T>::value,
typename std::decay<T>::type>::type POP_VAL_FROM_STACK(uint8_t*& stackPtr) {
  
  using ArrElPtr = typename std::decay<T>::type;

  stackPtr -= sizeof(T);
  ArrElPtr retval = (ArrElPtr) stackPtr;
  return retval;
}
