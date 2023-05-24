/* -*- mode: C++; tab-width: 2; indent-tabs-mode: t; c-basic-offset: 2 -*- */

#ifndef RLBOX_API
#define RLBOX_API

////////////////////////////////////////////////////////////////////////////////////////////////
//RLBOX API as defined in "RLBOX: Robust Library Sandboxing"                                  //
//                                                                                            //
//Compatible with C++ 11                                                                      //
//This API is designed so that an application can safely interact with sandboxed libraries.   //
////////////////////////////////////////////////////////////////////////////////////////////////

#include <functional>
#include <type_traits>
#include <map>
#include <cstring>
#include <cstdint>
#include <mutex>

namespace rlbox_detail {
	//https://stackoverflow.com/questions/13786888/check-if-member-exists-using-enable-if
	#define GENERATE_HAS_MEMBER(member)                                                        \
	struct TagDoesNotHaveMember_##member {};                                                    \
	struct TagHasMember_##member : TagDoesNotHaveMember_##member {};                            \
	template<typename> struct CheckHasMember_##member { typedef int type; };                    \
																								\
	template<typename T, typename CheckHasMember_##member<decltype(T::member)>::type = 0>     \
	std::true_type hasMemberHelper_##member(TagHasMember_##member);                             \
																								\
	template<typename T>                                                                        \
	std::false_type hasMemberHelper_##member(TagDoesNotHaveMember_##member);                    \
																								\
	template<typename T>                                                                        \
	using has_member_##member = decltype(hasMemberHelper_##member<T>(TagHasMember_##member()));

	GENERATE_HAS_MEMBER(impl_Handle32bitPointerArrays)
	#undef GENERATE_HAS_MEMBER
}

#define FIELD_NORMAL 0
#define FIELD_FREEZABLE 1

namespace rlbox
{
	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
	#define RLUNUSED(x) rlbox::UNUSED_PACK_HELPER {x}

	//C++11 doesn't have the _t and _v convenience helpers, so create these
	//Note the _v variants uses one C++ 14 feature we use ONLY FOR convenience. This generates some warnings that can be ignored.
	//To be fully C++11 compatible, we should inline the _v helpers below
	template<bool T, typename V>
	using my_enable_if_t = typename std::enable_if<T, V>::type;

	template<typename T>
	using my_remove_volatile_t = typename std::remove_volatile<T>::type;

	template<typename T>
	constexpr bool my_is_pointer_v = std::is_pointer<T>::value;

	template< class T >
	using my_remove_pointer_t = typename std::remove_pointer<T>::type;

	template<typename T>
	using my_remove_reference_t = typename std::remove_reference<T>::type;

	template<bool B, typename T, typename F>
	using my_conditional_t = typename std::conditional<B,T,F>::type;

	template<typename T>
	using my_add_volatile_t = typename std::add_volatile<T>::type;

	template<typename T1, typename T2>
	constexpr bool my_is_assignable_v = std::is_assignable<T1, T2>::value;

	template<typename T1, typename T2>
	constexpr bool my_is_same_v = std::is_same<T1, T2>::value;

	template<typename T>
	constexpr bool my_is_class_v = std::is_class<T>::value;

	template<typename T>
	constexpr bool my_is_reference_v = std::is_reference<T>::value;

	template<typename T>
	constexpr bool my_is_array_v = std::is_array<T>::value;

	template<typename T>
	constexpr bool my_is_union_v = std::is_union<T>::value;

	template<typename T>
	constexpr bool my_is_fundamental_or_enum_v = std::is_fundamental<T>::value || std::is_enum<T>::value;

	template<typename T1, typename T2>
	constexpr bool my_is_base_of_v = std::is_base_of<T1, T2>::value;

	template<typename T>
	constexpr bool my_is_void_v = std::is_void<T>::value;

	template<typename T>
	using my_remove_const_t = typename std::remove_const<T>::type;

	template<typename T>
	using my_add_lvalue_reference_t = typename std::add_lvalue_reference<T>::type;

	template<typename T>
	constexpr bool my_is_function_v = std::is_function<T>::value;

	template< class T >
	using my_decay_t = typename std::decay<T>::type;

	template<typename T>
	using my_remove_extent_t = typename std::remove_extent<T>::type;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Some additional helpers

	#define RLBOX_ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<T> && !my_is_pointer_v<my_remove_pointer_t<T>>;

	template<typename T>
	constexpr bool my_is_void_ptr_v = my_is_pointer_v<T> && my_is_void_v<my_remove_pointer_t<T>>;

	template<typename T>
	using my_decay_noconst_if_array_t = my_conditional_t<my_is_array_v<T>, my_decay_t<T>, T>;

	template<typename T>
	using my_decay_if_array_t = my_conditional_t<my_is_array_v<T>, const my_remove_pointer_t<my_decay_t<T>> *, T>;

	template<typename T>
	using valid_return_t = my_decay_noconst_if_array_t<T>;

	template<typename T>
	using my_remove_pointer_or_valid_return_t = my_conditional_t<my_is_function_v<my_remove_pointer_t<T>> || my_is_array_v<my_remove_pointer_t<T>> || my_is_array_v<T>, my_decay_if_array_t<T>, my_remove_pointer_t<T>>;

	template<typename T>
	using my_remove_pointer_or_valid_param_t = my_conditional_t<my_is_void_ptr_v<T>, T, my_remove_pointer_t<T>>;

	template<typename T>
	constexpr bool my_is_function_ptr_v = my_is_pointer_v<T> && my_is_function_v<my_remove_pointer_t<T>>;


	////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//https://github.com/facebook/folly/blob/master/folly/functional/Invoke.h
	//mimic C++17's std::invocable
	namespace invoke_detail {

		template< class... >
		using void_t = void;

		template <typename F, typename... Args>
		constexpr auto invoke(F&& f, Args&&... args) noexcept(
			noexcept(static_cast<F&&>(f)(static_cast<Args&&>(args)...)))
			-> decltype(static_cast<F&&>(f)(static_cast<Args&&>(args)...)) {
		return static_cast<F&&>(f)(static_cast<Args&&>(args)...);
		}
		template <typename M, typename C, typename... Args>
		constexpr auto invoke(M(C::*d), Args&&... args)
			-> decltype(std::mem_fn(d)(static_cast<Args&&>(args)...)) {
		return std::mem_fn(d)(static_cast<Args&&>(args)...);
		}

		template <typename F, typename... Args>
		using invoke_result_ =
		decltype(invoke(std::declval<F>(), std::declval<Args>()...));

		template <typename Void, typename F, typename... Args>
		struct is_invocable : std::false_type {};

		template <typename F, typename... Args>
		struct is_invocable<void_t<invoke_result_<F, Args...>>, F, Args...>
		: std::true_type {};
	}

	template <typename F, typename... Args>
	struct my_is_invocable : invoke_detail::is_invocable<void, F, Args...> {};
	
	template<typename F, typename... Args>
	constexpr bool my_is_invocable_v = my_is_invocable<F, Args...>::value;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TSandbox>
	class RLBoxSandbox;

	template<typename T, typename TSandbox>
	class tainted;

	template<typename T, typename TSandbox>
	class tainted_volatile;

	template<typename T, typename TSandbox>
	class tainted_freezable;

	template<typename T, typename TSandbox>
	class tainted_freezable_volatile;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class sandbox_wrapper_base {};

	template<typename T>
	class sandbox_wrapper_base_of {};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	class sandbox_app_ptr_wrapper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		T* field;
		sandbox_app_ptr_wrapper(T* f) : field(f) {}
	public:
		template<typename U>
		friend class RLBoxSandbox;

		inline T* UNSAFE_Unverified() const noexcept { return field; }
		template<typename TSandbox>
		inline T* UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept { return field; }
		inline T* UNSAFE_Sandboxed() const noexcept { return field; }
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//sandbox_stackarr_helper and sandbox_heaparr_helper implement move semantics as they are RAII
	template <typename T, typename TSandbox>
	class sandbox_stackarr_helper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		TSandbox* sandbox;
		T* field;
		size_t arrSize;
	public:

		sandbox_stackarr_helper(TSandbox* sandbox, T* field, size_t arrSize)
		{
			this->sandbox = sandbox;
			this->field = field;
			this->arrSize = arrSize;
		}
		sandbox_stackarr_helper(sandbox_stackarr_helper&& other)
		{
			sandbox = other.sandbox;
			field = other.field;
			arrSize = other.arrSize;
			other.sandbox = nullptr;
			other.field = nullptr;
			other.arrSize = 0;
		}

		sandbox_stackarr_helper& operator=(const sandbox_stackarr_helper&& other)  
		{
			if (this != &other)  
			{
				sandbox = other.sandbox;
				field = other.field;
				arrSize = other.arrSize;
				other.sandbox = nullptr;
				other.field = nullptr;
				other.arrSize = 0;
			}
		}

		~sandbox_stackarr_helper()
		{
			if(field != nullptr)
			{
				sandbox->impl_popStackArr((my_remove_const_t<T>*) field, arrSize);
			}
		}

		inline T* UNSAFE_Unverified() const noexcept { return field; }
		inline T* UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandboxP) const noexcept { return (T*) sandboxP->getSandboxedPointer(field); }
	};

	template <typename T, typename TSandbox>
	class sandbox_heaparr_helper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		TSandbox* sandbox;
		T* field;
	public:

		sandbox_heaparr_helper(sandbox_heaparr_helper&& other)
		{
			sandbox = other.sandbox;
			field = other.field;
			other.sandbox = nullptr;
			other.field = nullptr;
		}
		sandbox_heaparr_helper(TSandbox* sandbox, T* field)
		{
			this->sandbox = sandbox;
			this->field = field;
		}

		sandbox_heaparr_helper& operator=(const sandbox_heaparr_helper&& other)  
		{
			if (this != &other)  
			{
				sandbox = other.sandbox;
				field = other.field;
				other.sandbox = nullptr;
				other.field = nullptr;
			}
		}

		~sandbox_heaparr_helper()
		{
			if(field != nullptr)
			{
				sandbox->impl_freeInSandbox((my_remove_const_t<T>*) field);
			}
		}

		inline T* UNSAFE_Unverified() const noexcept { return field; }
		inline T* UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandboxP) const noexcept { return (T*) sandboxP->getSandboxedPointer(field); }
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename TSandbox>
	class sandbox_callback_state
	{
	public:
		RLBoxSandbox<TSandbox>* const sandbox;
		void* const actualCallback;
		sandbox_callback_state(RLBoxSandbox<TSandbox>* p_sandbox, void* p_actualCallback) : sandbox(p_sandbox), actualCallback(p_actualCallback)
		{
		}
	};

	template <typename T, typename TSandbox>
	class sandbox_callback_helper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		TSandbox* sandbox;
		T* registeredCallback;
		sandbox_callback_state<TSandbox>* stateObject;
	public:

		sandbox_callback_helper()
		{
			this->sandbox = nullptr;
			this->registeredCallback = nullptr;
			this->stateObject = nullptr;
		}

		sandbox_callback_helper(TSandbox* sandbox, T* registeredCallback, sandbox_callback_state<TSandbox>* stateObject)
		{
			this->sandbox = sandbox;
			this->registeredCallback = registeredCallback;
			this->stateObject = stateObject;
		}
		sandbox_callback_helper(sandbox_callback_helper&& other)
		{
			registeredCallback = other.registeredCallback;
			sandbox = other.sandbox;
			stateObject = other.stateObject;
			other.registeredCallback = nullptr;
			other.sandbox = nullptr;
			other.stateObject = nullptr;
		}

		sandbox_callback_helper& operator=(sandbox_callback_helper&& other)  
		{
			if (this != &other)  
			{
				registeredCallback = other.registeredCallback;
				sandbox = other.sandbox;
				stateObject = other.stateObject;
				other.registeredCallback = nullptr;
				other.sandbox = nullptr;
				other.stateObject = nullptr;
			}
			return *this;
		}

		void unregister()
		{
			if(registeredCallback != nullptr)
			{
				sandbox->template impl_UnregisterCallback<T>(stateObject->actualCallback);
				delete stateObject;
				this->sandbox = nullptr;
				this->registeredCallback = nullptr;
				this->stateObject = nullptr;
			}
		}

		~sandbox_callback_helper()
		{
			unregister();
		}

		inline T* UNSAFE_Unverified() const noexcept { return registeredCallback; }
		inline T* UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept { return registeredCallback; }
		inline T* UNSAFE_Sandboxed() const noexcept { return registeredCallback; }
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_fundamental_or_enum_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(RLBoxSandbox<TSandbox>* sandbox, T retRaw)
	{
		//primitives are returned by value
		auto retRawPtr = (tainted<T, TSandbox> *) &retRaw;
		return *retRawPtr;
	}

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_pointer_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(RLBoxSandbox<TSandbox>* sandbox, T retRaw)
	{
		//pointers are returned by value
		auto retRawPtr = (tainted<T, TSandbox> *) &retRaw;
		return *retRawPtr;
	}

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_array_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(RLBoxSandbox<TSandbox>* sandbox, my_remove_extent_t<T>* retRaw)
	{
		//arrays are normally returned by decaying into a pointer, and returning the pointer
		//but tainted<Foo[]> is returned by value, so copy happens automatically on return - so no additional copying needed
		auto retRawPtr = (tainted<T, TSandbox> *) retRaw;
		return *retRawPtr;
	}

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_class_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(RLBoxSandbox<TSandbox>* sandbox, T retRaw)
	{
		tainted<T, TSandbox> structCopy;
		memcpy(&structCopy, &retRaw, sizeof(T));
		//Structs may have fields which are pointers, each of these have to unswizzled
		//However, these may not be valid pointers as the library may leave garbage in some struct fields
		//for these fields, we set these to null
		structCopy.unsandboxPointersOrNull(sandbox);
		return structCopy;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename TSandbox, typename TRet, typename... TArgs>
	__attribute__ ((noinline)) TRet sandbox_callback_receiver(TArgs... params, void* state)
	{
		auto stateObj = static_cast<sandbox_callback_state<TSandbox>*>(state);
		using TFunc = TRet(RLBoxSandbox<TSandbox>*, tainted<TArgs, TSandbox>...);
		TFunc* actualCallback = (TFunc*) stateObj->actualCallback;
		return actualCallback(stateObj->sandbox, sandbox_convertToUnverified<TArgs>(stateObj->sandbox, params)...);
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Use a custom enum for returns as boolean returns are a bad idea
	//int returns are automatically cast to a boolean
	//Some APIs have overloads with boolean and non returns, so best to use a custom class
	enum class RLBox_Verify_Status
	{
		SAFE, UNSAFE
	};

	template<typename TField, typename T, RLBOX_ENABLE_IF(!my_is_class_v<T> && !my_is_reference_v<T> && !my_is_array_v<T>)>
	inline T getFieldCopy(TField field)
	{
		T copy = field;
		return copy;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename T, typename TSandbox>
	class tainted_base : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T>
	{
	};

	template<typename TRHS, typename TSandbox, template <typename, typename> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>>)>
	inline TRHS rlboxUnwrapOrReturnValue(const TWrap<TRHS, TSandbox> rhs) noexcept
	{
		return rhs.UNSAFE_Unverified();
	}

	template<typename TRHS>
	inline TRHS rlboxUnwrapOrReturnValue(const TRHS rhs) noexcept
	{
		return rhs;
	}

	template<typename T, typename TSandbox>
	class tainted : public tainted_base<T, TSandbox>
	{
		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted;

		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_volatile;

		//make sure tainted_freezable<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable;

		//make sure tainted_freezable_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable_volatile;

		template<typename U>
		friend class RLBoxSandbox;

	private:
		T field;

	public:

		tainted() = default;
		tainted(const tainted<T, TSandbox>& p) = default;

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_array_v<T2>)>
		tainted(const tainted_volatile<T, TSandbox>& p)
		{
			field = p.UNSAFE_Unverified();
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && !my_is_pointer_v<my_remove_extent_t<T2>>)>
		tainted(const tainted_volatile<T, TSandbox>& p)
		{
			memcpy(field, (void*) p.field, sizeof(T));
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && my_is_pointer_v<my_remove_extent_t<T2>>)>
		tainted(const tainted_volatile<T, TSandbox>& p)
		{
			for(unsigned long i = 0; i < sizeof(field)/sizeof(void*); i++)
			{
				field[i] = p[i].UNSAFE_Unverified();
			}
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		tainted(const std::nullptr_t& arg)
		{
			field = arg;
		}

		tainted(const tainted_freezable_volatile<T, TSandbox>& p)
		{
			field = p.UNSAFE_Unverified();
		}

		//we explicitly disable this constructor if it has one of the signatures above, 
		//	so that we give the above constructors a higher priority
		//	For now we only allow this for fundamental types as this is potentially unsafe for pointers and structs
		template<typename Arg, typename... Args, RLBOX_ENABLE_IF(!my_is_base_of_v<tainted_base<T, TSandbox>, my_remove_reference_t<Arg>> && my_is_fundamental_or_enum_v<T>)>
		tainted(Arg&& arg, Args&&... args) : field(std::forward<Arg>(arg), std::forward<Args>(args)...) { }

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
		{
			return field;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> UNSAFE_Unverified_Check_Range(RLBoxSandbox<TSandbox>* sandbox, size_t size) const noexcept
		{
			uintptr_t fieldVal = (uintptr_t) UNSAFE_Unverified();
			if(fieldVal < (fieldVal + size) && sandbox->isPointerInSandboxMemoryOrNull((void*)(fieldVal + size))){
				return (my_decay_if_array_t<T>)fieldVal;
			}
			return nullptr;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept
		{
			return field;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept
		{
			return (T) sandbox->getSandboxedPointer(field);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2> && !(my_is_array_v<T2> && my_is_pointer_v<my_remove_extent_t<T2>>))>
		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox)
		{
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && my_is_pointer_v<my_remove_extent_t<T2>> && !rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value)>
		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox)
		{
			void** start = (void**) field;
			void** end = (void**) (((uintptr_t)field) + sizeof(T));

			for(void** curr = start; curr < end; curr = (void**) (((uintptr_t)curr) + sizeof(void*)))
			{
				if(sandbox->isValidSandboxedPointer(*curr, my_is_function_ptr_v<my_remove_extent_t<T>>))
				{
					*curr = sandbox->getUnsandboxedPointer(*curr);
				}
				else
				{
					*curr = nullptr;
				}
			}
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && my_is_pointer_v<my_remove_extent_t<T2>> && rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value)>
		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox)
		{
			void** start = (void**) field;
			void** endRead = (void**) (((uintptr_t)field) + (sizeof(T)/2) - 4);
			void** endWrite = (void**) (((uintptr_t)field) + sizeof(T) - 8);

			for(void **currRead = endRead, **currWrite = endWrite; 
				currRead >= start; 
				currRead = (void**) (((uintptr_t)currRead) - 4), currWrite = (void**) (((uintptr_t)currWrite) - 8)
			)
			{
				void* curr = (void*)(((uintptr_t)*currRead) & 0xFFFFFFFF);
				if(sandbox->isValidSandboxedPointer(curr, my_is_function_ptr_v<my_remove_extent_t<T>>))
				{
					*currWrite = sandbox->getUnsandboxedPointer(curr);
				}
				else
				{
					*currWrite = nullptr;
				}
			}
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox)
		{
			if(sandbox->isValidSandboxedPointer((const void*)field, my_is_function_ptr_v<T>))
			{
				field = (T) sandbox->getUnsandboxedPointer(field);
			}
			else
			{
				field = nullptr;
			}
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		void assignPointerInSandbox(RLBoxSandbox<TSandbox>* sandbox, T pointerVal)
		{
			if(!sandbox->isPointerInSandboxMemoryOrNull(pointerVal))
			{
				abort();
			}
			field = pointerVal;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<valid_return_t<T>(T)> verifyFunction) const
		{
			return verifyFunction(UNSAFE_Unverified());
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(UNSAFE_Unverified()) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		//Non class pointers - one level pointers
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_one_level_ptr_v<T2> && !my_is_class_v<my_remove_pointer_t<T2>>)>
		inline my_remove_pointer_or_valid_return_t<T> copyAndVerify(std::function<my_remove_pointer_or_valid_return_t<T>(T)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if(maskedFieldPtr == nullptr)
			{
				return verifyFunction(nullptr);
			}

			my_remove_pointer_t<T> maskedField = *maskedFieldPtr;
			return verifyFunction(&maskedField);
		}

		//Class pointers - one level pointers

		//Even though this function is not enabled for function types, the C++ compiler complains that this is a function that
		//	returns a function type
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_one_level_ptr_v<T2> && my_is_class_v<my_remove_pointer_t<T2>>)>
		inline my_remove_pointer_or_valid_return_t<T> copyAndVerify(std::function<my_remove_pointer_or_valid_return_t<T>(tainted<my_remove_pointer_t<T>, TSandbox>*)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if(maskedFieldPtr == nullptr)
			{
				return verifyFunction(nullptr);
			}

			tainted<my_remove_pointer_t<T>, TSandbox> maskedField = *((tainted_volatile<my_remove_pointer_t<T>, TSandbox>*)maskedFieldPtr);
			return verifyFunction(&maskedField);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2>)>
		inline bool copyAndVerify(my_decay_noconst_if_array_t<T2> copy, size_t sizeOfCopy, std::function<RLBox_Verify_Status(T, size_t)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();

			if(sizeOfCopy >= sizeof(T))
			{
				memcpy(copy, maskedFieldPtr, sizeof(T));
				if(verifyFunction(copy, sizeof(T)) == RLBox_Verify_Status::SAFE)
				{
					return true;
				}
			}

			//something went wrong, clear the target for safety
			memset(copy, 0, sizeof(T));
			return false;
		}

		inline my_decay_if_array_t<T> copyAndVerifyArray(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, std::uint32_t elementCount, T defaultValue) const
		{
			typedef my_remove_pointer_t<T> nonPointerType;
			typedef my_remove_const_t<nonPointerType> nonPointerConstType;

			auto maskedFieldPtr = UNSAFE_Unverified();
			if (maskedFieldPtr == nullptr) {
				return nullptr;
			}
			auto maskedFieldInt = reinterpret_cast<uintptr_t>(maskedFieldPtr);
			static_assert(sizeof(nonPointerType) <= 0xFFFFFFFF, "Overflow on size of type in copyAndVerifyArray");
			auto arrayByteLen = static_cast<uint64_t>(sizeof(nonPointerType)) * static_cast<uint64_t>(elementCount);
			uintptr_t arrayEnd = maskedFieldInt + arrayByteLen;

			//check for overflow
			if(maskedFieldInt >= arrayEnd
				 || !sandbox->isPointerInSandboxMemoryOrNull(reinterpret_cast<void*>(arrayEnd)))
			{
				return defaultValue;
			}

			nonPointerConstType* copy = new nonPointerConstType[elementCount];
			memcpy(copy, maskedFieldPtr, arrayByteLen);
			auto isSafe = verifyFunction(copy) == RLBox_Verify_Status::SAFE;
			if(!isSafe) {
				delete[] copy;
			}
			return isSafe? copy : defaultValue;
		}

		inline my_decay_if_array_t<T> copyAndVerifyString(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if (maskedFieldPtr == nullptr) {
				return nullptr;
			}
			auto elementCount = strlen(maskedFieldPtr) + 1;

			auto ret = copyAndVerifyArray(sandbox, verifyFunction, elementCount, defaultValue);
			if(ret != defaultValue)
			{
				my_remove_const_t<my_remove_pointer_t<T>>* retNoConstAlias = (my_remove_const_t<my_remove_pointer_t<T>>*)ret;
				//ensure we have a trailing null on returned strings
				retNoConstAlias[elementCount] = '\0';
			}
			return ret;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted<T, TSandbox>& operator=(const TRHS& arg) noexcept
		{
			field = arg;
			return *this;
		}

		//we don't support app pointers in structs that are maintained in application memory.
		//so we dont provide operator=(const sandbox_app_ptr_wrapper<TRHS>& arg)

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_function_ptr_v<T> && my_is_assignable_v<T&, TRHS*>)>
		inline tainted<T, TSandbox>& operator=(const sandbox_callback_helper<TRHS, TSandbox>& arg) noexcept
		{
			//Generally tainted<T> stores the app version of data while tainted_volatile<T> stores the sandbox version of data
			//For the moment, function pointer fields are the one exception for effeciency
			//these always store sandboxed version of data, so that when function pointers (or structs containing function pointers)
			//  are passed to functions, they can just be passed in without any complex manipulation
			field = arg.UNSAFE_Sandboxed();
			return *this;
		}

		//we only allow tainted assignment from tainted_freezable if its a simple type
		//allowing pointers would let users write buggy patterns in code
		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS> && my_is_fundamental_or_enum_v<T>)>
		inline tainted<T, TSandbox>& operator=(const tainted_freezable<TRHS, TSandbox>& arg) noexcept
		{
			field = getSandboxSwizzledValue(arg.field, (void*) &field /* exampleUnsandboxedPtr */);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline tainted<T, TSandbox>& operator=(const tainted_freezable_volatile<TRHS, TSandbox>& arg) noexcept
		{
			field = arg.UNSAFE_Unverified();
			return *this;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox>& operator=(const std::nullptr_t& arg) noexcept
		{
			field = arg;
			return *this;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>& operator*() const noexcept
		{
			auto& ret = *((tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) field);
			return ret;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>* operator->()
		{
			return (tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) UNSAFE_Unverified();
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator==(const std::nullptr_t& arg) const
		{
			return field == arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator!=(const std::nullptr_t& arg) const
		{
			return field != arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator!() const
		{
			return field == nullptr;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-() const noexcept
		{
			auto result = - UNSAFE_Unverified();
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() + rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() - rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), - rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator*(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() * rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		inline tainted<T, TSandbox> getPointerIncrement(RLBoxSandbox<TSandbox>* sandbox, size_t size) const noexcept
		{
			auto result = UNSAFE_Unverified() + size;
			if (!sandbox->isPointerInSandboxMemoryOrNull(result))
			{
				abort();
			}
			auto retPtr = (tainted<T, TSandbox>*) &result;
			return *retPtr;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2>)>
		inline tainted<my_remove_extent_t<T>, TSandbox>& operator[] (size_t x) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			auto lastIndex = sizeof(T) / sizeof(my_remove_extent_t<T>);
			if(x >= lastIndex)
			{
				abort();
			}
			my_remove_extent_t<T>* locPtr = (my_remove_extent_t<T>*) &(maskedFieldPtr[x]);
			return *((tainted<my_remove_extent_t<T>, TSandbox> *) locPtr);
		}
	};

	template<typename T, typename TSandbox>
	class tainted_volatile : public tainted_base<T, TSandbox>
	{
		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted;

		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_volatile;

		//make sure tainted_freezable<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable;

		//make sure tainted_freezable_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable_volatile;

	private:
		my_add_volatile_t<T> field;

		tainted_volatile() = default;
		tainted_volatile(const tainted_volatile<T, TSandbox>& p) = default;

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getAppSwizzledValue(my_add_volatile_t<T> arg, void* exampleUnsandboxedPtr) const
		{
			return (my_decay_if_array_t<T>) arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getAppSwizzledValue(my_add_volatile_t<T> arg, void* exampleUnsandboxedPtr) const
		{
			// static_cast drops constness
			return (T) TSandbox::impl_GetUnsandboxedPointer(arg, exampleUnsandboxedPtr);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getSandboxSwizzledValue(T arg, void* exampleUnsandboxedPtr) const
		{
			return arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getSandboxSwizzledValue(T arg, void* exampleUnsandboxedPtr) const
		{
			return (T) TSandbox::impl_GetSandboxedPointer(arg, exampleUnsandboxedPtr);
		}

		template<typename TRHS, typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2> && rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value)>
		inline void assignField(TRHS& value)
		{
			auto fieldRef = (uint32_t*) &field;
			auto valueRef = (uint32_t*) &value;
			*fieldRef = *valueRef;
		}

		template<typename TRHS, typename T2=T, RLBOX_ENABLE_IF(!(my_is_pointer_v<T2> && rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value))>
		inline void assignField(TRHS& value)
		{
			field = value;
		}

	public:

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
		{
			//Have to cast to the correct type as function pointer arrays have some const problems otherwise
			auto fieldCopy = (my_add_volatile_t<T>*) &field;
			return getAppSwizzledValue(*fieldCopy, (void*) &field /* exampleUnsandboxedPtr */);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> UNSAFE_Unverified_Check_Range(RLBoxSandbox<TSandbox>* sandbox, size_t size) const noexcept
		{
			uintptr_t fieldVal = (uintptr_t) UNSAFE_Unverified();
			if(fieldVal < (fieldVal + size) && sandbox->isPointerInSandboxMemoryOrNull((void*)(fieldVal + size))){
				return (my_decay_if_array_t<T>)fieldVal;
			}
			return nullptr;
		}

		inline my_decay_if_array_t<T> UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept
		{
			return field;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		void assignPointerInSandbox(RLBoxSandbox<TSandbox>* sandbox, T pointerVal)
		{
			if(!sandbox->isPointerInSandboxMemoryOrNull(pointerVal))
			{
				abort();
			}
			field = (T) sandbox->getSandboxedPointer( pointerVal);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<valid_return_t<T>(T)> verifyFunction) const
		{
			return verifyFunction(UNSAFE_Unverified());
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(UNSAFE_Unverified()) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		//Non class pointers - one level pointers
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_one_level_ptr_v<T2> && !my_is_class_v<my_remove_pointer_t<T2>> && !my_is_void_ptr_v<T2>)>
		inline my_remove_pointer_or_valid_return_t<T> copyAndVerify(std::function<RLBox_Verify_Status(my_remove_pointer_or_valid_param_t<T>)> verifyFunction, my_remove_pointer_or_valid_param_t<T> defaultValue) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if(maskedFieldPtr == nullptr)
			{
				return defaultValue;
			}

			my_remove_pointer_t<T> maskedField = *maskedFieldPtr;
			return verifyFunction(maskedField) == RLBox_Verify_Status::SAFE? maskedField : defaultValue;
		}

		//Class pointers - one level pointers

		//Even though this function is not enabled for function types, the C++ compiler complains that this is a function that
		//	returns a function type
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_one_level_ptr_v<T2> && my_is_class_v<my_remove_pointer_t<T2>>)>
		inline my_remove_pointer_or_valid_return_t<T> copyAndVerify(std::function<my_remove_pointer_or_valid_return_t<T>(tainted<my_remove_pointer_t<T>, TSandbox>*)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if(maskedFieldPtr == nullptr)
			{
				return verifyFunction(nullptr);
			}

			tainted<my_remove_pointer_t<T>, TSandbox> maskedField = *((tainted_volatile<my_remove_pointer_t<T>, TSandbox>*)maskedFieldPtr);
			return verifyFunction(&maskedField);
		}

		//app_ptr
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline valid_return_t<T> copyAndVerifyAppPtr(RLBoxSandbox<TSandbox>* sandbox, std::function<valid_return_t<T>(T)> verifyFunction) const
		{
			auto p_appPtrMap = sandbox->getMaintainAppPtrMap();
			auto& ref_appPtrMapMtx = *(sandbox->getMaintainAppPtrMapMutex());
			std::lock_guard<std::mutex> lock(ref_appPtrMapMtx);

			auto fieldMask = (void*)(((uintptr_t)field) & 0xFFFFFFFF);
			auto it = p_appPtrMap->find(fieldMask);
			T val = nullptr;
			if (it != p_appPtrMap->end()) {
				val = (T) it->second;
			}
			return verifyFunction(val);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2>)>
		inline bool copyAndVerify(my_decay_noconst_if_array_t<T2> copy, size_t sizeOfCopy, std::function<RLBox_Verify_Status(T, size_t)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();

			if(sizeOfCopy >= sizeof(T))
			{
				memcpy(copy, maskedFieldPtr, sizeof(T));
				if(verifyFunction(copy, sizeof(T)) == RLBox_Verify_Status::SAFE)
				{
					return true;
				}
			}

			//something went wrong, clear the target for safety
			memset(copy, 0, sizeof(T));
			return false;
		}

		inline my_decay_if_array_t<T> copyAndVerifyArray(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, std::uint32_t elementCount, T defaultValue) const
		{
			typedef my_remove_pointer_t<T> nonPointerType;
			typedef my_remove_const_t<nonPointerType> nonPointerConstType;

			auto maskedFieldPtr = UNSAFE_Unverified();
			if (maskedFieldPtr == nullptr) {
				return nullptr;
			}
			auto maskedFieldInt = reinterpret_cast<uintptr_t>(maskedFieldPtr);
			static_assert(sizeof(nonPointerType) <= 0xFFFFFFFF, "Overflow on size of type in copyAndVerifyArray");
			auto arrayByteLen = static_cast<uint64_t>(sizeof(nonPointerType)) * static_cast<uint64_t>(elementCount);
			uintptr_t arrayEnd = maskedFieldInt + arrayByteLen;

			//check for overflow
			if(maskedFieldInt >= arrayEnd
				 || !sandbox->isPointerInSandboxMemoryOrNull(reinterpret_cast<void*>(arrayEnd)))
			{
				return defaultValue;
			}

			nonPointerConstType* copy = new nonPointerConstType[elementCount];
			memcpy(copy, maskedFieldPtr, arrayByteLen);
			auto isSafe = verifyFunction(copy) == RLBox_Verify_Status::SAFE;
			if(!isSafe) {
				delete[] copy;
			}
			return isSafe? copy : defaultValue;
		}

		inline my_decay_if_array_t<T> copyAndVerifyString(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if (maskedFieldPtr == nullptr) {
				return nullptr;
			}
			auto elementCount = strlen(maskedFieldPtr) + 1;

			auto ret = copyAndVerifyArray(sandbox, verifyFunction, elementCount, defaultValue);
			if(ret != defaultValue)
			{
				my_remove_const_t<my_remove_pointer_t<T>>* retNoConstAlias = (my_remove_const_t<my_remove_pointer_t<T>>*)ret;
				//ensure we have a trailing null on returned strings
				retNoConstAlias[elementCount] = '\0';
			}
			return ret;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const TRHS& arg) noexcept
		{
			assignField(arg);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T> && my_is_assignable_v<T&, TRHS*>)>
		inline tainted_volatile<T, TSandbox>& operator=(const sandbox_app_ptr_wrapper<TRHS>& arg) noexcept
		{
			auto val = arg.UNSAFE_Sandboxed();
			assignField(val);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_function_ptr_v<T> && my_is_assignable_v<T&, TRHS*>)>
		inline tainted_volatile<T, TSandbox>& operator=(const sandbox_callback_helper<TRHS, TSandbox>& arg) noexcept
		{
			auto val = arg.UNSAFE_Sandboxed();
			assignField(val);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const tainted<TRHS, TSandbox>& arg) noexcept
		{
			auto val = getSandboxSwizzledValue(arg.field, (void*) &field /* exampleUnsandboxedPtr */);
			assignField(val);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const tainted_freezable<TRHS, TSandbox>& arg) noexcept
		{
			auto val = getSandboxSwizzledValue(arg.field, (void*) &field /* exampleUnsandboxedPtr */);
			assignField(val);
			return *this;
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const tainted_freezable_volatile<TRHS, TSandbox>& arg) noexcept
		{
			//Avoid freeze check as we writing into sandboxed memory anyway
			auto val = arg.UNSAFE_SandboxedNoFreezeCheck();
			assignField(val);
			return *this;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<T, TSandbox>& operator=(const std::nullptr_t& arg) noexcept
		{
			assignField(arg);
			return *this;
		}

		inline tainted<T*, TSandbox> operator&() const noexcept
		{
			tainted<T*, TSandbox> ret;
			// static_cast drops constness
			ret.field = (T*) &field;
			return ret;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>& operator*() const noexcept
		{
			auto ret = (tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) getAppSwizzledValue(field, (void*) &field /* exampleUnsandboxedPtr */);
			return *ret;
		}

		inline tainted<T, TSandbox> getPointerIncrement(RLBoxSandbox<TSandbox>* sandbox, size_t size) const noexcept
		{
			auto result = UNSAFE_Unverified() + size;
			if (!sandbox->isPointerInSandboxMemoryOrNull(result))
			{
				abort();
			}
			auto retPtr = (tainted<T, TSandbox>*) &result;
			return *retPtr;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>* operator->()
		{
			return (tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) UNSAFE_Unverified();
		}

		//inline operator tainted<T, TSandbox>() const 
		//{
		//	tainted<T, TSandbox> fieldCopy(*this);
		//	return fieldCopy;
		//}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-() const noexcept
		{
			auto result = - UNSAFE_Unverified();
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() + rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() - rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), - rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator*(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() * rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && !rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value)>
		inline tainted_volatile<my_remove_extent_t<T>, TSandbox>& operator[] (size_t x) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			auto lastIndex = sizeof(T) / sizeof(my_remove_extent_t<T>);
			if(x >= lastIndex)
			{
				abort();
			}
			my_remove_extent_t<T>* locPtr = (my_remove_extent_t<T>*) &(maskedFieldPtr[x]);
			return *((tainted_volatile<my_remove_extent_t<T>, TSandbox> *) locPtr);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_array_v<T2> && rlbox_detail::has_member_impl_Handle32bitPointerArrays<TSandbox>::value)>
		inline tainted_volatile<my_remove_extent_t<T>, TSandbox>& operator[] (size_t x) const
		{
			auto maskedFieldPtr = (uint32_t*) UNSAFE_Unverified();
			auto lastIndex = sizeof(T) / sizeof(my_remove_extent_t<T>);
			if(x >= lastIndex)
			{
				abort();
			}
			my_remove_extent_t<T>* locPtr = (my_remove_extent_t<T>*) &(maskedFieldPtr[x]);
			return *((tainted_volatile<my_remove_extent_t<T>, TSandbox> *) locPtr);
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename T, typename TSandbox>
	class tainted_freezable_base : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T>
	{
	};

	template<typename T, typename TSandbox>
	class tainted_freezable : public tainted_freezable_base<T, TSandbox>
	{
		static_assert(my_is_pointer_v<T>, "Tainted frozen expected a pointer");

		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted;

		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_volatile;

		//make sure tainted_freezable<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable;

		//make sure tainted_freezable_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable_volatile;

		template<typename U>
		friend class RLBoxSandbox;

	private:
		T field;

	public:

		tainted_freezable() = default;
		tainted_freezable(const tainted_freezable<T, TSandbox>& p) = default;

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
		{
			return field;
		}

		inline my_decay_if_array_t<T> UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept
		{
			return (T) sandbox->getSandboxedPointer(field);
		}

		//Non class pointers - one level pointers
		template<typename T2=T, RLBOX_ENABLE_IF(my_is_one_level_ptr_v<T2> && !my_is_class_v<my_remove_pointer_t<T2>>)>
		inline my_remove_pointer_or_valid_return_t<T> copyAndVerify(std::function<my_remove_pointer_or_valid_return_t<T>(T)> verifyFunction) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
			if(maskedFieldPtr == nullptr)
			{
				return verifyFunction(nullptr);
			}

			my_remove_pointer_t<T> maskedField = *maskedFieldPtr;
			return verifyFunction(&maskedField);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_freezable_volatile<my_remove_pointer_t<T>, TSandbox>& operator*() const noexcept
		{
			auto& ret = *((tainted_freezable_volatile<my_remove_pointer_t<T>, TSandbox>*) field);
			return ret;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_freezable_volatile<my_remove_pointer_t<T>, TSandbox>* operator->()
		{
			return (tainted_freezable_volatile<my_remove_pointer_t<T>, TSandbox>*) UNSAFE_Unverified();
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator==(const std::nullptr_t& arg) const
		{
			return field == arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator!=(const std::nullptr_t& arg) const
		{
			return field != arg;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline bool operator!() const
		{
			return field == nullptr;
		}
	};

	template<typename T, typename TSandbox>
	class tainted_freezable_volatile : public tainted_base<T, TSandbox>
	{
		// static_assert(my_is_fundamental_or_enum_v<T>, "Can only freeze simple values");

		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted;

		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_volatile;

		//make sure tainted_freezable<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable;

		//make sure tainted_freezable_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_freezable_volatile;

	private:
		static std::mutex frozenListLock;
		static std::map<void*, T> frozenList;

		my_add_volatile_t<T> field;

		tainted_freezable_volatile()
		{
			static_assert(my_is_fundamental_or_enum_v<T>, "Can only freeze simple values");
		}
		tainted_freezable_volatile(const tainted_freezable_volatile<T, TSandbox>& p) = default;

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline void assignField(TRHS& value)
		{
			std::lock_guard<std::mutex> lock(frozenListLock);
			auto it = frozenList.find((void*) &field);
			if (it != frozenList.end()) {
				it->second = value;
			}
			field = value;
		}

	public:

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
		{
			std::lock_guard<std::mutex> lock(frozenListLock);
			auto it = frozenList.find((void*) &field);
			if (it == frozenList.end()) {
				printf("Value not frozen before read at location : %p\n", (void*) &field);
				abort();
			}
			auto value = it->second;
			if (value != field) {
				printf("Frozen Value changed before read at location : %p\n", (void*) &field);
				abort();
			}
			return value;
		}

		inline my_decay_if_array_t<T> UNSAFE_UnverifiedNoFreezeCheck() const noexcept
		{
			return field;
		}

		inline my_decay_if_array_t<T> UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept
		{
			return UNSAFE_Unverified();
		}

		inline my_decay_if_array_t<T> UNSAFE_SandboxedNoFreezeCheck() const noexcept
		{
			return field;
		}

		inline void freeze() noexcept
		{
			std::lock_guard<std::mutex> lock(frozenListLock);
			frozenList[(void*) &field] = field;
		}

		inline void unfreeze() noexcept
		{
			std::lock_guard<std::mutex> lock(frozenListLock);
			frozenList.erase((void*) &field);
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<valid_return_t<T>(T)> verifyFunction) const
		{
			return verifyFunction(UNSAFE_Unverified());
		}

		template<typename T2=T, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T2>)>
		inline T2 copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(UNSAFE_Unverified()) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox)
		{
		}

		template<typename TRHS, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted_freezable_volatile<T, TSandbox>& operator=(const TRHS& arg) noexcept
		{
			assignField(arg);
			return *this;
		}

		template<typename TRHS, template <typename, typename> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>> && my_is_assignable_v<T&, TRHS>)>
		inline tainted_freezable_volatile<T, TSandbox>& operator=(const TWrap<TRHS, TSandbox>& arg) noexcept
		{
			auto val = arg.UNSAFE_Unverified();
			assignField(val);
			return *this;
		}

		inline tainted_freezable<T*, TSandbox> operator&() const noexcept
		{
			tainted_freezable<T*, TSandbox> ret;
			// static_cast drops constness
			ret.field = (T*) &field;
			return ret;
		}

		template<typename T2=T, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-() const noexcept
		{
			auto result = - UNSAFE_Unverified();
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() + rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() - rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			auto result = TSandbox::impl_pointerIncrement(UNSAFE_Unverified(), - rlboxUnwrapOrReturnValue(rhs));
			return *((tainted<T, TSandbox>*) &result);
		}

		template<typename T2=T, typename TRHS, RLBOX_ENABLE_IF(!my_is_pointer_v<T2>)>
		inline tainted<T, TSandbox> operator*(const TRHS rhs) const noexcept
		{
			auto result = UNSAFE_Unverified() * rlboxUnwrapOrReturnValue(rhs);
			return *((tainted<T, TSandbox>*) &result);
		}
	};

	template<typename T, typename TSandbox>
	std::mutex tainted_freezable_volatile<T, TSandbox>::frozenListLock __attribute__((weak));

	template<typename T, typename TSandbox>
	std::map<void*, T> tainted_freezable_volatile<T, TSandbox>::frozenList __attribute__((weak));

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename T, typename TSandbox>
	tainted<T, TSandbox> convertToTaintedField(const tainted_volatile<T, TSandbox>& field)
	{
		tainted<T, TSandbox> temp(field);
		return temp;
	}

	template<typename T, typename TSandbox>
	tainted<T, TSandbox> convertToTaintedField(const tainted_freezable_volatile<T, TSandbox>& field)
	{
		tainted<T, TSandbox> temp = field.UNSAFE_Unverified();
		return temp;
	}

	#define helper_tainted_createField(fieldType, fieldName, isFrozen, TSandbox) tainted<fieldType, TSandbox> fieldName;
	#define helper_tainted_volatile_createField(fieldType, fieldName, isFrozen, TSandbox) my_conditional_t<isFrozen == 0, tainted_volatile<fieldType, TSandbox>, tainted_freezable_volatile<fieldType, TSandbox>> fieldName;
	#define helper_noOp()
	#define helper_fieldInit(fieldType, fieldName, isFrozen, TSandbox) fieldName = p.fieldName;
	#define helper_fieldCopy(fieldType, fieldName, isFrozen, TSandbox) { tainted<fieldType, TSandbox> temp = convertToTaintedField(fieldName); std::memcpy((void*) &(ret.fieldName), (void*) &temp, sizeof(ret.fieldName)); }
	#define helper_fieldCopyUnsandbox(fieldType, fieldName, isFrozen, TSandbox) { auto temp = fieldName.UNSAFE_Sandboxed(sandbox); std::memcpy((void*) &(ret.fieldName), (void*) &temp, sizeof(ret.fieldName)); }
	#define helper_fieldUnsandbox(fieldType, fieldName, isFrozen, TSandbox) fieldName.unsandboxPointersOrNull(sandbox);

	#define tainted_data_specialization(T, libId, TSandbox) \
	template<> \
	class tainted_volatile<T, TSandbox> \
	{ \
	public: \
		sandbox_fields_reflection_##libId##_class_##T(helper_tainted_volatile_createField, helper_noOp, TSandbox) \
		\
		inline T UNSAFE_Unverified() const noexcept \
		{ \
			T ret; \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldCopy, helper_noOp, TSandbox) \
			return ret;\
		} \
		\
		inline T UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept \
		{ \
			return *((T*)this); \
		} \
		\
		inline T copyAndVerify(std::function<T(tainted_volatile<T, TSandbox>&)> verifyFunction) \
		{ \
			return verifyFunction(*this); \
		} \
		\
		inline tainted<T*, TSandbox> operator&() const noexcept \
		{ \
			auto structAddr = (T*)this; \
			return *((tainted<T*, TSandbox>*)&structAddr); \
		} \
	};\
	template<> \
	class tainted<T, TSandbox> \
	{ \
	public: \
		sandbox_fields_reflection_##libId##_class_##T(helper_tainted_createField, helper_noOp, TSandbox) \
		tainted() = default; \
		tainted(const tainted<T, TSandbox>& p) = default; \
		\
		tainted(const tainted_volatile<T, TSandbox>& p) \
		{ \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldInit, helper_noOp, TSandbox) \
		} \
 		\
		inline T UNSAFE_Unverified() const noexcept \
		{ \
			/* Can't reinterpret_cast due to constness */ \
			return *((T*)this); \
		} \
		 \
		inline T UNSAFE_Sandboxed(RLBoxSandbox<TSandbox>* sandbox) const noexcept \
		{ \
			T ret; \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldCopyUnsandbox, helper_noOp, TSandbox) \
			return ret;\
		} \
		\
		inline T copyAndVerify(std::function<T(tainted<T, TSandbox>&)> verifyFunction) \
		{ \
			return verifyFunction(*this); \
		} \
		\
		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)> \
		inline tainted<T, TSandbox>& operator=(const TRHS& p) noexcept \
		{ \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldInit, helper_noOp, TSandbox) \
			return *this; \
		} \
		\
		template<typename TRHS, RLBOX_ENABLE_IF(my_is_assignable_v<T&, TRHS>)> \
		inline tainted<T, TSandbox>& operator=(const tainted_volatile<TRHS, TSandbox>& p) noexcept \
		{ \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldInit, helper_noOp, TSandbox) \
			return *this; \
		} \
		\
		inline void unsandboxPointersOrNull(RLBoxSandbox<TSandbox>* sandbox) \
		{ \
			sandbox_fields_reflection_##libId##_class_##T(helper_fieldUnsandbox, helper_noOp, TSandbox)\
		} \
	};

	#define rlbox_load_library_api(libId, TSandbox) namespace rlbox { \
		sandbox_fields_reflection_##libId##_allClasses(tainted_data_specialization, TSandbox) \
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename TSandbox, typename T, RLBOX_ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	inline T sandbox_removeWrapper(RLBoxSandbox<TSandbox>* sandbox, T& arg)
	{
		return arg;
	}

	template<typename TSandbox, typename TRHS, typename... TRHSRem, template<typename, typename...> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, TWrap<TRHS, TRHSRem...>>)>
	inline auto sandbox_removeWrapper(RLBoxSandbox<TSandbox>* sandbox, const TWrap<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_Sandboxed(sandbox))
	{
		return arg.UNSAFE_Sandboxed(sandbox);
	}

	template<typename TSandbox, typename TRHS, typename... TRHSRem>
	inline auto sandbox_removeWrapper(RLBoxSandbox<TSandbox>* sandbox, const tainted_freezable_volatile<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_SandboxedNoFreezeCheck())
	{
		return arg.UNSAFE_SandboxedNoFreezeCheck();
	}

	template <typename TSandbox, typename T, RLBOX_ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	inline T sandbox_removeWrapperUnsandboxed(RLBoxSandbox<TSandbox>* sandbox, T& arg)
	{
		return arg;
	}

	template<typename TSandbox, typename TRHS, typename... TRHSRem, template<typename, typename...> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, TWrap<TRHS, TRHSRem...>>)>
	inline auto sandbox_removeWrapperUnsandboxed(RLBoxSandbox<TSandbox>* sandbox, const TWrap<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_Unverified())
	{
		return arg.UNSAFE_Unverified();
	}

	template<typename TSandbox, typename TRHS, typename... TRHSRem>
	inline auto sandbox_removeWrapperUnsandboxed(RLBoxSandbox<TSandbox>* sandbox, const tainted_freezable_volatile<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_SandboxedNoFreezeCheck())
	{
		return arg.UNSAFE_UnverifiedNoFreezeCheck();
	}


	template<class TData> 
	class tainted_unwrapper {
	public:
		TData value_field;
	};

	template<typename T>
	tainted_unwrapper<T> sandbox_removeWrapper_t_helper(sandbox_wrapper_base_of<T>);

	template<typename T, RLBOX_ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	tainted_unwrapper<T> sandbox_removeWrapper_t_helper(T);

	template <typename T>
	using sandbox_removeWrapper_t = decltype(sandbox_removeWrapper_t_helper(std::declval<T>()).value_field);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<bool... Conds>
	struct and_ : std::true_type {};

	template<bool Cond, bool... Conds>
	struct and_<Cond, Conds...> : std::conditional<Cond, and_<Conds...>, std::false_type>::type {};

	template<typename TSandbox, typename TArg>
	std::true_type sandbox_callback_is_arg_tainted_helper(tainted<TArg ,TSandbox>);

	template<typename TSandbox, typename TArg, RLBOX_ENABLE_IF(my_is_fundamental_or_enum_v<TArg>)>
	std::true_type sandbox_callback_is_arg_tainted_helper(TArg);

	template<typename TSandbox, typename TArg, RLBOX_ENABLE_IF(!my_is_fundamental_or_enum_v<TArg>)>
	std::false_type sandbox_callback_is_arg_tainted_helper(TArg);

	template <typename TSandbox, typename TArg>
	using sandbox_callback_is_arg_tainted = decltype(sandbox_callback_is_arg_tainted_helper(std::declval<TArg>()));

	template <typename TSandbox, typename... TArgs>
	using sandbox_callback_all_args_are_tainted = and_<sandbox_callback_is_arg_tainted<TSandbox, TArgs>::value...>;

	template <typename... Args>
	using sandbox_function_have_all_args_fundamental_or_wrapped = and_<(my_is_base_of_v<sandbox_wrapper_base, my_remove_reference_t<Args>> || my_is_fundamental_or_enum_v<my_remove_reference_t<Args>>)...>;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TSandbox>
	class RLBoxSandbox : protected TSandbox
	{
	private:
		RLBoxSandbox(){}
		void* fnPointerMap = nullptr;
		std::mutex functionPointerCacheLock;

		uint32_t appPtrMapCounter = 0;
		std::mutex appPtrMapMutex;
		std::map<void*, void*> appPtrMap;

	public:
		static RLBoxSandbox* createSandbox(const char* sandboxRuntimePath, const char* libraryPath)
		{
			RLBoxSandbox* ret = new RLBoxSandbox();
			ret->impl_CreateSandbox(sandboxRuntimePath, libraryPath);
			return ret;
		}
		
		void destroySandbox()
		{
			this->impl_DestroySandbox();
		}

		inline auto getSandbox() -> decltype(this->impl_getSandbox())
		{
			return this->impl_getSandbox();
		}

		template<typename T>
		tainted<T*, TSandbox> mallocInSandbox(unsigned int count=1)
		{
			void* addr = this->impl_mallocInSandbox(sizeof(T) * count);
			if(!this->isValidSandboxedPointer(this->getSandboxedPointer(addr), false /* isFuncPtr */))
			{
				abort();
			}
			tainted<T*, TSandbox> ret;
			ret.field = static_cast<T*>(addr);
			return ret;
		}

		template<typename T>
		tainted_freezable<T*, TSandbox> mallocFrozenInSandbox()
		{
			void* addr = this->impl_mallocInSandbox(sizeof(T));
			if(!this->isValidSandboxedPointer(this->getSandboxedPointer(addr), false /* isFuncPtr */))
			{
				abort();
			}
			tainted_freezable<T*, TSandbox> ret;
			ret.field = static_cast<T*>(addr);
			return ret;
		}

		template <typename T, RLBOX_ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, T>)>
		void freeInSandbox(T val)
		{
			return this->impl_freeInSandbox(val.UNSAFE_Unverified());
		}

		template <typename T>
		void freeInSandbox(tainted_freezable<T*, TSandbox> val)
		{
			val->unfreeze();
			return this->impl_freeInSandbox(val.UNSAFE_Unverified());
		}

		inline size_t getTotalMemory()
		{
			return this->impl_getTotalMemory();
		}

		inline tainted<char*, TSandbox> getMaxPointer()
		{
			tainted<char*, TSandbox> ret;
			ret.field = this->impl_getMaxPointer();
			return ret;
		}

		template<typename T>
		inline void* getSandboxedPointer(T* p)
		{
			return this->impl_GetSandboxedPointer(p);
		}

		template<typename T>
		inline void* getUnsandboxedPointer(T* p)
		{
			return this->impl_GetUnsandboxedPointer(p);
		}

		inline bool isValidSandboxedPointer(const void* p, bool isFuncPtr)
		{
			return this->impl_isValidSandboxedPointer(p, isFuncPtr);
		}

		inline bool isPointerInSandboxMemoryOrNull(const void* p)
		{
			return this->impl_isPointerInSandboxMemoryOrNull(p);
		}

		inline bool isPointerInAppMemoryOrNull(const void* p)
		{
			return this->impl_isPointerInAppMemoryOrNull(p);
		}

		template <typename T, typename ... TArgs, RLBOX_ENABLE_IF(my_is_void_v<return_argument<T>> && sandbox_function_have_all_args_fundamental_or_wrapped<TArgs...>::value && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		void invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			// TODO: use std::forward?
			this->impl_InvokeFunction(fnPtr, sandbox_removeWrapper(this, params)...);
		}

		template <typename T, typename ... TArgs, RLBOX_ENABLE_IF(!my_is_void_v<return_argument<T>> && sandbox_function_have_all_args_fundamental_or_wrapped<TArgs...>::value && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		tainted<return_argument<T>, TSandbox> invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			// TODO: use std::forward?
			tainted<return_argument<T>, TSandbox> ret = sandbox_convertToUnverified<return_argument<T>>(this, this->impl_InvokeFunction(fnPtr, sandbox_removeWrapper(this, params)...));
			return ret;
		}

		template <typename T, typename ... TArgs, RLBOX_ENABLE_IF(sandbox_function_have_all_args_fundamental_or_wrapped<TArgs...>::value && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		return_argument<T> invokeWithFunctionPointerReturnAppPtr(T* fnPtr, TArgs&&... params)
		{
			auto ret = this->impl_InvokeFunctionReturnAppPtr(fnPtr, sandbox_removeWrapper(this, params)...);
			auto p_appPtrMap = getMaintainAppPtrMap();
			auto it = p_appPtrMap->find((void*) ret);
			return_argument<T> val = nullptr;
			if (it != p_appPtrMap->end()) {
				val = (return_argument<T>) it->second;
			}
			return val;
		}

		void* getFunctionPointerFromCache(const char* fnName, bool forSandboxFunction)
		{
      //if (strncmp(fnName, "vpx", 3) == 0) {
      //  printf("%s\n", fnName);
      //}
			void* fnPtr;
			std::lock_guard<std::mutex> lock(functionPointerCacheLock);
			if(!fnPointerMap)
			{
				fnPointerMap = new std::map<std::string, void*>;
			}

			auto& fnMapTyped = *(std::map<std::string, void*> *) fnPointerMap;
			auto fnPtrRef = fnMapTyped.find(fnName);

			if(fnPtrRef == fnMapTyped.end())
			{
				fnPtr = this->impl_LookupSymbol(fnName, forSandboxFunction);
				fnMapTyped[fnName] = fnPtr;
			}
			else
			{
				fnPtr = fnPtrRef->second;
			}

			return fnPtr;
		}
		template<typename T>
		inline sandbox_app_ptr_wrapper<T> app_ptr(T* arg)
		{
			auto p_appPtrMap = getMaintainAppPtrMap();

			std::lock_guard<std::mutex> lock(this->appPtrMapMutex);
			bool found = false;
			T* key;
			for (auto it = p_appPtrMap->begin(); it != p_appPtrMap->end(); it++)
			{
				if (it->second == arg)
				{
					key = (T*) it->first;
					found = true;
					break;
				}
			}

			if (!found)
			{
				this->appPtrMapCounter++;
				key = (T*)(uintptr_t)this->appPtrMapCounter;
				(*p_appPtrMap)[(void*)key] = (void*) arg;
			}

			return sandbox_app_ptr_wrapper<T>(key);
		}

		inline std::map<void*, void*>* getMaintainAppPtrMap()
		{
			return &(this->appPtrMap);
		}

		template<typename T>
		sandbox_stackarr_helper<T, TSandbox> stacktemp()
		{
			const size_t size = sizeof(T);
			T* argInSandbox = static_cast<T*>(this->impl_pushStackArr(size));
			memset((void*)argInSandbox, 0, size);

			return sandbox_stackarr_helper<T, TSandbox>(this, argInSandbox, size);
		}

		inline std::mutex* getMaintainAppPtrMapMutex()
		{
			return &(this->appPtrMapMutex);
		}

		template <typename T>
		inline sandbox_stackarr_helper<T, TSandbox> stackarr(T* arg, size_t size)
		{
			T* argInSandbox = static_cast<T*>(this->impl_pushStackArr(size));
			// static_cast drops constness
			memcpy((void*) argInSandbox, (void*) arg, size);

			sandbox_stackarr_helper<T, TSandbox> ret(this, argInSandbox, size);
			return ret;
		}
		inline sandbox_stackarr_helper<const char, TSandbox> stackarr(const char* str)
		{
			return stackarr(str, strlen(str) + 1);
		}
		inline sandbox_stackarr_helper<char, TSandbox> stackarr(char* str)
		{
			return stackarr(str, strlen(str) + 1);
		}

		template <typename T>
		inline sandbox_heaparr_helper<T, TSandbox> heaparr(T* arg, size_t size)
		{
			T* argInSandbox = static_cast<T*>(this->impl_mallocInSandbox(size));
			// static_cast drops constness
			memcpy((void*)argInSandbox, (void*)arg, size);

			sandbox_heaparr_helper<T, TSandbox> ret(this, argInSandbox);
			return ret;
		}
		inline sandbox_heaparr_helper<const char, TSandbox> heaparr(const char* str)
		{
			return heaparr(str, strlen(str) + 1);
		}
		inline sandbox_heaparr_helper<char, TSandbox> heaparr(char* str)
		{
			return heaparr(str, strlen(str) + 1);
		}

		template <typename TRet, typename... TArgs, RLBOX_ENABLE_IF(sandbox_callback_all_args_are_tainted<TSandbox, TArgs...>::value)>
		__attribute__ ((noinline))
		sandbox_callback_helper<TRet(sandbox_removeWrapper_t<TArgs>...), TSandbox> createCallback(TRet(*fnPtr)(RLBoxSandbox<TSandbox>*, TArgs...))
		{
			auto stateObject = new sandbox_callback_state<TSandbox>(this, (void*)(uintptr_t)fnPtr);
			void* callbackReciever = (void*)(uintptr_t) sandbox_callback_receiver<TSandbox, TRet, sandbox_removeWrapper_t<TArgs>...>;
			// TODO: use std::forward?
			auto callbackRegisteredAddress = this->template impl_RegisterCallback<TRet, sandbox_removeWrapper_t<TArgs>...>((void*)(uintptr_t)fnPtr, callbackReciever, (void*)stateObject);
			using fnType = TRet(sandbox_removeWrapper_t<TArgs>...);
			auto ret = sandbox_callback_helper<fnType, TSandbox>(this, (fnType*)(uintptr_t)callbackRegisteredAddress, stateObject);
			return ret;
		}
	};

	template<typename TLHS, typename TRHS, typename TSandbox, template <typename, typename> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>> && my_is_pointer_v<TLHS> && my_is_pointer_v<TRHS>)>
	inline tainted<TLHS, TSandbox> sandbox_reinterpret_cast(const TWrap<TRHS, TSandbox>& rhs) noexcept
	{
		tainted<TRHS, TSandbox> taintedVal = rhs;
		auto pret = (tainted<TLHS, TSandbox>*) &taintedVal;
		return *pret;
	}

	template<typename TSandbox, typename TRHS, typename TVal, typename TNum, template <typename, typename> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>>)>
	inline TWrap<TRHS*, TSandbox> memset(RLBoxSandbox<TSandbox>* sandbox, TWrap<TRHS*, TSandbox> ptr, TVal value, TNum num)
	{
		auto unum = rlboxUnwrapOrReturnValue(num);
		tainted<char*, TSandbox> endVal = sandbox_reinterpret_cast<char*>(ptr) + unum;
		if(unum >= sandbox->getTotalMemory() || !sandbox->isPointerInSandboxMemoryOrNull(endVal.UNSAFE_Unverified()))
		{
			printf("memset is out of bounds\n");
			abort();
		}
		std::memset((void*) ptr.UNSAFE_Unverified(), rlboxUnwrapOrReturnValue(value), unum);
		return ptr;
	}

	template<typename TSandbox, typename TRHS, typename TLHS, typename TNum, template <typename, typename> class TWrap, RLBOX_ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>>)>
	inline TWrap<TRHS*, TSandbox> memcpy(RLBoxSandbox<TSandbox>* sandbox, TWrap<TRHS*, TSandbox> dest, TLHS src, TNum num)
	{
		auto unum = rlboxUnwrapOrReturnValue(num);
		tainted<char*, TSandbox> endVal = sandbox_reinterpret_cast<char*>(dest) + unum;
		if(unum >= sandbox->getTotalMemory() || !sandbox->isPointerInSandboxMemoryOrNull(endVal.UNSAFE_Unverified()))
		{
			printf("memcpy is out of bounds\n");
			abort();
		}
		std::memcpy((void*) dest.UNSAFE_Unverified(), rlboxUnwrapOrReturnValue(src), unum);
		return dest;
	}

	#define sandbox_invoke(sandbox, fnName, ...) sandbox->invokeWithFunctionPointer((decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName, false), ##__VA_ARGS__)
	#define sandbox_invoke_return_app_ptr(sandbox, fnName, ...) sandbox->invokeWithFunctionPointerReturnAppPtr((decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName, false), ##__VA_ARGS__)
	#define sandbox_invoke_with_fnptr(sandbox, fnPtr, ...) sandbox->invokeWithFunctionPointer(fnPtr, ##__VA_ARGS__)
	#define sandbox_function(sandbox, fnName) sandbox_convertToUnverified<decltype(fnName)*>(sandbox, (decltype(fnName)*) sandbox->getFunctionPointerFromCache(#fnName, true))
	#undef RLUNUSED
}
#endif
