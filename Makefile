.PHONY: build mkdir_out run32 run64

.DEFAULT_GOAL = build

SANDBOXING_NACL_DIR=$(shell realpath ../Sandboxing_NaCl)
WASM_SANDBOX_DIR=$(shell realpath ../wasm-sandboxing)
EMSDK_DIR=$(shell realpath ../emsdk)
NACL_INCLUDES=-I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr
NACL_LIBS_32=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-32/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_32 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_32 -lrt -lpthread
NACL_LIBS_64=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
WASM_INCLUDES=-I$(WASM_SANDBOX_DIR)/wasm2c
WASM_LIBS_64=$(WASM_SANDBOX_DIR)/bin/libwasm_sandbox.a

# 1 - .a input lib file
# 2 - .js output file
define convert_to_wasm =
	emcc $(1) -O0 -s WASM=1 -s TOTAL_MEMORY=2147418112 -s ALLOW_MEMORY_GROWTH=0 -s LEGALIZE_JS_FFI=0 -s EMULATED_FUNCTION_POINTERS=1 -s "EXPORTED_FUNCTIONS=[$$($(WASM_SANDBOX_DIR)/builds/getLLVMFileFunctions $(1)), '_malloc', '_free']" -o $(2) && \
	$(WASM_SANDBOX_DIR)/bin/wasm2wat --inline-exports --inline-imports -f $(patsubst %.js,%.wasm,$(2)) -o $(patsubst %.js,%.wat,$(2)) && \
	$(WASM_SANDBOX_DIR)/bin/wasm2c $(patsubst %.js,%.wasm,$(2)) -o $(patsubst %.js,%.c,$(2)) && \
	$(WASM_SANDBOX_DIR)/builds/generateModuleSpecificConstants $(2) > $(patsubst %.js,%_rt.cpp,$(2)) && \
	gcc -g3 -fPIC -I $(WASM_SANDBOX_DIR)/wasm2c -c $(patsubst %.js,%.c,$(2)) -o $(patsubst %.js,%.o,$(2)) && \
	g++ -g3 -fPIC -std=c++11 $(WASM_SANDBOX_DIR)/wasm2c/wasm-rt-impl.cpp $(WASM_SANDBOX_DIR)/wasm2c/wasm-rt-syscall-impl.cpp $(patsubst %.js,%_rt.cpp,$(2)) $(patsubst %.js,%.o,$(2)) -I $(WASM_SANDBOX_DIR)/wasm2c -fPIC -shared -o $(patsubst %.js,%.so,$(2))
endef

mkdir_out:
	mkdir -p ./out/x32
	mkdir -p ./out/x64

out/x32/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -m32 $(WASM_INCLUDES) -std=c++14 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp -Wl,--export-dynamic -ldl -o $@

out/x32/libtest.so: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -m32 -std=c++11 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

out/x32/libtest.nexe: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	$(NACL_CLANG++32) -O3 -m32 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib $(CURDIR)/libtest.cpp -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init -o $@

out/x64/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ $(WASM_INCLUDES) -std=c++14 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp -Wl,--export-dynamic -ldl $(WASM_LIBS_64) -o $@

out/x64/libtest.so: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++11 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

out/x64/libtest.nexe: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	$(NACL_CLANG++64) -O3 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib $(CURDIR)/libtest.cpp -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o $@

.ONESHELL:
SHELL=/bin/bash
out/x64/libwasm_test.so: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	source $(EMSDK_DIR)/emsdk_env.sh
	emcc -std=c++11 -g3 -O0 $(CURDIR)/libtest.cpp -c -o ./out/x64/libwasm_test.o
	cd ./out/x64
	$(call convert_to_wasm, libwasm_test.o, libwasm_test.js)

build: out/x32/test out/x64/test out/x32/libtest.so out/x64/libtest.so out/x32/libtest.nexe out/x64/libtest.nexe out/x64/libwasm_test.so

run32:
	cd ./out/x32 && ./test

run64:
	cd ./out/x64 && ./test

clean:
	rm -rf ./out
