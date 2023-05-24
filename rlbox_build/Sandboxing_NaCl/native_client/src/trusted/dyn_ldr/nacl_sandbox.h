#ifndef NACL_SANDBOX_API
#define NACL_SANDBOX_API

#include <type_traits>
#include <functional>
#ifndef NACL_SANDBOX_API_NO_STL_DS
	#include <map>
#endif
#include <stdio.h>

#include "dyn_ldr_lib.h"
#ifndef NACL_SANDBOX_API_NO_OPTIONAL
	#include "helpers/optional.hpp"

	using nonstd::optional;
	using nonstd::nullopt;
#endif

//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
#define UNUSED(x) UNUSED_PACK_HELPER {x}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template< class T >
using my_remove_pointer_t = typename std::remove_pointer<T>::type;

template< class T >
using my_remove_reference_t = typename std::remove_reference<T>::type;

template< class T >
using my_remove_volatile_t = typename std::remove_volatile<T>::type;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename T>
inline T getMaskedField(unsigned long sandboxMask, T arg)
{
	unsigned long lowBits = ((uintptr_t)arg) & ((unsigned long)0xFFFFFFFF);
	if(lowBits == 0)
	{
		return 0;
	}

	return (T)(sandboxMask | lowBits);
}

template<typename T>
inline T getSandboxedField(T arg)
{
	T lowBits = (T) (((uintptr_t)arg) & ((unsigned long)0xFFFFFFFF));
	return lowBits;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TFunc>
class sandbox_callback_helper;

template<typename TFunc>
class sandbox_function_helper;

template<typename T, typename T2=void>
struct sandbox_unverified_data;

template<typename T, typename T2=void>
struct unverified_data;

template<typename T>
struct sandbox_unverified_data<T, typename std::enable_if<!std::is_pointer<T>::value && !std::is_class<T>::value && !std::is_reference<T>::value && !std::is_array<T>::value>::type>
{
	volatile T field;

	inline T UNSAFE_noVerify() const
	{
		return field;
	}

	inline T sandbox_copyAndVerify(std::function<T(T)> verify_fn) const
	{
		return verify_fn(field);
	}

	inline T sandbox_copyAndVerify(std::function<bool(T)> verify_fn, T defaultValue) const
	{
		return verify_fn(field)? field : defaultValue;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		//printf("Sbox Value - Wrapped Assignment\n");
		field = arg.field;
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		//printf("Sbox Value - Unverified Assignment\n");
		field = arg.field;
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const TRHS& arg) noexcept
	{
		//printf("Sbox Value - Direct Assignment\n");
		field = arg;
		return *this;
	}

	inline unverified_data<T*> operator&() const 
	{
		unverified_data<T*> ret = getMaskedAddress();
		return ret;
	}

	inline T getMasked() const
	{
		return field;
	}

	inline T* getMaskedAddress() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (T*) &field);
	}
};

template<typename T>
struct unverified_data<T, typename std::enable_if<!std::is_pointer<T>::value && !std::is_class<T>::value && !std::is_reference<T>::value && !std::is_array<T>::value>::type>
{
	T field;

	unverified_data() = default;
	unverified_data(unverified_data<T>& p) { field = p.field; }
	unverified_data(const unverified_data<T>& p) { field = p.field; }
	unverified_data(sandbox_unverified_data<T>& p) { field = p.field; }
	unverified_data(const sandbox_unverified_data<T>& p) { field = p.field; }

	template<typename Arg, typename... Args, ENABLE_IF(!std::is_same<Arg, unverified_data<T>>::value)>
	unverified_data(Arg&& arg, Args&&... args) : field(std::forward<Arg>(arg), std::forward<Args>(args)...) {}

	inline T UNSAFE_noVerify() const
	{
		return field;
	}

	inline T sandbox_copyAndVerify(std::function<T(T)> verify_fn) const
	{
		return verify_fn(field);
	}

	inline T sandbox_copyAndVerify(std::function<bool(T)> verify_fn, T defaultValue) const
	{
		return verify_fn(field)? field : defaultValue;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		//printf("Value - Wrapped Assignment\n");
		field = arg.field;
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		//printf("Value - Unverified Assignment\n");
		field = arg.field;
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const TRHS& arg) noexcept
	{
		//printf("Value - Direct Assignment\n");
		field = arg;
		return *this;
	}

	inline T getMasked() const
	{
		return field;
	}
};

template<typename T>
struct sandbox_unverified_data<T, typename std::enable_if<std::is_array<T>::value>::type>
{
	volatile T field;
	using arrElemType = my_remove_volatile_t<my_remove_reference_t<decltype(*field)>>;

	inline arrElemType* sandbox_onlyVerifyAddress() const
	{
		arrElemType* maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	inline bool sandbox_copyAndVerify(arrElemType* copy, size_t sizeOfCopy, std::function<bool(char*, size_t)> verify_fn) const
	{
		arrElemType* maskedFieldPtr = getMasked();

		if(sizeOfCopy >= sizeof(T))
		{
			memcpy(copy, maskedFieldPtr, sizeof(T));
			if(verify_fn(copy, sizeof(T)))
			{
				return true;
			}
		}

		//something went wrong, clear the target for safety
		memset(copy, 0, sizeof(T));
		return false;
	}

	inline unverified_data<T*> operator&() const 
	{
		unverified_data<T*> ret = getMaskedAddress();
		return ret;
	}

	inline sandbox_unverified_data<arrElemType>& operator[] (int x) const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		arrElemType* locPtr = &(field[x]);
		locPtr = getMaskedField(sandboxMask, locPtr);
		return *((sandbox_unverified_data<arrElemType> *) locPtr);
	}

	inline arrElemType* getMasked() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (arrElemType*) field);
	}

	inline T* getMaskedAddress() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (T*) &field);
	}
};

template<typename T>
struct unverified_data<T, typename std::enable_if<std::is_array<T>::value>::type>
{
	T field;
	using arrElemType = my_remove_reference_t<decltype(*field)>;

	inline arrElemType* sandbox_onlyVerifyAddress() const
	{
		arrElemType* maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	inline bool sandbox_copyAndVerify(arrElemType* copy, size_t sizeOfCopy, std::function<bool(arrElemType*, size_t)> verify_fn) const
	{
		arrElemType* maskedFieldPtr = getMasked();

		if(sizeOfCopy >= sizeof(T))
		{
			memcpy(copy, maskedFieldPtr, sizeof(T));
			if(verify_fn(copy, sizeof(T)))
			{
				return true;
			}
		}

		//something went wrong, clear the target for safety
		memset(copy, 0, sizeof(T));
		return false;
	}

	inline sandbox_unverified_data<arrElemType>& operator[] (int x) const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		arrElemType* locPtr = &(field[x]);
		locPtr = getMaskedField(sandboxMask, locPtr);
		return *((sandbox_unverified_data<arrElemType> *) locPtr);
	}

	inline operator sandbox_unverified_data<arrElemType>*() const 
	{
		return (sandbox_unverified_data<arrElemType>*) field;
	}

	inline arrElemType* getMasked() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (arrElemType*) field);
	}
};

template<typename T>
struct sandbox_unverified_data<T, typename std::enable_if<std::is_pointer<T>::value && !std::is_reference<T>::value && !std::is_array<T>::value && !std::is_function<my_remove_pointer_t<T>>::value>::type>
{
	volatile T field;

	//Only validate one level of indirection, eg int*, by returning the derefed value as we have to get a copy of the actual object

	//All *
	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value)>
	inline U sandbox_onlyVerifyAddress() const
	{
		U maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value)>
	inline U sandbox_onlyVerifyAddressRange(size_t size) const
	{
		uintptr_t maskedFieldPtr = (uintptr_t) getMasked();
		uintptr_t maskedFieldPtrEnd = (uintptr_t) (maskedFieldPtr + size);

		if((maskedFieldPtr > maskedFieldPtrEnd) || 
			(maskedFieldPtr & 0xFFFFFFFF00000000) != (maskedFieldPtrEnd & 0xFFFFFFFF00000000)
		)
		{
			return nullptr;
		}

		return (U) maskedFieldPtr;
	}

	inline T sandbox_copyAndVerifyUnsandboxedPointer(std::function<T(T)> verify_fn) const
	{
		return verify_fn(field);
	}

	inline T sandbox_copyAndVerifyUnsandboxedPointer(std::function<bool(T)> verify_fn, T defaultValue) const
	{
		T fieldCopy = field;
		return verify_fn(fieldCopy)? fieldCopy : defaultValue ;
	}

	//Primitive*
	template<typename U=T, ENABLE_IF(!std::is_pointer<my_remove_pointer_t<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<my_remove_pointer_t<U>(T)> verify_fn) const
	{
		U maskedFieldPtr = getMasked();
		return verify_fn(maskedFieldPtr);
	}


	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<bool(my_remove_pointer_t<U>)> verify_fn, my_remove_pointer_t<U> defaultValue) const
	{
		U maskedFieldPtr = getMasked();
		if(maskedFieldPtr == nullptr)
		{
			return defaultValue;
		}

		my_remove_pointer_t<U> maskedField = *maskedFieldPtr;
		return verify_fn(maskedField)? maskedField : defaultValue;
	}

	#ifndef NACL_SANDBOX_API_NO_OPTIONAL
		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<optional<my_remove_pointer_t<U>>(U)> verify_fn) const
		{
			U maskedFieldPtr = getMasked();
			return verify_fn(maskedFieldPtr);
		}

		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<bool(my_remove_pointer_t<U>)> verify_fn, optional<my_remove_pointer_t<U>> defaultValue) const
		{
			U maskedFieldPtr = getMasked();
			if(maskedFieldPtr == nullptr)
			{
				return defaultValue;
			}

			my_remove_pointer_t<U> maskedField = *maskedFieldPtr;
			return verify_fn(maskedField)? maskedField : defaultValue;
		}
	#endif

	//Class*
	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<my_remove_pointer_t<U>(sandbox_unverified_data<my_remove_pointer_t<U>> *)> verify_fn) const
	{
		auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
		return verify_fn(maskedFieldPtr);
	}

	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<bool(sandbox_unverified_data<my_remove_pointer_t<U>>)> verify_fn, my_remove_pointer_t<U> defaultValue) const
	{
		auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
		if(maskedFieldPtr == nullptr)
		{
			return defaultValue;
		}

		auto maskedField = *maskedFieldPtr;
		return verify_fn(maskedField)? maskedField : defaultValue;
	}

	#ifndef NACL_SANDBOX_API_NO_OPTIONAL
		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<optional<my_remove_pointer_t<U>>(U)> verify_fn) const
		{
			auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
			return verify_fn(maskedFieldPtr);
		}

		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<bool(sandbox_unverified_data<my_remove_pointer_t<U>>)> verify_fn, optional<my_remove_pointer_t<U>> defaultValue) const
		{
			auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
			if(maskedFieldPtr == nullptr)
			{
				return defaultValue;
			}

			auto maskedField = *maskedFieldPtr;
			return verify_fn(maskedField)? maskedField : defaultValue;
		}
	#endif

	inline T sandbox_copyAndVerifyArray(std::function<bool(T)> verify_fn, unsigned int elementCount, T defaultValue) const
	{
		typedef typename std::remove_pointer<T>::type nonPointerType;
		typedef typename std::remove_const<nonPointerType>::type nonPointerConstType;

		T maskedFieldPtr = getMasked();
		uintptr_t arrayEnd = ((uintptr_t)maskedFieldPtr) + sizeof(std::remove_pointer<T>) * elementCount;

		//check for overflow
		if((arrayEnd & 0xFFFFFFFF00000000) != (((uintptr_t)maskedFieldPtr) & 0xFFFFFFFF00000000))
		{
			return defaultValue;
		}

		nonPointerConstType* copy = new nonPointerConstType[elementCount];
		memcpy(copy, maskedFieldPtr, sizeof(nonPointerConstType) * elementCount);
		return verify_fn(copy)? copy : defaultValue;
	}

	inline T sandbox_copyAndVerifyString(std::function<bool(T)> verify_fn, T defaultValue) const
	{
		T maskedFieldPtr = getMasked();
		unsigned int elementCount = strlen(maskedFieldPtr) + 1;

		typedef typename std::remove_pointer<T>::type nonPointerType;
		typedef typename std::remove_const<nonPointerType>::type nonPointerConstType;

		uintptr_t arrayEnd = ((uintptr_t)maskedFieldPtr) + sizeof(std::remove_pointer<T>) * elementCount;

		//check for overflow
		if((arrayEnd & 0xFFFFFFFF00000000) != (((uintptr_t)maskedFieldPtr) & 0xFFFFFFFF00000000))
		{
			return defaultValue;
		}

		nonPointerConstType* copy = new nonPointerConstType[elementCount];
		memcpy(copy, maskedFieldPtr, sizeof(nonPointerConstType) * elementCount);
		//ensure we have a trailing null
		copy[elementCount - 1] = '\0';
		return verify_fn(copy)? copy : defaultValue;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		field = getSandboxedField(arg.field);
		//printf("Sbox Pointer - Wrapped Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		field = getSandboxedField(arg.field);
		//printf("Sbox Pointer - Unverified Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const TRHS& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline sandbox_unverified_data<T>& operator=(const std::nullptr_t& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline unverified_data<T> operator+(int x)
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		unverified_data<T> result = getMaskedField(sandboxMask, (T)(field + x));
		return result;
	}

	inline unverified_data<T*> operator&() const 
	{
		unverified_data<T*> ret = getMaskedAddress();
		return ret;
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>& operator*() const
	{
		auto r = (sandbox_unverified_data<my_remove_pointer_t<T>>*) getMasked();
		return *r;
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>* operator->()
	{
		return (sandbox_unverified_data<my_remove_pointer_t<T>>*) getMasked();
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>& operator[] (int x) const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		T locPtr = &(field[x]);
		locPtr = getMaskedField(sandboxMask, locPtr);
		return *((sandbox_unverified_data<my_remove_pointer_t<T>> *) locPtr);
	}

	inline T getMasked() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, field);
	}

	inline T* getMaskedAddress() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (T*) &field);
	}
};

template<typename T>
struct unverified_data<T, typename std::enable_if<std::is_pointer<T>::value && !std::is_reference<T>::value && !std::is_array<T>::value && !std::is_function<my_remove_pointer_t<T>>::value>::type>
{
	T field;

	unverified_data() = default;
	unverified_data(unverified_data<T>& p) { field = p.getMasked(); }
	unverified_data(const unverified_data<T>& p) { field = p.getMasked(); }
	unverified_data(sandbox_unverified_data<T>& p) { field = p.getMasked(); }
	unverified_data(const sandbox_unverified_data<T>& p) { field = p.getMasked(); }

	template<typename Arg, typename... Args, ENABLE_IF(!std::is_same<Arg, unverified_data<T>>::value)>
	unverified_data(Arg&& arg, Args&&... args) : field(std::forward<Arg>(arg), std::forward<Args>(args)...) {}

	//Only validate one level of indirection, eg int*, by returning the derefed value as we have to get a copy of the actual object

	//All *
	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value)>
	inline U sandbox_onlyVerifyAddress() const
	{
		U maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value)>
	inline U sandbox_onlyVerifyAddressRange(size_t size) const
	{
		uintptr_t maskedFieldPtr = (uintptr_t) getMasked();
		uintptr_t maskedFieldPtrEnd = (uintptr_t) maskedFieldPtr + size;

		if((maskedFieldPtr > maskedFieldPtrEnd) || 
			(maskedFieldPtr & 0xFFFFFFFF00000000) != (maskedFieldPtrEnd & 0xFFFFFFFF00000000)
		)
		{
			return nullptr;
		}

		return (U) maskedFieldPtr;
	}

	//Primitive*
	template<typename U=T, ENABLE_IF(!std::is_pointer<my_remove_pointer_t<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<my_remove_pointer_t<U>(T)> verify_fn) const
	{
		U maskedFieldPtr = getMasked();
		return verify_fn(maskedFieldPtr);
	}

	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<bool(my_remove_pointer_t<U>)> verify_fn, my_remove_pointer_t<U> defaultValue) const
	{
		U maskedFieldPtr = getMasked();
		if(maskedFieldPtr == nullptr)
		{
			return defaultValue;
		}

		my_remove_pointer_t<U> maskedField = *maskedFieldPtr;
		return verify_fn(maskedField)? maskedField : defaultValue;
	}

	#ifndef NACL_SANDBOX_API_NO_OPTIONAL
		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<optional<my_remove_pointer_t<U>>(U)> verify_fn) const
		{
			U maskedFieldPtr = getMasked();
			return verify_fn(maskedFieldPtr);
		}

		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && !std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<bool(my_remove_pointer_t<U>)> verify_fn, optional<my_remove_pointer_t<U>> defaultValue) const
		{
			U maskedFieldPtr = getMasked();
			if(maskedFieldPtr == nullptr)
			{
				return defaultValue;
			}

			my_remove_pointer_t<U> maskedField = *maskedFieldPtr;
			return verify_fn(maskedField)? maskedField : defaultValue;
		}
	#endif

	//Class*
	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<my_remove_pointer_t<U>(sandbox_unverified_data<my_remove_pointer_t<U>> *)> verify_fn) const
	{
		auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
		return verify_fn(maskedFieldPtr);
	}

	template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
	inline my_remove_pointer_t<U> sandbox_copyAndVerify(std::function<bool(sandbox_unverified_data<my_remove_pointer_t<U>>)> verify_fn, my_remove_pointer_t<U> defaultValue) const
	{
		auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
		if(maskedFieldPtr == nullptr)
		{
			return defaultValue;
		}

		auto maskedField = *maskedFieldPtr;
		return verify_fn(maskedField)? maskedField : defaultValue;
	}

	#ifndef NACL_SANDBOX_API_NO_OPTIONAL
		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<optional<my_remove_pointer_t<U>>(U)> verify_fn) const
		{
			auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
			return verify_fn(maskedFieldPtr);
		}

		template<typename U=T, ENABLE_IF(!std::is_pointer<std::remove_pointer<U>>::value && std::is_class<my_remove_pointer_t<U>>::value)>
		inline optional<my_remove_pointer_t<U>> sandbox_copyAndVerify(std::function<bool(sandbox_unverified_data<my_remove_pointer_t<U>>)> verify_fn, optional<my_remove_pointer_t<U>> defaultValue) const
		{
			auto maskedFieldPtr = (sandbox_unverified_data<my_remove_pointer_t<U>> *) getMasked();
			if(maskedFieldPtr == nullptr)
			{
				return defaultValue;
			}

			auto maskedField = *maskedFieldPtr;
			return verify_fn(maskedField)? maskedField : defaultValue;
		}
	#endif

	inline T sandbox_copyAndVerifyArray(std::function<bool(T)> verify_fn, unsigned int elementCount, T defaultValue) const
	{
		typedef typename std::remove_pointer<T>::type nonPointerType;
		typedef typename std::remove_const<nonPointerType>::type nonPointerConstType;

		T maskedFieldPtr = getMasked();
		uintptr_t arrayEnd = ((uintptr_t)maskedFieldPtr) + sizeof(std::remove_pointer<T>) * elementCount;

		//check for overflow
		if((arrayEnd & 0xFFFFFFFF00000000) != (((uintptr_t)maskedFieldPtr) & 0xFFFFFFFF00000000))
		{
			return defaultValue;
		}

		nonPointerConstType* copy = new nonPointerConstType[elementCount];
		memcpy(copy, maskedFieldPtr, sizeof(nonPointerConstType) * elementCount);
		return verify_fn(copy)? copy : defaultValue;
	}

	inline T sandbox_copyAndVerifyString(std::function<bool(T)> verify_fn, T defaultValue) const
	{
		T maskedFieldPtr = getMasked();
		unsigned int elementCount = strlen(maskedFieldPtr) + 1;
		
		typedef typename std::remove_pointer<T>::type nonPointerType;
		typedef typename std::remove_const<nonPointerType>::type nonPointerConstType;

		uintptr_t arrayEnd = ((uintptr_t)maskedFieldPtr) + sizeof(std::remove_pointer<T>) * elementCount;

		//check for overflow
		if((arrayEnd & 0xFFFFFFFF00000000) != (((uintptr_t)maskedFieldPtr) & 0xFFFFFFFF00000000))
		{
			return defaultValue;
		}

		nonPointerConstType* copy = new nonPointerConstType[elementCount];
		memcpy(copy, maskedFieldPtr, sizeof(nonPointerConstType) * elementCount);
		//ensure we have a trailing null
		copy[elementCount - 1] = '\0';
		return verify_fn(copy)? copy : defaultValue;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		field = arg.getMasked();
		//printf("Sbox Pointer - Wrapped Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		field = arg.getMasked();
		//printf("Sbox Pointer - Unverified Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const TRHS& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline unverified_data<T>& operator=(const std::nullptr_t& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline bool operator ==(const std::nullptr_t&) const
	{
		return field == 0;
	}

	inline bool operator !=(const std::nullptr_t&) const
	{
		return field != 0;
	}

	inline unverified_data<T> operator+(int x)
	{
		unsigned long sandboxMask = ((uintptr_t) field) & ((unsigned long)0xFFFFFFFF00000000);
		unverified_data<T> result = getMaskedField(sandboxMask, (T)(field + x));
		return result;
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>& operator*() const
	{
		auto r = (sandbox_unverified_data<my_remove_pointer_t<T>>*) getMasked();
		return *r;
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>* operator->()
	{
		return (sandbox_unverified_data<my_remove_pointer_t<T>>*) getMasked();
	}

	inline operator sandbox_unverified_data<my_remove_pointer_t<T>>*() const 
	{
		return (sandbox_unverified_data<my_remove_pointer_t<T>>*) field;
	}

	inline operator bool() const
	{
		return getMasked() != 0;
	}

	inline sandbox_unverified_data<my_remove_pointer_t<T>>& operator[] (int x) const
	{
		unsigned long sandboxMask = ((uintptr_t) field) & ((unsigned long)0xFFFFFFFF00000000);
		T locPtr = &(field[x]);
		locPtr = getMaskedField(sandboxMask, locPtr);
		return *((sandbox_unverified_data<my_remove_pointer_t<T>> *) locPtr);
	}

	inline T getMasked() const
	{
		return field;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class TData> 
class unverified_data_unwrapper {
public:
    TData value_field;
};

template<typename T>
unverified_data_unwrapper<T> sandbox_removeUnverified_helper(sandbox_unverified_data<T>);
template<typename T>
unverified_data_unwrapper<T> sandbox_removeUnverified_helper(unverified_data<T>);

template<typename T>
unverified_data<T> sandbox_removeUnverified_helper(T);

template <typename T>
using sandbox_removeUnverified = decltype(sandbox_removeUnverified_helper(std::declval<T>()).value_field);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TRet, typename... TArgs>
TRet(*sandbox_remove_unverified_data_on_args_helper(TRet(*)(TArgs...)))(sandbox_removeUnverified<TArgs>...);

template<typename T>
using sandbox_remove_unverified_data_on_args = decltype(sandbox_remove_unverified_data_on_args_helper(std::declval<T>()));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct sandbox_unverified_data<T, typename std::enable_if<std::is_function<my_remove_pointer_t<T>>::value>::type>
{
	volatile T field;

	inline T sandbox_onlyVerifyAddress() const
	{
		T maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, sandbox_remove_unverified_data_on_args<TRHS*>>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_callback_helper<TRHS>* arg) noexcept
	{
		field = (T) arg->callbackRegisteredAddress;
		//printf("Sbox fn pointer - CB Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS*>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_function_helper<TRHS> arg) noexcept
	{
		field = getSandboxedField(arg.address);
		//printf("Sbox fn pointer - SB Fn Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		field = getSandboxedField(arg.field);
		//printf("Sbox Pointer - Wrapped Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	sandbox_unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		field = getSandboxedField(arg.field);
		//printf("Sbox Pointer - Unverified Assignment\n");
		return *this;
	}

	inline sandbox_unverified_data<T>& operator=(const std::nullptr_t& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline unverified_data<T*> operator&() const 
	{
		unverified_data<T*> ret = getMaskedAddress();
		return ret;
	}

	inline T getMasked() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, field);
	}

	inline T* getMaskedAddress() const
	{
		unsigned long sandboxMask = ((uintptr_t) &field) & ((unsigned long)0xFFFFFFFF00000000);
		return getMaskedField(sandboxMask, (T*) &field);
	}
};

template<typename T>
struct unverified_data<T, typename std::enable_if<std::is_function<my_remove_pointer_t<T>>::value>::type>
{
	T field;

	inline T sandbox_onlyVerifyAddress() const
	{
		T maskedFieldPtr = getMasked();
		return maskedFieldPtr;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, sandbox_remove_unverified_data_on_args<TRHS*>>::value,
	unverified_data<T>&>::type operator=(const sandbox_callback_helper<TRHS>* arg) noexcept
	{
		field = (T) arg->callbackRegisteredAddress;
		//printf("Sbox fn pointer - CB Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS*>::value,
	unverified_data<T>&>::type operator=(const sandbox_function_helper<TRHS> arg) noexcept
	{
		field = arg.address;
		//printf("Sbox fn pointer - SB Fn Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept
	{
		field = arg.getMasked();
		//printf("Sbox Pointer - Wrapped Assignment\n");
		return *this;
	}

	template<typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,
	unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept
	{
		field = arg.getMasked();
		//printf("Sbox Pointer - Unverified Assignment\n");
		return *this;
	}

	inline unverified_data<T>& operator=(const std::nullptr_t& arg) noexcept
	{
		field = arg;
		//printf("Sbox Pointer - Direct Assignment\n");
		return *this;
	}

	inline bool operator ==(const std::nullptr_t&) const
	{
		return field == 0;
	}

	inline bool operator !=(const std::nullptr_t&) const
	{
		return field != 0;
	}

	inline operator bool() const
	{
		return getMasked() != 0;
	}

	inline T getMasked() const
	{
		return field;
	}
};


template<typename L, typename R, ENABLE_IF(!std::is_array<L>::value && !std::is_union<L>::value && !std::is_function<my_remove_pointer_t<R>>::value)>
inline void assignValue(L& lhs, R rhs)
{
	lhs = (L) rhs;
}

template<typename L, typename R, ENABLE_IF(std::is_function<my_remove_pointer_t<R>>::value)>
inline void assignValue(L& lhs, R rhs)
{
	//we do not assign function pointers while copying structs.
	//this have to be assigned by assigning a value of type sandbox_callback_helper
	UNUSED(lhs);
	UNUSED(rhs);
}

template<typename L, typename R, ENABLE_IF(std::is_array<L>::value)>
inline void assignValue(L& lhs, R rhs){
	memcpy((void *) lhs, (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_array<L>::value)>
inline void assignValue(unverified_data<L>& lhs, R rhs){
	memcpy((void *) lhs.field, (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_array<L>::value)>
inline void assignValue(sandbox_unverified_data<L>& lhs, R rhs){
	memcpy((void *) lhs.field, (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_array<L>::value)>
inline void assignValue(unverified_data<L>& lhs, unverified_data<R> rhs){
	memcpy((void *) lhs.field, (void *) rhs.field, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_array<L>::value)>
inline void assignValue(sandbox_unverified_data<L>& lhs, sandbox_unverified_data<R> rhs){
	memcpy((void *) lhs.field, (void *) rhs.field, sizeof(lhs));
}

//
template<typename L, typename R, ENABLE_IF(std::is_union<L>::value)>
inline void assignValue(L& lhs, R rhs){
	memcpy((void *) &lhs, (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_union<L>::value)>
inline void assignValue(unverified_data<L>& lhs, R rhs){
	memcpy((void *) &(lhs.field), (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_union<L>::value)>
inline void assignValue(sandbox_unverified_data<L>& lhs, R rhs){
	memcpy((void *) &(lhs.field), (void *) rhs, sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_union<L>::value)>
inline void assignValue(unverified_data<L>& lhs, unverified_data<R> rhs){
	memcpy((void *) &(lhs.field), (void *) &(rhs.field), sizeof(lhs));
}

template<typename L, typename R, ENABLE_IF(std::is_union<L>::value)>
inline void assignValue(sandbox_unverified_data<L>& lhs, sandbox_unverified_data<R> rhs){
	memcpy((void *) &(lhs.field), (void *) &(rhs.field), sizeof(lhs));
}


#define sandbox_unverified_data_createField(fieldType, fieldName) sandbox_unverified_data<fieldType> fieldName;
#define unverified_data_createField(fieldType, fieldName) unverified_data<fieldType> fieldName;
#define sandbox_unverified_data_noOp() 
#define sandbox_unverified_data_fieldAssign(fieldType, fieldName) assignValue(fieldName, arg.fieldName);
#define sandbox_unverified_data_fieldAssignMasked(fieldType, fieldName) assignValue(fieldName, arg.fieldName.getMasked());
#define sandbox_unverified_data_copyAssignMasked(fieldType, fieldName) assignValue(ret.fieldName, fieldName.getMasked());
 
#define sandbox_unverified_data_specialization(T, libId) \
template<> \
struct sandbox_unverified_data<T> \
{ \
	sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_createField, sandbox_unverified_data_noOp) \
 \
 	inline T UNSAFE_noVerify() const \
	{ \
		return *((T*)this); \
	} \
 \
	inline T sandbox_copyAndVerify(std::function<T(sandbox_unverified_data<T>&)> verify_fn) \
	{ \
		return verify_fn(*this); \
	} \
 \
	inline T sandbox_copyAndVerify(std::function<bool(sandbox_unverified_data<T>&)> verify_fn, T defaultValue) \
	{ \
		return verify_fn(*this)? *((T*)this) : defaultValue; \
	} \
 \
	template<typename TRHS> \
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value, \
	sandbox_unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept \
	{ \
		/*printf("Sbox Struct - Wrapped Assignment\n");*/ \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssign, sandbox_unverified_data_noOp) \
		return *this; \
	} \
 \
 	/* template<typename TRHS> */ \
	/* inline typename std::enable_if<std::is_assignable<T&, TRHS>::value,*/ \
	/* sandbox_unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept*/ \
	/*{*/ \
	/*	printf("Sbox Struct - Unverified Assignment\n");*/ \
	/*	sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssign, sandbox_unverified_data_noOp)*/ \
	/*	return *this;*/ \
	/*}*/ \
 \
	template<typename TRHS> \
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value, \
	sandbox_unverified_data<T>&>::type operator=(const TRHS& arg) noexcept \
	{ \
		/*printf("Sbox Struct - Direct Assignment\n");*/ \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssign, sandbox_unverified_data_noOp) \
		return *this; \
	} \
 \
	inline unverified_data<T*> operator&() const \
	{ \
		unverified_data<T*> ret = getMaskedAddress(); \
		return ret; \
	} \
 \
	inline T getMasked() const \
	{ \
		T ret; \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_copyAssignMasked, sandbox_unverified_data_noOp) \
		return ret; \
	} \
 \
	inline T* getMaskedAddress() const \
	{ \
		unsigned long sandboxMask = ((uintptr_t) this) & ((unsigned long)0xFFFFFFFF00000000); \
		return getMaskedField(sandboxMask, (T*) this); \
	} \
}; \
 \
template<> \
struct unverified_data<T> \
{ \
	sandbox_fields_reflection_##libId##_class_##T(unverified_data_createField, sandbox_unverified_data_noOp) \
 \
 	inline T UNSAFE_noVerify() const \
	{ \
		return *((T*)this); \
	} \
 \
	inline T sandbox_copyAndVerify(std::function<T(unverified_data<T>&)> verify_fn) \
	{ \
		return verify_fn(*this); \
	} \
 \
	inline T sandbox_copyAndVerify(std::function<bool(unverified_data<T>&)> verify_fn, T defaultValue) \
	{ \
		return verify_fn(*this)? *((T*)this) : defaultValue; \
	} \
 \
	template<typename TRHS> \
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value, \
	unverified_data<T>&>::type operator=(const sandbox_unverified_data<TRHS>& arg) noexcept \
	{ \
		/*printf("Struct - Wrapped Assignment\n");*/ \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssignMasked, sandbox_unverified_data_noOp) \
		return *this; \
	} \
 \
	template<typename TRHS> \
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value, \
	unverified_data<T>&>::type operator=(const unverified_data<TRHS>& arg) noexcept \
	{ \
		/*printf("Struct - Unverified Assignment\n");*/ \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssign, sandbox_unverified_data_noOp) \
		return *this; \
	} \
 \
 	template<typename TRHS> \
	inline typename std::enable_if<std::is_assignable<T&, TRHS>::value, \
	unverified_data<T>&>::type operator=(const TRHS& arg) noexcept \
	{ \
		/*printf("Struct - Direct Assignment\n");*/ \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_fieldAssign, sandbox_unverified_data_noOp) \
		return *this; \
	} \
 \
	inline T getMasked() const \
	{ \
		T ret; \
		sandbox_fields_reflection_##libId##_class_##T(sandbox_unverified_data_copyAssignMasked, sandbox_unverified_data_noOp) \
		return ret; \
	} \
};

#define sandbox_nacl_load_library_api(libId) sandbox_fields_reflection_##libId##_allClasses(sandbox_unverified_data_specialization)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename Ret, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(*) (Rest...));

template<typename Ret, typename F, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(F::*) (Rest...));

template<typename Ret, typename F, typename... Rest>
std::tuple<Rest...> fn_parameters_helper(Ret(F::*) (Rest...) const);

template <typename F>
decltype(fn_parameters_helper(&F::operator())) fn_parameters_helper(F);

template <typename T>
using fn_parameters = decltype(fn_parameters_helper(std::declval<T>()));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

inline void sandbox_error(const char* msg)
{
	printf("Sandbox error: %s\n", msg);
	exit(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class sandbox_stackarr_helper
{
public:
	T* arr;
	size_t size;
};

template <typename T>
inline sandbox_stackarr_helper<T> sandbox_stackarr(T* p_arr, size_t size) 
{ 
	sandbox_stackarr_helper<T> ret;
	ret.arr = p_arr;
	ret.size = size; 
	return ret; 
}

inline sandbox_stackarr_helper<const char> sandbox_stackarr(const char* p_arr) 
{ 
	return sandbox_stackarr(p_arr, strlen(p_arr) + 1); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class sandbox_heaparr_helper
{
public:
	NaClSandbox* sandbox;
	T* arr;

	~sandbox_heaparr_helper()
	{
		//printf("sandbox_heaparr_helper destructor called\n");
		freeInSandbox(sandbox, (void *)arr);
	}
};

template <typename T>
inline sandbox_heaparr_helper<T>* sandbox_heaparr(NaClSandbox* sandbox, T* arg, size_t size)
{
	T* argInSandbox = (T *) mallocInSandbox(sandbox, size);
	memcpy((void*) argInSandbox, (void*) arg, size);

	auto ret = new sandbox_heaparr_helper<T>();
	ret->sandbox = sandbox;
	ret->arr = argInSandbox;
	return ret; 
}

inline sandbox_heaparr_helper<const char>* sandbox_heaparr(NaClSandbox* sandbox, const char* str)
{
	return sandbox_heaparr(sandbox, str, strlen(str) + 1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template <typename T>
class sandbox_unsandboxed_ptr_helper
{
public:
	T field;
};

template<typename T, ENABLE_IF(std::is_pointer<T>::value)>
inline sandbox_unsandboxed_ptr_helper<T> sandbox_unsandboxed_ptr(T arg)
{
	sandbox_unsandboxed_ptr_helper<T> ret;
	ret.field = arg;
	return ret;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline typename std::enable_if<std::is_pointer<T>::value && !std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(NaClSandbox* sandbox, T retRaw)
{
	//printf("got a pointer return\n");
	auto retRawMasked = getMaskedField(getSandboxMemoryBase(sandbox), retRaw);
	auto retRawPtr = (unverified_data<T> *) &retRawMasked;
	return *retRawPtr;
}

template <typename T>
inline typename std::enable_if<std::is_class<T>::value && !std::is_reference<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(NaClSandbox* sandbox, T& retRaw)
{
	//printf("got a class return\n");
	//structs are returned as a pointer
	auto retRawMasked = getMaskedField(getSandboxMemoryBase(sandbox), &retRaw);
	auto retRawPtr = (sandbox_unverified_data<T> *) retRawMasked;
	unverified_data<T> ret;
	ret = *retRawPtr;
	return ret;
}

template <typename T>
inline typename std::enable_if<std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(NaClSandbox* sandbox, T retRaw)
{
	//printf("got a array return\n");
	//arrays are returned by pointer but are unverified_data<struct Foo> is copied by value
	auto retRawMasked = getMaskedField(getSandboxMemoryBase(sandbox), (void*) retRaw);
	auto retRawPtr = (unverified_data<T> *) retRawMasked;
	return *retRawPtr;
}

template <typename T>
inline typename std::enable_if<!std::is_pointer<T>::value && !std::is_void<T>::value && !std::is_class<T>::value && !std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(NaClSandbox* sandbox, T retRaw)
{
	//printf("got a value return\n");
	UNUSED(sandbox);
	auto retRawPtr = (unverified_data<T> *) &retRaw;
	return *retRawPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TArg>
inline typename std::enable_if<std::is_array<TArg>::value,
my_remove_reference_t<decltype(*std::declval<TArg>())>*
>::type sandbox_get_callback_param_nowrapper(NaClSandbox_Thread* threadData)
{
	//printf("callback pointer arg\n");
	return COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, my_remove_reference_t<decltype(*std::declval<TArg>())>*);
}


template<typename TArg>
inline typename std::enable_if<std::is_pointer<TArg>::value && !std::is_array<TArg>::value,
TArg>::type sandbox_get_callback_param_nowrapper(NaClSandbox_Thread* threadData)
{
	//printf("callback pointer arg\n");
	return COMPLETELY_UNTRUSTED_CALLBACK_PTR_PARAM(threadData, TArg);
}

template<typename TArg>
inline typename std::enable_if<!std::is_pointer<TArg>::value,
TArg>::type sandbox_get_callback_param_nowrapper(NaClSandbox_Thread* threadData)
{
	//printf("callback value arg\n");
	return COMPLETELY_UNTRUSTED_CALLBACK_STACK_PARAM(threadData, TArg);
}

template <typename T> struct template_parameter;

template <typename T, typename ...TRest>
struct template_parameter<unverified_data<T, TRest...>>
{
    using type = T;
};

template<typename TArg>
inline TArg sandbox_get_callback_param(NaClSandbox_Thread* threadData)
{
	typedef typename template_parameter<TArg>::type unwrappedType;
	auto cbRet = sandbox_get_callback_param_nowrapper<unwrappedType>(threadData);
	return sandbox_convertToUnverified<unwrappedType>(threadData->sandbox, cbRet);
}

template<typename... TArgs>
inline std::tuple<TArgs...> sandbox_get_callback_params_helper(NaClSandbox_Thread* threadData)
{
	//Note - we can't use make tuple here as this would(may?) iterate through the parameter right to left
	//So we use an initializer list which guarantees order of eval as left to right
	return std::tuple<TArgs...> {sandbox_get_callback_param<TArgs>(threadData)...};
}

template<typename Ret, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(*f) (Rest...), NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template<typename Ret, typename F, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(F::*f) (Rest...), NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template<typename Ret, typename F, typename... Rest>
inline std::tuple<Rest...> sandbox_get_callback_params(Ret(F::*f) (Rest...) const, NaClSandbox_Thread* threadData)
{
	UNUSED(f);
	return sandbox_get_callback_params_helper<Rest...>(threadData);
}

template <typename F>
decltype(sandbox_get_callback_params(&F::operator())) sandbox_get_callback_params(F);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline T sandbox_callback_return(NaClSandbox_Thread* threadData, T arg)
{
	UNUSED(threadData);
	return arg;
}

template <typename T>
inline T* sandbox_callback_return(NaClSandbox_Thread* threadData, T* arg)
{
	return CALLBACK_RETURN_PTR(threadData, T*, arg);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TFunc>
class sandbox_callback_helper
{
public:
	NaClSandbox* sandbox;
	short callbackSlotNum;
	uintptr_t callbackRegisteredAddress;

	inline uintptr_t getCallbackAddress()
	{
		return callbackRegisteredAddress;
	}

	~sandbox_callback_helper()
	{
		//printf("sandbox_callback_helper destructor called\n");
		if(!unregisterSandboxCallback(sandbox, callbackSlotNum))
		{
			sandbox_error("Unregister sandbox failed");
		}
	}
};

template<typename TFunc, ENABLE_IF(!std::is_void<return_argument<TFunc>>::value)>
__attribute__ ((noinline)) SANDBOX_CALLBACK return_argument<TFunc> sandbox_callback_receiver(uintptr_t sandboxPtr, void* state)
{
	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
	NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

	TFunc* fnPtr = (TFunc*)(uintptr_t) state;
	auto params = sandbox_get_callback_params(fnPtr, threadData);
	//printf("Calling callback function\n");
	auto ret = call_func(fnPtr, params);
	return sandbox_callback_return(threadData, ret);
}

template<typename TFunc, ENABLE_IF(std::is_void<return_argument<TFunc>>::value)>
__attribute__ ((noinline)) SANDBOX_CALLBACK return_argument<TFunc> sandbox_callback_receiver(uintptr_t sandboxPtr, void* state)
{
	NaClSandbox* sandbox = (NaClSandbox*) sandboxPtr;
	NaClSandbox_Thread* threadData = callbackParamsBegin(sandbox);

	TFunc* fnPtr = (TFunc*)(uintptr_t) state;
	auto params = sandbox_get_callback_params(fnPtr, threadData);
	//printf("Calling callback function\n");
	call_func(fnPtr, params);
}

template<typename Ret>
std::true_type sandbox_callback_all_args_are_unverified_data(Ret(*) ());

template<typename Ret, typename First, typename... Rest, ENABLE_IF(decltype(sandbox_callback_all_args_are_unverified_data(std::declval<Ret (*)(Rest...)>()))::value)>
std::true_type sandbox_callback_all_args_are_unverified_data(Ret(*) (unverified_data<First>, Rest...));

#define enable_if_sandbox_callback_all_args_are_unverified_data(fn) ENABLE_IF(decltype(sandbox_callback_all_args_are_unverified_data(fn))::value)

template <typename T>
__attribute__ ((noinline)) typename std::enable_if<std::is_function<T>::value,
sandbox_callback_helper<T>*>::type sandbox_callback(NaClSandbox* sandbox, T* fnPtr, enable_if_sandbox_callback_all_args_are_unverified_data(fnPtr))
{
	unsigned callbackSlotNum;

	//printf("Creating callback\n");
	if(!getFreeSandboxCallbackSlot(sandbox, &callbackSlotNum))
	{
		sandbox_error("No free callback slots left in sandbox");
	}

	uintptr_t callbackReceiver = (uintptr_t) sandbox_callback_receiver<T>;
	void* callbackState = (void*)(uintptr_t)fnPtr;

	auto callbackRegisteredAddress = registerSandboxCallbackWithState(sandbox, callbackSlotNum, callbackReceiver, callbackState);

	if(!callbackRegisteredAddress)
	{
		sandbox_error("Register sandbox failed");
	}

	//printf("Created callback: %u, %p\n", callbackSlotNum, (void*) callbackRegisteredAddress);

	auto ret = new sandbox_callback_helper<T>();
	ret->sandbox = sandbox;
	ret->callbackSlotNum = callbackSlotNum;
	ret->callbackRegisteredAddress = callbackRegisteredAddress;
	return ret; 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TFunc>
class sandbox_function_helper
{
public:
	TFunc* address;
};

inline void* sandbox_cacheAndRetrieveFnPtr(NaClSandbox* sandbox, const char* fnName);

template<typename T>
sandbox_function_helper<T> sandbox_function_impl(NaClSandbox* sandbox, const char* functionName)
{
	#ifdef NACL_SANDBOX_API_NO_STL_DS
	void* fnPtr = symbolTableLookupInSandbox(sandbox, functionName);
	#else
	void* fnPtr = sandbox_cacheAndRetrieveFnPtr(sandbox, functionName);
	#endif

	sandbox_function_helper<T> ret;
	ret.address = (T*)(uintptr_t) fnPtr;
	return ret;
}

#define sandbox_function(sandbox, fnName) sandbox_function_impl<decltype(fnName)>(sandbox, #fnName)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_stackarr_helper<T>);

template<typename T>
T* sandbox_removeWrapper_helper(sandbox_heaparr_helper<T>*);

template<typename Ret, typename... Rest>
Ret(*sandbox_removeWrapper_helper(sandbox_callback_helper<Ret(Rest...)>*))(typename template_parameter<Rest>::type...);

template<typename Ret, typename... Rest>
Ret(*sandbox_removeWrapper_helper(sandbox_function_helper<Ret(Rest...)>))(Rest...);

template<typename T>
T sandbox_removeWrapper_helper(sandbox_unsandboxed_ptr_helper<T>);

template<typename T>
T sandbox_removeWrapper_helper(sandbox_unverified_data<T>);
template<typename T>
T sandbox_removeWrapper_helper(unverified_data<T>);

template<typename T>
T sandbox_removeWrapper_helper(T);

template <typename T>
using sandbox_removeWrapper = decltype(sandbox_removeWrapper_helper(std::declval<T>()));

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename... T>
inline size_t sandbox_NaClAddParams(T... arg)
{
	UNUSED(arg...);
	return 0;
}

template <typename T, typename ... Targs>
inline size_t sandbox_NaClAddParams(T arg, Targs... rem)
{
	return sizeof(arg) + sandbox_NaClAddParams(rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline size_t sandbox_NaClStackArraySizeOf(T arg)
{
	UNUSED(arg);
	return 0;
}

template <typename T>
inline size_t sandbox_NaClStackArraySizeOf(sandbox_stackarr_helper<T> arg)
{
	return arg.size;
}

template<typename... T>
inline size_t sandbox_NaClAddStackArrParams(T... arg)
{
	UNUSED(arg...);
	return 0;
}

template <typename T, typename ... Targs>
inline size_t sandbox_NaClAddStackArrParams(T arg, Targs... rem)
{
	return sandbox_NaClStackArraySizeOf(arg) + sandbox_NaClAddStackArrParams(rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T arg)
{
	//printf("got a value arg\n");
	PUSH_VAL_TO_STACK(threadData, T, arg);
}

template <>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, float arg)
{
	//printf("got a float arg\n");
	PUSH_FLOAT_TO_STACK(threadData, float, arg);
}

template <>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, double arg)
{
	//printf("got a double arg\n");
	PUSH_FLOAT_TO_STACK(threadData, double, arg);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, T* arg)
{
	//printf("got a pointer arg\n");
	PUSH_PTR_TO_STACK(threadData, T*, arg);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_unsandboxed_ptr_helper<T> arg)
{
	//printf("got a unsandboxed pointer arg\n");
	PUSH_VAL_TO_STACK(threadData, uintptr_t, arg.field);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_stackarr_helper<T> arg)
{
	//printf("got a stack array arg\n");
	PUSH_GEN_ARRAY_TO_STACK(threadData, arg.arr, arg.size);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_heaparr_helper<T>* arg)
{
	//printf("got a heap copy ptr arg\n");
	PUSH_PTR_TO_STACK(threadData, T*, arg->arr);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_callback_helper<T>* arg)
{
	//printf("got a callback arg\n");
	PUSH_VAL_TO_STACK(threadData, uintptr_t, arg->callbackRegisteredAddress);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_function_helper<T> arg)
{
	//printf("got a sandbox function arg\n");
	PUSH_PTR_TO_STACK(threadData, uintptr_t, arg.address);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, unverified_data<T> arg)
{
	sandbox_handleNaClArg(threadData, arg.field);
}

template <typename T>
inline void sandbox_handleNaClArg(NaClSandbox_Thread* threadData, sandbox_unverified_data<T> arg)
{
	sandbox_handleNaClArg(threadData, arg.field);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
inline typename std::enable_if<std::is_class<T>::value,
void>::type sandbox_dealWithNaClReturnArg(NaClSandbox_Thread* threadData)
{
	//printf("pushing return argument slot on stack\n");

	#if defined(_M_IX86) || defined(__i386__)
		#error 32 bit not yet suppported!
	#elif defined(_M_X64) || defined(__x86_64__)
		ALLOCATE_STACK_VARIABLE(threadData, T, tPtr);
		sandbox_handleNaClArg(threadData, tPtr);		
	#else
		#error Unknown platform!
	#endif
}

template <typename T>
inline typename std::enable_if<!std::is_class<T>::value,
void>::type sandbox_dealWithNaClReturnArg(NaClSandbox_Thread* threadData)
{
	UNUSED(threadData);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename... Targs>
inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, Targs... arg)
{
	UNUSED(threadData);
	UNUSED(arg...);
}

template <typename T, typename ... Targs>
inline void sandbox_dealWithNaClArgs(NaClSandbox_Thread* threadData, T arg, Targs... rem)
{
	sandbox_handleNaClArg(threadData, arg);
	sandbox_dealWithNaClArgs(threadData, rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_void<return_argument<T>>::value,
void>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	//printf("got a void return\n");
	UNUSED(threadData);
}

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_pointer<return_argument<T>>::value,
unverified_data<return_argument<T>>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	//printf("got a pointer return\n");
	return sandbox_convertToUnverified<return_argument<T>>(threadData->sandbox, (return_argument<T>)functionCallReturnPtr(threadData));
}

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_floating_point<return_argument<T>>::value,
unverified_data<return_argument<T>>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	//printf("got a double return\n");
	return sandbox_convertToUnverified<return_argument<T>>(threadData->sandbox, (return_argument<T>)functionCallReturnDouble(threadData));
}

template <typename T, typename ... Targs>
inline typename std::enable_if<std::is_class<return_argument<T>>::value && !std::is_reference<T>::value,
unverified_data<return_argument<T>>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	//printf("got a class return\n");
	//structs are returned as a pointer
	return sandbox_convertToUnverified<return_argument<T>>(threadData->sandbox, *((return_argument<T>*)functionCallReturnPtr(threadData)) );
}

template <typename T, typename ... Targs>
inline typename std::enable_if<!std::is_pointer<return_argument<T>>::value && !std::is_void<return_argument<T>>::value && !std::is_floating_point<return_argument<T>>::value && !std::is_class<return_argument<T>>::value,
unverified_data<return_argument<T>>>::type sandbox_invokeNaClReturn(NaClSandbox_Thread* threadData)
{
	//printf("got a value return\n");
	return sandbox_convertToUnverified<return_argument<T>>(threadData->sandbox, (return_argument<T>)functionCallReturnRawPrimitiveInt(threadData));
}

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
struct is_invocable_port : invoke_detail::is_invocable<void, F, Args...> {};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
inline void sandbox_checkSignatureAndCallNaClFn(NaClSandbox_Thread* threadData, void* fnPtr, typename std::enable_if<is_invocable_port<T, sandbox_removeWrapper<Targs>...>::value>::type* dummy = nullptr)
{
	UNUSED(dummy);
	invokeFunctionCall(threadData, fnPtr);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T, typename ... Targs>
__attribute__ ((noinline)) return_argument<decltype(sandbox_invokeNaClReturn<T>)> sandbox_invoker_with_ptr(NaClSandbox* sandbox, void* fnPtr, typename std::enable_if<std::is_function<T>::value>::type* dummy, Targs ... param)
{
	UNUSED(dummy);
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(param...), sandbox_NaClAddStackArrParams(param...) /* size of any arrays being pushed on the stack */);
	//printf("StackArr Size: %u\n", (unsigned)sandbox_NaClAddStackArrParams(param...));
	sandbox_dealWithNaClReturnArg<return_argument<T>>(threadData);
	sandbox_dealWithNaClArgs(threadData, param...);
	sandbox_checkSignatureAndCallNaClFn<T, Targs ...>(threadData, fnPtr);
	return sandbox_invokeNaClReturn<T>(threadData);
}

template <typename T, typename ... Targs>
__attribute__ ((noinline)) return_argument<T> sandbox_invoker_with_ptr_ret_unsandboxed_ptr(NaClSandbox* sandbox, void* fnPtr, typename std::enable_if<std::is_function<T>::value>::type* dummy, Targs ... param)
{
	UNUSED(dummy);
	NaClSandbox_Thread* threadData = preFunctionCall(sandbox, sandbox_NaClAddParams(param...), sandbox_NaClAddStackArrParams(param...) /* size of any arrays being pushed on the stack */);
	//printf("StackArr Size: %u\n", (unsigned)sandbox_NaClAddStackArrParams(param...));
	sandbox_dealWithNaClReturnArg<return_argument<T>>(threadData);
	sandbox_dealWithNaClArgs(threadData, param...);
	sandbox_checkSignatureAndCallNaClFn<T, Targs ...>(threadData, fnPtr);
	auto ret = (uintptr_t) functionCallReturnRawPrimitiveInt(threadData);
	return (return_argument<T>) ret;
}

#ifndef NACL_SANDBOX_API_NO_STL_DS
	#define initCPPApi(sandbox) sandbox->extraState = new std::map<std::string, void*>

	inline void* sandbox_cacheAndRetrieveFnPtr(NaClSandbox* sandbox, const char* fnName)
	{
		auto& fnMap = *(std::map<std::string, void*> *) sandbox->extraState;
		auto fnPtrRef = fnMap.find(fnName);
		void* fnPtr;
		if(fnPtrRef == fnMap.end())
		{
			//printf("Auto Symbol lookup for %s\n", fnName);
			fnPtr = symbolTableLookupInSandbox(sandbox, fnName);
			fnMap[fnName] = fnPtr;
		}
		else
		{
			fnPtr = fnPtrRef->second;
		}

		return fnPtr;
	}
#else
	#define initCPPApi(sandbox) do { } while(0)
#endif


#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
	#define sandbox_invoke_with_name_and_ptr(sandbox, fnName, fnPtr, ...) sandbox_invoker_with_ptr<decltype(fnName)>(sandbox, fnPtr, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_with_name_and_ptr_ret_unsandboxed_ptr(sandbox, fnName, fnPtr, ...) sandbox_invoker_with_ptr_ret_unsandboxed_ptr<decltype(fnName)>(sandbox, fnPtr, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_with_ptr(sandbox, fnPtr, ...) sandbox_invoker_with_ptr<my_remove_pointer_t<decltype(fnPtr)>>(sandbox, (void*) fnPtr, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_with_ptr_ret_unsandboxed_ptr(sandbox, fnPtr, ...) sandbox_invoker_with_ptr_ret_unsandboxed_ptr<my_remove_pointer_t<decltype(fnPtr)>>(sandbox, (void*) fnPtr, nullptr, ##__VA_ARGS__)
	#ifndef NACL_SANDBOX_API_NO_STL_DS
		#define sandbox_invoke(sandbox, fnName, ...) sandbox_invoker_with_ptr<decltype(fnName)>(sandbox, sandbox_cacheAndRetrieveFnPtr(sandbox, #fnName), nullptr, ##__VA_ARGS__)
		#define sandbox_invoke_ret_unsandboxed_ptr(sandbox, fnName, ...) sandbox_invoker_with_ptr_ret_unsandboxed_ptr<decltype(fnName)>(sandbox, sandbox_cacheAndRetrieveFnPtr(sandbox, #fnName), nullptr, ##__VA_ARGS__)
	#endif
#ifdef __clang__
	#pragma clang diagnostic pop
#endif


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
inline unverified_data<T*> newInSandbox(NaClSandbox* sandbox, unsigned count = 1)
{
	auto ret = (T*) getMaskedField(getSandboxMemoryBase(sandbox), mallocInSandbox(sandbox, sizeof(T) * count));
	auto casted = (unverified_data<T*> *) &ret;
	return *casted;
}

#endif
