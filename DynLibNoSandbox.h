#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <utility>
#include <stdint.h>

namespace DynLibNoSandbox_detail {
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

	//Adapted to C++ 11 from C++14 version at https://stackoverflow.com/questions/37602057/why-isnt-a-for-loop-a-compile-time-expression
	template<std::size_t N>
	struct num { static const constexpr auto value = N; };

	template <class F, unsigned int I, unsigned int N, typename std::enable_if<!(I < N)>::type* = nullptr>
	void for_helper(F func)
	{
	}

	template <class F, unsigned int I, unsigned int N, typename std::enable_if<(I < N)>::type* = nullptr>
	void for_helper(F func)
	{
		func(num<I>{});
		for_helper<F, I + 1, N>(func);
	}

	template <std::size_t N, typename F>
	void for_(F func)
	{
		for_helper<F, 0, N>(func);
	}

};

class DynLibNoSandbox;
extern thread_local DynLibNoSandbox* dynLibNoSandbox_SavedState;

class DynLibNoSandbox
{
private:
	static const unsigned int CALLBACK_SLOT_COUNT = 32;
	void* allowedFunctions[CALLBACK_SLOT_COUNT];
	void* functionState[CALLBACK_SLOT_COUNT];
	void* libHandle = nullptr;
	int pushPopCount = 0;

	template<unsigned int N, typename TRet, typename... TArgs> 
	static TRet impl_CallbackReceiver(TArgs... params)
	{
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) dynLibNoSandbox_SavedState->allowedFunctions[N];
		return fnPtr(params..., dynLibNoSandbox_SavedState->functionState[N]);
	}

public:
	inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		libHandle = dlopen(libraryPath, RTLD_LAZY);
		if(!libHandle)
		{
			printf("Library Load Failed: %s\n", libraryPath);
			exit(1);
		}
	}
	inline void* impl_mallocInSandbox(size_t size)
	{
		return malloc(size);
	}
	inline void impl_freeInSandbox(void* val)
	{
		free(val);
	}
	inline void* impl_pushStackArr(size_t size)
	{
		pushPopCount++;
		return malloc(size);
	}
	inline void impl_popStackArr(void* ptr, size_t size)
	{
		pushPopCount--;
		if(pushPopCount < 0)
		{
			printf("Error - DynLibNoSandbox popCount was negative.\n");
			exit(1);
		}
		return free(ptr);
	}

	static inline void* impl_GetUnsandboxedPointer(void* p, void* exampleUnsandboxedPtr)
	{
		return p;
	}
	
	static inline void* impl_GetSandboxedPointer(void* p, void* exampleSandboxedPtr)
	{
		return p;
	}

	inline void* impl_KeepAddressInSandboxedRange(void* p)
	{
		return p;
	}

	inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		return true;
	}

	inline bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return true;
	}

	template<typename TRet, typename... TArgs>
	void* impl_RegisterCallback(void* callback, void* state)
	{
		void* result = nullptr;
		//for loop with constexpr i
		DynLibNoSandbox_detail::for_<CALLBACK_SLOT_COUNT>([&](auto i) {
			if(!result && allowedFunctions[i.value] == nullptr)
			{
				allowedFunctions[i.value] = (void*)(uintptr_t) callback;
				functionState[i.value] = state;
				result = (void*)(uintptr_t) impl_CallbackReceiver<i.value, TRet, TArgs...>;
			}
		});

		return result;
	}

	template<typename TRet, typename... TArgs>
	void impl_UnregisterCallback(TRet(*callback)(TArgs...))
	{
		for(unsigned int i = 0; i < CALLBACK_SLOT_COUNT; i++)
		{
			if(allowedFunctions == (void*)(uintptr_t)callback)
			{
				allowedFunctions[i] = nullptr;
				functionState[i] = nullptr;
				break;
			}
		}
	}

	inline void* impl_LookupSymbol(const char* name)
	{
		auto ret = dlsym(libHandle, name);
		if(!ret)
		{
			printf("Symbol not found: %s. Error %s.", name, dlerror());
			exit(1);
		}
		return ret;
	}

	template <typename T, typename ... TArgs>
	DynLibNoSandbox_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
	{
		dynLibNoSandbox_SavedState = this;
		return (*fnPtr)(params...);
	}

	template <typename T, typename ... TArgs>
	DynLibNoSandbox_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
	{
		return impl_InvokeFunction(fnPtr, params...);
	}
};