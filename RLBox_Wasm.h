#ifndef RLBOX_API_WASM
#define RLBOX_API_WASM

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <mutex>
#include <type_traits>
#include "wasm_sandbox.h"

class RLBox_Wasm
{
private:
	WasmSandbox* sandbox;
	static std::vector<WasmSandbox*> sandboxList;
	std::mutex callbackMutex;
	class WasmSandboxStateWrapper
	{
	public:
		RLBox_Wasm* sandbox;
		void* originalState;
		void* fnPtr;
		WasmSandboxCallback* registeredCallback;

		WasmSandboxStateWrapper(RLBox_Wasm* sandbox, void* originalState, void* fnPtr)
		{
			this->sandbox = sandbox;
			this->originalState = originalState;
			this->fnPtr = fnPtr;
		}
	};
	std::map<void*, WasmSandboxStateWrapper*> callbackSlotInfo;

	template<typename TRet, typename... TArgs> 
	static TRet impl_CallbackReceiver(void* callbackState, TArgs... params)
	{
		WasmSandboxStateWrapper* callbackStateC = (WasmSandboxStateWrapper*) callbackState;
		using fnType = TRet(*)(TArgs..., void*);
		fnType fnPtr = (fnType)(uintptr_t) callbackStateC->fnPtr;
		return fnPtr(params..., callbackStateC->originalState);
	}

	template<typename T, typename std::enable_if<std::is_function<T>::value>::type* = nullptr>
	static T* impl_GetSandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return sandbox->registerInternalCallback(ptr);
	}

	template<typename T, typename std::enable_if<!std::is_function<T>::value>::type* = nullptr>
	static T* impl_GetSandboxedPointer_helper(WasmSandbox* sandbox, T* ptr)
	{
		return sandbox->getSandboxedPointer(ptr);
	}

public:
	#if defined(_M_X64) || defined(__x86_64__)
		static const bool impl_Handle32bitPointerArrays;
		uint32_t impl_MaintainAppPtrMapCounter = 0;
		std::mutex impl_MaintainAppPtrMapMutex;
		std::map<void*, void*> impl_MaintainAppPtrMap;
	#endif

	inline void impl_CreateSandbox(const char* sandboxRuntimePath, const char* libraryPath)
	{
		sandbox = WasmSandbox::createSandbox(libraryPath);
		if(!sandbox)
		{
			printf("Failed to create sandbox for: %s\n", libraryPath);
			abort();
		}

		sandboxList.push_back(sandbox);
	}

	inline void* impl_mallocInSandbox(size_t size)
	{
		return sandbox->mallocInSandbox(size);
	}

	//parameter val is a sandboxed pointer
	inline void impl_freeInSandbox(void* val)
	{
		sandbox->freeInSandbox(val);
	}

	inline size_t impl_getTotalMemory()
	{
		return sandbox->getTotalMemory();
	}

	inline char* impl_getMaxPointer()
	{
		void* maxPtr = (void*) (((uintptr_t)sandbox->getTotalMemory()) - 1);
		return (char*) sandbox->getUnsandboxedPointer(maxPtr);
	}

	inline void* impl_pushStackArr(size_t size)
	{
		//This is not supported
		return impl_mallocInSandbox(size);
	}

	inline void impl_popStackArr(void* ptr, size_t size)
	{
		//This is not supported
		return impl_freeInSandbox(ptr);
	}

	template<typename T>
	static inline T* impl_GetUnsandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		const bool isFuncPtr = std::is_function<T>::value;
		for(WasmSandbox* sandbox : sandboxList)
		{
			size_t memSize = sandbox->getTotalMemory();
			uintptr_t base = (uintptr_t) sandbox->getSandboxMemoryBase();
			uintptr_t exampleVal = (uintptr_t)exampleUnsandboxedPtr;
			if(exampleVal >= base && exampleVal < (base + memSize))
			{
				if(isFuncPtr) {
					return (T*) sandbox->getUnsandboxedFuncPointer(p);
				} else {
					return (T*) sandbox->getUnsandboxedPointer(p);
				}
			}
		}
		printf("Could not find sandbox for address: %p\n", p);
		abort();
	}

	template<typename T>
	static inline T* impl_GetSandboxedPointer(T* p, void* exampleUnsandboxedPtr)
	{
		const bool isFuncPtr = std::is_function<T>::value;
		for(WasmSandbox* sandbox : sandboxList)
		{
			size_t memSize = sandbox->getTotalMemory();
			uintptr_t base = (uintptr_t) sandbox->getSandboxMemoryBase();
			uintptr_t pVal = (uintptr_t)exampleUnsandboxedPtr;
			if(pVal >= base && pVal < (base + memSize))
			{
				return (T*) impl_GetSandboxedPointer_helper(sandbox, p);
			}
		}
		printf("Could not find sandbox for address: %p\n", p);
		abort();
	}

	template<typename T>
	inline T* impl_GetUnsandboxedPointer(T* p)
	{
		const bool isFuncPtr = std::is_function<T>::value;
		if (isFuncPtr) {
			return (T*) sandbox->getUnsandboxedFuncPointer(p);
		} else {
			return (T*) sandbox->getUnsandboxedPointer(p);
		}
	}

	template<typename T>
	inline T* impl_GetSandboxedPointer(T* p)
	{
		return (T*) impl_GetSandboxedPointer_helper(sandbox, p);
	}

	template<typename T>
	inline bool impl_isValidSandboxedPointer(T* p)
	{
		const bool isFuncPtr = std::is_function<T>::value;
		if (isFuncPtr) {
			return (sandbox->getUnsandboxedFuncPointer(p)) != nullptr;
		} else {
			return ((uintptr_t) p) < sandbox->getTotalMemory();
		}
	}

	inline bool impl_isPointerInSandboxMemoryOrNull(const void* p)
	{
		return sandbox->isAddressInSandboxMemoryOrNull(p);
	}

	inline bool impl_isPointerInAppMemoryOrNull(const void* p)
	{
		return sandbox->isAddressInNonSandboxMemoryOrNull(p);
	}

	template<typename TRet, typename... TArgs>
	inline void* impl_RegisterCallback(void* key, void* callback, void* state)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);
		auto stateWrapper = new WasmSandboxStateWrapper(this, state, callback);
		callbackSlotInfo[key] = stateWrapper;
		using funcType = TRet(*)(void*, TArgs...);
		auto callbackStub = (funcType) impl_CallbackReceiver<TRet, TArgs...>;
		WasmSandboxCallback* registeredCallback = sandbox->registerCallback(callbackStub, (void*)stateWrapper);
		stateWrapper->registeredCallback = registeredCallback;
		return (void*)(uintptr_t)registeredCallback->callbackSlot;
	}

	inline void impl_UnregisterCallback(void* key)
	{
		std::lock_guard<std::mutex> lock(callbackMutex);

		auto it = callbackSlotInfo.find(key);
		if(it != callbackSlotInfo.end())
		{
			WasmSandboxStateWrapper* slotInfo = it->second;
			callbackSlotInfo.erase(it);
			sandbox->unregisterCallback(slotInfo->registeredCallback);
			delete slotInfo;
		}
	}

	inline void* impl_LookupSymbol(const char* name)
	{
		return sandbox->symbolLookup(name);
	}

	template <typename TRet, typename ... TOrigArgs, typename ... TArgs>
	TRet impl_InvokeFunction(TRet(*fnPtr)(TOrigArgs...), TArgs... params)
	{
		return sandbox->invokeFunction(fnPtr, params...);
	}

	template <typename TRet, typename ... TOrigArgs, typename ... TArgs>
	TRet impl_InvokeFunctionReturnAppPtr(TRet(*fnPtr)(TOrigArgs...), TArgs... params)
	{
		using TargetFuncType = uint32_t(*)(TArgs...);
		uintptr_t rawRet = (uintptr_t) sandbox->invokeFunction((TargetFuncType) fnPtr, params...);
		return (TRet) rawRet;
	}
};

std::vector<WasmSandbox*> RLBox_Wasm::sandboxList __attribute__((weak));

#endif