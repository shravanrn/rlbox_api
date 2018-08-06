.PHONY: build mkdir_out run32 run64

.DEFAULT_GOAL = build

SANDBOXING_NACL_DIR=$(shell realpath ../Sandboxing_NaCl)
NACL_INCLUDES=-I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr
NACL_LIBS_32=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-32/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_32 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_32 -lrt -lpthread
NACL_LIBS_64=-L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar

mkdir_out:
	mkdir -p ./out/x32
	mkdir -p ./out/x64

out/x32/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -m32 $(NACL_INCLUDES) -std=c++14 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp $(CURDIR)/RLBox_DynLib.cpp -ldl $(NACL_LIBS_32) -o $@

out/x32/libtest.so: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -m32 -std=c++11 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

out/x32/libtest.nexe: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	$(NACL_CLANG++32) -O3 -m32 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib $(CURDIR)/libtest.cpp -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_32-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init -o $@

out/x64/test: mkdir_out $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ $(NACL_INCLUDES) -std=c++14 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp $(CURDIR)/RLBox_DynLib.cpp -ldl $(NACL_LIBS_64) -o $@

out/x64/libtest.so: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++11 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

out/x64/libtest.nexe: mkdir_out $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	$(NACL_CLANG++64) -O3 -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib $(CURDIR)/libtest.cpp -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o $@

build: out/x32/test out/x64/test out/x32/libtest.so out/x64/libtest.so out/x32/libtest.nexe out/x64/libtest.nexe

run32:
	cd ./out/x32 && ./test

run64:
	cd ./out/x64 && ./test

clean:
	rm -rf ./out