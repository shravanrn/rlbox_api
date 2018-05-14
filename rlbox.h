#ifndef RLBOX_API
#define RLBOX_API

#include <functional>
#include <type_traits>

namespace rlbox
{
	//https://stackoverflow.com/questions/19532475/casting-a-variadic-parameter-pack-to-void
	struct UNUSED_PACK_HELPER { template<typename ...Args> UNUSED_PACK_HELPER(Args const & ... ) {} };
	#define UNUSED(x) rlbox_api_detail::UNUSED_PACK_HELPER {x}

	template<bool T, typename V>
	using my_enable_if_t = typename std::enable_if<T, V>::type;
	#define ENABLE_IF_P(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

	template<typename T>
	using my_remove_volatile_t = typename std::remove_volatile<T>::type;

	template<typename T>
	constexpr bool my_is_pointer_v = std::is_pointer<T>::value;

	template< class T >
	using my_remove_pointer_t = typename std::remove_pointer<T>::type;

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<my_remove_pointer_t<T>>;

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

	//Use a custom enum for returns as boolean returns are a bad idea
	//int returns are automatically cast to a boolean
	//Some APIs have overloads with boolean and non returns, so best to use a custom class
	enum class RLBox_Verify_Status
	{
		SAFE, UNSAFE
	};

	//template<typename T>
	//inline T getUnsandboxedValue(T field)
	//{
	//	return field;
	//}

	template<typename TField, typename T, ENABLE_IF_P(!my_is_class_v<T> && !my_is_reference_v<T> && !my_is_array_v<T>)>
	inline T getFieldCopy(TField field)
	{
		T copy = field;
		return copy;
	}


	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	class RLBox_Sandbox
	{
	public:
		virtual void createSandbox(char* sandboxRuntimePath, char* libraryPath) = 0;
		void* mallocInSandbox(size_t size);
		void freeInSandbox(void* val);

		template<typename T>
		void* registerCallback(std::function<T> callback);

		void unregisterCallback(void* callback);
	};

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	template<typename T>
	class tainted_base
	{
		virtual inline T UNSAFE_Unverified() const = 0;
	};

	template<typename T>
	class tainted_volatile;

	template<typename T>
	class tainted : public tainted_base<T>
	{
		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U>
		friend class tainted;

	private:
		T field;

		template<typename TRHS, template <typename> typename TWrap, ENABLE_IF_P(my_is_base_of_v<tainted_base<TRHS>, TWrap<TRHS>>)>
		inline TRHS unwrap(const TWrap<TRHS> rhs) const noexcept
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
		tainted(const tainted<T>& p) = default;

		template<typename T2=T, ENABLE_IF_P(my_is_fundamental_v<T2>)>
		tainted(const tainted_volatile<T>& p)
		{
			field = p.field;
		}

		//we explicitly disable this constructor if it has one of the signatures above, 
		//	so that we give the above constructors a higher priority
		//	For now we only allow this for fundamental types as this is potentially unsafe for pointers and structs
		template<typename Arg, typename... Args, ENABLE_IF_P(!my_is_base_of_v<tainted_base<T>, my_remove_reference_t<Arg>> && my_is_fundamental_v<T>)>
		tainted(Arg&& arg, Args&&... args) : field(std::forward<Arg>(arg), std::forward<Args>(args)...) { }

		inline T UNSAFE_Unverified() const noexcept override
		{
			return field;
		}

		template<typename T2=T, ENABLE_IF_P(my_is_fundamental_v<T2>)>
		inline T copyAndVerify(std::function<T(T)> verifyFunction) const
		{
			return verifyFunction(field);
		}

		template<typename T2=T, ENABLE_IF_P(my_is_fundamental_v<T2>)>
		inline T copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			return verifyFunction(field) == RLBox_Verify_Status::SAFE? field : defaultValue;
		}

		template<typename TRHS, ENABLE_IF_P(my_is_fundamental_v<T> && my_is_assignable_v<T&, TRHS>)>
		inline tainted<T>& operator=(const TRHS& arg) noexcept
		{
			field = arg;
			return *this;
		}

		inline tainted<T> operator-() const noexcept
		{
			tainted<T> result = -field;
			return result;
		}

		template<typename T2=T, ENABLE_IF_P(my_is_pointer_v<T2>)>
		inline tainted_volatile<my_remove_pointer_t<T>>& operator*() const noexcept
		{
			return *((tainted_volatile<my_remove_pointer_t<T>>*) field);
		}

		template<typename TRHS>
		inline tainted<T> operator+(const TRHS rhs) const noexcept
		{
			tainted<T> result = field + unwrap(rhs);
			return result;
		}

		template<typename TRHS>
		inline tainted<T> operator-(const TRHS rhs) const noexcept
		{
			tainted<T> result = field - unwrap(rhs);
			return result;
		}

		template<typename TRHS>
		inline tainted<T> operator*(const TRHS rhs) const noexcept
		{
			tainted<T> result = field * unwrap(rhs);
			return result;
		}
	};

	template<typename T>
	class tainted_volatile : public tainted_base<T>
	{
		//make sure tainted_volatile<T1> can access private members of tainted_volatile<T2>
		template <typename U>
		friend class tainted_volatile;

		//make sure tainted<T1> can access private members of tainted<T2>
		template <typename U>
		friend class tainted;

	private:
		my_add_volatile_t<T> field;

		tainted_volatile() = default;
		tainted_volatile(const tainted_volatile<T>& p) = default;

		template<typename T2=T, ENABLE_IF_P(!my_is_pointer_v<T2>)>
		inline T getValueOrSwizzledValue() const
		{
			return field;
		}

		template<typename T2=T, ENABLE_IF_P(my_is_pointer_v<T2>)>
		inline T getValueOrSwizzledValue() const
		{
			return getUnsandboxedValue(field);
		}
	public:

		inline T UNSAFE_Unverified() const override
		{
			return getValueOrSwizzledValue();
		}
	};

	#undef ENABLE_IF_P
	#undef UNUSED
}
#endif
