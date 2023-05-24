#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <utility>
#include <map>
#include "uswitchsandbox.h"
#include "uswitch.hpp"

namespace RLBox_USwitch_detail {
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
struct RLBox_USwitch_RetTypeHelper {
    T data_;
    inline T* pointer() {
        return &data_;
    }
    inline T data() {
        return data_;
    }
};

template <>
struct RLBox_USwitch_RetTypeHelper<void> {
    constexpr inline void* pointer() {
        return nullptr;
    }
    constexpr inline void data() {}
};

class RLBox_USwitch
{
private:
    USwitchSandbox* sandbox;
    int pushPopCount = 0;
    std::map<void *, int> callbackKVMap;
    std::vector<std::unique_ptr<std::pair<void *, void *>>> callbackArguments;
    template<typename TRet, typename... TArgs>
    static TRet callbackHandler(uswctx_t ctx, void *data, TArgs... args) {
        std::pair<void *, void *> p = *(std::pair<void *, void *> *)data;
        auto callback = (TRet (*)(TArgs..., void *))p.first;
        void *state = p.second;
        return callback(args..., state);
    }
public:
    inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
    {
        sandbox = new USwitchSandbox(libraryPath, 1024l << 20, 2l << 20, 32);
        if (!sandbox->init()) {
            printf("Could not initialize sandbox.\n");
            abort();
        }
    }

    inline void impl_DestroySandbox()
    {
        delete sandbox;
    }

    inline USwitchSandbox* impl_getSandbox()
    {
        return sandbox;
    }

    inline void* impl_mallocInSandbox(size_t size)
    {
        return sandbox->malloc_in_sandbox(size);
    }

    inline void impl_freeInSandbox(void* val)
    {
        sandbox->free_in_sandbox(val);
    }

    inline size_t impl_getTotalMemory()
    {
        return sandbox->memory_size;
    }

    inline char* impl_getMaxPointer()
    {
        return (char *)(sandbox->memory + sandbox->memory_size);
    }

    inline void* impl_pushStackArr(size_t size)
    {
        pushPopCount++;
        return sandbox->malloc_in_sandbox(size);
    }

    inline void impl_popStackArr(void* ptr, size_t size)
    {
        pushPopCount--;
        if(pushPopCount < 0)
        {
            printf("Error - RLBox_USwitch popCount was negative.\n");
            abort();
        }
        sandbox->free_in_sandbox(ptr);
    }

    template<typename T>
    static inline void* impl_GetUnsandboxedPointer(T* p, void* exampleUnsandboxedPtr)
    {
        return const_cast<void*>((const void*)p);
    }

    template<typename T>
    static inline void* impl_GetSandboxedPointer(T* p, void* exampleUnsandboxedPtr)
    {
        return const_cast<void*>((const void*)p);
    }

    template<typename T>
    inline void* impl_GetUnsandboxedPointer(T* p)
    {
        if (impl_isPointerInSandboxMemoryOrNull((const void*)p)) {
            return const_cast<void*>((const void*)p);
        } else {
            return nullptr;
        }
    }

    template<typename T>
    inline void* impl_GetSandboxedPointer(T* p)
    {
        return const_cast<void*>((const void*)p);
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
        return (p == 0) || (p >= sandbox->memory && p < sandbox->memory + sandbox->memory_size);
    }

    inline bool impl_isPointerInAppMemoryOrNull(const void* p)
    {
        return (p == nullptr) || (!impl_isPointerInSandboxMemoryOrNull(p));
    }

    template<typename T>
    static inline T* impl_pointerIncrement(T* p, int64_t increment)
    {
        return p + increment;
    }

    template<typename TRet, typename... TArgs>
    inline void* impl_RegisterCallback(void* key, void* callback, void* state)
    {
        int cid;
        callbackArguments.push_back(std::make_unique<std::pair<void *, void *>>(callback, state));
        auto func = (TRet (*)(uswctx_t, void *, TArgs...))callbackHandler<TRet, TArgs...>;
        void *ptr = (void *)uswitch_register_callback_get_fp(16, sandbox->get_context(),
            (void *)callbackArguments.back().get(), func, &cid);
        if (ptr) {
            callbackKVMap[key] = cid;
        }
        return ptr;
    }

    template<typename TFunc>
    inline void impl_UnregisterCallback(void* key)
    {
        //auto it = callbackKVMap.find(key);
        //if (it == callbackKVMap.end()) {
        //    return;
        //}
        //uswitch_unregister_callback(sandbox->get_context(), it->second);
        //callbackKVMap.erase(it);
    }

    inline void* impl_LookupSymbol(const char* name, bool forSandboxFunction)
    {
        void *addr = sandbox->get_symbol_addr(name);
        //printf("%s %p\n", name, addr);
        return addr;
    }

    template <typename T, typename ... TArgs>
    RLBox_USwitch_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
    {
        using U = RLBox_USwitch_detail::return_argument<T>;
        RLBox_USwitch_RetTypeHelper<U> ret;
        uswitch_call_dynamic(sandbox->get_context(), fnPtr, ret.pointer(), params...);
        return ret.data();
    }

    template <typename T, typename ... TArgs>
    RLBox_USwitch_detail::return_argument<T> impl_InvokeFunctionWithExtraData(T* fnPtr, uswctx_t ctx, TArgs... params)
    {
        using U = RLBox_USwitch_detail::return_argument<T>;
        RLBox_USwitch_RetTypeHelper<U> ret;
        uswitch_call_dynamic(ctx, fnPtr, ret.pointer(), params...);
        return ret.data();
    }

    template <typename T, typename ... TArgs>
    RLBox_USwitch_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
    {
        return impl_InvokeFunction(fnPtr, params...);
    }
};
