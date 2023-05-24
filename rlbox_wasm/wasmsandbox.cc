#include <string>
#include <vector>
#include <mutex>
#include <cstdio>
#include <cinttypes>
#include <dlfcn.h>
#include "wasmsandbox.h"
#include "wasm2c/wasm-rt.h"

static std::once_flag wasm2c_runtime_initialized;
struct WasmSandbox::wasm_rt_memory_t : public ::wasm_rt_memory_t {};

WasmSandbox::WasmSandbox(const char *library_, const char *prefix_)
    : has_init(false), library(library_), prefix(prefix_), max_heap_size(4096l << 20),
      max_pages(4096l << 4), sandbox(nullptr), handle(nullptr), sandbox_info(nullptr) {}

WasmSandbox::~WasmSandbox() {
    if (sandbox_info) {
        if (sandbox_info->destroy_wasm2c_sandbox && sandbox) {
            sandbox_info->destroy_wasm2c_sandbox(sandbox);
        }
    }
    if (handle) {
        dlclose(handle);
    }
}

bool WasmSandbox::init() {
    if (has_init) {
        return false;
    }
    handle = dlopen(library.c_str(), RTLD_LAZY);
    if (!handle) {
        return false;
    }
    const std::string &func_name = prefix + "get_wasm2c_sandbox_info";
    auto get_info_func = (wasm2c_sandbox_funcs_t (*)())dlsym(handle, func_name.c_str());
    if (!get_info_func) {
        return false;
    }
    sandbox_info.reset(new wasm2c_sandbox_funcs_t(get_info_func()));
    std::call_once(wasm2c_runtime_initialized, sandbox_info->wasm_rt_sys_init);
    sandbox = sandbox_info->create_wasm2c_sandbox(max_pages);
    if (!sandbox) {
        return false;
    }
    *(WasmSandbox **)((uint8_t *)sandbox + 8) = this;
    if (!sandbox_info->wasm_rt_sys_init || !sandbox_info->lookup_wasm2c_nonfunc_export) {
        return false;
    }
    sandbox_memory_info = (wasm_rt_memory_t *)sandbox_info->lookup_wasm2c_nonfunc_export(sandbox, "w2c_memory");
    if (!sandbox_memory_info) {
        return false;
    }
    memory_base = sandbox_memory_info->data;
    if ((uintptr_t)memory_base & 0xffffffffl) {
        // we force alignment of 4GB
        return false;
    }
    malloc_ptr = get_symbol_addr("malloc");
    free_ptr = get_symbol_addr("free");
    if (!malloc_ptr || !free_ptr) {
        return false;
    }
    arg_mem = malloc_in_sandbox_unsandboxed(4096);
    has_init = true;
    return true;
}

void *WasmSandbox::get_symbol_addr(const std::string &symbol) {
    if (!handle) {
        return nullptr;
    }
    return dlsym(handle, ("w2c_" + symbol).c_str());
}

uint32_t WasmSandbox::malloc_in_sandbox(size_t size) {
    auto malloc_wasm = (uint32_t (*)(void *, uint32_t))malloc_ptr;
    return malloc_wasm(sandbox, (uint32_t)size);
}

void WasmSandbox::free_in_sandbox(uint32_t ptr) {
    auto free_wasm = (void (*)(void *, uint32_t))free_ptr;
    free_wasm(sandbox, ptr);
}

int WasmSandbox::register_callback(void *handler, size_t type_hash, void *data) {
    int cid = callback_handlers[type_hash].size();
    callback_handlers[type_hash].push_back(std::make_pair(handler, data));
    return cid;
}

uint32_t WasmSandbox::register_wasm2c_callback(void *handler, uint32_t type) {
    return sandbox_info->add_wasm2c_callback(sandbox, type, handler, WASM_RT_EXTERNAL_FUNCTION);
}

std::pair<void *, void *> WasmSandbox::get_callback(size_t type_hash, int cid) {
    auto it = callback_handlers.find(type_hash);
    if (it == callback_handlers.end()) {
        return std::make_pair(nullptr, nullptr);
    }
    std::vector<std::pair<void *, void*>> &callbacks = it->second;
    if (cid >= callbacks.size()) {
        return std::make_pair(nullptr, nullptr);
    }
    return callbacks[cid];
}

WasmSandboxCleaner::WasmSandboxCleaner() {}

void WasmSandboxCleaner::add_cleaner(const std::function<void ()> &cleaner) {
    std::lock_guard<std::mutex> lock(mutex);
    cleaners.push_back(cleaner);
}

void WasmSandboxCleaner::clean() {
    std::lock_guard<std::mutex> lock(mutex);
    for (auto &&cleaner : cleaners) {
        cleaner();
    }
}

WASM_PUBLIC WasmSandboxCleaner wasm_sandbox_cleaner;
