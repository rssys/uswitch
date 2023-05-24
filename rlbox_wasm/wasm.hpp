#pragma once
#include <tuple>
#include <type_traits>
#include "wasm2c/wasm-rt.h"

namespace rlbox_wasm {
    
template <typename T>
constexpr size_t type_hash() {
    const char *name = __PRETTY_FUNCTION__;
    size_t len = 0;
    for (len = 0; name[len]; len++);
    const uint64_t m = 0xc6a4a7935bd1e995ull;
    const int r = 47;
    const uint64_t seed = 0;
    uint64_t h = seed ^ (len * m);
    for (size_t j = 0; j < len / 8; ++j) {
        size_t i = j * 8;
        uint64_t k = ((uint64_t)(uint8_t)name[i]) | ((uint64_t)(uint8_t)name[i+1] << 8) |
            ((uint64_t)(uint8_t)name[i+2] << 16) | ((uint64_t)(uint8_t)name[i+3] << 24) |
            ((uint64_t)(uint8_t)name[i+4] << 32) | ((uint64_t)(uint8_t)name[i+5] << 40) |
            ((uint64_t)(uint8_t)name[i+6] << 48) | ((uint64_t)(uint8_t)name[i+7] << 56);
        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
    }
    size_t i = len / 8 * 8;
    switch (len & 7) {
    case 7: h ^= ((uint64_t)(uint8_t)name[i+6]) << 48;
    case 6: h ^= ((uint64_t)(uint8_t)name[i+5]) << 40;
    case 5: h ^= ((uint64_t)(uint8_t)name[i+4]) << 32;
    case 4: h ^= ((uint64_t)(uint8_t)name[i+3]) << 24;
    case 3: h ^= ((uint64_t)(uint8_t)name[i+2]) << 16;
    case 2: h ^= ((uint64_t)(uint8_t)name[i+1]) << 8;
    case 1: h ^= ((uint64_t)(uint8_t)name[i+0]);
            h *= m;
    }
    h ^= h >> r;
    h *= m;
    h ^= h >> r;
    return (size_t)h;
}

template <typename T>
struct TypeHash {
    static constexpr size_t Value = type_hash<T>();
    template <size_t V>
    struct ForceCompileTimeEval {};
    using Type = ForceCompileTimeEval<Value>;
};

template <typename T, typename = void>
struct WasmType {
    using Type = void;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_I32;
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<
        (std::is_integral_v<T> || std::is_enum_v<T>) && sizeof(T) <= sizeof(int32_t)
    >
> {
    using Type = int32_t;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_I32;
    static inline Type sandbox(WasmSandbox *s, T val) {
        return (Type)val;
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return (T)val;
    }
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<
        (std::is_integral_v<T> || std::is_enum_v<T>) &&
        (sizeof(T) > sizeof(int32_t)) && (sizeof(T) <= sizeof(int64_t))
    >
> {
    using Type = int64_t;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_I64;
    static inline Type sandbox(WasmSandbox *s, T val) {
        return (Type)val;
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return (T)val;
    }
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<std::is_same_v<T, float>>
> {
    using Type = float;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_F32;
    static inline Type sandbox(WasmSandbox *s, T val) {
        return (Type)val;
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return (T)val;
    }
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<std::is_same_v<T, double>>
> {
    using Type = double;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_F64;
    static inline Type sandbox(WasmSandbox *s, T val) {
        return (Type)val;
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return (T)val;
    }
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<std::is_pointer_v<T>>
> {
    using Type = uint32_t;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_I32;
    static inline Type sandbox(WasmSandbox *s, T val) {
        return s->get_sandboxed_pointer<T>((void *)val);
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return (T)s->get_unsandboxed_pointer<T>(val);
    }
};

template <typename T>
struct WasmType<T,
    std::enable_if_t<std::is_class_v<T>>
> {
    using Type = uint32_t;
    static constexpr wasm_rt_type_t TypeId = WASM_RT_I32;
    static inline Type sandbox(WasmSandbox *s, T &val) {
        return s->get_sandboxed_pointer<T>(&val);
    }
    static inline T unsandbox(WasmSandbox *s, Type val) {
        return *(T)s->get_unsandboxed_pointer<T>(val);
    }
};

template <typename T>
using SandboxedType = typename WasmType<T>::Type;

template <typename I, typename T, typename... U>
struct Apply;

template <size_t... I, typename T, typename... U>
struct Apply<std::index_sequence<I...>, T, U...> {
    using F = SandboxedType<T> (*)(void *, SandboxedType<U>...);
    template <typename A>
    static inline T apply(WasmSandbox *sandbox, void *func, A *args) {
        auto f = (F)func;
        return WasmType<T>::unsandbox(sandbox,
            f(sandbox->get_sandbox(), WasmType<U>::sandbox(sandbox, std::get<I>(*args))...));
    }
};

template <size_t... I, typename... U>
struct Apply<std::index_sequence<I...>, void, U...> {
    using F = void (*)(void *, SandboxedType<U>...);
    template <typename A>
    static inline void apply(WasmSandbox *sandbox, void *func, A *args) {
        auto f = (F)func;
        f(sandbox->get_sandbox(), WasmType<U>::sandbox(sandbox, std::get<I>(*args))...);
    }
};

template <bool C, typename T, typename... U>
struct CallImpl;

template <typename T, typename... U>
struct CallImpl<false, T, U...> {
    using Arg = std::tuple<U...>;
    static inline T call(WasmSandbox *sandbox, void *func, U... args) {
        Arg *arg = (Arg *)sandbox->malloc_for_arguments(sizeof(Arg));
        new (arg) Arg (args...);
        T ret = Apply<std::make_index_sequence<sizeof...(U)>, T, U...>::apply(sandbox, func, arg);
        sandbox->free_arguments(arg);
        return ret;
    }
};

template <typename... U>
struct CallImpl<false, void, U...> {
    using Arg = std::tuple<U...>;
    static inline void call(WasmSandbox *sandbox, void *func, U... args) {
        Arg *arg = (Arg *)sandbox->malloc_for_arguments(sizeof(Arg));
        new (arg) Arg (args...);
        Apply<std::make_index_sequence<sizeof...(U)>, void, U...>::apply(sandbox, func, arg);
        sandbox->free_arguments(arg);
    }
};

template <>
struct CallImpl<false, void> {
    static inline void call(WasmSandbox *sandbox, void *func) {
        ((void (*)(void *))func)(sandbox->get_sandbox());
    }
};

template <typename T>
struct CallImpl<false, T> {
    using F = SandboxedType<T> (*)(void *);
    static inline T call(WasmSandbox *sandbox, void *func) {
        return WasmType<T>::unsandbox(sandbox, ((F)func)(sandbox->get_sandbox()));
    }
};

template <typename T, typename... U>
struct CallImpl<true, T, U...> {
    using Arg = std::tuple<T, U...>;
    static inline T call(WasmSandbox *sandbox, void *func, U... args) {
        Arg *arg = (Arg *)sandbox->malloc_for_arguments(sizeof(Arg));
        new (arg) Arg (T(), args...);
        Apply<std::make_index_sequence<sizeof...(U) + 1>, void, T, U...>
            ::apply(sandbox, func, arg);
        T ret = std::get<0>(*arg);
        sandbox->free_arguments(arg);
        return ret;
    }
};

template <typename T>
struct CallImpl2;

template <typename T, typename... U>
struct CallImpl2<T (*)(U...)> {
    using Type = CallImpl<std::is_class_v<T>, T, U...>;
};

template <typename T>
using Call = typename CallImpl2<T>::Type;

template <typename T, typename... S>
static inline uint32_t get_type_index(WasmSandbox *sandbox) {
    wasm_rt_type_t types[] = {
        WasmType<S>::TypeId...,
        WasmType<T>::TypeId
    };
    uint32_t num_args, num_ret;
    if constexpr (std::is_class_v<T>) {
        num_args = sizeof...(S) + 1;
        num_ret = 0;
    } else {
        num_args = sizeof...(S);
        num_ret = !std::is_void_v<T>;
    }
    return sandbox->get_sandbox_functions()->lookup_wasm2c_func_index(sandbox->get_sandbox(),
        num_args, num_ret, types);
}

template <typename I, bool C, typename T, typename... U>
struct CallbackImpl;

template <size_t... I, typename T, typename... U>
struct CallbackImpl<std::index_sequence<I...>, false, T, U...> {
    using CallbackFunction = T (*)(U..., void *);
    static constexpr size_t TypeHash = type_hash<CallbackFunction>();
    using WasmFunction = SandboxedType<T> (*)(void *, SandboxedType<U>...);
    template <size_t J>
    static SandboxedType<T> handler(void *s, SandboxedType<U>... args) {
        WasmSandbox *sandbox = WasmSandbox::from_sandbox(s);
        auto &&[func, data] = sandbox->get_callback(TypeHash, J);
        if (!func) {
            return SandboxedType<T>();
        }
        return WasmType<T>::sandbox(sandbox,
            ((CallbackFunction)func)(WasmType<U>::unsandbox(sandbox, args)..., data));
    }
    static inline bool register_callback(WasmSandbox *sandbox, CallbackFunction func, void *data, uint32_t &res) {
        int cid = sandbox->register_callback((void *)func, TypeHash, data);
        constexpr static WasmFunction Table[] = {
            (handler<I>)...
        };
        if ((unsigned int)cid >= sizeof...(I) || cid < 0) {
            return false;
        }
        res = sandbox->register_wasm2c_callback((void *)Table[cid], get_type_index<T, U...>(sandbox));
        return true;
    }
};

template <size_t... I, typename... U>
struct CallbackImpl<std::index_sequence<I...>, false, void, U...> {
    using CallbackFunction = void (*)(U..., void *);
    static constexpr size_t TypeHash = type_hash<CallbackFunction>();
    using WasmFunction = void (*)(void *, SandboxedType<U>...);
    template <size_t J>
    static void handler(void *s, SandboxedType<U>... args) {
        WasmSandbox *sandbox = WasmSandbox::from_sandbox(s);
        auto &&[func, data] = sandbox->get_callback(TypeHash, J);
        if (!func) {
            return;
        }
        ((CallbackFunction)func)(WasmType<U>::unsandbox(sandbox, args)..., data);
    }
    static inline bool register_callback(WasmSandbox *sandbox, CallbackFunction func, void *data, uint32_t &res) {
        int cid = sandbox->register_callback((void *)func, TypeHash, data);
        constexpr static WasmFunction Table[] = {
            (handler<I>)...
        };
        if ((unsigned int)cid >= sizeof...(I) || cid < 0) {
            return false;
        }
        res = sandbox->register_wasm2c_callback((void *)Table[cid], get_type_index<void, U...>(sandbox));
        return true;
    }
};

template <size_t... I, typename T, typename... U>
struct CallbackImpl<std::index_sequence<I...>, true, T, U...> {
    using CallbackFunction = T (*)(U..., void *);
    static constexpr size_t TypeHash = type_hash<CallbackFunction>();
    using WasmFunction = void (*)(void *, uint32_t, SandboxedType<U>...);
    template <size_t J>
    static void handler(void *s, uint32_t p, SandboxedType<U>... args) {
        WasmSandbox *sandbox = WasmSandbox::from_sandbox(s);
        auto &&[func, data] = sandbox->get_callback(TypeHash, J);
        if (!func) {
            return;
        }
        *(T *)sandbox->get_unsandboxed_pointer<T>(p) =
            ((CallbackFunction)func)(WasmType<U>::unsandbox(sandbox, args)..., data);
    }
    static inline bool register_callback(WasmSandbox *sandbox, CallbackFunction func, void *data, uint32_t &res) {
        int cid = sandbox->register_callback((void *)func, TypeHash, data);
        constexpr static WasmFunction Table[] = {
            (handler<I>)...
        };
        if ((unsigned int)cid >= sizeof...(I) || cid < 0) {
            return false;
        }
        res = sandbox->register_wasm2c_callback((void *)Table[cid], get_type_index<T, U...>(sandbox));
        return true;
    }
};


template <size_t N, typename T>
struct CallbackImpl2;

template <size_t N, typename T, typename... U>
struct CallbackImpl2<N, T (*)(U...)> {
    using Type = CallbackImpl<std::make_index_sequence<N>, std::is_class_v<T>, T, U...>;
};

template <size_t N, typename T>
using Callback = typename CallbackImpl2<N, T>::Type;
}

#define wasm_call(sandbox, type, func, ...) \
    rlbox_wasm::Call<type>::call(sandbox, func, ##__VA_ARGS__)
