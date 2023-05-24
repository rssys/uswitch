#pragma once
#include <unordered_map>
#include <set>
#include <mutex>
#include <memory>
#include <string>
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
    std::unordered_map<std::string, std::weak_ptr<T>> sandboxes;
    std::vector<std::weak_ptr<T>> availSandboxes;
    std::unordered_map<T *, std::shared_ptr<T>> inUseSandboxes;
    std::mutex mutex;
    bool isSandboxPerInstance;
    bool resetSandbox;
    bool sandboxEnforceLimits;
    int count;
    static USwitchSandboxResource *factory() {
      return new T;
    }
public:
    SandboxManager() {
      count = 0;
      const char *env = PR_GetEnv("MOZ_RLBOX_PER_INSTANCE");
      isSandboxPerInstance = env && strcmp(env, "1") == 0;
      env = PR_GetEnv("MOZ_RLBOX_RESET_SANDBOX");
      resetSandbox = env && strcmp(env, "1") == 0;
    }

    ~SandboxManager() {
    }

    inline std::shared_ptr<T> createSandbox(const std::string &name) {
      bool perInstance = isSandboxPerInstance || name.empty();
      if (resetSandbox && perInstance) {
        std::shared_ptr<T> ptr;
        {
          std::lock_guard<std::mutex> lock(mutex);
          while (!availSandboxes.empty()) {
            ptr = availSandboxes.back().lock();
            availSandboxes.pop_back();
            if (ptr) {
              break;
            }
          }
        }
        if (ptr) {
          if (ptr->rlbox_sandbox->getSandbox()->reset()) {
            T *p = ptr.get();
            {
              std::lock_guard<std::mutex> lock(mutex);
              inUseSandboxes[p] = ptr;
            }
            return std::shared_ptr<T>(p, [this] (T *s) {
              std::lock_guard<std::mutex> lock(mutex);
              auto it = inUseSandboxes.find(s);
              if (it == inUseSandboxes.end()) {
                return;
              }
              availSandboxes.emplace_back(it->second);
              inUseSandboxes.erase(it);
            });
          }
        }
        //static int count = 0;
        //++count;
        //printf("count: %d\n", count);
        ptr = std::static_pointer_cast<T>(uswitch_sandbox_manager.create_sandbox(factory));
        T *p = ptr.get();
        {
          std::lock_guard<std::mutex> lock(mutex);
          inUseSandboxes[p] = ptr;
        }
        return std::shared_ptr<T>(p, [this] (T *s) {
          std::lock_guard<std::mutex> lock(mutex);
          auto it = inUseSandboxes.find(s);
          if (it == inUseSandboxes.end()) {
            return;
          }
          availSandboxes.emplace_back(it->second);
          inUseSandboxes.erase(it);
        });
      }
      if (perInstance) {
        uswitch_sandbox_semaphore.wait();
        T *sandbox = new T;
        return std::shared_ptr<T>(sandbox, [this] (T *sandbox) {
          delete sandbox;
          uswitch_sandbox_semaphore.signal();
        });
      }

      std::lock_guard<std::mutex> lock(mutex);

      auto it = sandboxes.find(name) ;
      if (it != sandboxes.end()) {
        std::shared_ptr<T> sandbox = it->second.lock();
        if (sandbox) {
          return sandbox;
        } else {
          sandboxes.erase(it);
        }
      }
      auto ret = std::static_pointer_cast<T>(uswitch_sandbox_manager.create_sandbox(factory));
      sandboxes[name] = ret;
      return ret;
    }
};