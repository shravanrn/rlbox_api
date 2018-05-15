#ifndef RLBOX_API
#define RLBOX_API

#include <functional>
#include <type_traits>
#include <map>

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

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	//Some additional helpers

	#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<my_remove_pointer_t<T>>;

	template<typename T>
	using my_array_element_t = my_remove_reference_t<decltype(*(std::declval<T>()))>;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template <typename T>
	class app_ptr_wrap
	{
	private:
		T* field;
	public:
 		template<typename U>
		friend app_ptr_wrap<U> app_ptr(U* arg);

		template <typename U1, typename U2>
		friend class tainted_volatile;
	};

	template<typename T>
	inline app_ptr_wrap<T> app_ptr(T* arg)
	{
		app_ptr_wrap<T> ret;
		ret.field = arg;
		return ret;
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
	class tainted_base
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
			return (T) TSandbox::getUnsandboxedPointer(arg);
		}


		template<typename T2=T, ENABLE_IF(!my_is_pointer_v<T2>)>
		inline T getSandboxSwizzledValue(T arg) const
		{
			return arg;
		}

		template<typename T2=T, ENABLE_IF(my_is_pointer_v<T2>)>
		inline T getSandboxSwizzledValue(T arg) const
		{
			return (T) TSandbox::getSandboxedPointer(arg);
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
		inline tainted_volatile<T, TSandbox>& operator=(const app_ptr_wrap<TRHS>& arg) noexcept
		{
			field = arg.field;
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
			ret->initMemoryIsolation(sandboxRuntimePath, libraryPath);
			return ret;
		}

		template<typename T>
		tainted<T*, TSandbox> newInSandbox(unsigned int count=1)
		{
			void* addr = this->mallocInSandbox(sizeof(T) * count);
			void* rangeCheckedAddr = this->keepAddressInSandboxedRange(addr);
			tainted<T*, TSandbox> ret;
			ret.field = (T*) rangeCheckedAddr;
			return ret;
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_same_v<return_argument<T>, void>)>
		void invokeStatic(T* fnPtr, TArgs... params)
		{
			fnPtr(params...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_same_v<return_argument<T>, void>)>
		tainted<return_argument<T>, TSandbox> invokeStatic(T* fnPtr, TArgs... params)
		{
			return_argument<T> fnRet = fnPtr(params...);
			tainted<return_argument<T>, TSandbox> ret = sandbox_convertToUnverified<return_argument<T>>((TSandbox*) this, fnRet);
			return ret;
		}

		template <typename T, typename ... TArgs, ENABLE_IF(my_is_same_v<return_argument<T>, void>)>
		return_argument<T> invokeWithFunctionPointer(T* fnPtr, TArgs... params)
		{
			this->invokeFunction(fnPtr, params...);
		}

		template <typename T, typename ... TArgs, ENABLE_IF(!my_is_same_v<return_argument<T>, void>)>
		tainted<return_argument<T>, TSandbox> invokeWithFunctionPointer(T* fnPtr, TArgs... params)
		{
			return_argument<T> fnRet = this->invokeFunction(fnPtr, params...);
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
	};

	#define sandbox_invoke(sandbox, fnName, ...) sandbox->invokeWithFunctionPointer((decltype(fnName)*)sandbox->getFunctionPointerFromCache(#fnName), ##__VA_ARGS__)
	#define sandbox_invoke_static(sandbox, fnName, ...) sandbox->invokeStatic(&fnName, ##__VA_ARGS__)
	#define sandbox_invoke_with_fnptr(sandbox, fnPtr, ...) sandbox->invokeWithFunctionPointer(fnPtr, ##__VA_ARGS__)


	#undef ENABLE_IF
	#undef UNUSED
}
#endif
