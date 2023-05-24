CC=clang
CXX=clang++
override CFLAGS += -O3 -fPIC
CFLAGS32=$(CFLAGS) -m32
CFLAGS64=$(CFLAGS) -m64
LDFLAGS32=$(LDFLAGS) -m32
LDFLAGS64=$(LDFLAGS) -m64
HOST32=i686-pc-linux-gnu
HOST64=x86_64-linux-gnu

# Folder names to store 32-bit and 64-bit builds respectively
BUILDS32=x86
BUILDS64=x64

# paths to various external resources
NACL_NASM=$(shell realpath ../../NASM_NaCl/nasm)
SANDBOXING_NACL_DIR=$(shell realpath ../../Sandboxing_NaCl)
NACL_CLANG32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-clang
NACL_CLANG64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-clang
NACL_CLANG++32=$(NACL_CLANG32)++
NACL_CLANG++64=$(NACL_CLANG64)++
NACL_AR32=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/i686-nacl-ar
NACL_AR64=$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/bin/x86_64-nacl-ar
PROCESS_SANDBOX_DIR=$(shell realpath ../../ProcessSandbox)
RLBOX_DIR=$(shell realpath ../../rlbox_api)

# paths to the appropriate ProcessSandbox_otherside executables
OTHERSIDE_EXEC32 ?= $(PROCESS_SANDBOX_DIR)/ProcessSandbox_otherside_jpeg32
OTHERSIDE_EXEC64 ?= $(PROCESS_SANDBOX_DIR)/ProcessSandbox_otherside_jpeg64
OTHERSIDE_EXEC32_SIMD ?= $(PROCESS_SANDBOX_DIR)/ProcessSandbox_otherside_jpeg32_simd
OTHERSIDE_EXEC64_SIMD ?= $(PROCESS_SANDBOX_DIR)/ProcessSandbox_otherside_jpeg64_simd

# Core to pin main process (or only process) to, for the 'run' and 'debug' targets
MAINCORE=1
# Core to pin sandbox process to, for the 'run_dynamic_ps' and 'debug_dynamic_ps' targets
SANDBOXCORE=3

# Default library process priority, for the 'run' and 'debug' targets
DEFAULTLIBPRIO=0

BUILDS_BASENAMES=non_nacl_build nacl_build
ALLBUILDS32=$(addprefix $(BUILDS32)/,$(BUILDS_BASENAMES)) $(addprefix $(BUILDS32)/,$(addsuffix _simd,$(BUILDS_BASENAMES)))
ALLBUILDS64=$(addprefix $(BUILDS64)/,$(BUILDS_BASENAMES)) $(addprefix $(BUILDS64)/,$(addsuffix _simd,$(BUILDS_BASENAMES)))
PRODUCTS_BASENAMES=example_dynamic_non_nacl example_dynamic_nacl example_dynamic_ps
PRODUCTS32=$(addprefix $(BUILDS32)/,$(PRODUCTS_BASENAMES)) $(addprefix $(BUILDS32)/,$(addsuffix _simd, $(PRODUCTS_BASENAMES)))
PRODUCTS64=$(addprefix $(BUILDS64)/,$(PRODUCTS_BASENAMES)) $(addprefix $(BUILDS64)/,$(addsuffix _simd, $(PRODUCTS_BASENAMES))) $(addprefix $(BUILDS64)/,$(addsuffix _safe_api, $(PRODUCTS_BASENAMES))) $(addprefix $(BUILDS64)/,$(addsuffix _simd_safe_api, $(PRODUCTS_BASENAMES))) $(BUILDS64)/scaling_benchmark_nacl $(BUILDS64)/scaling_benchmark_ps $(BUILDS64)/scaling_benchmark_ps_spin
PRODUCTS=$(PRODUCTS32) $(PRODUCTS64)

RUN_BASENAMES=run_dynamic_non_nacl run_dynamic_non_nacl_simd run_dynamic_nacl run_dynamic_nacl_simd run_dynamic_nacl_safe_api run_dynamic_nacl_simd_safe_api run_dynamic_ps run_dynamic_ps_simd run_dynamic_ps_safe_api run_dynamic_ps_simd_safe_api
RUN32=$(addprefix $(BUILDS32)/,$(RUN_BASENAMES))
RUN64=$(addprefix $(BUILDS64)/,$(RUN_BASENAMES))

.PHONY : all all32 all64 run_all run_all32 run_all64 $(RUN32) $(RUN64) dbg_dynamic_non_nacl dbg_dynamic_nacl dbg_dynamic_ps debug clean kill_old_processes

all : $(PRODUCTS)
all32 : $(PRODUCTS32)
all64 : $(PRODUCTS64)
build32 : $(ALLBUILDS32)
build64 : $(ALLBUILDS64)

run_all32 : $(RUN32)
run_all64 : $(RUN64)
run_all : run_all32 run_all64

define prepare_build =
	cd .. && autoreconf -fiv
	rm -rf $(1)
	mkdir $(1)
	cd $(1)
endef

define create_libjpeg_nexe_32 =
	mkdir -p mainCombine && \
	cd ./mainCombine && \
	$(NACL_AR32) -x ../.libs/libjpeg.a && \
	$(NACL_CLANG++32) -fPIC -m32 -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib32 -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-32/lib -ldyn_ldr_sandbox_init -o ./libjpeg.nexe
endef

define create_libjpeg_nexe_64 =
	mkdir -p mainCombine && \
	cd ./mainCombine && \
	$(NACL_AR64) -x ../.libs/libjpeg.a && \
	$(NACL_CLANG++64) -fPIC -m64 -B$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib/ -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -Wl,-rpath-link,$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib ./*.o -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/toolchain/linux_x86/pnacl_newlib/x86_64-nacl/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl-x86-64/lib -ldyn_ldr_sandbox_init -o ./libjpeg.nexe
endef

SIMDFLAGS=
NONSIMDFLAGS=--without-simd
NACLFLAGS32= NASM=$(NACL_NASM) AR=$(NACL_AR32) NAFLAGS='-nacl -DCOMPILING_FOR_NACL'
NACLFLAGS64= NASM=$(NACL_NASM) AR=$(NACL_AR64) NAFLAGS='-nacl -DCOMPILING_FOR_NACL'

$(BUILDS32)/nacl_build :
	mkdir -p $(BUILDS32)
	$(call prepare_build,$@) && \
	sh ../../../configure --host $(HOST32) --disable-shared CFLAGS='$(CFLAGS32) -fPIC' LDFLAGS='$(LDFLAGS32) -fPIC' CC=$(NACL_CLANG32) $(NONSIMDFLAGS) $(NACLFLAGS32) && \
	make clean && \
	make && \
	$(call create_libjpeg_nexe_32)

$(BUILDS64)/nacl_build :
	mkdir -p $(BUILDS64)
	$(call prepare_build,$@) && \
	sh ../../../configure --host $(HOST64) CFLAGS='$(CFLAGS64) -fPIC' LDFLAGS='$(LDFLAGS64) -fPIC' CC=$(NACL_CLANG64) $(NONSIMDFLAGS) $(NACLFLAGS64) && \
	make clean && \
	make && \
	$(call create_libjpeg_nexe_64)

$(BUILDS32)/nacl_build_simd :
	mkdir -p $(BUILDS32)
	$(call prepare_build,$@) && \
	sh ../../../configure --host $(HOST32) --disable-shared CFLAGS='$(CFLAGS32) -fPIC' LDFLAGS='$(LDFLAGS32) -fPIC' CC=$(NACL_CLANG32) $(SIMDFLAGS) $(NACLFLAGS32) && \
	make clean && \
	make && \
	$(call create_libjpeg_nexe_32)

$(BUILDS64)/nacl_build_simd :
	mkdir -p $(BUILDS64)
	$(call prepare_build,$@) && \
	sh ../../../configure --host $(HOST64) CFLAGS='$(CFLAGS64) -fPIC' LDFLAGS='$(LDFLAGS64) -fPIC' CC=$(NACL_CLANG64) $(SIMDFLAGS) $(NACLFLAGS64) && \
	make clean && \
	make && \
	$(call create_libjpeg_nexe_64)

$(BUILDS32)/non_nacl_build :
	mkdir -p $(BUILDS32)
	$(call prepare_build,$@) && \
	sh ../../../configure -host $(HOST32) CFLAGS='$(CFLAGS32)' LDFLAGS='$(LDFLAGS32)' CC=$(CC) $(NONSIMDFLAGS) NASM=$(NACL_NASM) && \
	make clean && \
	make

$(BUILDS64)/non_nacl_build :
	mkdir -p $(BUILDS64)
	$(call prepare_build,$@) && \
	sh ../../../configure -host $(HOST64) CFLAGS='$(CFLAGS64)' LDFLAGS='$(LDFLAGS64)' CC=$(CC) $(NONSIMDFLAGS) NASM=$(NACL_NASM) && \
	make clean && \
	make

$(BUILDS32)/non_nacl_build_simd :
	mkdir -p $(BUILDS32)
	$(call prepare_build,$@) && \
	sh ../../../configure -host $(HOST32) CFLAGS='$(CFLAGS32)' LDFLAGS='$(LDFLAGS32)' CC=$(CC) $(SIMDFLAGS) NASM=$(NACL_NASM) && \
	make clean && \
	make

$(BUILDS64)/non_nacl_build_simd :
	mkdir -p $(BUILDS64)
	$(call prepare_build,$@) && \
	sh ../../../configure -host $(HOST64) CFLAGS='$(CFLAGS64)' LDFLAGS='$(LDFLAGS64)' CC=$(CC) $(SIMDFLAGS) NASM=$(NACL_NASM) && \
	make clean && \
	make

$(BUILDS32)/example_dynamic_non_nacl : ../example_dynamic_non_nacl.cpp $(BUILDS32)/non_nacl_build
	$(CXX) $(CFLAGS32) -std=c++11 -I$(BUILDS32)/non_nacl_build $< -o $@ -ldl

$(BUILDS64)/example_dynamic_non_nacl : ../example_dynamic_non_nacl.cpp $(BUILDS64)/non_nacl_build
	$(CXX) $(CFLAGS64) -std=c++11 -I$(BUILDS64)/non_nacl_build $< -o $@ -ldl

$(BUILDS32)/example_dynamic_non_nacl_simd : ../example_dynamic_non_nacl.cpp $(BUILDS32)/non_nacl_build_simd
	$(CXX) $(CFLAGS32) -std=c++11 -I$(BUILDS32)/non_nacl_build_simd $< -o $@ -ldl

$(BUILDS64)/example_dynamic_non_nacl_simd : ../example_dynamic_non_nacl.cpp $(BUILDS64)/non_nacl_build_simd
	$(CXX) $(CFLAGS64) -std=c++11 -I$(BUILDS64)/non_nacl_build_simd $< -o $@ -ldl

$(BUILDS32)/example_dynamic_non_nacl_safe_api:
$(BUILDS64)/example_dynamic_non_nacl_safe_api:
$(BUILDS32)/example_dynamic_non_nacl_simd_safe_api:
$(BUILDS64)/example_dynamic_non_nacl_simd_safe_api:


$(BUILDS32)/example_dynamic_nacl.o : ../example_dynamic_nacl.cpp $(BUILDS32)/nacl_build
	$(CXX) -c $(CFLAGS32) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS32)/nacl_build -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

$(BUILDS64)/example_dynamic_nacl.o : ../example_dynamic_nacl.cpp $(BUILDS64)/nacl_build
	$(CXX) -c $(CFLAGS64) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

$(BUILDS32)/example_dynamic_nacl_simd.o : ../example_dynamic_nacl.cpp $(BUILDS32)/nacl_build_simd
	$(CXX) -c $(CFLAGS32) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS32)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

$(BUILDS64)/example_dynamic_nacl_simd.o : ../example_dynamic_nacl.cpp $(BUILDS64)/nacl_build_simd
	$(CXX) -c $(CFLAGS64) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

$(BUILDS64)/example_dynamic_nacl_safe_api.o : ../example_dynamic_safe_api.cpp $(BUILDS64)/nacl_build
	$(CXX) -c $(CFLAGS64) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

$(BUILDS64)/example_dynamic_nacl_simd_safe_api.o : ../example_dynamic_safe_api.cpp $(BUILDS64)/nacl_build_simd
	$(CXX) -c $(CFLAGS64) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@

FLAGS_WHEN_LINKING_NACL_EXAMPLES_32 = -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -fPIC -Wl,-rpath=$(SANDBOXING_NACL_DIR)/native_client/scons-out/opt-linux-x86-32/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/opt-linux-x86-32/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_32 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_32 -lrt -lpthread
FLAGS_WHEN_LINKING_NACL_EXAMPLES_64 = -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -fPIC -Wl,-rpath=$(SANDBOXING_NACL_DIR)/native_client/scons-out/opt-linux-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/opt-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread
FLAGS_WHEN_LINKING_NACL_EXAMPLES_64_DEBUG = -pie -Wl,-z,relro -Wl,-z,now -Wl,-z,noexecstack -fPIC -Wl,-rpath=$(SANDBOXING_NACL_DIR)/native_client/scons-out/dbg-linux-x86-64/lib -L$(SANDBOXING_NACL_DIR)/native_client/scons-out/dbg-linux-x86-64/lib -ldyn_ldr -lsel -lnacl_error_code -lenv_cleanser -lnrd_xfer -lnacl_perf_counter -lnacl_base -limc -lnacl_fault_inject -lnacl_interval -lplatform_qual_lib -lvalidators -ldfa_validate_caller_x86_64 -lcpu_features -lvalidation_cache -lplatform -lgio -lnccopy_x86_64 -lrt -lpthread

$(BUILDS32)/example_dynamic_nacl : $(BUILDS32)/example_dynamic_nacl.o
	$(CXX) $(CFLAGS32) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_32)

$(BUILDS64)/example_dynamic_nacl : $(BUILDS64)/example_dynamic_nacl.o
	$(CXX) $(CFLAGS64) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS32)/example_dynamic_nacl_simd : $(BUILDS32)/example_dynamic_nacl_simd.o
	$(CXX) $(CFLAGS32) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_32)

$(BUILDS64)/example_dynamic_nacl_simd : $(BUILDS64)/example_dynamic_nacl_simd.o
	$(CXX) $(CFLAGS64) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/example_dynamic_nacl_safe_api : $(BUILDS64)/example_dynamic_nacl_safe_api.o
	$(CXX) $(CFLAGS64) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/example_dynamic_nacl_simd_safe_api : $(BUILDS64)/example_dynamic_nacl_simd_safe_api.o
	$(CXX) $(CFLAGS64) $< -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/example_dynamic_new_scaling_dyn : ../example_dynamic_new_scaling.cpp
	$(CXX) $(CFLAGS64) -std=c++14 -fPIC -DUSE_DYN -I$(BUILDS64)/non_nacl_build_simd -I$(RLBOX_DIR) $< -o $@ -lpthread -ldl

$(BUILDS64)/example_dynamic_new_scaling_nacl : ../example_dynamic_new_scaling.cpp
	$(CXX) $(CFLAGS64) -std=c++14 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ -I$(RLBOX_DIR) $< -o $@ -lpthread $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/example_dynamic_new_scaling_nacl_dbg : ../example_dynamic_new_scaling.cpp
	$(CXX) -fPIC -m64 -g -O3 -std=c++14 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ -I$(RLBOX_DIR) $< -o $@ -lpthread $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64_DEBUG)

$(BUILDS64)/example_dynamic_new_scaling_create_dyn : ../example_dynamic_new_scaling_create.cpp
	$(CXX) $(CFLAGS64) -std=c++14 -fPIC -DUSE_DYN -I$(BUILDS64)/non_nacl_build_simd -I$(RLBOX_DIR) $< -o $@ -lpthread -ldl

$(BUILDS64)/example_dynamic_new_scaling_create_nacl : ../example_dynamic_new_scaling_create.cpp
	$(CXX) $(CFLAGS64) -std=c++14 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ -I$(RLBOX_DIR) $< -o $@ -lpthread $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/scaling_benchmark_nacl : ../scaling_benchmark.cpp
	$(CXX) -c $(CFLAGS64) -std=c++11 -fPIC -DUSE_NACL -I$(BUILDS64)/nacl_build_simd -I$(SANDBOXING_NACL_DIR)/native_client/src/trusted/dyn_ldr/ $< -o $@.o
	$(CXX) $(CFLAGS64) $@.o -o $@ $(FLAGS_WHEN_LINKING_NACL_EXAMPLES_64)

$(BUILDS64)/scaling_benchmark_none : ../scaling_benchmark.cpp
	$(CXX) $(CFLAGS64) -std=c++11 -fPIC -DUSE_NONE -I$(BUILDS64)/non_nacl_build_simd $< -o $@ $(BUILDS64)/non_nacl_build_simd/.libs/libturbojpeg.a

$(BUILDS32)/example_dynamic_ps.o : ../example_dynamic_nacl.cpp $(BUILDS32)/non_nacl_build
	$(CXX) -c $(CFLAGS32) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS32)/non_nacl_build -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS64)/example_dynamic_ps.o : ../example_dynamic_nacl.cpp $(BUILDS64)/non_nacl_build
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS64)/non_nacl_build -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS32)/example_dynamic_ps_simd.o : ../example_dynamic_nacl.cpp $(BUILDS32)/non_nacl_build_simd
	$(CXX) -c $(CFLAGS32) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS32)/non_nacl_build_simd -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS64)/example_dynamic_ps_safe_api.o : ../example_dynamic_safe_api.cpp $(BUILDS64)/non_nacl_build
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS64)/non_nacl_build -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS64)/example_dynamic_ps_simd_safe_api.o : ../example_dynamic_safe_api.cpp $(BUILDS64)/non_nacl_build_simd
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS64)/non_nacl_build_simd -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS64)/example_dynamic_ps_simd.o : ../example_dynamic_nacl.cpp $(BUILDS64)/non_nacl_build_simd
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS64)/non_nacl_build_simd -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@

$(BUILDS32)/example_dynamic_ps : $(BUILDS32)/example_dynamic_ps.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg32.a
	$(CXX) $(CFLAGS32) $^ -o $@ -lrt -lpthread

$(BUILDS64)/example_dynamic_ps : $(BUILDS64)/example_dynamic_ps.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a
	$(CXX) $(CFLAGS64) $^ -o $@ -lrt -lpthread

$(BUILDS32)/example_dynamic_ps_simd : $(BUILDS32)/example_dynamic_ps_simd.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg32.a
	$(CXX) $(CFLAGS32) $^ -o $@ -lrt -lpthread

$(BUILDS64)/example_dynamic_ps_simd : $(BUILDS64)/example_dynamic_ps_simd.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a
	$(CXX) $(CFLAGS64) $^ -o $@ -lrt -lpthread

$(BUILDS64)/example_dynamic_ps_safe_api : $(BUILDS64)/example_dynamic_ps_safe_api.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a
	$(CXX) $(CFLAGS64) $^ -o $@ -lrt -lpthread

$(BUILDS64)/example_dynamic_ps_simd_safe_api : $(BUILDS64)/example_dynamic_ps_simd_safe_api.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a
	$(CXX) $(CFLAGS64) $^ -o $@ -lrt -lpthread

$(BUILDS64)/scaling_benchmark_ps : ../scaling_benchmark.cpp
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -I.. -I$(BUILDS64)/non_nacl_build_simd -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@.o
	$(CXX) $(CFLAGS64) $@.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a -o $@ -lrt -lpthread

$(BUILDS64)/scaling_benchmark_ps_spin : ../scaling_benchmark.cpp
	$(CXX) -c $(CFLAGS64) -std=c++11 -DUSE_PROCESS -DUSE_PROCESS_SPIN -I.. -I$(BUILDS64)/non_nacl_build_simd -I$(PROCESS_SANDBOX_DIR) -DUSE_LIBJPEG $< -o $@.o
	$(CXX) $(CFLAGS64) $@.o $(PROCESS_SANDBOX_DIR)/libProcessSandbox_jpeg64.a -o $@ -lrt -lpthread

$(BUILDS32)/run_dynamic_non_nacl : $(BUILDS32)/example_dynamic_non_nacl kill_old_processes
	echo && echo dynamic_non_nacl 32-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS32)/example_dynamic_non_nacl ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/non_nacl_build/.libs/libjpeg.so

$(BUILDS64)/run_dynamic_non_nacl : $(BUILDS64)/example_dynamic_non_nacl kill_old_processes
	echo && echo dynamic_non_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_non_nacl ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build/.libs/libjpeg.so

$(BUILDS32)/run_dynamic_non_nacl_simd : $(BUILDS32)/example_dynamic_non_nacl_simd kill_old_processes
	echo && echo dynamic_non_nacl_simd 32-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS32)/example_dynamic_non_nacl_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/non_nacl_build_simd/.libs/libjpeg.so

$(BUILDS64)/run_dynamic_non_nacl_simd : $(BUILDS64)/example_dynamic_non_nacl_simd kill_old_processes
	echo && echo dynamic_non_nacl_simd 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_non_nacl_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so

$(BUILDS32)/run_dynamic_nacl : $(BUILDS32)/example_dynamic_nacl kill_old_processes
	echo && echo dynamic_nacl 32-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS32)/example_dynamic_nacl ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/nacl_build/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-32/staging/irt_core.nexe

$(BUILDS64)/run_dynamic_nacl : $(BUILDS64)/example_dynamic_nacl kill_old_processes
	echo && echo dynamic_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_nacl ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/nacl_build/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe

$(BUILDS32)/run_dynamic_nacl_simd : $(BUILDS32)/example_dynamic_nacl_simd kill_old_processes
	echo && echo dynamic_nacl_simd 32-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS32)/example_dynamic_nacl_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/nacl_build_simd/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-32/staging/irt_core.nexe

$(BUILDS64)/run_dynamic_nacl_simd : $(BUILDS64)/example_dynamic_nacl_simd kill_old_processes
	echo && echo dynamic_nacl_simd 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_nacl_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe

$(BUILDS64)/run_dynamic_nacl_safe_api : $(BUILDS64)/example_dynamic_nacl_safe_api kill_old_processes
	echo && echo dynamic_nacl_safe_api 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_nacl_safe_api ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/nacl_build/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe

$(BUILDS64)/run_dynamic_nacl_simd_safe_api : $(BUILDS64)/example_dynamic_nacl_simd_safe_api kill_old_processes
	echo && echo dynamic_nacl_simd_safe_api 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_nacl_simd_safe_api ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe

$(BUILDS32)/run_dynamic_ps : $(BUILDS32)/example_dynamic_ps $(OTHERSIDE_EXEC32) kill_old_processes
	echo && echo dynamic_ps 32-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS32)/non_nacl_build/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS32)/example_dynamic_ps ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/non_nacl_build/.libs/libjpeg.so $(OTHERSIDE_EXEC32) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_dynamic_ps : $(BUILDS64)/example_dynamic_ps $(OTHERSIDE_EXEC64) kill_old_processes
	echo && echo dynamic_ps 64-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS64)/non_nacl_build/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS64)/example_dynamic_ps ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build/.libs/libjpeg.so $(OTHERSIDE_EXEC64) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS32)/run_dynamic_ps_simd : $(BUILDS32)/example_dynamic_ps_simd $(OTHERSIDE_EXEC32_SIMD) kill_old_processes
	echo && echo dynamic_ps_simd 32-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS32)/non_nacl_build_simd/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS32)/example_dynamic_ps_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS32)/non_nacl_build_simd/.libs/libjpeg.so $(OTHERSIDE_EXEC32_SIMD) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_dynamic_ps_simd : $(BUILDS64)/example_dynamic_ps_simd $(OTHERSIDE_EXEC64_SIMD) kill_old_processes
	echo && echo dynamic_ps_simd 64-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS64)/non_nacl_build_simd/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS64)/example_dynamic_ps_simd ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so $(OTHERSIDE_EXEC64_SIMD) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_dynamic_ps_safe_api : $(BUILDS64)/example_dynamic_ps_safe_api $(OTHERSIDE_EXEC64) kill_old_processes
	echo && echo dynamic_ps_safe_api 64-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS64)/non_nacl_build/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS64)/example_dynamic_ps_safe_api ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build/.libs/libjpeg.so $(OTHERSIDE_EXEC64) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_dynamic_ps_simd_safe_api : $(BUILDS64)/example_dynamic_ps_simd_safe_api $(OTHERSIDE_EXEC64_SIMD) kill_old_processes
	echo && echo dynamic_ps_simd_safe_api 64-bit: && echo && \
	LD_LIBRARY_PATH=$(BUILDS64)/non_nacl_build_simd/.libs:$(LD_LIBRARY_PATH) \
	$(GDB) ./$(BUILDS64)/example_dynamic_ps_simd_safe_api ../test.jpeg ../test_out.jpeg ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so $(OTHERSIDE_EXEC64_SIMD) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_scaling_benchmark_nacl: $(BUILDS64)/scaling_benchmark_nacl kill_old_processes
	echo && echo run_scaling_benchmark_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/scaling_benchmark_nacl ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe

$(BUILDS64)/run_scaling_benchmark_ps: $(BUILDS64)/scaling_benchmark_ps kill_old_processes
	echo && echo run_scaling_benchmark_ps 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/scaling_benchmark_ps $(OTHERSIDE_EXEC64_SIMD) $(MAINCORE) $(SANDBOXCORE) $(DEFAULTLIBPRIO)

$(BUILDS64)/run_scaling_benchmark_ps_spin: $(BUILDS64)/scaling_benchmark_ps_spin kill_old_processes
	echo && echo run_scaling_benchmark_ps_spin 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/scaling_benchmark_ps_spin $(OTHERSIDE_EXEC64_SIMD) $(MAINCORE) $(SANDBOXCORE) $(DEFAULTLIBPRIO)

$(BUILDS64)/run_scaling_benchmark_none: $(BUILDS64)/scaling_benchmark_none kill_old_processes
	echo && echo run_scaling_benchmark_none 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/scaling_benchmark_none $(OTHERSIDE_EXEC64_SIMD) $(MAINCORE) $(SANDBOXCORE)

$(BUILDS64)/run_dynamic_new_scaling_dyn_test: $(BUILDS64)/example_dynamic_new_scaling_dyn
	echo && echo run_dynamic_new_scaling_dyn 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_dyn ../test.jpeg "" ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so 1 1

$(BUILDS64)/run_dynamic_new_scaling_dyn_bench_max: $(BUILDS64)/example_dynamic_new_scaling_dyn
	echo && echo run_dynamic_new_scaling_dyn 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_dyn ../test.jpeg "" ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so 1000 -1

$(BUILDS64)/run_dynamic_new_scaling_nacl_bench_test: $(BUILDS64)/example_dynamic_new_scaling_nacl
	echo && echo run_dynamic_new_scaling_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_nacl ../test.jpeg $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe 1 1

$(BUILDS64)/run_dynamic_new_scaling_nacl_bench_max: $(BUILDS64)/example_dynamic_new_scaling_nacl
	echo && echo run_dynamic_new_scaling_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_nacl ../test.jpeg $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe 100 -1

$(BUILDS64)/run_dynamic_new_scaling_nacl_dbg_bench_max: $(BUILDS64)/example_dynamic_new_scaling_nacl_dbg
	echo && echo run_dynamic_new_scaling_nacl_dbg 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_nacl_dbg ../test.jpeg $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe 100 -1

$(BUILDS64)/run_dynamic_new_scaling_create_dyn_test: $(BUILDS64)/example_dynamic_new_scaling_create_dyn
	echo && echo run_dynamic_new_scaling_create_dyn 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_create_dyn "" ./$(BUILDS64)/non_nacl_build_simd/.libs/libjpeg.so

$(BUILDS64)/run_dynamic_new_scaling_create_nacl_bench_test: $(BUILDS64)/example_dynamic_new_scaling_create_nacl
	echo && echo run_dynamic_new_scaling_create_nacl 64-bit: && echo && \
	taskset -c $(MAINCORE) $(GDB) ./$(BUILDS64)/example_dynamic_new_scaling_create_nacl $(SANDBOXING_NACL_DIR)/native_client/scons-out/nacl_irt-x86-64/staging/irt_core.nexe ./$(BUILDS64)/nacl_build_simd/mainCombine/libjpeg.nexe

$(BUILDS32)/dbg_dynamic_non_nacl : debug $(BUILDS32)/run_dynamic_non_nacl
$(BUILDS32)/dbg_dynamic_non_nacl_simd : debug $(BUILDS32)/run_dynamic_non_nacl_simd
$(BUILDS32)/dbg_dynamic_nacl : debug $(BUILDS32)/run_dynamic_nacl
$(BUILDS32)/dbg_dynamic_nacl_simd : debug $(BUILDS32)/run_dynamic_nacl_simd
$(BUILDS32)/dbg_dynamic_ps : debug $(BUILDS32)/run_dynamic_ps
$(BUILDS32)/dbg_dynamic_ps_simd : debug $(BUILDS32)/run_dynamic_ps_simd
$(BUILDS64)/dbg_dynamic_non_nacl : debug $(BUILDS64)/run_dynamic_non_nacl
$(BUILDS64)/dbg_dynamic_non_nacl_simd : debug $(BUILDS64)/run_dynamic_non_nacl_simd
$(BUILDS64)/dbg_dynamic_nacl : debug $(BUILDS64)/run_dynamic_nacl
$(BUILDS64)/dbg_dynamic_nacl_simd : debug $(BUILDS64)/run_dynamic_nacl_simd
$(BUILDS64)/dbg_dynamic_ps : debug $(BUILDS64)/run_dynamic_ps
$(BUILDS64)/dbg_dynamic_ps_simd : debug $(BUILDS64)/run_dynamic_ps_simd

debug :
	$(eval GDB ?= gdb --args)
	$(eval CFLAGS += -g)
	$(eval CFLAGS32 += -g)
	$(eval CFLAGS64 += -g)

kill_old_processes :
	-pkill -f ProcessSandbox_otherside

clean :
	-rm -rf $(BUILDS32) $(BUILDS64) *.o
