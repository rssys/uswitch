#ifndef RLBOX_API_NACL
#define RLBOX_API_NACL

#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <map>
#include <mutex>
#include <type_traits>
#include <functional>
#include <vector>
#include <algorithm>
#include "dyn_ldr_lib.h"

namespace RLBox_NaCl_detail {
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


	//https://stackoverflow.com/questions/10766112/c11-i-can-go-from-multiple-args-to-tuple-but-can-i-go-from-tuple-to-multiple
	template <typename F, typename Tuple, bool Done, int Total, int... N>
	struct call_impl
	{
		static inline return_argument<F> call(F f, Tuple && t)
		{
			return call_impl<F, Tuple, Total == 1 + sizeof...(N), Total, N..., sizeof...(N)>::call(f, std::forward<Tuple>(t));
		}
	};

	template <typename F, typename Tuple, int Total, int... N>
	struct call_impl<F, Tuple, true, Total, N...>
	{
		static inline return_argument<F> call(F f, Tuple && t)
		{
			return f(std::get<N>(std::forward<Tuple>(t))...);
		}
	};

	template <typename F, typename Tuple>
	inline return_argument<F> call_func(F f, Tuple && t)
	{
		typedef typename std::decay<Tuple>::type ttype;
		return call_impl<F, Tuple, 0 == std::tuple_size<ttype>::value, std::tuple_size<ttype>::value>::call(f, std::forward<Tuple>(t));
	}

	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct RLUNUSED { template<typename ...Args> RLUNUSED(Args const & ... ) {} };
};

#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

class RLBox_NaCl
{
private:
	NaClSandbox* sandbox;
	static std::once_flag initFlag;
	std::mutex createAndCallbackMutex;
	#if defined(_M_IX86) || defined(__i386__)
		static std::mutex sandboxListMutex;
		static std::vector<NaClSandbox*> sandboxList;
	#endif

	class NaClSandboxStateWrapper
	{
	public:
		void* originalState;
		void* fnPtr;
		unsigned slotNumber;

		NaClSandboxStateWrapper(void* originalState, void* fnPtr, unsigned slotNumber)
		{
			this->originalState = originalState;
			this->fnPtr = fnPtr;
			this->slotNumber = slotNumber;
		}
	};

	std::map<void*, NaClSandboxStateWrapper*> callbackSlotInfo;

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TArg>
	inline typename std::enable_if<std::is_fundamental<TArg>::value,
	TArg>::type sandbox_get_callback_param(NaClSandbox_Thread* threadData)
	{
		return COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, TArg);
	}

	template<typename TArg>
	inline typename std::enable_if<std::is_pointer<TArg>::value,
	TArg>::type sandbox_get_callback_param(NaClSandbox_Thread* threadData)
	{
		return COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, TArg);
	}

	template<typename TArg>
	inline typename std::enable_if<std::is_array<TArg>::value,
	typename std::decay<TArg>::type>::type sandbox_get_callback_param(NaClSandbox_Thread* threadData)
	{
		return COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, typename std::remove_reference<decltype(*std::declval<TArg>())>::type*);
	}

	template<typename... TArgs>
	inline std::tuple<typename std::decay<TArgs>::type..., void*> sandbox_get_callback_params(NaClSandbox_Thread* threadData, void* state)
	{
		//Note - we can't use make tuple here as this would(may?) iterate through the parameter right to left
		//So we use an initializer list which guarantees order of eval as left to right
		return std::tuple<typename std::decay<TArgs>::type..., void*> 
		{ sandbox_get_callback_param<TArgs>(threadData)..., state };
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	inline T sandbox_callback_return(NaClSandbox_Thread* threadData, T arg)
	{
		RLBox_NaCl_detail::RLUNUSED{threadData};
		return arg;
	}

	template <typename T>
	inline T* sandbox_callback_return(NaClSandbox_Thread* threadData, T* arg)
	{
		return CALLBACK_RETURN_PTR(threadData, T*, arg);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TRet, typename... TArgs, ENABLE_IF(!std::is_void<TRet>::value)>
	static SANDBOX_CALLBACK TRet impl_CallbackReceiver(uintptr_t sandboxPtr, void* state)
	{
		RLBox_NaCl* rlbox_sandbox = (RLBox_NaCl*) (((NaClSandbox*) sandboxPtr)->extraState);
		NaClSandbox_Thread* threadData = callbackParamsBegin(rlbox_sandbox->sandbox);

		auto stateWrapper = (NaClSandboxStateWrapper*) state;
		auto params = rlbox_sandbox->sandbox_get_callback_params<TArgs...>(threadData, stateWrapper->originalState);
		auto fnPtr = (TRet(*)(TArgs..., void*))(uintptr_t) stateWrapper->fnPtr;
		auto ret = RLBox_NaCl_detail::call_func(fnPtr, params);
		return rlbox_sandbox->sandbox_callback_return(threadData, ret);
	}

	template<typename TRet, typename... TArgs, ENABLE_IF(std::is_void<TRet>::value)>
	static SANDBOX_CALLBACK TRet impl_CallbackReceiver(uintptr_t sandboxPtr, void* state)
	{
		RLBox_NaCl* rlbox_sandbox = (RLBox_NaCl*) (((NaClSandbox*) sandboxPtr)->extraState);
		NaClSandbox_Thread* threadData = callbackParamsBegin(rlbox_sandbox->sandbox);

		auto stateWrapper = (NaClSandboxStateWrapper*) state;
		auto params = rlbox_sandbox->sandbox_get_callback_params<TArgs...>(threadData, stateWrapper->originalState);
		auto fnPtr = (TRet(*)(TArgs..., void*))(uintptr_t) stateWrapper->fnPtr;
		RLBox_NaCl_detail::call_func(fnPtr, params);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename... T>
	inline size_t sandbox_NaClAddParams(T... arg)
	{
			return 0;
	}

	template <typename T, typename ... Targs>
	inline size_t sandbox_NaClAddParams(T arg, Targs... rem)
	{
		return sizeof(arg) + sandbox_NaClAddParams(rem...);
	}

	template <typename T, ENABLE_IF(std::is_class<T>::value)>
	inline size_t sandbox_NaClAddReturnArg()
	{
		//pushing return argument slot on stack
		#if defined(_M_IX86) || defined(__i386__) || defined(_M_X64) || defined(__x86_64__)
			return sizeof(T) + sizeof(T*);
		#else
			#error Unknown platform!
		#endif
	}

	template <typename T, ENABLE_IF(!std::is_class<T>::value)>
	size_t sandbox_NaClAddReturnArg()
	{
		return 0;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, ENABLE_IF(!std::is_floating_point<T>::value)>
	inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T arg)
	{
		PUSH_VAL_TO_STACK(threadData, T, arg);
	}

	template <typename T, ENABLE_IF(std::is_floating_point<T>::value)>
	inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T arg)
	{
		PUSH_FLOAT_TO_STACK(threadData, T, arg);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	inline typename std::enable_if<std::is_class<T>::value,
	void*>::type sandbox_dealWithNaClReturnArg(NaClSandbox_Thread* threadData)
	{
		//pushing return argument slot on stack
		#if defined(_M_IX86) || defined(__i386__)
			uintptr_t target = threadData->stack_ptr_forParameters + sizeof(void*);
			sandbox_handleNaClArg(threadData, impl_GetSandboxedPointer((void*)target));
			ALLOCATE_STACK_VARIABLE(threadData, T, tPtr);
			RLBox_NaCl_detail::RLUNUSED{tPtr};
			return (void*) target;
		#elif defined(_M_X64) || defined(__x86_64__)
			ALLOCATE_STACK_VARIABLE(threadData, T, tPtr);
			sandbox_handleNaClArg(threadData, impl_GetSandboxedPointer(tPtr));
			return tPtr;
		#else
			#error Unknown platform!
		#endif
	}

	template <typename T>
	inline typename std::enable_if<!std::is_class<T>::value,
	void*>::type sandbox_dealWithNaClReturnArg(NaClSandbox_Thread* threadData)
	{
		RLBox_NaCl_detail::RLUNUSED{threadData};
		return nullptr;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TFuncRet>
	inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, TFuncRet(*fnPtr)())
	{
		RLBox_NaCl_detail::RLUNUSED{threadData};
		RLBox_NaCl_detail::RLUNUSED{fnPtr};
	}


	template <typename TFuncRet, typename TFuncArg, typename... TFuncArgs, typename TArg, typename ... TArgs>
	inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, TFuncRet(*fnPtr)(TFuncArg, TFuncArgs...), TArg param, TArgs... params)
	{
		sandbox_handleNaClArg(threadData, (TFuncArg) param);
		using TRemFuncType = TFuncRet(*)(TFuncArgs...);
		sandbox_dealWithNaClArgs(threadData, (TRemFuncType) nullptr, params...);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, typename ... Targs>
	inline typename std::enable_if<std::is_void<RLBox_NaCl_detail::return_argument<T>>::value,
	void>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		RLBox_NaCl_detail::RLUNUSED{threadData};
		RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
	}

	template <typename T, typename ... Targs>
	inline typename std::enable_if<std::is_pointer<RLBox_NaCl_detail::return_argument<T>>::value,
	RLBox_NaCl_detail::return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
		return (RLBox_NaCl_detail::return_argument<T>)functionCallReturnPtr(threadData);
	}

	template <typename T, typename ... Targs>
	inline typename std::enable_if<std::is_same<RLBox_NaCl_detail::return_argument<T>, float>::value,
	RLBox_NaCl_detail::return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
		return (RLBox_NaCl_detail::return_argument<T>)functionCallReturnFloat(threadData);
	}

	template <typename T, typename ... Targs>
	inline typename std::enable_if<std::is_same<RLBox_NaCl_detail::return_argument<T>, double>::value,
	RLBox_NaCl_detail::return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
		return (RLBox_NaCl_detail::return_argument<T>)functionCallReturnDouble(threadData);
	}

	template <typename T, typename ... Targs>
	inline typename std::enable_if<std::is_class<RLBox_NaCl_detail::return_argument<T>>::value && !std::is_reference<T>::value,
	RLBox_NaCl_detail::return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		//structs are returned as a pointer
		#if defined(_M_IX86) || defined(__i386__)
			auto ptr = (RLBox_NaCl_detail::return_argument<T>*) returnPtrSlot;
		#elif defined(_M_X64) || defined(__x86_64__)
			RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
			auto ptr = ((RLBox_NaCl_detail::return_argument<T>*)functionCallReturnPtr(threadData));
		#else
			#error Unknown platform!
		#endif
		auto& ret = *ptr;
		return ret;
	}

	template <typename T, typename ... Targs>
	inline typename std::enable_if<!std::is_pointer<RLBox_NaCl_detail::return_argument<T>>::value && !std::is_void<RLBox_NaCl_detail::return_argument<T>>::value && !std::is_floating_point<RLBox_NaCl_detail::return_argument<T>>::value && !std::is_class<RLBox_NaCl_detail::return_argument<T>>::value,
	RLBox_NaCl_detail::return_argument<T>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData, void* returnPtrSlot)
	{
		RLBox_NaCl_detail::RLUNUSED{returnPtrSlot};
		return (RLBox_NaCl_detail::return_argument<T>)functionCallReturnRawPrimitiveInt(threadData);
	}

	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

public:
	inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		std::call_once(initFlag, [](){ initializeDlSandboxCreator(0 /* No logging */); });
		{
			std::lock_guard<std::mutex> lock(createAndCallbackMutex);
			sandbox = createDlSandbox(sandboxRuntimePath, libraryPath);
		}
		if(!sandbox)
		{
			printf("Failed to create sandbox for: %s\n", libraryPath);
			abort();
		}
		sandbox->extraState = (void*) this;
		#if defined(_M_IX86) || defined(__i386__)
			std::lock_guard<std::mutex> lock(sandboxListMutex);
			sandboxList.push_back(sandbox);
		#endif
	}

	inline void impl_DestroySandbox()
	{
		#if defined(_M_IX86) || defined(__i386__)
			std::lock_guard<std::mutex> lock(sandboxListMutex);
			sandboxList.erase(std::remove(sandboxList.begin(), sandboxList.end(), sandbox), sandboxList.end());
		#endif
		destroyDlSandbox(sandbox);
	}

	inline NaClSandbox* impl_getSandbox()
	{
		return sandbox;
	}

	inline void* impl_mallocInSandbox(size_t size)
	{
		return mallocInSandbox(sandbox, size);
	}

	//parameter val is a sandboxed pointer
	inline void impl_freeInSandbox(void* val)
	{
		freeInSandbox(sandbox, val);
	}

	inline size_t impl_getTotalMemory()
	{
		#if defined(_M_IX86) || defined(__i386__)
			size_t memSize = 0x3FFFFFFF;
		#elif defined(_M_X64) || defined(__x86_64__)
			size_t memSize = 0xFFFFFFFF;
		#else
			#error Unsupported platform!
		#endif
		return memSize;
	}

	inline char* impl_getMaxPointer()
	{
		#if defined(_M_IX86) || defined(__i386__)
			uintptr_t sboxMem = 0x3FFFFFFF;
		#elif defined(_M_X64) || defined(__x86_64__)
			uintptr_t sboxMem = 0xFFFFFFFF;
		#else
			#error Unsupported platform!
		#endif
		return (char*)impl_GetUnsandboxedPointer((void*)sboxMem);
	}

	inline void* impl_pushStackArr(size_t size)
	{
		// NaClSandbox_Thread* threadData = getThreadData(sandbox);
		// size_t paddedSize = ROUND_UP_TO_POW2(size, STACKALIGNMENT);
		// threadData->stack_ptr_arrayLocation = ADJUST_STACK_PTR(threadData->stack_ptr_arrayLocation, paddedSize);

		// For the moment because of the design of this API, we can't use support passing stack arrays
		return mallocInSandbox(sandbox, size);
	}
	inline void impl_popStackArr(void* ptr, size_t size)
	{
		// NaClSandbox_Thread* threadData = getThreadData(sandbox);
		// size_t paddedSize = ROUND_UP_TO_POW2(size, STACKALIGNMENT);
		// threadData->stack_ptr_arrayLocation = ADJUST_STACK_PTR(threadData->stack_ptr_arrayLocation, -paddedSize);

		// For the moment because of the design of this API, we can't use support passing stack arrays
		return freeInSandbox(sandbox, ptr);
	}

	//Nice trick to sandbox and unsandbox pointers without knowing a reference to the sandbox
	//Gain the sandbox memory's base address from the address of the pointer itself, since the pointer val
	template<typename T>
	static inline void* impl_GetUnsandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		#if defined(_M_IX86) || defined(__i386__)
			if(p == 0) { return 0; }
			std::lock_guard<std::mutex> lock(sandboxListMutex);
			for(NaClSandbox* sandbox : sandboxList)
			{
				size_t memSize = 0x3FFFFFFF;
				uintptr_t base = getSandboxMemoryBase(sandbox);
				uintptr_t exampleVal = (uintptr_t)exampleUnsandboxedPtr;
				if(exampleVal >= base && exampleVal < (base + memSize))
				{
					return (void*) getUnsandboxedAddress(sandbox, (uintptr_t)const_cast<void*>((const void*)p));
				}
			}
			printf("Could not find sandbox for address: %p\n", const_cast<void*>((const void*)p));
			abort();
		#elif defined(_M_X64) || defined(__x86_64__)
			//32 bit mask
			uintptr_t suffix = ((uintptr_t)const_cast<void*>((const void*)p)) & 0xFFFFFFFF;
			if(suffix == 0) { return 0; }
			uintptr_t mask = ((uintptr_t)exampleUnsandboxedPtr) & 0xFFFFFFFF00000000;
			uintptr_t ret = mask | suffix;
			return (void*) ret;
		#else
			#error Unsupported platform!
		#endif
	}

	template<typename T>
	static inline void* impl_GetSandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		#if defined(_M_IX86) || defined(__i386__)
			if(p == 0) { return 0; }
			std::lock_guard<std::mutex> lock(sandboxListMutex);
			for(NaClSandbox* sandbox : sandboxList)
			{
				size_t memSize = 0x3FFFFFFF;
				uintptr_t base = getSandboxMemoryBase(sandbox);
				uintptr_t pVal = (uintptr_t) const_cast<void*>((const void*)p);
				if(pVal >= base && pVal < (base + memSize))
				{
					return (void*) getSandboxedAddress(sandbox, pVal);
				}
			}
			printf("Could not find sandbox for address: %p\n", const_cast<void*>((const void*)p));
			abort();
		#else
			//32 bit mask
			uintptr_t ret = ((uintptr_t)p) & 0xFFFFFFFF;
			return (void*) ret;
		#endif
	}

	template<typename T>
	inline void* impl_GetUnsandboxedPointer(T* p)
	{
		auto ret = (void*) getUnsandboxedAddress(sandbox, (uintptr_t)const_cast<void*>((const void*)p));
		return ret;
	}

	template<typename T>
	inline void* impl_GetSandboxedPointer(T* p)
	{
		auto ret = (void*) getSandboxedAddress(sandbox, (uintptr_t)const_cast<void*>((const void*)p));
		return ret;
	}

	inline bool impl_isValidSandboxedPointer(const void* p, bool isFuncPtr)
	{
		#if defined(_M_IX86) || defined(__i386__)
			uintptr_t max = 1;
			max = max << 30;
			return ((uintptr_t)p) < max;
		#elif defined(_M_X64) || defined(__x86_64__)
			uintptr_t max = 1;
			max = max << 32;
			return impl_isPointerInSandboxMemoryOrNull(p) || ((uintptr_t)p) < max;
		#else
			#error Unsupported platform!
		#endif
	}

	inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		return isAddressInSandboxMemoryOrNull(sandbox, (uintptr_t) p);
	}

	inline bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return isAddressInNonSandboxMemoryOrNull(sandbox, (uintptr_t) p);
	}

	template<typename T>
	static inline T* impl_pointerIncrement(T* p, int64_t increment)
	{
		#if defined(_M_IX86) || defined(__i386__)
			std::lock_guard<std::mutex> lock(sandboxListMutex);
			for(NaClSandbox* sandbox : sandboxList)
			{
				size_t memSize = 0x3FFFFFFF;
				uintptr_t base = getSandboxMemoryBase(sandbox);
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
		#elif defined(_M_X64) || defined(__x86_64__)
			auto result = p + increment;
			uintptr_t suffix = ((uintptr_t)const_cast<void*>((const void*)result)) & 0xFFFFFFFF;
			uintptr_t mask = ((uintptr_t)const_cast<void*>((const void*)p)) & 0xFFFFFFFF00000000;
			uintptr_t ret = mask | suffix;
			return (T*) ret;
		#else
			#error Unsupported platform!
		#endif
	}

	template<typename TRet, typename... TArgs>
	inline void* impl_RegisterCallback(void* key, void* callback, void* state)
	{
		std::lock_guard<std::mutex> lock(createAndCallbackMutex);

		unsigned slotNumber;
		if(!getFreeSandboxCallbackSlot(sandbox, &slotNumber))
		{
			return nullptr;
		}

		auto stateWrapper = new NaClSandboxStateWrapper(state, callback, slotNumber);
		callbackSlotInfo[key] = stateWrapper;
		return (void*) registerSandboxCallbackWithState(sandbox, slotNumber, (uintptr_t) impl_CallbackReceiver<TRet, TArgs...>, (void*) stateWrapper);
	}

	template<typename TFunc>
	inline void impl_UnregisterCallback(void* key)
	{
		std::lock_guard<std::mutex> lock(createAndCallbackMutex);

		auto it = callbackSlotInfo.find(key);
		if(it != callbackSlotInfo.end())
		{
			NaClSandboxStateWrapper* slotInfo = it->second;
			callbackSlotInfo.erase(it);
			unregisterSandboxCallback(sandbox, slotInfo->slotNumber);
			delete slotInfo;
		}
	}

	inline void* impl_LookupSymbol(const char* name, bool forSandboxFunction)
	{
		auto ret = symbolTableLookupInSandbox(sandbox, name);
		if(!ret)
		{
			printf("Symbol not found: %s.\n", name);
			abort();
		}
		return ret;
	}

	template <typename T, typename ... TArgs>
	RLBox_NaCl_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
	{
		NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(params...) + sandbox_NaClAddReturnArg<RLBox_NaCl_detail::return_argument<T>>(), 0 /* size of any arrays being pushed on the stack */);
		auto returnPtrSlot = sandbox_dealWithNaClReturnArg<RLBox_NaCl_detail::return_argument<T>>(threadData);
		sandbox_dealWithNaClArgs(threadData, fnPtr, params...);
		invokeFunctionCall(threadData, (void*)(uintptr_t) fnPtr);
		return sandbox_invokeNaClReturn<T>(threadData, returnPtrSlot);
	}

	template <typename T, typename ... TArgs>
	RLBox_NaCl_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
	{
		NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(params...) + sandbox_NaClAddReturnArg<RLBox_NaCl_detail::return_argument<T>>(), 0 /* size of any arrays being pushed on the stack */);
		sandbox_dealWithNaClReturnArg<RLBox_NaCl_detail::return_argument<T>>(threadData);
		sandbox_dealWithNaClArgs(threadData, fnPtr, params...);
		invokeFunctionCall(threadData, (void*)(uintptr_t) fnPtr);
		auto ret = (uintptr_t) functionCallReturnRawPrimitiveInt(threadData);
		return (RLBox_NaCl_detail::return_argument<T>) ret;
	}
};

std::once_flag RLBox_NaCl::initFlag __attribute__((weak));
#if defined(_M_IX86) || defined(__i386__)
	std::mutex RLBox_NaCl::sandboxListMutex __attribute__((weak));
	std::vector<NaClSandbox*> RLBox_NaCl::sandboxList __attribute__((weak));
#endif

#undef ENABLE_IF

#endif