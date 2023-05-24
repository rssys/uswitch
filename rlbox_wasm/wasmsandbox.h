#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <functional>
#include <type_traits>
#include <cinttypes>

#define WASM_PUBLIC __attribute__((visibility("default")))

struct wasm2c_sandbox_funcs_t;
class RLBox_Wasm;
class WasmSandbox {
public:
    void *custom_data;
    struct wasm_rt_memory_t;
    WASM_PUBLIC WasmSandbox(const char *library_, const char *prefix_);
    WASM_PUBLIC ~WasmSandbox();
    WASM_PUBLIC bool init();
    WASM_PUBLIC void *get_symbol_addr(const std::string &symbol);
    inline void *get_symbol_addr(const char *symbol) {
        return get_symbol_addr(std::string(symbol));
    }
    WASM_PUBLIC uint32_t malloc_in_sandbox(size_t size);
    WASM_PUBLIC void free_in_sandbox(uint32_t ptr);
    template <typename T>
    inline void *get_unsandboxed_pointer(uint32_t p) {
        if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
            return (void *)(uintptr_t)p;
        } else if (p == 0) {
            return nullptr;
        } else {
            return is_pointer_valid<T>(p) ? (void *)(memory_base + p) : nullptr;
        }
    }
    template <typename T>
    inline uint32_t get_sandboxed_pointer(void *p) {
        if constexpr (std::is_function_v<std::remove_pointer_t<T>>) {
            return (uint32_t)(uintptr_t)p;
        } else if (!p) {
            return 0;
        } else {
            return (uint32_t)((uint8_t *)p - memory_base);
        }
    }
    WASM_PUBLIC inline void *malloc_in_sandbox_unsandboxed(size_t size) {
        return get_unsandboxed_pointer<void *>(malloc_in_sandbox(size));
    }
    WASM_PUBLIC inline void free_in_sandbox_unsandboxed(void *ptr) {
        free_in_sandbox(get_sandboxed_pointer<void *>(ptr));
    }
    WASM_PUBLIC inline void *get_sandbox() {
        return sandbox;
    }
    template <typename T>
    inline bool is_pointer_valid(uint32_t p) {
        return p < max_heap_size;
    }
    WASM_PUBLIC int register_callback(void *handler, size_t type_hash, void *data);
    WASM_PUBLIC uint32_t register_wasm2c_callback(void *handler, uint32_t type);
    WASM_PUBLIC std::pair<void *, void *> get_callback(size_t type_hash, int cid);
    inline wasm2c_sandbox_funcs_t *get_sandbox_functions() {
        return sandbox_info.get();
    }
    static inline WasmSandbox *from_sandbox(void *sandbox) {
        return *(WasmSandbox **)((uint8_t *)sandbox + 8);
    }
    WASM_PUBLIC inline void *malloc_for_arguments(size_t size) {
        if (size < 4096) {
            return arg_mem;
        } else {
            return malloc_in_sandbox_unsandboxed(size);
        }
    }
    WASM_PUBLIC inline void free_arguments(void *ptr) {
        if (ptr != arg_mem) {
            free_in_sandbox_unsandboxed(ptr);
        }
    }
private:
    friend class RLBox_Wasm;
    bool has_init;
    std::string library;
    std::string prefix;
    void *handle;
    void *sandbox;
    uint8_t *memory_base;
    size_t max_heap_size;
    size_t max_pages;
    void *malloc_ptr;
    void *free_ptr;
    void *arg_mem;
    std::unique_ptr<wasm2c_sandbox_funcs_t> sandbox_info;
    std::unordered_map<size_t, std::vector<std::pair<void *, void *>>> callback_handlers;
    wasm_rt_memory_t *sandbox_memory_info;

};

class WasmSandboxCleaner {
public:
    WASM_PUBLIC WasmSandboxCleaner();
    WASM_PUBLIC void add_cleaner(const std::function<void ()> &cleaner);
    WASM_PUBLIC void clean();
private:
    std::mutex mutex;
    std::vector<std::function<void ()>> cleaners;
};

extern WASM_PUBLIC WasmSandboxCleaner wasm_sandbox_cleaner;