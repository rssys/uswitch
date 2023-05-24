#pragma once
#include <unordered_map>
#include <mutex>
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <unistd.h>
#include <fcntl.h>

static inline std::string getLibraryPath(const std::string &lib) {
  char buf[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
  if (len == -1) {
    return "";
  }
  std::string path(buf, len);
  size_t pos = path.rfind('/');
  if (pos == std::string::npos) {
    return "";
  }
  path.resize(pos + 1);
  return path + lib;
}

template <typename T, int SandboxLimits, int SoftLimits>
class SandboxManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<T>> sandboxes;
    std::recursive_mutex mutex;
    bool isSandboxPerInstance;
    bool sandboxEnforceLimits;
    int count;
public:
    SandboxManager() {
      count = 0;
      const char *env = PR_GetEnv("MOZ_RLBOX_PER_INSTANCE");
      isSandboxPerInstance = env && strcmp(env, "1") == 0;
      env = PR_GetEnv("MOZ_RLBOX_SANDBOX_LIMIT");
      sandboxEnforceLimits = env && strcmp(env, "1") == 0;
      wasm_sandbox_cleaner.add_cleaner([this] {
        sandboxes.clear();
      });
    }

    ~SandboxManager() {
    }

    inline std::shared_ptr<T> createSandbox(const std::string &name) {
      if (isSandboxPerInstance || name.empty()) {
        T *sandbox = new T;
        return std::shared_ptr<T>(sandbox);
      }

      std::lock_guard<std::recursive_mutex> lock(mutex);

      auto it = sandboxes.find(name);
      if (it != sandboxes.end()) {
        return it->second;
      }
      T *sandbox = new T;
      std::shared_ptr<T> ret;
      if (sandboxEnforceLimits) {
        ret = std::shared_ptr<T>(sandbox, [this] (T *sandbox) {
          delete sandbox;
          std::lock_guard<std::recursive_mutex> lock(mutex);
          if (sandboxEnforceLimits && sandboxes.size() > SoftLimits) {
            sandboxes.erase(sandboxes.begin());
          }
        });
      } else {
        ret = std::shared_ptr<T>(sandbox);
      }
      sandboxes[name] = ret;
      return ret;
    }
};