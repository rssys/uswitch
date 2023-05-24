#pragma once
#include <type_traits>
#include <tuple>
#include <functional>
#include <cstring>
#include "uswitch.h"

#define FLATTEN __attribute__((flatten))
#define ALWAYS_INLINE __attribute__((always_inline))

namespace uswitch {
template <typename T>
struct MemberFunctionPtrType;

template <typename T, typename S, typename... U>
struct MemberFunctionPtrType<T (S::*)(U...) const> {
    using Type = T (*)(U...);
};

template <typename T, typename S, typename... U>
struct MemberFunctionPtrType<T (S::*)(U...)> {
    using Type = T (*)(U...);
};

template <typename T>
ALWAYS_INLINE NOCANARY inline T &&forward(typename std::remove_reference<T>::type &t) noexcept {
    return static_cast<T &&>(t);
}

template <typename T>
ALWAYS_INLINE NOCANARY inline T &&forward(typename std::remove_reference<T>::type &&t) noexcept {
    return static_cast<T &&>(t);
}

template <size_t I, typename T>
struct Leaf {
    template <typename U>
    ALWAYS_INLINE explicit Leaf(U &&val_) : val(forward<U>(val_)) {}
    T val;
};

template <typename T, typename... U>
struct TupleImpl;

template <size_t... I, typename... U>
struct TupleImpl<std::index_sequence<I...>, U...> : public Leaf<I, U>... {
    template <typename... S>
    ALWAYS_INLINE NOCANARY inline TupleImpl(S&&... args) : Leaf<I, U>(forward<S>(args))... {}
    template <typename T, typename... S>
    ALWAYS_INLINE NOCANARY inline T apply(T (*func)(S...)) {
        return func(forward<U>(((Leaf<I, U> *)this)->val)...);
    }
    template <typename T, typename... S>
    ALWAYS_INLINE NOCANARY inline T apply2(uswctx_t ctx, void *data, T (*func)(S...)) {
        return func(ctx, data, forward<U>(((Leaf<I, U> *)this)->val)...);
    }
};

template <typename... T>
using Tuple = TupleImpl<std::make_index_sequence<sizeof...(T)>, T...>;

template <typename F>
struct Call {
    using T = decltype(&F::operator());
    using S = typename MemberFunctionPtrType<T>::Type;
    template <typename... U>
    static inline int call(U&&... args) {
        return Call<S>::call(forward<U>(args)...);
    }
};

template <typename T, typename... U>
struct Call<T (*)(U...)> {
    using Handler = T (*)(U...);
    struct Arg {
        Handler handler;
        T ret;
        std::tuple<U...> args;
        Arg(Handler handler_, U... args_) : handler(handler_), args(args_...) {}
    };
    static NOCANARY void entry(void *data) {
        Arg *arg = (Arg *)data;
        arg->ret = std::apply(arg->handler, arg->args);
    }
    static inline int call(uswctx_t ctx, Handler func, T &ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (func, args...);
        int res = uswitch_call(ctx, entry, arg);
        ret = ((Arg *)arg)->ret;
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
    static inline int call(uswctx_t ctx, Handler func, T *ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (func, args...);
        int res = uswitch_call(ctx, entry, arg);
        if (ret) {
            *ret = ((Arg *)arg)->ret;
        }
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
};

template <typename... U>
struct Call<void (*)(U...)> {
    using Handler = void (*)(U...);
    struct Arg {
        Handler handler;
        std::tuple<U...> args;
        Arg(Handler handler_, U... args_) : handler(handler_), args(args_...) {}
    };
    static NOCANARY void entry(void *data) {
        Arg *arg = (Arg *)data;
        std::apply(arg->handler, arg->args);
    }
    static inline int call(uswctx_t ctx, Handler func, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (func, args...);
        int res = uswitch_call(ctx, entry, arg);
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
    static inline int call(uswctx_t ctx, Handler func, void *ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (func, args...);
        int res = uswitch_call(ctx, entry, arg);
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
};

template <>
struct Call<void (*)(void)> {
    using Handler = void (*)(void);
    static NOCANARY void entry(void *data) {
        Handler handler = (Handler)data;
        handler();
    }
    static inline int call(uswctx_t ctx, Handler func) {
        return uswitch_call(ctx, entry, (void *)func);
    }
    static inline int call(uswctx_t ctx, Handler func, void *ret) {
        return uswitch_call(ctx, entry, (void *)func);
    }
};

template <auto F>
struct CallStatic;

template <typename T, typename... U, T (*F)(U...)>
struct CallStatic<F> {
    struct Arg {
        T ret;
        std::tuple<U...> args;
        Arg(U... args_) : args(args_...) {}
    };
    static NOCANARY void entry(void *data) {
        Arg *arg = (Arg *)data;
        arg->ret = std::apply(F, arg->args);
    }
    static inline int call(uswctx_t ctx, T &ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (args...);
        int res = uswitch_call(ctx, entry, arg);
        ret = ((Arg *)arg)->ret;
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
    static inline int call(uswctx_t ctx, T *ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (args...);
        int res = uswitch_call(ctx, entry, arg);
        if (ret) {
            *ret = ((Arg *)arg)->ret;
        }
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
};

template <typename... U, void (*F)(U...)>
struct CallStatic<F> {
    using Arg = std::tuple<U...>;
    static NOCANARY void entry(void *data) {
        Arg *arg = (Arg *)data;
        std::apply(F, *arg);
    }
    static inline int call(uswctx_t ctx, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (args...);
        int res = uswitch_call(ctx, entry, arg);
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
    static inline int call(uswctx_t ctx, void *ret, U... args) {
        void *arg = uswitch_push_stack(ctx, sizeof(Arg));
        if (!arg) {
            return -1;
        }
        new (arg) Arg (args...);
        int res = uswitch_call(ctx, entry, arg);
        uswitch_push_stack(ctx, -sizeof(Arg));
        return res;
    }
};

template <void (*F)(void)>
struct CallStatic<F> {
    static NOCANARY void entry(void *data) {
        F();
    }
    static inline int call(uswctx_t ctx) {
        return uswitch_call(ctx, entry, nullptr);
    }
    static inline int call(uswctx_t ctx, void *ret) {
        return uswitch_call(ctx, entry, nullptr);
    }
};

template <typename F>
struct DirectlyPassableArgument;

template <typename T, typename... U>
struct DirectlyPassableArgument<T (*)(U...)> {
    static constexpr bool Value = false;
    //static constexpr bool Value = std::is_trivially_copyable<T>::value && sizeof(T) <= sizeof(long) &&
    //    (std::is_trivially_copyable<U>::value && ...) &&
    //    ((sizeof(U) <= sizeof(long)) && ...) && sizeof...(U) <= 6;
};

template <typename... U>
struct DirectlyPassableArgument<void (*)(U...)> {
    static constexpr bool Value = false;
    //static constexpr bool Value = (std::is_trivially_copyable<U>::value && ...) &&
    //    ((sizeof(U) <= sizeof(long)) && ...) && sizeof...(U) <= 6;
};

template <typename F, bool DirectlyPassable>
struct CallbackImpl;

template <typename T, typename... U>
struct CallbackImpl<T (*)(U...), false> {
    struct Arg {
        T ret;
        Tuple<U...> args;
        Arg(U... args_) : args(args_...) {}
    };
    using CallbackFunction = T (*)(U...);
    static ALWAYS_INLINE NOCANARY inline T callback(int callback_id, U... args) {
        Arg arg(args...);
        long ret;
        if (uswitch_callback(callback_id, &ret, (long)&arg, 0, 0, 0, 0, 0) == -1) {
            return arg.ret;
        }
        return arg.ret;
    }
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        Arg *arg = (Arg *)arg1;
        if (uswitch_check_stack_pointer(ctx, (void *)arg, sizeof(Arg)) == -1) {
            return -1;
        }
        CallbackFunction func = (CallbackFunction)data1;
        arg->ret = arg->args.apply(func);
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func) {
        return uswitch_register_callback(ctx, handler, (void *)func, nullptr);
    }
};

template <typename... U>
struct CallbackImpl<void (*)(U...), false> {
    using Arg = Tuple<U...>;
    using CallbackFunction = void (*)(U...);
    static ALWAYS_INLINE NOCANARY inline void callback(int callback_id, U... args) {
        Arg arg(args...);
        uswitch_callback(callback_id, nullptr, (long)&arg, 0, 0, 0, 0, 0);
    }
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        Arg *arg = (Arg *)arg1;
        if (uswitch_check_stack_pointer(ctx, (void *)arg, sizeof(Arg)) == -1) {
            return -1;
        }
        CallbackFunction func = (CallbackFunction)data1;
        arg->apply(func);
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func) {
        return uswitch_register_callback(ctx, handler, (void *)func, nullptr);
    }
};

template <>
struct CallbackImpl<void (*)(void), false> {
    using CallbackFunction = void (*)(void);
    static ALWAYS_INLINE NOCANARY inline void callback(int callback_id) {
        uswitch_callback(callback_id, nullptr, 0, 0, 0, 0, 0, 0);
    }
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        CallbackFunction func = (CallbackFunction)data1;
        func();
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func) {
        return uswitch_register_callback(ctx, handler, (void *)func, nullptr);
    }
};

template <typename F>
struct CallbackImpl2 {
    using T = decltype(&F::operator());
    using S = typename MemberFunctionPtrType<T>::Type;
    using Type = CallbackImpl<S, DirectlyPassableArgument<S>::Value>;
};

template <typename T, typename... U>
struct CallbackImpl2<T (*)(U...)> {
    using Type = CallbackImpl<T (*)(U...), DirectlyPassableArgument<T (*)(U...)>::Value>;
};

template <typename F>
using Callback = typename CallbackImpl2<F>::Type;

template <int CallbackId, typename F, bool DirectlyPassable>
struct CallbackStaticImpl;

template <int CallbackId, typename T, typename... U>
struct CallbackStaticImpl<CallbackId, T (*)(U...), false> {
    struct Arg {
        T ret;
        Tuple<U...> args;
        Arg(U... args_) : args(args_...) {}
    };
    using CallbackFunction = T (*)(U...);
    static ALWAYS_INLINE NOCANARY inline T callback(U... args) {
        Arg arg(args...);
        long ret;
        if (uswitch_callback(CallbackId, &ret, (long)&arg, 0, 0, 0, 0, 0) == -1) {
            return arg.ret;
        }
        return arg.ret;
    }
};

template <int CallbackId, typename... U>
struct CallbackStaticImpl<CallbackId, void (*)(U...), false> {
    using Arg = Tuple<U...>;
    using CallbackFunction = void (*)(U...);
    static ALWAYS_INLINE NOCANARY inline void callback(U... args) {
        Arg arg(args...);
        uswitch_callback(CallbackId, nullptr, (long)&arg, 0, 0, 0, 0, 0);
    }
};

template <int CallbackId>
struct CallbackStaticImpl<CallbackId, void (*)(void), false> {
    using CallbackFunction = void (*)(void);
    static ALWAYS_INLINE NOCANARY inline void callback() {
        uswitch_callback(CallbackId, nullptr, 0, 0, 0, 0, 0, 0);
    }
};

template <int CallbackId, typename F>
struct CallbackStaticImpl2 {
    using T = decltype(&F::operator());
    using S = typename MemberFunctionPtrType<T>::Type;
    using Type = CallbackStaticImpl<CallbackId, S, DirectlyPassableArgument<S>::Value>;
};

template <int CallbackId, typename T, typename... U>
struct CallbackStaticImpl2<CallbackId, T (*)(U...)> {
    using Type = CallbackStaticImpl<CallbackId, T (*)(U...), DirectlyPassableArgument<T (*)(U...)>::Value>;
};

template <int CallbackId, typename F>
using CallbackStatic = typename CallbackStaticImpl2<CallbackId, F>::Type;

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

template <int TypedCallbackId, typename F, bool DirectlyPassable>
struct CallbackTypedStaticImpl;

template <int TypedCallbackId, typename T, typename... U>
struct CallbackTypedStaticImpl<TypedCallbackId, T (*)(U...), false> {
    struct Arg {
        T ret;
        Tuple<U...> args;
        Arg(U... args_) : args(args_...) {}
    };
    using CallbackFunction = T (*)(U...);
    static NOCANARY FLATTEN inline T callback(U... args) {
        Arg arg(args...);
        long ret;
        if (uswitch_callback_typed(TypeHash<CallbackFunction>::Value, TypedCallbackId, &ret,
            (long)&arg, 0, 0, 0, 0, 0) == -1) {
            return arg.ret;
        }
        return arg.ret;
    }
};

template <int TypedCallbackId, typename... U>
struct CallbackTypedStaticImpl<TypedCallbackId, void (*)(U...), false> {
    using Arg = Tuple<U...>;
    using CallbackFunction = void (*)(U...);
    static ALWAYS_INLINE NOCANARY inline void callback(U... args) {
        Arg arg(args...);
        uswitch_callback_typed(TypeHash<CallbackFunction>::Value, TypedCallbackId, nullptr,
            (long)&arg, 0, 0, 0, 0, 0);
    }
};

template <int TypedCallbackId>
struct CallbackTypedStaticImpl<TypedCallbackId, void (*)(void), false> {
    using CallbackFunction = void (*)(void);
    static ALWAYS_INLINE NOCANARY inline void callback() {
        uswitch_callback_typed(TypeHash<CallbackFunction>::Value, TypedCallbackId, nullptr, 0, 0, 0, 0, 0, 0);
    }
};

template <int TypedCallbackId, typename F>
struct CallbackTypedStaticImpl2 {
    using T = decltype(&F::operator());
    using S = typename MemberFunctionPtrType<T>::Type;
    using Type = CallbackTypedStaticImpl<TypedCallbackId, S, DirectlyPassableArgument<S>::Value>;
};

template <int TypedCallbackId, typename T, typename... U>
struct CallbackTypedStaticImpl2<TypedCallbackId, T (*)(U...)> {
    using Type = CallbackTypedStaticImpl<TypedCallbackId, T (*)(U...), DirectlyPassableArgument<T (*)(U...)>::Value>;
};

template <int TypedCallbackId, typename F>
using CallbackTypedStatic = typename CallbackTypedStaticImpl2<TypedCallbackId, F>::Type;

template <typename F, bool DirectlyPassable>
struct CallbackWithExtraDataImpl;

template <typename T, typename... U>
struct CallbackWithExtraDataImpl<T (*)(uswctx_t, void *, U...), false> {
    struct Arg {
        T ret;
        Tuple<U...> args;
        Arg(U... args_) : args(args_...) {}
    };
    using CallbackFunction = T (*)(uswctx_t, void *, U...);
    using CallbackFunctionWithoutData = T (*)(U...);
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        Arg *arg = (Arg *)arg1;
        if (uswitch_check_stack_pointer(ctx, (void *)arg, sizeof(Arg)) == -1) {
            return -1;
        }
        CallbackFunction func = (CallbackFunction)data1;
        arg->ret = arg->args.apply2(ctx, data2, func);
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func, void *data) {
        return uswitch_register_callback(ctx, handler, (void *)func, data);
    }
};

template <typename... U>
struct CallbackWithExtraDataImpl<void (*)(uswctx_t, void *, U...), false> {
    using Arg = Tuple<U...>;
    using CallbackFunction = void (*)(uswctx_t, void *, U...);
    using CallbackFunctionWithoutData = void (*)(U...);
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        Arg *arg = (Arg *)arg1;
        if (uswitch_check_stack_pointer(ctx, (void *)arg, sizeof(Arg)) == -1) {
            return -1;
        }
        CallbackFunction func = (CallbackFunction)data1;
        arg->apply2(ctx, data2, func);
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func, void *data) {
        return uswitch_register_callback(ctx, handler, (void *)func, data);
    }
};

template <>
struct CallbackWithExtraDataImpl<void (*)(uswctx_t, void *), false> {
    using CallbackFunction = void (*)(uswctx_t, void *);
    using CallbackFunctionWithoutData = void (*)();
    static long handler(uswctx_t ctx, void *data1, void *data2, long arg1, long arg2, long arg3,
        long arg4, long arg5, long arg6) {
        CallbackFunction func = (CallbackFunction)data1;
        func(ctx, data2);
        return 0;
    }
    static inline int register_callback(uswctx_t ctx, CallbackFunction func, void *data) {
        return uswitch_register_callback(ctx, handler, (void *)func, data);
    }
};

template <typename F>
struct CallbackWithExtraDataImpl2 {
    using T = decltype(&F::operator());
    using S = typename MemberFunctionPtrType<T>::Type;
    using Type = CallbackWithExtraDataImpl<S, DirectlyPassableArgument<S>::Value>;
};

template <typename T, typename... U>
struct CallbackWithExtraDataImpl2<T (*)(U...)> {
    using Type = CallbackWithExtraDataImpl<T (*)(U...), DirectlyPassableArgument<T (*)(U...)>::Value>;
};

template <typename F>
using CallbackWithExtraData = typename CallbackWithExtraDataImpl2<F>::Type;

template <typename F, int N, typename T>
struct RegisterCallbackGetFPImpl;

template <typename F, int N, size_t... I>
struct RegisterCallbackGetFPImpl<F, N, std::index_sequence<I...>> {
    using T = typename Callback<F>::CallbackFunction;
    static inline T register_callback(uswctx_t ctx, F func, int *cid = nullptr) {
        int id = Callback<F>::register_callback(ctx, func);
        int tcbid = uswitch_get_typed_callback_id(ctx, type_hash<T>(), id);
        constexpr static T Table[] = {
            (uswitch::CallbackTypedStatic<I, T>::callback)...
        };
        if (tcbid >= N || tcbid == -1) {
            return nullptr;
        }
        if (cid) {
            *cid = id;
        }
        return Table[tcbid];
    }
};

template <typename F, int N, typename T>
struct RegisterCallbackGetFPWithExtraDataImpl;

template <typename F, int N, size_t... I>
struct RegisterCallbackGetFPWithExtraDataImpl<F, N, std::index_sequence<I...>> {
    using T = typename CallbackWithExtraData<F>::CallbackFunctionWithoutData;
    static inline T register_callback(uswctx_t ctx, F func, void *data, int *cid = nullptr) {
        int id = CallbackWithExtraData<F>::register_callback(ctx, func, data);
        int tcbid = uswitch_get_typed_callback_id(ctx, type_hash<T>(), id);
        constexpr static T Table[] = {
            (uswitch::CallbackTypedStatic<I, T>::callback)...
        };
        if (tcbid >= N || tcbid == -1) {
            return nullptr;
        }
        if (cid) {
            *cid = id;
        }
        return Table[tcbid];
    }
};

template <int N>
struct RegisterCallbackGetFP {
    template <typename F>
    static inline auto register_callback(uswctx_t ctx, F func, int *cid = nullptr) {
        return RegisterCallbackGetFPImpl<F, N, std::make_index_sequence<N>>::register_callback(ctx, func, cid);
    }
    template <typename F>
    static inline auto register_callback(uswctx_t ctx, void *data, F func, int *cid = nullptr) {
        return RegisterCallbackGetFPWithExtraDataImpl<F, N, std::make_index_sequence<N>>::register_callback(
            ctx, func, data, cid);
    }
};

}

template <typename T, typename... U>
inline int uswitch_call_dynamic(uswctx_t ctx, T func, U&&... args) {
    return uswitch::Call<T>::call(ctx, func, uswitch::forward<U>(args)...);
}

#define uswitch_call_static(ctx, func, ...) uswitch::CallStatic<func>::call( \
    (ctx), ##__VA_ARGS__)

template <typename T>
inline int uswitch_register_callback_dynamic(uswctx_t ctx, T func) {
    return uswitch::Callback<T>::register_callback(ctx, func);
}

template <typename T>
inline int uswitch_register_callback_dynamic(uswctx_t ctx, T func, void *data) {
    return uswitch::CallbackWithExtraData<T>::register_callback(ctx, func, data);
}

#define uswitch_callback_dynamic(callback_id, func_type, ...) uswitch::Callback<func_type>::callback( \
    (callback_id), ##__VA_ARGS__)

#define uswitch_callback_static(callback_id, func_type, ...) uswitch::CallbackStatic<(callback_id), func_type>::callback( \
    __VA_ARGS__)

template <int CallbackId, typename T>
constexpr auto uswitch_callback_function() {
    return uswitch::CallbackStatic<CallbackId, T>::callback;
}

#define uswitch_register_callback_get_fp(max_id, ...) \
    uswitch::RegisterCallbackGetFP<max_id>::register_callback(__VA_ARGS__)

inline int uswitch_call_without_mprotect(uswctx_t ctx, const std::function<void ()> &func) {
    return uswitch_call_without_mprotect(ctx, [] (void *data) {
        std::function<void ()> *func = (std::function<void ()> *)data;
        (*func)();
        delete func;
    }, new std::function<void ()>(func));
}
