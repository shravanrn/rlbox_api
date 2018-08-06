#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <utility>
#include <stdint.h>
#include <mutex>

namespace RLBox_DynLib_detail {
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
};

#define ENABLE_IF(...) typename std::enable_if<__VA_ARGS__>::type* = nullptr

class RLBox_DynLib
{
private:
	static thread_local RLBox_DynLib* dynLib_SavedState;
	std::mutex callbackMutex;
	static const unsigned int CALLBACK_SLOT_COUNT = 32;
	void* allowedFunctions[CALLBACK_SLOT_COUNT];
	void* functionState[CALLBACK_SLOT_COUNT];
	void* callbackUniqueKey[CALLBACK_SLOT_COUNT];
	void* libHandle = nullptr;
	int pushPopCount = 0;

	template<unsigned int N, typename TRet, typename... TArgs> 
	static TRet impl_CallbackReceiver(TArgs... params)
	{
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) dynLib_SavedState->allowedFunctions[N];
		return fnPtr(params..., dynLib_SavedState->functionState[N]);
	}

	template<unsigned int I, unsigned int N, typename TRet, typename... TArgs, typename std::enable_if<!(I < N)>::type* = nullptr>
	void impl_RegisterCallback_helper(void* key, void* callback, void* state, void*& result)
	{
	}

	template<unsigned int I, unsigned int N, typename TRet, typename... TArgs, typename std::enable_if<(I < N)>::type* = nullptr>
	void impl_RegisterCallback_helper(void* key, void* callback, void* state, void*& result)
	{
		if(!result && callbackUniqueKey[I] == nullptr)
		{
			callbackUniqueKey[I] = key;
			allowedFunctions[I] = callback;
			functionState[I] = state;
			result = (void*)(uintptr_t) impl_CallbackReceiver<I, TRet, TArgs...>;
		}

		impl_RegisterCallback_helper<I+1, N, TRet, TArgs...>(key, callback, state, result);
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

	//parameter val is a sandboxed pointer
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
			printf("Error - RLBox_DynLib popCount was negative.\n");
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

	inline void* impl_GetUnsandboxedPointer(void* p)
	{
		return p;
	}
	
	inline void* impl_GetSandboxedPointer(void* p)
	{
		return p;
	}

	inline void* impl_KeepAddressInSandboxedRange(void* p)
	{
		return p;
	}

	inline bool impl_isValidSandboxedPointer(const void* p)
	{
		return true;
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
	inline void* impl_RegisterCallback(void* key, void* callback, void* state)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		void* result = nullptr;
		impl_RegisterCallback_helper<0, CALLBACK_SLOT_COUNT, TRet, TArgs...>(key, callback, state, result);
		return result;
	}

	inline void impl_UnregisterCallback(void* key)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		for(unsigned int i = 0; i < CALLBACK_SLOT_COUNT; i++)
		{
			if(callbackUniqueKey[i] == key)
			{
				callbackUniqueKey[i] = nullptr;
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
	RLBox_DynLib_detail::return_argument<T> impl_InvokeFunction(T* fnPtr, TArgs... params)
	{
		dynLib_SavedState = this;
		return (*fnPtr)(params...);
	}

	template <typename T, typename ... TArgs>
	RLBox_DynLib_detail::return_argument<T> impl_InvokeFunctionReturnAppPtr(T* fnPtr, TArgs... params)
	{
		return impl_InvokeFunction(fnPtr, params...);
	}
};

#undef ENABLE_IF