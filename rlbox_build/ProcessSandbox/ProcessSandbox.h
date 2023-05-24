#include "ProcessSandbox_sharedmem.h"
#include "myHelpers.h"  // ERROR()
#include "pthread.h"
#include <mutex>
#include <map>

#undef PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
#ifdef USE_DUMMY_LIB
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_DUMMY_LIB
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_DUMMY_LIB
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "library_helpers.h"
#endif
#elif defined(USE_TEST)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_TEST
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_TEST
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "test_dyn_lib_helpers.h"
#endif
#elif defined(USE_RLBOXTEST)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_RLBOXTEST
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_RLBOXTEST
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "rlboxtestlib_helpers.h"
#endif
#elif defined(USE_LIBJPEG)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBJPEG
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBJPEG
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "jpeglib_helpers.h"
#endif
#elif defined(USE_LIBPNG)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBPNG
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBPNG
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "pnglib_helpers.h"
#endif
#elif defined(USE_ZLIB)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_ZLIB
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_ZLIB
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
	#include "zlib_helpers.h"
#endif
#elif defined(USE_PNGDEC)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_PNGDEC
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_PNGDEC
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "pngdec_helpers.h"
#endif
#elif defined(USE_LIBTHEORA)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBTHEORA
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBTHEORA
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "libtheora_helpers.h"
#endif
#elif defined(USE_LIBVPX)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBVPX
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBVPX
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "libvpx_helpers.h"
#endif
#elif defined(USE_LIBVORBIS)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBVORBIS
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBVORBIS
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "libvorbis_helpers.h"
#endif
#elif defined(USE_LIBEVENT)
#ifndef INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBEVENT
    #define INCLUDED_PROCESS_SANDBOX_WITH_USE_LIBEVENT
    #define PROCESS_SANDBOX_PROCEED_WITH_INCLUDE
    #include "libevent_helpers.h"
#endif
#else
#error Please define one of USE_DUMMY_LIB, USE_TEST, USE_RLBOXTEST, USE_LIBJPEG, USE_LIBPNG, USE_ZLIB, USE_PNGDEC, USE_LIBTHEORA, USE_LIBVPX, or USE_LIBVORBIS while compiling.
#endif

#ifdef PROCESS_SANDBOX_PROCEED_WITH_INCLUDE

    // Include the library name in the class name so that ProcessSandboxes for different libraries play nice
    #define PROCESS_SANDBOX_CLASSNAME_FROM_LIB(lib) PROCESS_SANDBOX_CLASSNAME_FROM_LIB_(lib)  /* fully expand 'lib' */
    #define PROCESS_SANDBOX_CLASSNAME_FROM_LIB_(lib) lib##ProcessSandbox
    #define PROCESS_SANDBOX_CLASSNAME PROCESS_SANDBOX_CLASSNAME_FROM_LIB(LIB)

    class PROCESS_SANDBOX_CLASSNAME {
      public:
        // othersidepath: the path to the ProcessSandbox_otherside executable
        // maincore: core to pin the main application to
        // sbcore: core to pin the sandbox process to
        PROCESS_SANDBOX_CLASSNAME(const char* othersidepath, unsigned maincore, unsigned sbcore);

        void* getSandboxMemoryBase() const;

        // Returns NULL on failure
        void* mallocInSandbox(uint32_t size);

        void freeInSandbox(void* ptr);

        void destroySandbox();

        // fptr: the function pointer you wish to use as a callback
        // extraState: idk what this is, it wasn't required for the C API,
        //   but Shravan requires it for the C++ API
        // Returns a new function pointer, which you should use instead
        //   of the original function pointer whenever wanting to pass
        //   it as a callback to a function in the sandboxed library
        // Another way to say this is, you should never pass raw function
        //   pointers into the sandboxed library, only function pointers
        //   which have been given to you by registerCallback()
        template<typename cb_type>
        cb_type registerCallback(cb_type fptr, void* extraState);

        // Here, cb should be a pointer previously obtained from registerCallback()
        template<typename cb_type>
        void unregisterCallback(cb_type cb);

		#define SIGNATURE(fname, rettype, ...) \
			rettype (inv_##fname) (__VA_ARGS__); \
			/* Include a static wrapper for each member function */ \
			static rettype (fname) (PROCESS_SANDBOX_CLASSNAME*, ##__VA_ARGS__);

        FOR_EACH_LIBRARY_FUNCTION(SIGNATURE)

    #undef SIGNATURE

        // make this sandbox "active", i.e., switch to atomic-spinlock rather than sleeping
        void makeActiveSandbox();

        // make this sandbox "inactive", i.e., sleep and yield processor while waiting, until woken by cond variable
        void makeInactiveSandbox();

      protected:
        persandbox_shared_state_t* sandboxSharedState;
        void* threadStates;  // std::map<pid_t, perthread_shared_state_t*>
        const char* shmempath;
        pid_t sandboxPID;

        // Here, callbacks[0][2] is represents the 3rd type-0 callback, etc
        // the first element of the pair is the callback pointer, and the
        //   second element of the pair is the state passed when that
        //   callback was registered
        std::pair<void*, void*> callbacks[CALLBACKS_PER_TYPE][CALLBACK_TYPES];
        int fork_server_writefd, fork_server_readfd;
        bool use_fork_server;
        static std::map<std::string, pair<int, int>> fork_servers;
        static std::mutex fork_server_mutex;
        static size_t sandboxCount;

      private:
        pid_t createSandboxProcess(const char* othersidepath, unsigned sbcore);
        pid_t createSandboxProcessForkServer(const char* othersidepath, unsigned sbcore);
        void* createSharedMem();
        perthread_shared_state_t* getActiveThreadState();
        void invokeFunctionCall(perthread_shared_state_t*);
        void* mallocInSandboxWithThreadState(uint32_t size, perthread_shared_state_t* threadState);

        // must hold this mutex to actually make a call to sandbox
        // if mutex is locked, then some other thread is using this sandbox
        pthread_mutex_t sandboxMutex;
        void initSandboxMutex();

        friend void* mySbrk(ssize_t size);
    };

#ifndef PROCESS_SANDBOX_H_INCLUDED
    template<typename ...Types>
    void printTypes()
    {
        printf("PrintTypes: \n %s\n", __PRETTY_FUNCTION__);
    }
#endif

    // implementation included here because it has to be in the header for templates
    template<typename cb_type>
    inline cb_type PROCESS_SANDBOX_CLASSNAME::registerCallback(cb_type fptr, void* extraState) {
      if(fptr == NULL) ERROR("are you sure you want to register a null pointer?\n")
      unsigned char type;
      if (std::is_assignable<LIB::CB_TYPE_0&, cb_type>::value) type = 0;
      else if (std::is_assignable<LIB::CB_TYPE_1&, cb_type>::value) type = 1;
      else if (std::is_assignable<LIB::CB_TYPE_2&, cb_type>::value) type = 2;
      else if (std::is_assignable<LIB::CB_TYPE_3&, cb_type>::value) type = 3;
      else if (std::is_assignable<LIB::CB_TYPE_4&, cb_type>::value) type = 4;
      else if (std::is_assignable<LIB::CB_TYPE_5&, cb_type>::value) type = 5;
      else if (std::is_assignable<LIB::CB_TYPE_6&, cb_type>::value) type = 6;
      else if (std::is_assignable<LIB::CB_TYPE_7&, cb_type>::value) type = 7;
      else {
        printTypes<cb_type, LIB::CB_TYPE_0,LIB::CB_TYPE_1,LIB::CB_TYPE_2,LIB::CB_TYPE_3,LIB::CB_TYPE_4,LIB::CB_TYPE_5,LIB::CB_TYPE_6,LIB::CB_TYPE_7>();
        ERROR("registerCallback(): Invalid type\n")
      }
      for(unsigned index = 0; index < CALLBACKS_PER_TYPE; index++) {
        if(callbacks[type][index].first == NULL) {
          callbacks[type][index] = std::pair<void*, void*>((void*)fptr, extraState);
          return (cb_type) sandboxSharedState->cbWrappers[type][index];
        }
      }
      ERROR("registerCallback(): Too many callbacks of type %u\n", type)
    }

    template<typename cb_type>
    inline void PROCESS_SANDBOX_CLASSNAME::unregisterCallback(cb_type cb) {
      unsigned char type;
      if (std::is_assignable<LIB::CB_TYPE_0&, cb_type>::value) type = 0;
      else if (std::is_assignable<LIB::CB_TYPE_1&, cb_type>::value) type = 1;
      else if (std::is_assignable<LIB::CB_TYPE_2&, cb_type>::value) type = 2;
      else if (std::is_assignable<LIB::CB_TYPE_3&, cb_type>::value) type = 3;
      else if (std::is_assignable<LIB::CB_TYPE_4&, cb_type>::value) type = 4;
      else if (std::is_assignable<LIB::CB_TYPE_5&, cb_type>::value) type = 5;
      else if (std::is_assignable<LIB::CB_TYPE_6&, cb_type>::value) type = 6;
      else if (std::is_assignable<LIB::CB_TYPE_7&, cb_type>::value) type = 7;
      else {
        printTypes<cb_type, LIB::CB_TYPE_0,LIB::CB_TYPE_1,LIB::CB_TYPE_2,LIB::CB_TYPE_3,LIB::CB_TYPE_4,LIB::CB_TYPE_5,LIB::CB_TYPE_6,LIB::CB_TYPE_7>();
        ERROR("unregisterCallback(): Invalid type\n")
      }
      for(unsigned index = 0; index < CALLBACKS_PER_TYPE; index++) {
        if((cb_type) sandboxSharedState->cbWrappers[type][index] == cb) {
          callbacks[type][index].first = NULL;
          return;
        }
      }
      ERROR("unregisterCallback(): callback wasn't registered\n")
    }

#endif  // PROCESS_SANDBOX_PROCEED_WITH_INCLUDE

#define PROCESS_SANDBOX_H_INCLUDED
