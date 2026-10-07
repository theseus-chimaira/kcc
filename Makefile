CC ?= gcc
KCC ?= ./kcc
KCC_SELF_FLAGS ?= -P=stdc+kcc -DHOST_UNIX=1 -Iself/include/ -Hself/include/
PDP10_PREFIX ?= $(PREFIX)
PDP10_DAS ?= $(PDP10_PREFIX)/bin/das
PDP10_DLINK ?= $(PDP10_PREFIX)/bin/dlink
DAIMOS_REPO ?= ../DAIMOS
NATIVE_BUILD_DIR ?= build-native-v1
NATIVE_KCCFLAGS ?= -Pgnu99 -O -x=pdp6 -m=gas
NATIVE_CPPFLAGS ?= -DHOST_DAIMOS=1 -DHOST_UNIX=0 -Iself/include/ -Hself/include/

CFLAGS += -std=c99 -funsigned-char
LDFLAGS ?=
INSTALL ?= install
RM ?= rm -f

PREFIX ?= /usr/local
PDP10_PREFIX ?= $(PREFIX)
BINDIR ?= $(PDP10_PREFIX)/bin
KCCLIBDIR ?= $(PDP10_PREFIX)/lib/kcc
RUNTIMEDIR = runtime

SRCS = \
	cc.c ccasmb.c cccreg.c cccse.c cccode.c ccdata.c ccdbug.c ccdecl.c \
	ccerr.c cceval.c ccgen.c ccgen1.c ccgen2.c ccgswi.c ccjskp.c cclex.c \
	ccnode.c ccout.c ccoututil.c ccpp.c ccsrc.c ccreg.c ccstmt.c ccsym.c cctype.c ccopt.c \
	ccvla.c
OBJS = $(SRCS:.c=.o)
ASMS = $(SRCS:.c=.s)
NATIVE_ASMS = $(SRCS:%.c=$(NATIVE_BUILD_DIR)/%-v1.s)
NATIVE_OBJS = $(SRCS:%.c=$(NATIVE_BUILD_DIR)/%-v1.dobj)
NATIVE_CPP_PHASE_ASM = $(NATIVE_BUILD_DIR)/ccppout-v1.s
NATIVE_CPP_PHASE_OBJ = $(NATIVE_BUILD_DIR)/ccppout-v1.dobj
NATIVE_CORE_PHASE_ASM = $(NATIVE_BUILD_DIR)/ccppin-v1.s
NATIVE_CORE_PHASE_OBJ = $(NATIVE_BUILD_DIR)/ccppin-v1.dobj
NATIVE_CPP_DRIVER_ASM = $(NATIVE_BUILD_DIR)/cc-cpp-v1.s
NATIVE_CPP_DRIVER_OBJ = $(NATIVE_BUILD_DIR)/cc-cpp-v1.dobj
NATIVE_CPP_OUT_ASM = $(NATIVE_BUILD_DIR)/ccout-cpp-v1.s
NATIVE_CPP_OUT_OBJ = $(NATIVE_BUILD_DIR)/ccout-cpp-v1.dobj
NATIVE_CPP_ERR_ASM = $(NATIVE_BUILD_DIR)/ccerr-cpp-v1.s
NATIVE_CPP_ERR_OBJ = $(NATIVE_BUILD_DIR)/ccerr-cpp-v1.dobj
NATIVE_CPP_SYM_ASM = $(NATIVE_BUILD_DIR)/ccsym-cpp-v1.s
NATIVE_CPP_SYM_OBJ = $(NATIVE_BUILD_DIR)/ccsym-cpp-v1.dobj
NATIVE_CPP_DATA_ASM = $(NATIVE_BUILD_DIR)/ccdata-cpp-v1.s
NATIVE_CPP_DATA_OBJ = $(NATIVE_BUILD_DIR)/ccdata-cpp-v1.dobj
NATIVE_CORE_DATA_ASM = $(NATIVE_BUILD_DIR)/ccdata-core-v1.s
NATIVE_CORE_DATA_OBJ = $(NATIVE_BUILD_DIR)/ccdata-core-v1.dobj
NATIVE_CORE_ERR_ASM = $(NATIVE_BUILD_DIR)/ccerr-core-v1.s
NATIVE_CORE_ERR_OBJ = $(NATIVE_BUILD_DIR)/ccerr-core-v1.dobj
NATIVE_CORE_OUT_ASM = $(NATIVE_BUILD_DIR)/ccout-core-v1.s
NATIVE_CORE_OUT_OBJ = $(NATIVE_BUILD_DIR)/ccout-core-v1.dobj
NATIVE_CORE_GEN_ASM = $(NATIVE_BUILD_DIR)/ccgen-core-v1.s
NATIVE_CORE_GEN_OBJ = $(NATIVE_BUILD_DIR)/ccgen-core-v1.dobj
NATIVE_CORE_DRIVER_ASM = $(NATIVE_BUILD_DIR)/cc-core-v1.s
NATIVE_CORE_DRIVER_OBJ = $(NATIVE_BUILD_DIR)/cc-core-v1.dobj
NATIVE_GEN_DRIVER_ASM = $(NATIVE_BUILD_DIR)/cckgen-gen-v1.s
NATIVE_GEN_DRIVER_OBJ = $(NATIVE_BUILD_DIR)/cckgen-gen-v1.dobj
NATIVE_GEN_DATA_ASM = $(NATIVE_BUILD_DIR)/ccdata-gen-v1.s
NATIVE_GEN_DATA_OBJ = $(NATIVE_BUILD_DIR)/ccdata-gen-v1.dobj
NATIVE_GEN_ERR_ASM = $(NATIVE_BUILD_DIR)/ccerr-gen-v1.s
NATIVE_GEN_ERR_OBJ = $(NATIVE_BUILD_DIR)/ccerr-gen-v1.dobj
NATIVE_GEN_EVAL_ASM = $(NATIVE_BUILD_DIR)/ccevalgen-v1.s
NATIVE_GEN_EVAL_OBJ = $(NATIVE_BUILD_DIR)/ccevalgen-v1.dobj
NATIVE_GEN_TYPE_ASM = $(NATIVE_BUILD_DIR)/cctype-gen-v1.s
NATIVE_GEN_TYPE_OBJ = $(NATIVE_BUILD_DIR)/cctype-gen-v1.dobj
NATIVE_GEN_STMT_ASM = $(NATIVE_BUILD_DIR)/ccgen1-gen-v1.s
NATIVE_GEN_STMT_OBJ = $(NATIVE_BUILD_DIR)/ccgen1-gen-v1.dobj
NATIVE_GEN_CODE_ASM = $(NATIVE_BUILD_DIR)/ccgen-gen-v1.s
NATIVE_GEN_CODE_OBJ = $(NATIVE_BUILD_DIR)/ccgen-gen-v1.dobj
NATIVE_GEN_KPIN_ASM = $(NATIVE_BUILD_DIR)/ccppin-gen-v1.s
NATIVE_GEN_KPIN_OBJ = $(NATIVE_BUILD_DIR)/ccppin-gen-v1.dobj
NATIVE_GEN_KPOUT_ASM = $(NATIVE_BUILD_DIR)/cckpout-gen-v1.s
NATIVE_GEN_KPOUT_OBJ = $(NATIVE_BUILD_DIR)/cckpout-gen-v1.dobj
NATIVE_GEN_KPWRITE_ASM = $(NATIVE_BUILD_DIR)/cckpwrite-gen-v1.s
NATIVE_GEN_KPWRITE_OBJ = $(NATIVE_BUILD_DIR)/cckpwrite-gen-v1.dobj
NATIVE_PARSE_DRIVER_ASM = $(NATIVE_BUILD_DIR)/cc-parse-v1.s
NATIVE_PARSE_DRIVER_OBJ = $(NATIVE_BUILD_DIR)/cc-parse-v1.dobj
NATIVE_PARSE_ERR_ASM = $(NATIVE_BUILD_DIR)/ccerr-parse-v1.s
NATIVE_PARSE_ERR_OBJ = $(NATIVE_BUILD_DIR)/ccerr-parse-v1.dobj
NATIVE_PARSE_DATA_ASM = $(NATIVE_BUILD_DIR)/ccdata-parse-v1.s
NATIVE_PARSE_DATA_OBJ = $(NATIVE_BUILD_DIR)/ccdata-parse-v1.dobj
NATIVE_PARSE_BIND_ASM = $(NATIVE_BUILD_DIR)/ccbind-parse-v1.s
NATIVE_PARSE_BIND_OBJ = $(NATIVE_BUILD_DIR)/ccbind-parse-v1.dobj
NATIVE_OPT_DRIVER_ASM = $(NATIVE_BUILD_DIR)/cckopt-opt-v1.s
NATIVE_OPT_DRIVER_OBJ = $(NATIVE_BUILD_DIR)/cckopt-opt-v1.dobj
NATIVE_OPT_DATA_ASM = $(NATIVE_BUILD_DIR)/ccdata-opt-v1.s
NATIVE_OPT_DATA_OBJ = $(NATIVE_BUILD_DIR)/ccdata-opt-v1.dobj
NATIVE_OPT_OUT_ASM = $(NATIVE_BUILD_DIR)/ccout-opt-v1.s
NATIVE_OPT_OUT_OBJ = $(NATIVE_BUILD_DIR)/ccout-opt-v1.dobj
NATIVE_OPT_ERR_ASM = $(NATIVE_BUILD_DIR)/ccerr-opt-v1.s
NATIVE_OPT_ERR_OBJ = $(NATIVE_BUILD_DIR)/ccerr-opt-v1.dobj
NATIVE_OPT_KPREAD_ASM = $(NATIVE_BUILD_DIR)/cckpread-opt-v1.s
NATIVE_OPT_KPREAD_OBJ = $(NATIVE_BUILD_DIR)/cckpread-opt-v1.dobj
NATIVE_RUNTIME_DIR = $(NATIVE_BUILD_DIR)/runtime
NATIVE_DAIMOS_LIBC_ROOT = $(abspath $(NATIVE_BUILD_DIR)/daimos-libc)
NATIVE_DAIMOS_LIBC_DIR = $(NATIVE_DAIMOS_LIBC_ROOT)/libc
NATIVE_DAIMOS_LIBC = $(NATIVE_DAIMOS_LIBC_DIR)/libc.a
NATIVE_DAIMOS_SYSCALL_OBJ = $(NATIVE_DAIMOS_LIBC_DIR)/syscall.dobj
NATIVE_BOOTSTRAP_ASM = $(NATIVE_RUNTIME_DIR)/daimos-bootstrap-v1.s
NATIVE_BOOTSTRAP_OBJ = $(NATIVE_RUNTIME_DIR)/daimos-bootstrap-v1.dobj
NATIVE_CRT0_OBJ = $(NATIVE_RUNTIME_DIR)/crt0-v1.dobj
NATIVE_SYSCALL_HELPERS_OBJ = $(NATIVE_RUNTIME_DIR)/syscall-helpers-v1.dobj
NATIVE_LINK_RUNTIME = $(NATIVE_CRT0_OBJ) $(NATIVE_BOOTSTRAP_OBJ) \
	$(NATIVE_DAIMOS_SYSCALL_OBJ) $(NATIVE_SYSCALL_HELPERS_OBJ)
NATIVE_KCPP_DXR = $(NATIVE_BUILD_DIR)/KCPP.dxr
NATIVE_KPARSE_DXR = $(NATIVE_BUILD_DIR)/KPARSE.dxr
NATIVE_KGEN_DXR = $(NATIVE_BUILD_DIR)/KGEN.dxr
NATIVE_KOPT_DXR = $(NATIVE_BUILD_DIR)/KOPT.dxr
NATIVE_DRIVER_ASM = $(NATIVE_RUNTIME_DIR)/daimos-driver-v1.s
NATIVE_DRIVER_OBJ = $(NATIVE_RUNTIME_DIR)/daimos-driver-v1.dobj
NATIVE_DRIVER_DXR = $(NATIVE_BUILD_DIR)/KCC.dxr
NATIVE_PHASE_DXRS = $(NATIVE_KCPP_DXR) $(NATIVE_KPARSE_DXR) \
	$(NATIVE_KGEN_DXR) $(NATIVE_KOPT_DXR)
NATIVE_HEADERS = $(wildcard *.h) $(wildcard self/include/*.h)
NATIVE_PHASE_ASMS = \
	$(NATIVE_CPP_PHASE_ASM) $(NATIVE_CORE_PHASE_ASM) \
	$(NATIVE_CPP_DRIVER_ASM) $(NATIVE_CPP_OUT_ASM) $(NATIVE_CPP_ERR_ASM) \
	$(NATIVE_CPP_SYM_ASM) $(NATIVE_CPP_DATA_ASM) \
	$(NATIVE_CORE_DATA_ASM) $(NATIVE_CORE_ERR_ASM) \
	$(NATIVE_CORE_OUT_ASM) $(NATIVE_CORE_GEN_ASM) $(NATIVE_CORE_DRIVER_ASM) \
	$(NATIVE_GEN_DRIVER_ASM) $(NATIVE_GEN_DATA_ASM) $(NATIVE_GEN_ERR_ASM) \
	$(NATIVE_GEN_EVAL_ASM) $(NATIVE_GEN_TYPE_ASM) $(NATIVE_GEN_STMT_ASM) \
	$(NATIVE_GEN_CODE_ASM) $(NATIVE_GEN_KPIN_ASM) $(NATIVE_GEN_KPOUT_ASM) \
	$(NATIVE_GEN_KPWRITE_ASM) $(NATIVE_PARSE_DRIVER_ASM) $(NATIVE_PARSE_ERR_ASM) \
	$(NATIVE_PARSE_DATA_ASM) $(NATIVE_PARSE_BIND_ASM) \
	$(NATIVE_OPT_DRIVER_ASM) $(NATIVE_OPT_DATA_ASM) \
	$(NATIVE_OPT_OUT_ASM) $(NATIVE_OPT_ERR_ASM) $(NATIVE_OPT_KPREAD_ASM)

NATIVE_KCPP_OBJS = \
	$(NATIVE_CPP_DRIVER_OBJ) $(NATIVE_BUILD_DIR)/ccasmb-v1.dobj \
	$(NATIVE_CPP_DATA_OBJ) $(NATIVE_CPP_ERR_OBJ) $(NATIVE_CPP_OUT_OBJ) \
	$(NATIVE_BUILD_DIR)/ccpp-v1.dobj $(NATIVE_CPP_PHASE_OBJ) \
	$(NATIVE_CPP_SYM_OBJ) $(NATIVE_BUILD_DIR)/ccsrc-v1.dobj

NATIVE_KCC1_OBJS = \
	$(NATIVE_CORE_DRIVER_OBJ) $(NATIVE_BUILD_DIR)/ccasmb-v1.dobj \
	$(NATIVE_BUILD_DIR)/cccreg-v1.dobj $(NATIVE_BUILD_DIR)/cccse-v1.dobj \
	$(NATIVE_BUILD_DIR)/cccode-v1.dobj $(NATIVE_CORE_DATA_OBJ) \
	$(NATIVE_BUILD_DIR)/ccdbug-v1.dobj $(NATIVE_BUILD_DIR)/ccdecl-v1.dobj \
	$(NATIVE_CORE_ERR_OBJ) $(NATIVE_BUILD_DIR)/cceval-v1.dobj \
	$(NATIVE_CORE_GEN_OBJ) $(NATIVE_BUILD_DIR)/ccgen1-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccgen2-v1.dobj $(NATIVE_BUILD_DIR)/ccgswi-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccjskp-v1.dobj $(NATIVE_BUILD_DIR)/cclex-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccnode-v1.dobj $(NATIVE_CORE_OUT_OBJ) \
	$(NATIVE_BUILD_DIR)/ccreg-v1.dobj $(NATIVE_BUILD_DIR)/ccstmt-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccsym-v1.dobj $(NATIVE_BUILD_DIR)/cctype-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccopt-v1.dobj $(NATIVE_BUILD_DIR)/ccoututil-v1.dobj \
	$(NATIVE_CORE_PHASE_OBJ) $(NATIVE_BUILD_DIR)/ccvla-v1.dobj

NATIVE_KPARSE_OBJS = \
	$(NATIVE_PARSE_DRIVER_OBJ) $(NATIVE_BUILD_DIR)/ccasmb-v1.dobj \
	$(NATIVE_PARSE_DATA_OBJ) $(NATIVE_PARSE_BIND_OBJ) $(NATIVE_BUILD_DIR)/ccdbug-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccdecl-v1.dobj $(NATIVE_PARSE_ERR_OBJ) \
	$(NATIVE_BUILD_DIR)/cceval-v1.dobj $(NATIVE_BUILD_DIR)/cclex-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccnode-v1.dobj $(NATIVE_CORE_PHASE_OBJ) \
	$(NATIVE_BUILD_DIR)/ccstmt-v1.dobj $(NATIVE_BUILD_DIR)/ccsym-v1.dobj \
	$(NATIVE_BUILD_DIR)/cctype-v1.dobj $(NATIVE_BUILD_DIR)/ccoututil-v1.dobj \
	$(NATIVE_BUILD_DIR)/cckirwrite-v1.dobj $(NATIVE_BUILD_DIR)/ccvla-v1.dobj

NATIVE_KGEN_OBJS = \
	$(NATIVE_GEN_DRIVER_OBJ) \
	$(NATIVE_BUILD_DIR)/cccreg-v1.dobj $(NATIVE_BUILD_DIR)/cccse-v1.dobj \
	$(NATIVE_BUILD_DIR)/cccode-v1.dobj $(NATIVE_GEN_DATA_OBJ) \
	$(NATIVE_BUILD_DIR)/ccdbug-v1.dobj $(NATIVE_GEN_ERR_OBJ) \
	$(NATIVE_GEN_EVAL_OBJ) \
	$(NATIVE_GEN_CODE_OBJ) $(NATIVE_GEN_STMT_OBJ) \
	$(NATIVE_BUILD_DIR)/ccgen2-v1.dobj $(NATIVE_BUILD_DIR)/ccgswi-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccjskp-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccnode-v1.dobj $(NATIVE_BUILD_DIR)/ccreg-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccsym-v1.dobj \
	$(NATIVE_GEN_TYPE_OBJ) $(NATIVE_BUILD_DIR)/ccopt-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccoututil-v1.dobj $(NATIVE_GEN_KPOUT_OBJ) \
	$(NATIVE_GEN_KPWRITE_OBJ) $(NATIVE_BUILD_DIR)/cckirread-v1.dobj \
	$(NATIVE_BUILD_DIR)/ccvla-v1.dobj

NATIVE_KOPT_OBJS = \
	$(NATIVE_OPT_DRIVER_OBJ) $(NATIVE_OPT_KPREAD_OBJ) $(NATIVE_OPT_OUT_OBJ) \
	$(NATIVE_BUILD_DIR)/ccoututil-v1.dobj $(NATIVE_OPT_DATA_OBJ) \
	$(NATIVE_OPT_ERR_OBJ) $(NATIVE_BUILD_DIR)/ccasmb-v1.dobj

RUNTIME = \
	$(RUNTIMEDIR)/pdp6rt-adjbp.s $(RUNTIMEDIR)/pdp6rt-kdfad.s \
	$(RUNTIMEDIR)/pdp6rt-kdfsb.s $(RUNTIMEDIR)/pdp6rt-kdfmp.s \
	$(RUNTIMEDIR)/pdp6rt-kdfdv.s $(RUNTIMEDIR)/ka10rt-adjbp.s \
	$(RUNTIMEDIR)/ka10rt-kdfad.s $(RUNTIMEDIR)/ka10rt-kdfsb.s \
	$(RUNTIMEDIR)/ka10rt-kdfmp.s $(RUNTIMEDIR)/ka10rt-kdfdv.s \
	$(RUNTIMEDIR)/ks10rt-adjbp.s $(RUNTIMEDIR)/ks10rt-kdfad.s \
	$(RUNTIMEDIR)/ks10rt-kdfsb.s $(RUNTIMEDIR)/ks10rt-kdfmp.s \
	$(RUNTIMEDIR)/ks10rt-kdfdv.s $(RUNTIMEDIR)/kccrt-zero.s \
	$(RUNTIMEDIR)/kccrt-dimode-div.s

all: kcc runtime

.DELETE_ON_ERROR:

kcc: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

ccgen.o ccgen1.o ccgen2.o: cc.h ccgen.h

asm self-asm: $(ASMS)

native-asm: $(NATIVE_ASMS)

native-objects: $(NATIVE_OBJS)

native-kcpp-objects: $(NATIVE_KCPP_OBJS)

native-kcc1-objects: $(NATIVE_KCC1_OBJS)

native-kparse-objects: $(NATIVE_KPARSE_OBJS)

native-kgen-objects: $(NATIVE_KGEN_OBJS)

native-kopt-objects: $(NATIVE_KOPT_OBJS)

native-phase-objects: native-kcpp-objects native-kcc1-objects native-kparse-objects native-kgen-objects native-kopt-objects

native-phase-dxrs: $(NATIVE_PHASE_DXRS)

native-driver: $(NATIVE_DRIVER_DXR)

$(NATIVE_BUILD_DIR):
	mkdir -p $@

$(NATIVE_RUNTIME_DIR):
	mkdir -p $@

$(NATIVE_DAIMOS_LIBC): $(KCC)
	$(MAKE) -C $(DAIMOS_REPO)/userland/libc build \
		PDP10_PREFIX='$(PDP10_PREFIX)' BUILD_ROOT='$(NATIVE_DAIMOS_LIBC_ROOT)' \
		CC='$(abspath $(KCC))'

$(NATIVE_DAIMOS_SYSCALL_OBJ): $(NATIVE_DAIMOS_LIBC)
	@test -f '$@'

$(NATIVE_BOOTSTRAP_ASM): runtime/daimos-bootstrap.c $(KCC) | $(NATIVE_RUNTIME_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) \
		-I$(DAIMOS_REPO)/userland/libc \
		-I$(DAIMOS_REPO)/system/kernel/boot -I$(DAIMOS_REPO)/system/kernel/core \
		-I$(DAIMOS_REPO)/system/kernel/drivers -I$(DAIMOS_REPO)/system/kernel/fs \
		-I$(DAIMOS_REPO)/system/kernel/mm -I$(DAIMOS_REPO)/system/kernel/modules \
		-I$(DAIMOS_REPO)/system/kernel/proc -I$(DAIMOS_REPO)/system/kernel/storage \
		-I$(PDP10_PREFIX)/include -S $< -o $@

$(NATIVE_BOOTSTRAP_OBJ): $(NATIVE_BOOTSTRAP_ASM)
	$(PDP10_DAS) -F -C -O $@ $<

$(NATIVE_DRIVER_ASM): runtime/daimos-driver.c $(KCC) | $(NATIVE_RUNTIME_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) \
		-I$(DAIMOS_REPO)/userland/libc \
		-I$(DAIMOS_REPO)/system/kernel/boot -I$(DAIMOS_REPO)/system/kernel/core \
		-I$(DAIMOS_REPO)/system/kernel/drivers -I$(DAIMOS_REPO)/system/kernel/fs \
		-I$(DAIMOS_REPO)/system/kernel/mm -I$(DAIMOS_REPO)/system/kernel/modules \
		-I$(DAIMOS_REPO)/system/kernel/proc -I$(DAIMOS_REPO)/system/kernel/storage \
		-I$(PDP10_PREFIX)/include -S $< -o $@

$(NATIVE_DRIVER_OBJ): $(NATIVE_DRIVER_ASM)
	$(PDP10_DAS) -F -C -O $@ $<

$(NATIVE_DRIVER_DXR): $(NATIVE_DRIVER_OBJ) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(@:.dxr=.map) \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_DRIVER_OBJ) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_CRT0_OBJ): runtime/daimos-crt0.s | $(NATIVE_RUNTIME_DIR)
	$(PDP10_DAS) -F -C -O $@ $<

$(NATIVE_SYSCALL_HELPERS_OBJ): $(DAIMOS_REPO)/userland/libc/syscall_helpers.s | $(NATIVE_RUNTIME_DIR)
	$(PDP10_DAS) -F -C -O $@ $<

define NATIVE_LINK_PHASE
$(1): $(2) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $$@ -M $$(@:.dxr=.map) \
		$(NATIVE_LINK_RUNTIME) $(2) $(NATIVE_DAIMOS_LIBC)
endef

$(eval $(call NATIVE_LINK_PHASE,$(NATIVE_KCPP_DXR),$(NATIVE_KCPP_OBJS)))
$(eval $(call NATIVE_LINK_PHASE,$(NATIVE_KPARSE_DXR),$(NATIVE_KPARSE_OBJS)))
$(eval $(call NATIVE_LINK_PHASE,$(NATIVE_KGEN_DXR),$(NATIVE_KGEN_OBJS)))
$(eval $(call NATIVE_LINK_PHASE,$(NATIVE_KOPT_DXR),$(NATIVE_KOPT_OBJS)))

$(NATIVE_ASMS) $(NATIVE_PHASE_ASMS): $(NATIVE_HEADERS)

$(NATIVE_BUILD_DIR)/%-v1.s: %.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -S $< -o $@

$(NATIVE_BUILD_DIR)/%-v1.dobj: $(NATIVE_BUILD_DIR)/%-v1.s
	$(PDP10_DAS) -F -C -O $@ $<

$(NATIVE_BUILD_DIR)/ccppout-v1.s: ccppout.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccppin-v1.s: ccppin.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cc-cpp-v1.s: cc.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccout-cpp-v1.s: ccout.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccerr-cpp-v1.s: ccerr.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccsym-cpp-v1.s: ccsym.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccdata-cpp-v1.s: ccdata.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CPP=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccdata-core-v1.s: ccdata.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccerr-core-v1.s: ccerr.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccout-core-v1.s: ccout.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccgen-core-v1.s: ccgen.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cc-core-v1.s: cc.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_CORE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cckgen-gen-v1.s: cckgen.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccdata-gen-v1.s: ccdata.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccerr-gen-v1.s: ccerr.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccevalgen-v1.s: ccevalgen.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cctype-gen-v1.s: cctype.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccgen1-gen-v1.s: ccgen1.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccgen-gen-v1.s: ccgen.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccppin-gen-v1.s: ccppin.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cckpout-gen-v1.s: cckpout.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cckpwrite-gen-v1.s: cckpwrite.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_GEN=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cc-parse-v1.s: cc.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_PARSE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccerr-parse-v1.s: ccerr.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_PARSE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccdata-parse-v1.s: ccdata.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_PARSE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccbind-parse-v1.s: ccbind.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_PARSE=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cckopt-opt-v1.s: cckopt.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_OPT=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccdata-opt-v1.s: ccdata.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_OPT=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccout-opt-v1.s: ccout.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_OPT=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/ccerr-opt-v1.s: ccerr.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_OPT=1 -S $< -o $@

$(NATIVE_BUILD_DIR)/cckpread-opt-v1.s: cckpread.c $(KCC) | $(NATIVE_BUILD_DIR)
	$(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS) -DKCC_PHASE_OPT=1 -S $< -o $@

runtime: $(RUNTIME)

install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 kcc $(DESTDIR)$(BINDIR)/kcc
	cmp kcc $(DESTDIR)$(BINDIR)/kcc
	$(MAKE) install-runtime

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(DESTDIR)$(KCCLIBDIR)/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	@for f in $(RUNTIME); do \
		$(RM) "$(DESTDIR)$(KCCLIBDIR)/$${f##*/}"; \
	done

.SUFFIXES: .c .s
.c.s:
	$(KCC) $(KCC_SELF_FLAGS) -S $<

clean:
	$(RM) $(ASMS) $(OBJS) kcc
	$(RM) -r $(NATIVE_BUILD_DIR)

.PHONY: all asm self-asm native-asm native-objects native-kcpp-objects \
	native-kcc1-objects native-kparse-objects native-kgen-objects native-kopt-objects native-phase-objects native-phase-dxrs native-driver runtime install install-runtime \
	uninstall clean
