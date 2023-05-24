#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <utility>
#include <map>
#include <mutex>
#include "wasmsandbox.h"
#include "wasm.hpp"

namespace RLBox_Wasm_detail {
    //https://stackoverflow.com/questions/6512019/can-we-get-the-type-of-a-lambda-argument
    template<typename Ret, typename... Rest>
    Ret return_argument_helper(Ret(*) (Rest...));

    template<typename Ret, typename F, typename... Rest>
    Ret return_argument_helper(Ret(F::*) (Rest...));

    template<typename Ret, typename F, typename... Rest>
    Ret return_argument_helper(Ret(F::*) (Rest...) const);

    template <typename F>
    decltype(return_argument_helper(&F::operator())) return_argument_helper(F);

    template <typename T>
    using return_argument = decltype(return_argument_helper(std::declval<T>()));
};

template <typename T>
struct RLBox_Wasm_RetTypeHelper {
    T data_;
    inline T* pointer() {
        return &data_;
    }
    inline T data() {
        return data_;
    }
};

template <>
struct RLBox_Wasm_RetTypeHelper<void> {
    constexpr inline void* pointer() {
        return nullptr;
    }
    constexpr inline void data() {}
};

class RLBox_Wasm
{
private:
    WasmSandbox* sandbox;
    std::recursive_mutex mutex;
    int pushPopCount = 0;
public:
    inline void impl_CreateSandbox(const char* prefix, const char* libraryPath)
    {
        sandbox = new WasmSandbox(libraryPath, prefix);
        if (!sandbox->init()) {
            printf("Could not initialize sandbox.\n");
            abort();
        }
    }

    inline void impl_DestroySandbox()
    {
        delete sandbox;
    }

    inline WasmSandbox* impl_getSandbox()
    {
        return sandbox;
    }

    inline void* impl_mallocInSandbox(size_t size)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        return sandbox->get_unsandboxed_pointer<uint8_t>(sandbox->malloc_in_sandbox(size));
    }

    inline void impl_freeInSandbox(void* val)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        sandbox->free_in_sandbox(sandbox->get_sandboxed_pointer<uint8_t>(val));
    }

    inline size_t impl_getTotalMemory()
    {
        return sandbox->max_heap_size;
    }

    inline char* impl_getMaxPointer()
    {
        return (char *)(sandbox->memory_base + sandbox->max_heap_size);
    }

    inline void* impl_pushStackArr(size_t size)
    {
        pushPopCount++;
        return impl_mallocInSandbox(size);
    }

    inline void impl_popStackArr(void* ptr, size_t size)
    {
        pushPopCount--;
        if(pushPopCount < 0)
        {
            printf("Error - RLBox_Wasm popCount was negative.\n");
            abort();
        }
        impl_freeInSandbox(ptr);
    }

    template<typename T>
    static inline void* impl_GetUnsandboxedPointer(T* p, void* exampleUnsandboxedPtr)
    {
        if (!p) {
            return nullptr;
        }
        uint32_t p_sandbox = (uint32_t)(uintptr_t)p;
        uintptr_t base = (uintptr_t)exampleUnsandboxedPtr;
        base &= ~0xffffffffl;
        return (void *)(base + p_sandbox);
    }

    template<typename T>
    static inline void* impl_GetSandboxedPointer(T* p, void* exampleUnsandboxedPtr)
    {
        return (void *)((uintptr_t)p & 0xffffffffl);
    }

    template<typename T>
    inline void* impl_GetUnsandboxedPointer(T* p)
    {
        return sandbox->get_unsandboxed_pointer<T>((uint32_t)(uintptr_t)p);
    }

    template<typename T>
    inline void* impl_GetSandboxedPointer(T* p)
    {
        return (void *)(uintptr_t)sandbox->get_sandboxed_pointer<T>((void *)p);
    }

    inline bool impl_isValidSandboxedPointer(const void* p, bool isFuncPtr)
    {
        if(isFuncPtr) {
            return false;
        } else {
            return impl_isPointerInSandboxMemoryOrNull(p);
        }
    }

    inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
    {
        return (p == nullptr) || sandbox->is_pointer_valid<uint8_t>((uint32_t)(uintptr_t)p);
    }

    inline bool impl_isPointerInAppMemoryOrNull(const void* p)
    {
        return (p == nullptr) || (!impl_isPointerInSandboxMemoryOrNull(p));
    }

    template<typename T>
    static inline T* impl_pointerIncrement(T* p, int64_t increment)
    {
        return nullptr;
    }

    template<typename TRet, typename... TArgs>
    inline void* impl_RegisterCallback(void* key, void* callback, void* state)
    {
        uint32_t p;
        auto func = (TRet (*)(TArgs..., void *))callback;
        if (!rlbox_wasm::Callback<16, TRet (*)(TArgs...)>::register_callback(sandbox, func, state, p)) {
            return nullptr;
        }
        return (void *)(uintptr_t)p;
    }

    template<typename TFunc>
    inline void impl_UnregisterCallback(void* key)
    {
        
    }

    inline void* impl_LookupSymbol(const char* name, bool forSandboxFunction)
    {
        void *addr = sandbox->get_symbol_addr(name);
        //printf("%s %p\n", name, addr);
        return addr;
    }

    template <typename T, typename ... TArgs>
    RLBox_Wasm_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
    {
        std::lock_guard<std::recursive_mutex> lock(mutex);
        return wasm_call(sandbox, T *, (void *)fnPtr, params...);
    }

    template <typename T, typename ... TArgs>
    RLBox_Wasm_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
    {
        return impl_InvokeFunction(fnPtr, params...);
    }
};
