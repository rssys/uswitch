#ifndef PROCESS_SANDBOX_API
#define PROCESS_SANDBOX_API

#include <type_traits>
#include <functional>
#include <stdio.h>

#ifndef PROCESS_SANDBOX_API_NO_OPTIONAL
	#include "optional.hpp"

	using nonstd::optional;
	using nonstd::nullopt;
#endif

#ifndef UNUSED
	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
	#define UNUSED(x) UNUSED_PACK_HELPER {x}
#endif

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
	return arg;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename ProcessSanbox, typename TFunc>
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

	inline void applyMask(unsigned long sandboxMask)
	{		
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

	inline void applyMask(unsigned long sandboxMask)
	{		
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

	inline void applyMask(unsigned long sandboxMask)
	{
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

	inline void applyMask(unsigned long sandboxMask)
	{
		//arrays only exist in structs, and when they are part of structs they are copied by value. no masking needed
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
		uintptr_t maskedFieldPtrEnd = (uintptr_t) maskedFieldPtr + size;

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

	#ifndef PROCESS_SANDBOX_API_NO_OPTIONAL
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

	#ifndef PROCESS_SANDBOX_API_NO_OPTIONAL
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

	inline void applyMask(unsigned long sandboxMask)
	{
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

	#ifndef PROCESS_SANDBOX_API_NO_OPTIONAL
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

	#ifndef PROCESS_SANDBOX_API_NO_OPTIONAL
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

	inline void applyMask(unsigned long sandboxMask)
	{
		field = getMaskedField(sandboxMask, field);
	}
};


////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<class TData> 
class unverified_data_unwrapper {
public:
    TData value_field;
};

template<typename T>
unverified_data_unwrapper<T> sandbox_removeWrapper_helper(sandbox_unverified_data<T>);
template<typename T>
unverified_data_unwrapper<T> sandbox_removeWrapper_helper(unverified_data<T>);

template<typename T>
unverified_data<T> sandbox_removeWrapper_helper(T);

template <typename T>
using sandbox_removeWrapper = decltype(sandbox_removeWrapper_helper(std::declval<T>()).value_field);

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename TRet, typename... TArgs>
TRet(*sandbox_remove_unverified_data_on_args_helper(TRet(*)(TArgs...)))(sandbox_removeWrapper<TArgs>...);

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

	template<typename ProcessSandbox, typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, sandbox_remove_unverified_data_on_args<TRHS*>>::value,
	sandbox_unverified_data<T>&>::type operator=(const sandbox_callback_helper<ProcessSandbox, TRHS>* arg) noexcept
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

	inline void applyMask(unsigned long sandboxMask)
	{
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

	template<typename ProcessSandbox, typename TRHS>
	inline typename std::enable_if<std::is_assignable<T&, sandbox_remove_unverified_data_on_args<TRHS*>>::value,
	unverified_data<T>&>::type operator=(const sandbox_callback_helper<ProcessSandbox, TRHS>* arg) noexcept
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

	inline void applyMask(unsigned long sandboxMask)
	{
		field = getMaskedField(sandboxMask, field);
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
#define unverified_data_applyMask(fieldType, fieldName) fieldName.applyMask(sandboxMask);
 
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
 \
	inline void applyMask(unsigned long sandboxMask) \
	{ \
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
 \
	inline void applyMask(unsigned long sandboxMask) \
	{ \
		sandbox_fields_reflection_##libId##_class_##T(unverified_data_applyMask, sandbox_unverified_data_noOp) \
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

inline sandbox_stackarr_helper<char> sandbox_stackarr(char* p_arr) 
{ 
	return sandbox_stackarr(p_arr, strlen(p_arr) + 1); 
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T>
class sandbox_heaparr_helper
{
public:
	PROCESS_SANDBOX_CLASSNAME* sandbox;
	T* arr;

	~sandbox_heaparr_helper()
	{
		//printf("sandbox_heaparr_helper destructor called\n");
		sandbox->freeInSandbox((void *)arr);
	}
};

template <typename T>
inline sandbox_heaparr_helper<T>* sandbox_heaparr(PROCESS_SANDBOX_CLASSNAME* sandbox, T* arg, size_t size)
{
	T* argInSandbox = (T *) sandbox->mallocInSandbox(size);
	memcpy((void*) argInSandbox, (void*) arg, size);

	auto ret = new sandbox_heaparr_helper<T>();
	ret->sandbox = sandbox;
	ret->arr = argInSandbox;
	return ret; 
}

inline sandbox_heaparr_helper<const char>* sandbox_heaparr(PROCESS_SANDBOX_CLASSNAME* sandbox, const char* str)
{
	return sandbox_heaparr(sandbox, str, strlen(str) + 1);
}

inline sandbox_heaparr_helper<char>* sandbox_heaparr(PROCESS_SANDBOX_CLASSNAME* sandbox, char* str)
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

template <typename ProcessSandbox, typename T>
inline typename std::enable_if<std::is_pointer<T>::value && !std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(ProcessSandbox* sandbox, T retRaw)
{
	//printf("got a pointer return\n");
	auto retRawMasked = getMaskedField((unsigned long)sandbox->getSandboxMemoryBase(), retRaw);
	auto retRawPtr = (unverified_data<T> *) &retRawMasked;
	return *retRawPtr;
}

template <typename ProcessSandbox, typename T>
inline typename std::enable_if<std::is_class<T>::value && !std::is_reference<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(ProcessSandbox* sandbox, T retRaw)
{
	//printf("got a class return\n");
	unverified_data<T> ret;
	ret = retRaw;
	ret.applyMask((unsigned long)(uintptr_t)sandbox->getSandboxMemoryBase());
	return ret;
}

template <typename ProcessSandbox, typename T>
inline typename std::enable_if<std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(ProcessSandbox* sandbox, my_remove_reference_t<decltype(*(std::declval<T>()))>* retRaw)
{
	//printf("got a array return\n");
	//arrays are returned by pointer but are unverified_data<struct Foo> is copied by value
	auto retRawMasked = getMaskedField((unsigned long)sandbox->getSandboxMemoryBase(), retRaw);
	auto retRawPtr = (unverified_data<T> *) retRawMasked;
	return *retRawPtr;
}

template <typename ProcessSandbox, typename T>
inline typename std::enable_if<!std::is_pointer<T>::value && !std::is_void<T>::value && !std::is_class<T>::value && !std::is_array<T>::value,
unverified_data<T>>::type sandbox_convertToUnverified(ProcessSandbox* sandbox, T retRaw)
{
	//printf("got a value return\n");
	auto retRawPtr = (unverified_data<T> *) &retRaw;
	return *retRawPtr;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename ProcessSandbox, typename TFunc>
class sandbox_callback_helper
{
public:
	ProcessSandbox* sandbox;
	sandbox_remove_unverified_data_on_args<TFunc> callbackRegisteredAddress;
	TFunc* fnPtr;

	inline uintptr_t getCallbackAddress()
	{
		return callbackRegisteredAddress;
	}

	~sandbox_callback_helper()
	{
		//printf("sandbox_callback_helper destructor called\n");
		sandbox->unregisterCallback(callbackRegisteredAddress);
		
	}
};

template<typename Ret>
std::true_type sandbox_callback_all_args_are_unverified_data(Ret(*) ());

template<typename Ret, typename First, typename... Rest, ENABLE_IF(decltype(sandbox_callback_all_args_are_unverified_data(std::declval<Ret (*)(Rest...)>()))::value)>
std::true_type sandbox_callback_all_args_are_unverified_data(Ret(*) (unverified_data<First>, Rest...));

#define enable_if_sandbox_callback_all_args_are_unverified_data(fn) ENABLE_IF(decltype(sandbox_callback_all_args_are_unverified_data(fn))::value)

template<typename ProcessSandbox, typename TRet, typename... TArgs>
__attribute__ ((noinline)) TRet sandbox_callback_receiver(TArgs... params, void* fnPtrState)
{
	using fnType = TRet(unverified_data<TArgs>...);
	sandbox_callback_helper<ProcessSandbox, fnType>* cbHelper = (sandbox_callback_helper<ProcessSandbox, fnType>*) fnPtrState;

	return cbHelper->fnPtr(sandbox_convertToUnverified<ProcessSandbox, TArgs>(cbHelper->sandbox, params)...);
}

template <typename ProcessSandbox, typename TRet, typename... TArgs>
__attribute__ ((noinline)) sandbox_callback_helper<ProcessSandbox, TRet(TArgs...)>* 
sandbox_callback(ProcessSandbox* sandbox, TRet(*fnPtr)(TArgs...), enable_if_sandbox_callback_all_args_are_unverified_data(fnPtr))
{
	sandbox_remove_unverified_data_on_args<TRet(*)(TArgs...)> unwrappedFnPtr = 
		(sandbox_remove_unverified_data_on_args<TRet(*)(TArgs...)>) sandbox_callback_receiver<ProcessSandbox, TRet, sandbox_removeWrapper<TArgs>...>;

	auto ret = new sandbox_callback_helper<ProcessSandbox, TRet(TArgs...)>();
	ret->sandbox = sandbox;
	ret->fnPtr = fnPtr;

	auto callbackRegisteredAddress = sandbox->registerCallback(unwrappedFnPtr, (void*) ret);

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

template<typename ProcessSandbox, typename T>
sandbox_function_helper<my_remove_pointer_t<T>> sandbox_function_impl(ProcessSandbox* sandbox, T fnPtr)
{
	sandbox_function_helper<my_remove_pointer_t<T>> ret;
	ret.address = fnPtr;
	return ret;
}

#define sandbox_function(sandbox, fnName) sandbox_function_impl(sandbox, &PROCESS_SANDBOX_CLASSNAME::inv_##fnName)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////


//these are the valid types of parameters
template <typename T>
inline void sandbox_checkParameterTypes(T arg)
{
	//printf("got a value arg\n");
}

template <>
inline void sandbox_checkParameterTypes(float arg)
{
	//printf("got a float arg\n");
}

template <>
inline void sandbox_checkParameterTypes(double arg)
{
	//printf("got a double arg\n");
}

template <typename T>
inline void sandbox_checkParameterTypes(T* arg)
{
	//printf("got a pointer arg\n");
}

template <typename T>
inline void sandbox_checkParameterTypes(sandbox_unsandboxed_ptr_helper<T> arg)
{
	//printf("got a unsandboxed pointer arg\n");
}

template <typename T>
inline void sandbox_checkParameterTypes(sandbox_stackarr_helper<T> arg)
{
	//printf("got a stack array arg\n");
}

template <typename T>
inline void sandbox_checkParameterTypes(sandbox_heaparr_helper<T>* arg)
{
	//printf("got a heap copy ptr arg\n");
}

template <typename ProcessSandbox, typename T>
inline void sandbox_checkParameterTypes(sandbox_callback_helper<ProcessSandbox, T>* arg)
{
	//printf("got a callback arg\n");
}

template <typename T>
inline void sandbox_checkParameterTypes(unverified_data<T> arg)
{
	sandbox_checkParameterTypes(arg.field);
}

template <typename T>
inline void sandbox_checkParameterTypes(sandbox_unverified_data<T> arg)
{
	sandbox_checkParameterTypes(arg.field);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename... Targs>
inline void sandbox_dealWithNaClArgs(Targs... arg)
{
	UNUSED(arg...);
}

template <typename T, typename ... Targs>
inline void sandbox_dealWithNaClArgs(T arg, Targs... rem)
{
	sandbox_checkParameterTypes(arg);
	sandbox_dealWithNaClArgs(rem...);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename ProcessSandbox, typename T, typename ... TArgs>
inline typename std::enable_if<std::is_void<return_argument<T>>::value,
void>::type sandbox_invokeProcessReturn(ProcessSandbox* sandbox, T fnPtr, TArgs... params)
{
	//printf("got a void return\n");
	(sandbox->*fnPtr)(sandbox_handleProcessArg(sandbox, params)...);
}

template <typename ProcessSandbox, typename T, typename ... TArgs>
inline typename std::enable_if<!std::is_void<return_argument<T>>::value,
unverified_data<return_argument<T>>>::type sandbox_invokeProcessReturn(ProcessSandbox* sandbox, T fnPtr, TArgs... params)
{
	//printf("got a non void return\n");
	auto ret = (sandbox->*fnPtr)(sandbox_handleProcessArg(sandbox, params)...);
	return sandbox_convertToUnverified(sandbox, ret);
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template <typename ProcessSandbox, typename T>
inline T sandbox_handleProcessArg(ProcessSandbox* sandbox, T arg)
{
	return arg;
}

template <typename ProcessSandbox, typename T>
inline T sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_unsandboxed_ptr_helper<T> arg)
{
	return arg.field;
}

template <typename ProcessSandbox, typename T>
inline T* sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_stackarr_helper<T> arg)
{
	void* sharedMemPtr = sandbox->mallocInSandbox(arg.size);
	memcpy(sharedMemPtr, arg.arr, arg.size);
	return (T*) sharedMemPtr;
}

template <typename ProcessSandbox, typename T>
inline T* sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_heaparr_helper<T>* arg)
{
	return arg->arr;
}

template <typename ProcessSandbox, typename T>
inline sandbox_remove_unverified_data_on_args<T> sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_callback_helper<ProcessSandbox, T>* arg)
{
	return arg->callbackRegisteredAddress;
}

template <typename ProcessSandbox, typename T>
inline T* sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_function_helper<T> arg)
{
	return arg.address;
}

template <typename ProcessSandbox, typename T>
inline T sandbox_handleProcessArg(ProcessSandbox* sandbox, unverified_data<T> arg)
{
	return arg.field;
}

template <typename ProcessSandbox, typename T>
inline T sandbox_handleProcessArg(ProcessSandbox* sandbox, sandbox_unverified_data<T> arg)
{
	return arg.field;
}

template <typename ProcessSandbox, typename T, typename ... Targs>
__attribute__ ((noinline)) return_argument<decltype(sandbox_invokeProcessReturn<ProcessSandbox, T, Targs...>)> sandbox_invoker_with_ptr(ProcessSandbox* sandbox, T fnPtr, typename std::enable_if<std::is_function<my_remove_pointer_t<T>>::value || std::is_member_function_pointer<T>::value>::type* dummy, Targs ... param)
{
	UNUSED(dummy);
	//check that callback types are wrapped
	sandbox_dealWithNaClArgs(param...);
	return sandbox_invokeProcessReturn(sandbox, fnPtr, param...);
}

template <typename ProcessSandbox, typename T, typename ... Targs>
__attribute__ ((noinline)) return_argument<T> sandbox_invoker_with_ptr_ret_unsandboxed_ptr(ProcessSandbox* sandbox, T fnPtr, typename std::enable_if<std::is_function<my_remove_pointer_t<T>>::value || std::is_member_function_pointer<T>::value>::type* dummy, Targs ... param)
{
	UNUSED(dummy);
	//check that callback types are wrapped
	sandbox_dealWithNaClArgs(param...);
	return (sandbox->*fnPtr)(sandbox_handleProcessArg(sandbox, param)...);
}

#define initCPPApi(sandbox) do { } while(0)

#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif
	#define sandbox_invoke(sandbox, fnName, ...) sandbox_invoker_with_ptr(sandbox, &PROCESS_SANDBOX_CLASSNAME::inv_##fnName, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_ret_unsandboxed_ptr(sandbox, fnName, ...) sandbox_invoker_with_ptr_ret_unsandboxed_ptr(sandbox, &PROCESS_SANDBOX_CLASSNAME::inv_##fnName, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_with_ptr(sandbox, fnPtr, ...) sandbox_invoker_with_ptr(sandbox, fnPtr, nullptr, ##__VA_ARGS__)
	#define sandbox_invoke_with_ptr_ret_unsandboxed_ptr(sandbox, fnPtr, ...) sandbox_invoker_with_ptr_ret_unsandboxed_ptr(sandbox, fnPtr, nullptr, ##__VA_ARGS__)

#ifdef __clang__
	#pragma clang diagnostic pop
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T, typename ProcessSandbox>
inline unverified_data<T*> newInSandbox(ProcessSandbox* sandbox, unsigned count = 1)
{
	auto ret = (T*) getMaskedField((unsigned long) sandbox->getSandboxMemoryBase(), sandbox->mallocInSandbox(sizeof(T) * count));
	auto casted = (unverified_data<T*> *) &ret;
	return *casted;
}

template<typename ProcessSandbox>
inline bool isAddressInSandboxMemoryOrNull(ProcessSandbox* sandbox, uintptr_t addr)
{
	if(addr == 0) return 1;

	//mask the base as the sandbox only allocates 2GB pages aligned to 2GB for now. 
	auto base = 0xFFFFFFFF00000000 & ((unsigned long) sandbox->getSandboxMemoryBase());
	auto addrBase = 0xFFFFFFFF00000000 & addr;
	return base == addrBase;
}

template<typename ProcessSandbox>
inline bool isAddressInNonSandboxMemoryOrNull(ProcessSandbox* sandbox, uintptr_t addr)
{
	if(addr == 0) return 1;
	
	return !isAddressInSandboxMemoryOrNull(sandbox, addr);
}

template<typename ProcessSandbox>
inline void freeInSandbox(ProcessSandbox* sandbox, void* addr)
{
	sandbox->freeInSandbox(addr);
}

inline bool initializeDlSandboxCreator(int logging)
{
	return 1;
}

template<typename ProcessSandbox>
inline ProcessSandbox* createDlSandbox(char* path)
{
	return new ProcessSandbox(path, 1, 3);
}

#define PROCESS_SANDBOX_CPPAPI_ONCE
#endif
