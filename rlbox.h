#ifndef RLBOX_API
#define RLBOX_API

#include <functional>
#include <type_traits>

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

namespace rlbox_api_detail
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

	template<typename T>
	constexpr bool my_is_one_level_ptr_v = my_is_pointer_v<my_remove_pointer_t<T>>;

	template<typename T>
	using my_remove_reference_t = typename std::remove_reference<T>::type;

	//Use a custom enum for returns as boolean returns are a bad idea
	//int returns are automatically cast to a boolean
	//Some APIs have overloads with boolean and non returns, so best to use a custom class
	enum class RLBox_Verify_Status
	{
		SAFE, UNSAFE
	};

	template<typename TField, typename T, bool TUsesSandboxAddress>
	inline my_enable_if_t<my_is_pointer_v<T> && TUsesSandboxAddress,
	T> getValueOrSwizzledValue(TField field)
	{
		return field;
	}

	template<typename TField, typename T, bool TUsesSandboxAddress>
	inline my_enable_if_t<!(my_is_pointer_v<T> && TUsesSandboxAddress),
	T> getValueOrSwizzledValue(TField field)
	{
		return field;
	}

	template<typename TField, typename T>
	inline T getFieldCopy(TField field)
	{
		T copy = field;
		return T;
	}


	template<typename TField, bool TUsesSandboxAddress>
	class tainted_base
	{

	private:

		TField field;
		using T = my_remove_volatile_t<my_remove_reference_t<TField>>;

	public:

		inline T unsafeUnverified() const
		{
			return getValueOrSwizzledValue<TField, T, TUsesSandboxAddress>(field);
		}

		template<typename T2=T, ENABLE_IF_P(!my_is_pointer_v<T2>)>
		inline T copyAndVerify(std::function<T(T)> verifyFunction) const
		{
			T copy = getFieldCopy();
			return verifyFunction(copy);
		}

		template<typename T2=T, ENABLE_IF_P(!my_is_pointer_v<T2>)>
		inline T copyAndVerify(std::function<RLBox_Verify_Status(T)> verifyFunction, T defaultValue) const
		{
			T copy = getFieldCopy();
			return verifyFunction(copy) == RLBox_Verify_Status::SAFE? copy : defaultValue;
		}
	};
}


template<typename T>
class tainted : public tainted_base<T, false>
{
};

#undef ENABLE_IF
#undef UNUSED
#endif
