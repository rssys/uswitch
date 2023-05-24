#ifndef RLBOX_API_PROCESS
#define RLBOX_API_PROCESS

#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <utility>
#include <stdint.h>
#include <mutex>
#include <limits>
#include <map>
#include <algorithm>
#include "ProcessSandbox.h"

namespace RLBox_Process_detail {
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

	template<typename TProcSandbox, typename Ret, typename... Rest>
	Ret(*injectSandboxParamInFnType_helper(Ret(*) (Rest...)))(TProcSandbox*, Rest...);

	template <typename TProcSandbox, typename T>
	using injectSandboxParamInFnType = decltype(injectSandboxParamInFnType_helper<TProcSandbox>(std::declval<T>()));
};

#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

template<typename TProcSandbox>
class RLBox_Process
{
private:
	static thread_local RLBox_Process* dynLib_SavedState;
	static std::mutex sandboxListMutex;
	static std::vector<TProcSandbox*> sandboxList;
	static std::map<std::string, int> sandboxMap;
	std::mutex callbackMutex;
	std::map<void*, void*> callbackKVMap;
	void* libHandle = nullptr;
	TProcSandbox* procSandbox = nullptr;
	int pushPopCount = 0;

	static inline size_t getTotalMemoryHelper()
	{
		#if defined(_M_IX86) || defined(__i386__)
			return 1024ull * 1024ull * 256ull;
		#elif defined(_M_X64) || defined(__x86_64__)
			return 1024ull * 1024ull * 1024ull * 4ull;
		#else
			#error Unsupported platform!
		#endif
	}
public:
	inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		//dlopen with null pointer points to the current app
		libHandle = dlopen(nullptr, RTLD_LAZY);
		if(!libHandle)
		{
			printf("Could not open symbol table of my app\n");
			abort();
		}
		
		procSandbox = new TProcSandbox(libraryPath, 9999 /* maincore: special marker for don't change */, 3 /* sbox_process_core */);
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		sandboxList.push_back(procSandbox);
	}

	inline void impl_DestroySandbox()
	{
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		sandboxList.erase(std::remove(sandboxList.begin(), sandboxList.end(), procSandbox), sandboxList.end());
		procSandbox->destroySandbox();
	}

	inline TProcSandbox* impl_getSandbox()
	{
		return procSandbox;
	}

	inline void* impl_mallocInSandbox(size_t size)
	{
		return procSandbox->mallocInSandbox(size);
	}

	//parameter val is a sandboxed pointer
	inline void impl_freeInSandbox(void* val)
	{
		procSandbox->freeInSandbox(val);
	}

	inline size_t impl_getTotalMemory()
	{
		return getTotalMemoryHelper();
	}

	inline char* impl_getMaxPointer()
	{
		auto base = (uintptr_t) procSandbox->getSandboxMemoryBase();
		auto ending = base + getTotalMemoryHelper() - 1;
		return (char*)ending;
	}

	inline void* impl_pushStackArr(size_t size)
	{
		pushPopCount++;
		return procSandbox->mallocInSandbox(size);
	}

	inline void impl_popStackArr(void* ptr, size_t size)
	{
		pushPopCount--;
		if(pushPopCount < 0)
		{
			printf("Error - RLBox_Process popCount was negative.\n");
			abort();
		}
		return procSandbox->freeInSandbox(ptr);
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
			//Hard to find if this is a valid func ptr in process, so just be conservative
			return false;
		} else {
			return impl_isPointerInSandboxMemoryOrNull(p);
		}
	}

	inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		auto base = (uintptr_t) procSandbox->getSandboxMemoryBase();
		auto ending = base + getTotalMemoryHelper() - 1;
		auto pVal = (uintptr_t) p;
		return (pVal == 0) || (base <= pVal && pVal <= ending);
	}

	inline bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return (p == nullptr) || (!impl_isPointerInSandboxMemoryOrNull(p));
	}

	template<typename T>
	static inline T* impl_pointerIncrement(T* p, int64_t increment)
	{
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		for(TProcSandbox* sandbox : sandboxList)
		{
			size_t memSize = getTotalMemoryHelper();
			uintptr_t base = (uintptr_t) sandbox->getSandboxMemoryBase();
			uintptr_t pVal = (uintptr_t) const_cast<void*>((const void*)p);
			if(pVal >= base && pVal < (base + memSize))
			{
				auto ret = p + increment;
				uintptr_t retv = (uintptr_t) const_cast<void*>((const void*)ret);
				if(retv >= base && retv < (base + memSize))
				{
					return ret;
				}
				else
				{
					printf("Incrementing address %p resulted in an out of bounds\n", (void*)retv);
					abort();
				}
			}
		}
		printf("Could not find sandbox for address: %p\n", const_cast<void*>((const void*)p));
		abort();
	}

	template<typename TRet, typename... TArgs>
	inline void* impl_RegisterCallback(void* key, void* callback, void* state)
	{
		using FuncType = TRet(*)(TArgs...);
		auto ret = procSandbox->template registerCallback<FuncType>((FuncType)callback, state);
		{
			std::lock_guard<std::mutex> lock(callbackMutex);
			callbackKVMap[key] =  const_cast<void*>((const void*)ret);
		}
		return const_cast<void*>((const void*)ret);
	}

	template<typename TFunc>
	inline void impl_UnregisterCallback(void* key)
	{
		void* cb = nullptr;
		{
			std::lock_guard<std::mutex> lock(callbackMutex);
			auto iter = callbackKVMap.find(key);
			if(iter == callbackKVMap.end())
			{
				abort();
			}
			cb = iter->second;
			callbackKVMap.erase(iter);
		}

		auto cbCast = (TFunc*) cb;
		procSandbox->unregisterCallback(cbCast);
	}

	inline void* impl_LookupSymbol(const char* name, bool forSandboxFunction)
	{
		if(!forSandboxFunction) {
			std::string convertedName = "ProcessSandbox_";
			convertedName += name;
			auto ret = dlsym(libHandle, convertedName.c_str());
			if(!ret)
			{
				printf("Symbol not found: %s. Error %s.\n", name, dlerror());
				abort();
			}
			return ret;
		} else {
			size_t len = strlen(name) + 1;
			auto copiedName = (char*) procSandbox->mallocInSandbox(len);
			strcpy(copiedName, name);
			void* ret = procSandbox->inv_invokeDlSym(copiedName);
			procSandbox->freeInSandbox(copiedName);
			return ret;
		}
	}

	template <typename T, typename ... TArgs>
	RLBox_Process_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
	{
		auto castPointer = (RLBox_Process_detail::injectSandboxParamInFnType<TProcSandbox, T*>) (uintptr_t) fnPtr;
		dynLib_SavedState = this;
		return (*castPointer)(procSandbox, params...);
	}

	template <typename T, typename ... TArgs>
	RLBox_Process_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
	{
		return impl_InvokeFunction(fnPtr, params...);
	}
};

template<typename TProcSandbox>
thread_local RLBox_Process<TProcSandbox>* RLBox_Process<TProcSandbox>::dynLib_SavedState = nullptr;

template<typename TProcSandbox>
std::mutex RLBox_Process<TProcSandbox>::sandboxListMutex __attribute__((weak));

template<typename TProcSandbox>
std::vector<TProcSandbox*> RLBox_Process<TProcSandbox>::sandboxList __attribute__((weak));


#undef ENABLE_IF

#endif