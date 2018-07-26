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
#include <string.h>

namespace rlbox
{
	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
	#define UNUSED(x) rlbox_api_detail::UNUSED_PACK_HELPER {x}

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
	constexpr bool my_is_fundamental_v = std::is_fundamental<T>::value;

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

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Some additional helpers

	#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<my_remove_pointer_t<T>>;

	template<typename T>
	using my_array_element_t = my_remove_reference_t<decltype(*(std::declval<T>()))>;

	template<typename T>
	using my_decay_noconst_if_array_t = my_conditional_t<my_is_array_v<T>, my_decay_t<T>, T>;

	template<typename T>
	using my_decay_if_array_t = my_conditional_t<my_is_array_v<T>, const my_remove_pointer_t<my_decay_t<T>> *, T>;

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

	class sandbox_wrapper_base {};

	template<typename T>
	class sandbox_wrapper_base_of {};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	class sandbox_app_ptr_wrapper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		T* field;
	public:
 		template<typename U>
		friend class RLBoxSandbox;

		inline T* UNSAFE_Unverified() const noexcept { return field; }
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
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T, typename TSandbox>
	class sandbox_callback_helper : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T*>
	{
	private:
		TSandbox* sandbox;
		T* registeredCallback;
	public:

		sandbox_callback_helper(TSandbox* sandbox, T* registeredCallback)
		{
			this->sandbox = sandbox;
			this->registeredCallback = registeredCallback;
		}
		sandbox_callback_helper(sandbox_callback_helper&& other)
		{
			registeredCallback = other.registeredCallback;
			sandbox = other.sandbox;
			other.registeredCallback = nullptr;
			other.sandbox = nullptr;
		}

		sandbox_callback_helper& operator=(const sandbox_callback_helper&& other)  
		{
			if (this != &other)  
			{
				registeredCallback = other.registeredCallback;
				sandbox = other.sandbox;
				other.registeredCallback = nullptr;
				other.sandbox = nullptr;
			}
		}

		~sandbox_callback_helper()
		{
			if(registeredCallback != nullptr)
			{
				sandbox->impl_UnregisterCallback(registeredCallback);
			}
		}

		inline T* UNSAFE_Unverified() const noexcept { return registeredCallback; }
	};

	template <typename TFunc, typename TSandbox>
	class sandbox_callback_state
	{
	public:
		TSandbox * const sandbox;
		TFunc * const actualCallback;
		sandbox_callback_state(TSandbox* p_sandbox, TFunc* p_actualCallback) : sandbox(p_sandbox), actualCallback(p_actualCallback)
		{
		}
	};

	template<typename T, typename TSandbox>
	class tainted;


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

	template <typename T, typename TSandbox>
	inline my_enable_if_t<my_is_pointer_v<T>,
	tainted<T, TSandbox>> sandbox_convertToUnverified(TSandbox* sandbox, T retRaw)
	{
		//pointers are returned by value
		auto retRawPtr = (tainted<T, TSandbox> *) &retRaw;
		return *retRawPtr;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename TSandbox, typename TRet, typename... TArgs>
	__attribute__ ((noinline)) TRet sandbox_callback_receiver(TArgs... params, void* state)
	{
		auto stateObj = (sandbox_callback_state<TRet(tainted<TArgs, TSandbox>...), TSandbox>*) state;
		return stateObj->actualCallback(sandbox_convertToUnverified<TArgs>(stateObj->sandbox, params)...);
	}

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
	class tainted_base : public sandbox_wrapper_base, public sandbox_wrapper_base_of<T>
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

		template<typename TRHS, template <typename, typename> class TWrap, ENABLE_IF(my_is_base_of_v<tainted_base<TRHS, TSandbox>, TWrap<TRHS, TSandbox>>)>
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

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
		{
			return field;
		}

		//Even though this function is not enabled for array types, the C++ compiler complains that this is a a function that
		//	accepts a "function type" param that returns an array type. So we decay the type for array types
		template<typename T2=T, ENABLE_IF(my_is_fundamental_v<T2>)>
		inline T2 copyAndVerify(std::function<my_conditional_t<my_is_array_v<T>, my_decay_t<T>, T>(T)> verifyFunction) const
		{
			return verifyFunction(field);
		}

		template<typename T2=T, ENABLE_IF(my_is_fundamental_v<T2>)>
		inline T2 copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(field) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		template<typename T2=T, ENABLE_IF(my_is_array_v<T2>)>
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

		inline my_decay_if_array_t<T> copyAndVerifyArray(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, unsigned int elementCount, T defaultValue) const
		{
			typedef my_remove_pointer_t<T> nonPointerType;
			typedef my_remove_const_t<nonPointerType> nonPointerConstType;

			auto maskedFieldPtr = UNSAFE_Unverified();
			uintptr_t arrayEnd = ((uintptr_t)maskedFieldPtr) + sizeof(nonPointerType) * elementCount;

			//check for overflow
			if(!sandbox->isPointerInSandboxMemoryOrNull((void*)arrayEnd))
			{
				return defaultValue;
			}

			nonPointerConstType* copy = new nonPointerConstType[elementCount];
			memcpy(copy, maskedFieldPtr, sizeof(nonPointerConstType) * elementCount);
			return verifyFunction(copy) == RLBox_Verify_Status::SAFE? copy : defaultValue;
		}

		inline my_decay_if_array_t<T> copyAndVerifyString(RLBoxSandbox<TSandbox>* sandbox, std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			auto maskedFieldPtr = UNSAFE_Unverified();
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
		inline my_decay_if_array_t<T> getAppSwizzledValue(T arg) const
		{
			return arg;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getAppSwizzledValue(T arg) const
		{
			return (T) TSandbox::impl_GetUnsandboxedPointer(arg);
		}

		template<typename T2=T, ENABLE_IF(!my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getSandboxSwizzledValue(T arg) const
		{
			return arg;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline my_decay_if_array_t<T> getSandboxSwizzledValue(T arg) const
		{
			return (T) TSandbox::impl_GetSandboxedPointer(arg);
		}
	public:

		inline my_decay_if_array_t<T> UNSAFE_Unverified() const noexcept
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

	template <typename T, ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	inline T sandbox_removeWrapper(T& arg)
	{
		return arg;
	}

	template<typename TRHS, typename... TRHSRem, template<typename, typename...> class TWrap, ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, TWrap<TRHS, TRHSRem...>>)>
	inline auto sandbox_removeWrapper(TWrap<TRHS, TRHSRem...>& arg) -> decltype(arg.UNSAFE_Unverified())
	{
		return arg.UNSAFE_Unverified();
	}

	template<class TData> 
	class tainted_unwrapper {
	public:
		TData value_field;
	};

	template<typename T>
	tainted_unwrapper<T> sandbox_removeWrapper_t_helper(sandbox_wrapper_base_of<T>);

	template<typename T, ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
	tainted_unwrapper<T> sandbox_removeWrapper_t_helper(T);

	template <typename T>
	using sandbox_removeWrapper_t = decltype(sandbox_removeWrapper_t_helper(std::declval<T>()).value_field);

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename TSandbox, typename Ret>
	std::true_type sandbox_callback_all_args_are_tainted_helper(Ret(*) ());

	template<typename TSandbox, typename Ret, typename First, typename... Rest, ENABLE_IF(decltype(sandbox_callback_all_args_are_tainted_helper<TSandbox>(std::declval<Ret (*)(Rest...)>()))::value)>
	std::true_type sandbox_callback_all_args_are_tainted_helper(Ret(*) (tainted<First, TSandbox>, Rest...));

	template <typename TSandbox, typename TFunc>
	using sandbox_callback_all_args_are_tainted = decltype(sandbox_callback_all_args_are_tainted_helper(std::declval<TFunc>()));

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

		template<typename T, ENABLE_IF(!my_is_base_of_v<sandbox_wrapper_base, T>)>
		void freeInSandbox(T* val)
		{
			return this->impl_freeInSandbox(val);
		}

		template <typename T, ENABLE_IF(my_is_base_of_v<sandbox_wrapper_base, T>)>
		void freeInSandbox(T val)
		{
			return this->impl_freeInSandbox(val->UNSAFE_Unverified());
		}		

		bool isPointerInSandboxMemoryOrNull(const void* p)
		{
			return this->impl_isPointerInSandboxMemoryOrNull(p);
		}

		bool isPointerInAppMemoryOrNull(const void* p)
		{
			return this->impl_isPointerInAppMemoryOrNull(p);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_void_v<return_argument<T>> && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		void invokeStatic(T* fnPtr, TArgs&&... params)
		{
			fnPtr(sandbox_removeWrapper(params)...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_void_v<return_argument<T>> && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		tainted<return_argument<T>, TSandbox> invokeStatic(T* fnPtr, TArgs&&... params)
		{
			return_argument<T> fnRet = fnPtr(sandbox_removeWrapper(params)...);
			tainted<return_argument<T>, TSandbox> ret = sandbox_convertToUnverified<return_argument<T>>((TSandbox*) this, fnRet);
			return ret;
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_void_v<return_argument<T>> && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		void invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			this->impl_InvokeFunction(fnPtr, sandbox_removeWrapper(params)...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_void_v<return_argument<T>> && my_is_invocable_v<T, sandbox_removeWrapper_t<TArgs>...>)>
		tainted<return_argument<T>, TSandbox> invokeWithFunctionPointer(T* fnPtr, TArgs&&... params)
		{
			return_argument<T> fnRet = this->impl_InvokeFunction(fnPtr, sandbox_removeWrapper(params)...);
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
				fnPtr = this->impl_LookupSymbol(fnName);
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
			T* argInSandbox = (T*) this->impl_mallocInSandbox(size);
			memcpy((void*) argInSandbox, (void*) arg, size);

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

		template <typename TRet, typename... TArgs, ENABLE_IF(sandbox_callback_all_args_are_tainted<TSandbox, TRet(*)(TArgs...)>::value)>
		__attribute__ ((noinline))
		sandbox_callback_helper<TRet(sandbox_removeWrapper_t<TArgs>...), TSandbox> createCallback(TRet(*fnPtr)(TArgs...))
		{
			auto stateObject = new sandbox_callback_state<TRet(TArgs...), TSandbox>(this, fnPtr);
			void* callbackReciever = (void*)(uintptr_t) sandbox_callback_receiver<TSandbox, TRet, sandbox_removeWrapper_t<TArgs>...>;
			auto callbackRegisteredAddress = this->template impl_RegisterCallback<TRet, sandbox_removeWrapper_t<TArgs>...>(callbackReciever, (void*)stateObject);
			using fnType = TRet(sandbox_removeWrapper_t<TArgs>...);
			auto ret = sandbox_callback_helper<fnType, TSandbox>(this, (fnType*)(uintptr_t)callbackRegisteredAddress);
			return ret;
		}

	};

	#define sandbox_invoke(sandbox, fnName, ...) sandbox->invokeWithFunctionPointer((decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName), ##__VA_ARGS__)
	#define sandbox_invoke_static(sandbox, fnName, ...) sandbox->invokeStatic(&fnName, ##__VA_ARGS__)
	#define sandbox_invoke_with_fnptr(sandbox, fnPtr, ...) sandbox->invokeWithFunctionPointer(fnPtr, ##__VA_ARGS__)


	#undef ENABLE_IF
	#undef UNUSED
}
#endif
