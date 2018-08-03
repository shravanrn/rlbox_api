.PHONY: build

.DEFAULT_GOAL = build

SANDBOXING_NACL_DIR=$(shell realpath ../Sandboxing_NaCl)
NACL_INCLUDES = -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr
NACL_LIBS = -L$(SANDBOXING_NACL_DIR)/native_client/scons-out-firefox/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar

test: $(CURDIR)/test.cpp $(CURDIR)/rlbox.h $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ $(NACL_INCLUDES) -std=c++11 -g3 -Wall $(CURDIR)/test.cpp $(CURDIR)/libtest.cpp $(CURDIR)/RLBox_DynLib.cpp -ldl $(NACL_LIBS) -o $@

libtest.so: $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	g++ -std=c++11 -g3 -shared -fPIC $(CURDIR)/libtest.cpp -o $@

libtest.nexe: $(CURDIR)/libtest.cpp $(CURDIR)/libtest.h
	$(NACL_CLANG++64) -fPIC -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib $(CURDIR)/libtest.cpp -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o ./libtest.nexe

build: test libtest.so libtest.nexe

clean:
	rm -f ./test
	rm -f ./libtest.so
	rm -f ./libtest.nexe