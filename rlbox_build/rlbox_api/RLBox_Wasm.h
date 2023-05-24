#ifndef RLBOX_API_WASM
#define RLBOX_API_WASM

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <mutex>
#include "wasm_sandbox.h"

class RLBox_Wasm
{
private:
	WasmSandbox* sandbox;
	static std::mutex sandboxListMutex;
	static std::vector<WasmSandbox*> sandboxList;
	std::mutex callbackMutex;
	std::mutex threadMutex;
	class WasmSandboxStateWrapper
	{
	public:
		RLBox_Wasm* sandbox;
		void* originalState;
		void* fnPtr;
		WasmSandboxCallback* registeredCallback;

		WasmSandboxStateWrapper(RLBox_Wasm* sandbox, void* originalState, void* fnPtr)
		{
			this->sandbox = sandbox;
			this->originalState = originalState;
			this->fnPtr = fnPtr;
		}
	};
	std::map<void*, WasmSandboxStateWrapper*> callbackSlotInfo;

	//https://stackoverflow.com/questions/23467635/is-there-a-variant-of-stdlock-guard-that-unlocks-at-construction-and-locks-at
	template <class T>
	class unlock_guard
	{
	public:
		unlock_guard(T& mutex) : mutex_(mutex) {
			mutex_.unlock();
		}

		~unlock_guard() {
			mutex_.lock();
		}

		unlock_guard(const unlock_guard&) = delete;
		unlock_guard& operator=(const unlock_guard&) = delete;

	private:
		T& mutex_;
	};

	template<typename TRet, typename... TArgs> 
	static TRet impl_CallbackReceiver(void* callbackState, TArgs... params)
	{
		WasmSandboxStateWrapper* callbackStateC = (WasmSandboxStateWrapper*) callbackState;
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) callbackStateC->fnPtr;
		unlock_guard<std::mutex> unlock(callbackStateC->sandbox->threadMutex);
		return fnPtr(params..., callbackStateC->originalState);
	}

	template<typename T, typename std::enable_if<std::is_function<T>::value>::type* = nullptr>
	static inline void* impl_GetSandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return sandbox->registerInternalCallback(ptr);
	}

	template<typename T, typename std::enable_if<!std::is_function<T>::value>::type* = nullptr>
	static inline void* impl_GetSandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return sandbox->getSandboxedPointer(ptr);
	}

	template<typename T, typename std::enable_if<std::is_function<T>::value>::type* = nullptr>
	static inline void* impl_GetUnsandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return (void*) sandbox->getUnsandboxedFuncPointer(const_cast<void*>((const void*)ptr));
	}

	template<typename T, typename std::enable_if<!std::is_function<T>::value>::type* = nullptr>
	static inline void* impl_GetUnsandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return (void*) sandbox->getUnsandboxedPointer(const_cast<void*>((const void*)ptr));
	}

public:
	#if defined(_M_X64) || defined(__x86_64__)
		static const bool impl_Handle32bitPointerArrays;
	#endif

	inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		sandbox = WasmSandbox::createSandbox(libraryPath);
		if(!sandbox)
		{
			printf("Failed to create sandbox for: %s\n", libraryPath);
			abort();
		}

		std::lock_guard<std::mutex> lock(sandboxListMutex);
		sandboxList.push_back(sandbox);
	}

	inline void impl_DestroySandbox()
	{
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		sandboxList.erase(std::remove(sandboxList.begin(), sandboxList.end(), sandbox), sandboxList.end());
	}

	inline WasmSandbox* impl_getSandbox()
	{
		return sandbox;
	}

	inline void* impl_mallocInSandbox(size_t size)
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		return sandbox->mallocInSandbox(size);
	}

	//parameter val is a sandboxed pointer
	inline void impl_freeInSandbox(void* val)
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		sandbox->freeInSandbox(val);
	}

	inline size_t impl_getTotalMemory()
	{
		return sandbox->getTotalMemory();
	}

	inline char* impl_getMaxPointer()
	{
		void* maxPtr = (void*) (((uintptr_t)sandbox->getTotalMemory()) - 1);
		return (char*) sandbox->getUnsandboxedPointer(maxPtr);
	}

	inline void* impl_pushStackArr(size_t size)
	{
		//This is not supported
		return impl_mallocInSandbox(size);
	}

	inline void impl_popStackArr(void* ptr, size_t size)
	{
		//This is not supported
		return impl_freeInSandbox(ptr);
	}

	template<typename T>
	static inline void* impl_GetUnsandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		if(p == 0) { return 0; }
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		for(WasmSandbox* sandbox : sandboxList)
		{
			size_t memSize = sandbox->getTotalMemory();
			uintptr_t base = (uintptr_t) sandbox->getSandboxMemoryBase();
			uintptr_t exampleVal = (uintptr_t)exampleUnsandboxedPtr;
			if(exampleVal >= base && exampleVal < (base + memSize))
			{
				return impl_GetUnsandboxedPointer_helper(sandbox, p);
			}
		}
		printf("Could not find sandbox for address: %p\n", const_cast<void*>((const void*)p));
		abort();
	}

	template<typename T>
	static inline void* impl_GetSandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		for(WasmSandbox* sandbox : sandboxList)
		{
			size_t memSize = sandbox->getTotalMemory();
			uintptr_t base = (uintptr_t) sandbox->getSandboxMemoryBase();
			uintptr_t pVal = (uintptr_t) exampleUnsandboxedPtr;
			if(pVal >= base && pVal < (base + memSize))
			{
				return impl_GetSandboxedPointer_helper(sandbox, p);
			}
		}
		printf("Could not find sandbox for address: %p\n", const_cast<void*>((const void*)p));
		abort();
	}

	template<typename T>
	inline void* impl_GetUnsandboxedPointer(T* p)
	{
		return impl_GetUnsandboxedPointer_helper(sandbox, p);
	}

	template<typename T>
	inline void* impl_GetSandboxedPointer(T* p)
	{
		auto ret = impl_GetSandboxedPointer_helper(sandbox, p);
		return ret;
	}

	inline bool impl_isValidSandboxedPointer(const void* p, bool isFuncPtr)
	{
		if (isFuncPtr) {
			return p == nullptr || sandbox->getUnsandboxedFuncPointer(p) != nullptr;
		} else {
			return ((uintptr_t) p) < sandbox->getTotalMemory();
		}
	}

	inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		return sandbox->isAddressInSandboxMemoryOrNull(p);
	}

	inline bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return sandbox->isAddressInNonSandboxMemoryOrNull(p);
	}

	template<typename T>
	static inline T* impl_pointerIncrement(T* p, int64_t increment)
	{
		std::lock_guard<std::mutex> lock(sandboxListMutex);
		for(WasmSandbox* sandbox : sandboxList)
		{
			size_t memSize = sandbox->getTotalMemory();
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
		std::lock_guard<std::mutex> lock(callbackMutex);
		auto stateWrapper = new WasmSandboxStateWrapper(this, state, callback);
		callbackSlotInfo[key] = stateWrapper;
		using funcType = TRet(*)(void*, TArgs...);
		auto callbackStub = (funcType) impl_CallbackReceiver<TRet, TArgs...>;
		WasmSandboxCallback* registeredCallback = sandbox->registerCallback(callbackStub, (void*)stateWrapper);
		stateWrapper->registeredCallback = registeredCallback;
		return (void*)(uintptr_t)registeredCallback->callbackSlot;
	}

	template<typename TFunc>
	inline void impl_UnregisterCallback(void* key)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);

		auto it = callbackSlotInfo.find(key);
		if(it != callbackSlotInfo.end())
		{
			WasmSandboxStateWrapper* slotInfo = it->second;
			callbackSlotInfo.erase(it);
			sandbox->unregisterCallback(slotInfo->registeredCallback);
			delete slotInfo;
		}
	}

	inline void* impl_LookupSymbol(const char* name, bool forSandboxFunction)
	{
		return sandbox->symbolLookup(name);
	}

	template <typename TRet, typename ... TOrigArgs, typename ... TArgs>
	TRet impl_InvokeFunction(TRet(*fnPtr)(TOrigArgs...), TArgs... params)
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		return sandbox->invokeFunction(fnPtr, params...);
	}

	template <typename TRet, typename ... TOrigArgs, typename ... TArgs>
	TRet impl_InvokeFunctionReturnAppPtr(TRet(*fnPtr)(TOrigArgs...), TArgs... params)
	{
		std::lock_guard<std::mutex> lock(threadMutex);
		using TargetFuncType = uint32_t(*)(TArgs...);
		uintptr_t rawRet = (uintptr_t) sandbox->invokeFunction((TargetFuncType) fnPtr, params...);
		return (TRet) rawRet;
	}
};

std::mutex RLBox_Wasm::sandboxListMutex __attribute__((weak));
std::vector<WasmSandbox*> RLBox_Wasm::sandboxList __attribute__((weak));

#endif