#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>

class DynLibNoSandbox;
extern thread_local DynLibNoSandbox* dynLibNoSandbox_SavedState;

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
};

class DynLibNoSandbox;
thread_local DynLibNoSandbox* dynLibNoSandbox_SavedState = nullptr;

class DynLibNoSandbox
{
private:
	void* allowedFunctions[32];
	void* functionState[32];
	void* libHandle = nullptr;
	int pushPopCount = 0;

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

	static inline void* impl_GetUnsandboxedPointer(void* p)
	{
		return p;
	}
	
	static inline void* impl_GetSandboxedPointer(void* p)
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
	static TRet impl_CallbackReceiver0(TArgs... params)
	{
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) dynLibNoSandbox_SavedState->allowedFunctions[0];
		return fnPtr(params..., dynLibNoSandbox_SavedState->functionState[0]);
	}

	template<typename TRet, typename... TArgs>
	void* impl_RegisterCallback(void* callback, void* state)
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
		{
			if(allowedFunctions[i] == nullptr)
			{
				allowedFunctions[i] = (void*)(uintptr_t) callback;
				functionState[i] = state;
				return (void*)(uintptr_t) impl_CallbackReceiver0<TRet, TArgs...>;
			}
		}

		return nullptr;
	}

	template<typename TRet, typename... TArgs>
	void impl_UnregisterCallback(TRet(*callback)(TArgs...))
	{
		for(unsigned int i = 0; i < sizeof(allowedFunctions)/sizeof(void*); i++)
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
};