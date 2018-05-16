#ifndef RLBOX_API
#define RLBOX_API

#include <functional>
#include <type_traits>
#include <map>
#include <string.h>

namespace rlbox
{
	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
	#define UNUSED(x) rlbox_api_detail::UNUSED_PACK_HELPER {x}

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

	//C++11 doesn't have the _t and _v convenience helpers, so create these
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
	constexpr bool my_is_fundamental_v = std::is_fundamental<T>::value;

	template<typename T1, typename T2>
	constexpr bool my_is_base_of_v = std::is_base_of<T1, T2>::value;

	template<typename T>
	constexpr bool my_is_void_v = std::is_void<T>::value;

	template<typename T>
	using my_remove_const_t = typename std::remove_const<T>::type;	

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Some additional helpers

	#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<my_remove_pointer_t<T>>;

	template<typename T>
	using my_array_element_t = my_remove_reference_t<decltype(*(std::declval<T>()))>;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class sandbox_wrapper_base {};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	class sandbox_app_ptr_wrapper : public sandbox_wrapper_base
	{
	private:
		T* field;
	public:
 		template<typename U>
		friend class RLBoxSandbox;

		inline T* UNSAFE_Unverified() const noexcept { return field; }
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, typename TSandbox>
	class sandbox_stackarr_helper : public sandbox_wrapper_base
	{
	public:
		TSandbox* sandbox;
		T* field;
		size_t arrSize;

		sandbox_stackarr_helper() = default;
		sandbox_stackarr_helper(sandbox_stackarr_helper&) = default;
		sandbox_stackarr_helper(sandbox_stackarr_helper&& other)
		{
			field = other.field;
			arrSize = other.arrSize;
			other.field = nullptr;
			other.arrSize = 0;
		}

		sandbox_stackarr_helper& operator=(const sandbox_stackarr_helper&& other)  
		{
			if (this != &other)  
			{
				field = other.field;
				arrSize = other.arrSize;
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
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	//Use a custom enum for returns as boolean returns are a bad idea
	//int returns are automatically cast to a boolean
	//Some APIs have overloads with boolean and non returns, so best to use a custom class
	enum class RLBox_Verify_Status
	{
		SAFE, UNSAFE
	};

	template<typename TField, typename T, ENABLE_IF(!my_is_class_v<T> && !my_is_reference_v<T> && !my_is_array_v<T>)>
	inline T getFieldCopy(TField field)
	{
		T copy = field;
		return copy;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename T, typename TSandbox>
	class tainted_base : public sandbox_wrapper_base
	{
	};

	template<typename T, typename TSandbox>
	class tainted_volatile;

	template<typename T, typename TSandbox>
	class tainted : public tainted_base<T, TSandbox>
	{
		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U1, typename U2>
		friend class tainted;

		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U1, typename U2>
		friend class tainted_volatile;

		template<typename U>
		friend class RLBoxSandbox;

	private:
		T field;

		template<typename TRHS, template <typename, typename> typename TWrap, ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>>)>
		inline TRHS unwrap(const TWrap<TRHS, TSandbox> rhs) const noexcept
		{
			return rhs.UNSAFE_Unverified();
		}

		template<typename TRHS>
		inline TRHS unwrap(const TRHS rhs) const noexcept
		{
			return rhs;
		}

	public:

		tainted() = default;
		tainted(const tainted<T, TSandbox>& p) = default;

		template<typename T2=T, ENABLE_IF(my_is_fundamental_v<T2>)>
		tainted(const tainted_volatile<T, TSandbox>& p)
		{
			field = p.field;
		}

		//we explicitly disable this constructor if it has one of the signatures above, 
		//	so that we give the above constructors a higher priority
		//	For now we only allow this for fundamental types as this is potentially unsafe for pointers and structs
		template<typename Arg, typename... Args, ENABLE_IF(!my_is_base_of_v<tainted_base<T, TSandbox>, my_remove_reference_t<Arg>> && my_is_fundamental_v<T>)>
		tainted(Arg&& arg, Args&&... args) : field(std::forward<Arg>(arg), std::forward<Args>(args)...) { }

		inline T UNSAFE_Unverified() const noexcept
		{
			return field;
		}

		template<typename T2=T, ENABLE_IF(my_is_fundamental_v<T2>)>
		inline T copyAndVerify(std::function<T(T)> verifyFunction) const
		{
			return verifyFunction(field);
		}

		template<typename T2=T, ENABLE_IF(my_is_fundamental_v<T2>)>
		inline T copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(field) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		template<typename TRHS, ENABLE_IF(my_is_fundamental_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted<T, TSandbox>& operator=(const TRHS& arg) noexcept
		{
			field = arg;
			return *this;
		}

		//we don't support app pointers in structs that are maintained in application memory.

		inline tainted<T, TSandbox> operator-() const noexcept
		{
			tainted<T, TSandbox> result = -field;
			return result;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>& operator*() const noexcept
		{
			auto& ret = *((tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) field);
			return ret;
		}

		template<typename TRHS>
		inline tainted<T, TSandbox> operator+(const TRHS rhs) const noexcept
		{
			tainted<T, TSandbox> result = field + unwrap(rhs);
			return result;
		}

		template<typename TRHS>
		inline tainted<T, TSandbox> operator-(const TRHS rhs) const noexcept
		{
			tainted<T, TSandbox> result = field - unwrap(rhs);
			return result;
		}

		template<typename TRHS>
		inline tainted<T, TSandbox> operator*(const TRHS rhs) const noexcept
		{
			tainted<T, TSandbox> result = field * unwrap(rhs);
			return result;
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

	private:
		my_add_volatile_t<T> field;

		tainted_volatile() = default;
		tainted_volatile(const tainted_volatile<T, TSandbox>& p) = default;

		template<typename T2=T, ENABLE_IF(!my_is_pointer_v<T2>)>
		inline T getAppSwizzledValue(T arg) const
		{
			return arg;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline T getAppSwizzledValue(T arg) const
		{
			return (T) TSandbox::impl_GetUnsandboxedPointer(arg);
		}

		template<typename T2=T, ENABLE_IF(!my_is_pointer_v<T2>)>
		inline T getSandboxSwizzledValue(T arg) const
		{
			return arg;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline T getSandboxSwizzledValue(T arg) const
		{
			return (T) TSandbox::impl_GetSandboxedPointer(arg);
		}
	public:

		inline T UNSAFE_Unverified() const noexcept
		{
			return getAppSwizzledValue(field);
		}

		template<typename TRHS, ENABLE_IF(my_is_fundamental_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const TRHS& arg) noexcept
		{
			field = arg;
			return *this;
		}

		template<typename TRHS, ENABLE_IF(my_is_pointer_v<T> && my_is_assignable_v<T&, TRHS*>)>
		inline tainted_volatile<T, TSandbox>& operator=(const sandbox_app_ptr_wrapper<TRHS>& arg) noexcept
		{
			field = arg.UNSAFE_Unverified();
			return *this;
		}

		template<typename TRHS, ENABLE_IF(my_is_assignable_v<T&, TRHS>)>
		inline tainted_volatile<T, TSandbox>& operator=(const tainted<TRHS, TSandbox>& arg) noexcept
		{
			field = getSandboxSwizzledValue(arg.field);
			return *this;
		}

		inline tainted<T*, TSandbox> operator&() const noexcept
		{
			tainted<T*, TSandbox> ret;
			ret.field = (T*) &field;
			return ret;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>, TSandbox>& operator*() const noexcept
		{
			auto ret = (tainted_volatile<my_remove_pointer_t<T>, TSandbox>*) getAppSwizzledValue(field);
			return *ret;
		}
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_array_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(TSandbox* sandbox, my_array_element_t<T>* retRaw)
	{
		//arrays are returned by pointer but tainted<Foo[]> is returned by value, so copy happens automatically
		auto retRawPtr = (tainted<T, TSandbox> *) retRaw;
		return *retRawPtr;
	}

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_fundamental_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(TSandbox* sandbox, T retRaw)
	{
		//primitives are returned by value
		auto retRawPtr = (tainted<T, TSandbox> *) &retRaw;
		return *retRawPtr;
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	inline T sandbox_removeWrapper(T& arg)
	{
		return arg;
	}

	template<typename TRHS, typename... TRHSRem, template<typename, typename...> typename TWrap, ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, TWrap<TRHS, TRHSRem...>>)>
	inline auto sandbox_removeWrapper(TWrap<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_Unverified())
	{
		return arg.UNSAFE_Unverified();
	}

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TSandbox>
	class RLBoxSandbox : protected TSandbox
	{
	private:
		RLBoxSandbox(){}
		void* fnPointerMap = nullptr;

	public:
		static RLBoxSandbox* createSandbox(const char* sandboxRuntimePath, const char* libraryPath)
		{
			RLBoxSandbox* ret = new RLBoxSandbox();
			ret->impl_CreateSandbox(sandboxRuntimePath, libraryPath);
			return ret;
		}

		template<typename T>
		tainted<T*, TSandbox> mallocInSandbox(unsigned int count=1)
		{
			void* addr = this->impl_mallocInSandbox(sizeof(T) * count);
			void* rangeCheckedAddr = this->impl_KeepAddressInSandboxedRange(addr);
			tainted<T*, TSandbox> ret;
			ret.field = (T*) rangeCheckedAddr;
			return ret;
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_same_v<return_argument<T>, void>)>
		void invokeStatic(T* fnPtr, TArgs&&... params)
		{
			fnPtr(sandbox_removeWrapper(params)...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_same_v<return_argument<T>, void>)>
		tainted<return_argument<T>, TSandbox> invokeStatic(T* fnPtr, TArgs&&... params)
		{
			return_argument<T> fnRet = fnPtr(sandbox_removeWrapper(params)...);
			tainted<return_argument<T>, TSandbox> ret = sandbox_convertToUnverified<return_argument<T>>((TSandbox*) this, fnRet);
			return ret;
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_same_v<return_argument<T>, void>)>
		void invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			this->invokeFunction(fnPtr, sandbox_removeWrapper(params)...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_same_v<return_argument<T>, void>)>
		tainted<return_argument<T>, TSandbox> invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			return_argument<T> fnRet = this->invokeFunction(fnPtr, sandbox_removeWrapper(params)...);
			tainted<return_argument<T>, TSandbox> ret = sandbox_convertToUnverified<return_argument<T>>((TSandbox*) this, fnRet);
			return ret;
		}

		void* getFunctionPointerFromCache(const char* fnName)
		{
			void* fnPtr;
			if(!fnPointerMap)
			{
				fnPointerMap = new std::map<std::string, void*>;
			}

			auto& fnMapTyped = *(std::map<std::string, void*> *) fnPointerMap;
			auto fnPtrRef = fnMapTyped.find(fnName);

			if(fnPtrRef == fnMapTyped.end())
			{
				fnPtr = this->lookupSymbol(fnName);
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
			sandbox_app_ptr_wrapper<T> ret;
			ret.field = arg;
			return ret;
		}

		template <typename T>
		inline sandbox_stackarr_helper<T, TSandbox> stackarr(T* arg, size_t size)
		{
			T* argInSandbox = (T*) this->impl_pushStackArr(size);
			memcpy((void*) argInSandbox, (void*) arg, size);

			sandbox_stackarr_helper<T, TSandbox> ret;
			ret.sandbox = this;
			ret.field = argInSandbox;
			ret.arrSize = size;
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
	};

	#define sandbox_invoke(sandbox, fnName, ...) sandbox->invokeWithFunctionPointer((decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName), ##__VA_ARGS__)
	#define sandbox_invoke_static(sandbox, fnName, ...) sandbox->invokeStatic(&fnName, ##__VA_ARGS__)
	#define sandbox_invoke_with_fnptr(sandbox, fnPtr, ...) sandbox->invokeWithFunctionPointer(fnPtr, ##__VA_ARGS__)


	#undef ENABLE_IF
	#undef UNUSED
}
#endif
