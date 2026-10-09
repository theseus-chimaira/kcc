# Build layout and external source trees.
# Override DAIMOS_REPO to use a different checkout; never assume KCC and
# DAIMOS are sibling directories. The default matches the project layout.
KCC_ROOT != pwd
DAIMOS_REPO ?= $(HOME)/git/DAIMOS
HOST_BUILD_DIR ?= build
NATIVE_BUILD_DIR ?= build-native
RUNTIMEDIR = runtime

# Host compiler and installation locations.
CC ?= cc
CFLAGS += -std=c99 -funsigned-char
LDFLAGS ?=
INSTALL ?= install
RM ?= rm -f
PREFIX ?= /usr/local
PDP10_PREFIX ?= $(HOME)/git/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
KCCLIBDIR ?= $(LIBDIR)/kcc
KCC ?= $(KCC_ROOT)/$(HOST_BUILD_DIR)/kcc
MAKEDEPEND ?= makedepend
KCC_SELF_FLAGS ?= -P=stdc+kcc -DHOST_UNIX=1 -Iself/include/ -Hself/include/

# PDP-10 assembler/linker and native compilation personality.
PDP10_DAS ?= $(PDP10_PREFIX)/bin/das
PDP10_DLINK ?= $(PDP10_PREFIX)/bin/dlink
NATIVE_KCCFLAGS ?= -Pgnu99 -O -x=pdp6 -m=gas
NATIVE_CPPFLAGS ?= -DHOST_DAIMOS=1 -DHOST_UNIX=0 -Iself/include/ -Hself/include/
COMPILE_NATIVE = $(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS)
ASSEMBLE_NATIVE = $(PDP10_DAS) -F -C -O $@ $<

# DAIMOS libc include paths and inputs used to detect native ABI changes.
DAIMOS_CPP_INCLUDES = -I$(DAIMOS_REPO)/userland/libc \
	-I$(DAIMOS_REPO)/system/kernel/boot -I$(DAIMOS_REPO)/system/kernel/core \
	-I$(DAIMOS_REPO)/system/kernel/drivers -I$(DAIMOS_REPO)/system/kernel/fs \
	-I$(DAIMOS_REPO)/system/kernel/mm -I$(DAIMOS_REPO)/system/kernel/modules \
	-I$(DAIMOS_REPO)/system/kernel/proc -I$(DAIMOS_REPO)/system/kernel/storage \
	-I$(PDP10_PREFIX)/include
DAIMOS_LIBC_SRCS = $(DAIMOS_REPO)/userland/libc/Makefile \
	$(DAIMOS_REPO)/userland/libc/crt0.s $(DAIMOS_REPO)/userland/libc/logevent.c \
	$(DAIMOS_REPO)/userland/libc/memcpy.s $(DAIMOS_REPO)/userland/libc/memmove.s \
	$(DAIMOS_REPO)/userland/libc/process_ctype.c $(DAIMOS_REPO)/userland/libc/stat_time.c \
	$(DAIMOS_REPO)/userland/libc/stdlib.c $(DAIMOS_REPO)/userland/libc/string.c \
	$(DAIMOS_REPO)/userland/libc/syscall.s $(DAIMOS_REPO)/userland/libc/syscall_helpers.s \
	$(DAIMOS_REPO)/userland/libc/text.c $(DAIMOS_REPO)/userland/libc/u.c

# Discard built-in suffix rules; retain only PDP-10 assembly inference.
.SUFFIXES:
.SUFFIXES: .s .dobj
.s.dobj:
	$(ASSEMBLE_NATIVE)

# A prefixed, source-suffixed list permits portable GNU/BSD substitutions.
HOST_BUILD_SRCS = \
	$(HOST_BUILD_DIR)/cc.c $(HOST_BUILD_DIR)/ccasmb.c $(HOST_BUILD_DIR)/cccreg.c $(HOST_BUILD_DIR)/cccse.c $(HOST_BUILD_DIR)/cccode.c $(HOST_BUILD_DIR)/ccdata.c \
	$(HOST_BUILD_DIR)/ccdbug.c $(HOST_BUILD_DIR)/ccdecl.c $(HOST_BUILD_DIR)/ccerr.c $(HOST_BUILD_DIR)/cceval.c $(HOST_BUILD_DIR)/ccgen.c $(HOST_BUILD_DIR)/ccgen1.c \
	$(HOST_BUILD_DIR)/ccgen2.c $(HOST_BUILD_DIR)/ccgswi.c $(HOST_BUILD_DIR)/ccjskp.c $(HOST_BUILD_DIR)/cclex.c $(HOST_BUILD_DIR)/ccnode.c $(HOST_BUILD_DIR)/ccout.c \
	$(HOST_BUILD_DIR)/ccoututil.c $(HOST_BUILD_DIR)/ccpp.c $(HOST_BUILD_DIR)/ccsrc.c $(HOST_BUILD_DIR)/ccreg.c $(HOST_BUILD_DIR)/ccstmt.c $(HOST_BUILD_DIR)/ccsym.c \
	$(HOST_BUILD_DIR)/cctype.c $(HOST_BUILD_DIR)/ccopt.c $(HOST_BUILD_DIR)/ccvla.c
OBJS = $(HOST_BUILD_SRCS:.c=.o)
ASMS = $(HOST_BUILD_SRCS:.c=.s)

NATIVE_BUILD_SRCS = \
	$(NATIVE_BUILD_DIR)/cc.c $(NATIVE_BUILD_DIR)/ccasmb.c $(NATIVE_BUILD_DIR)/cccreg.c $(NATIVE_BUILD_DIR)/cccse.c $(NATIVE_BUILD_DIR)/cccode.c $(NATIVE_BUILD_DIR)/ccdata.c \
	$(NATIVE_BUILD_DIR)/ccdbug.c $(NATIVE_BUILD_DIR)/ccdecl.c $(NATIVE_BUILD_DIR)/ccerr.c $(NATIVE_BUILD_DIR)/cceval.c $(NATIVE_BUILD_DIR)/ccgen.c $(NATIVE_BUILD_DIR)/ccgen1.c \
	$(NATIVE_BUILD_DIR)/ccgen2.c $(NATIVE_BUILD_DIR)/ccgswi.c $(NATIVE_BUILD_DIR)/ccjskp.c $(NATIVE_BUILD_DIR)/cclex.c $(NATIVE_BUILD_DIR)/ccnode.c $(NATIVE_BUILD_DIR)/ccout.c \
	$(NATIVE_BUILD_DIR)/ccoututil.c $(NATIVE_BUILD_DIR)/ccpp.c $(NATIVE_BUILD_DIR)/ccsrc.c $(NATIVE_BUILD_DIR)/ccreg.c $(NATIVE_BUILD_DIR)/ccstmt.c $(NATIVE_BUILD_DIR)/ccsym.c \
	$(NATIVE_BUILD_DIR)/cctype.c $(NATIVE_BUILD_DIR)/ccopt.c $(NATIVE_BUILD_DIR)/ccvla.c
NATIVE_ASMS = $(NATIVE_BUILD_SRCS:.c=.s)
NATIVE_OBJS = $(NATIVE_BUILD_SRCS:.c=.dobj)
NATIVE_RUNTIME_DIR = $(NATIVE_BUILD_DIR)/runtime
NATIVE_DAIMOS_LIBC_ROOT = $(KCC_ROOT)/$(NATIVE_BUILD_DIR)/daimos-libc
NATIVE_DAIMOS_LIBC_DIR = $(NATIVE_DAIMOS_LIBC_ROOT)/libc
NATIVE_DAIMOS_LIBC = $(NATIVE_DAIMOS_LIBC_DIR)/libc.a
NATIVE_DAIMOS_SYSCALL_OBJ = $(NATIVE_DAIMOS_LIBC_DIR)/syscall.dobj
NATIVE_LINK_RUNTIME = $(NATIVE_RUNTIME_DIR)/crt0.dobj $(NATIVE_RUNTIME_DIR)/daimos-bootstrap.dobj \
	$(NATIVE_DAIMOS_SYSCALL_OBJ) $(NATIVE_RUNTIME_DIR)/syscall-helpers.dobj
NATIVE_KCPP_DXR = $(NATIVE_BUILD_DIR)/KCPP.dxr
NATIVE_KPARSE_DXR = $(NATIVE_BUILD_DIR)/KPARSE.dxr
NATIVE_KGEN_DXR = $(NATIVE_BUILD_DIR)/KGEN.dxr
NATIVE_KOPT_DXR = $(NATIVE_BUILD_DIR)/KOPT.dxr
NATIVE_DRIVER_DXR = $(NATIVE_BUILD_DIR)/KCC.dxr
NATIVE_PHASE_DXRS = $(NATIVE_KCPP_DXR) $(NATIVE_KPARSE_DXR) \
	$(NATIVE_KGEN_DXR) $(NATIVE_KOPT_DXR)
NATIVE_PHASE_ASMS = $(NATIVE_CPP_ASMS) $(NATIVE_CORE_ASMS) $(NATIVE_GEN_ASMS) \
	$(NATIVE_PARSE_ASMS) $(NATIVE_OPT_ASMS)

# Native phase link membership: each list names logical .c variants, some of
# which are not physical source files (for example ccdata-gen.c). Compile
# rules map variants to their real sources and select the phase defines.
# Portable GNU/BSD suffix substitution derives .dobj paths; runtime objects
# retain their separate directory. Preserve link order.
NATIVE_KCPP_MODULES = \
	$(NATIVE_BUILD_DIR)/cc-cpp.c $(NATIVE_BUILD_DIR)/ccasmb.c $(NATIVE_BUILD_DIR)/ccdata-cpp.c $(NATIVE_BUILD_DIR)/ccerr-cpp.c $(NATIVE_BUILD_DIR)/ccout-cpp.c $(NATIVE_BUILD_DIR)/ccpp.c \
	$(NATIVE_BUILD_DIR)/ccppout.c $(NATIVE_BUILD_DIR)/ccsym-cpp.c $(NATIVE_BUILD_DIR)/ccsrc.c
NATIVE_KCPP_OBJS = $(NATIVE_KCPP_MODULES:.c=.dobj)

# Historical standalone CORE object target (not linked by make native).
NATIVE_KCC1_MODULES = \
	$(NATIVE_BUILD_DIR)/cc-core.c $(NATIVE_BUILD_DIR)/ccasmb.c $(NATIVE_BUILD_DIR)/cccreg.c $(NATIVE_BUILD_DIR)/cccse.c $(NATIVE_BUILD_DIR)/cccode.c $(NATIVE_BUILD_DIR)/ccdata-core.c \
	$(NATIVE_BUILD_DIR)/ccdbug.c $(NATIVE_BUILD_DIR)/ccdecl.c $(NATIVE_BUILD_DIR)/ccerr-core.c $(NATIVE_BUILD_DIR)/cceval.c $(NATIVE_BUILD_DIR)/ccgen-core.c $(NATIVE_BUILD_DIR)/ccgen1.c \
	$(NATIVE_BUILD_DIR)/ccgen2.c $(NATIVE_BUILD_DIR)/ccgswi.c $(NATIVE_BUILD_DIR)/ccjskp.c $(NATIVE_BUILD_DIR)/cclex.c $(NATIVE_BUILD_DIR)/ccnode.c $(NATIVE_BUILD_DIR)/ccout-core.c \
	$(NATIVE_BUILD_DIR)/ccreg.c $(NATIVE_BUILD_DIR)/ccstmt.c $(NATIVE_BUILD_DIR)/ccsym.c $(NATIVE_BUILD_DIR)/cctype.c $(NATIVE_BUILD_DIR)/ccopt.c $(NATIVE_BUILD_DIR)/ccoututil.c \
	$(NATIVE_BUILD_DIR)/ccppin.c $(NATIVE_BUILD_DIR)/ccvla.c
NATIVE_KCC1_OBJS = $(NATIVE_KCC1_MODULES:.c=.dobj)

NATIVE_KPARSE_MODULES = \
	$(NATIVE_BUILD_DIR)/cc-parse.c $(NATIVE_BUILD_DIR)/ccasmb.c $(NATIVE_BUILD_DIR)/ccdata-parse.c $(NATIVE_BUILD_DIR)/ccbind-parse.c $(NATIVE_BUILD_DIR)/ccdbug.c $(NATIVE_BUILD_DIR)/ccdecl.c \
	$(NATIVE_BUILD_DIR)/ccerr-parse.c $(NATIVE_BUILD_DIR)/cceval.c $(NATIVE_BUILD_DIR)/cclex.c $(NATIVE_BUILD_DIR)/ccnode.c $(NATIVE_BUILD_DIR)/ccppin.c $(NATIVE_BUILD_DIR)/ccstmt.c \
	$(NATIVE_BUILD_DIR)/ccsym.c $(NATIVE_BUILD_DIR)/cctype.c $(NATIVE_BUILD_DIR)/ccoututil.c $(NATIVE_BUILD_DIR)/cckirwrite.c $(NATIVE_BUILD_DIR)/ccvla.c
NATIVE_KPARSE_OBJS = $(NATIVE_KPARSE_MODULES:.c=.dobj) $(NATIVE_RUNTIME_DIR)/daimos-chain.dobj $(NATIVE_RUNTIME_DIR)/daimos-path.dobj

NATIVE_KGEN_MODULES = \
	$(NATIVE_BUILD_DIR)/cckgen-gen.c $(NATIVE_BUILD_DIR)/cccreg.c $(NATIVE_BUILD_DIR)/cccse.c $(NATIVE_BUILD_DIR)/cccode-gen.c $(NATIVE_BUILD_DIR)/ccdata-gen.c $(NATIVE_BUILD_DIR)/ccdbug.c \
	$(NATIVE_BUILD_DIR)/ccerr-gen.c $(NATIVE_BUILD_DIR)/ccevalgen.c $(NATIVE_BUILD_DIR)/ccgen-gen.c $(NATIVE_BUILD_DIR)/ccgen1-gen.c $(NATIVE_BUILD_DIR)/ccgen2.c $(NATIVE_BUILD_DIR)/ccgswi.c \
	$(NATIVE_BUILD_DIR)/ccjskp.c $(NATIVE_BUILD_DIR)/ccnode-gen.c $(NATIVE_BUILD_DIR)/ccreg.c $(NATIVE_BUILD_DIR)/ccsym-gen.c $(NATIVE_BUILD_DIR)/cctype-gen.c $(NATIVE_BUILD_DIR)/ccopt.c \
	$(NATIVE_BUILD_DIR)/ccoututil.c $(NATIVE_BUILD_DIR)/cckpout-gen.c $(NATIVE_BUILD_DIR)/cckpwrite-gen.c $(NATIVE_BUILD_DIR)/cckirread.c $(NATIVE_BUILD_DIR)/ccvla.c
NATIVE_KGEN_OBJS = $(NATIVE_KGEN_MODULES:.c=.dobj) $(NATIVE_RUNTIME_DIR)/daimos-chain.dobj $(NATIVE_RUNTIME_DIR)/daimos-path.dobj

NATIVE_KOPT_MODULES = \
	$(NATIVE_BUILD_DIR)/cckopt-opt.c $(NATIVE_BUILD_DIR)/cckpread-opt.c $(NATIVE_BUILD_DIR)/ccout-opt.c $(NATIVE_BUILD_DIR)/ccoututil.c $(NATIVE_BUILD_DIR)/ccdata-opt.c $(NATIVE_BUILD_DIR)/ccerr-opt.c \
	$(NATIVE_BUILD_DIR)/ccasmb.c
NATIVE_KOPT_OBJS = $(NATIVE_KOPT_MODULES:.c=.dobj) $(NATIVE_RUNTIME_DIR)/daimos-path.dobj

# Architecture-specific runtime assembly installed alongside host KCC.
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

all: depend
	$(MAKE) host-built

host-built: $(KCC) runtime

kcc: $(KCC)

$(HOST_BUILD_DIR):
	mkdir -p $@

.DELETE_ON_ERROR:

$(KCC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

$(HOST_BUILD_DIR)/ccgen.o $(HOST_BUILD_DIR)/ccgen1.o $(HOST_BUILD_DIR)/ccgen2.o: cc.h ccgen.h

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

# Complete host-side bootstrap for DAIMOS: four native compiler phases,
# driver, startup/runtime objects and native libc archive.  These files
# are consumed by DAIMOS's boot-image staging at NATIVE_BUILD_DIR.  This
# requires DAIMOS_REPO's libc to provide malloc/free and the native heap ABI;
# an older DAIMOS checkout will fail at link time instead of shipping broken
# executables.
# Keep the individual targets for iterative development; "make native"
# must never leave an apparently prepared but incomplete bootstrap tree.
native: depend
	$(MAKE) native-built

native-built: $(KCC) $(NATIVE_PHASE_DXRS) $(NATIVE_DRIVER_DXR) \
	$(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_BUILD_DIR):
	mkdir -p $@

$(NATIVE_RUNTIME_DIR):
	mkdir -p $@

$(NATIVE_DAIMOS_LIBC): $(KCC) $(DAIMOS_LIBC_SRCS)
	$(MAKE) -C $(DAIMOS_REPO)/userland/libc build \
		PDP10_PREFIX='$(PDP10_PREFIX)' BUILD_ROOT='$(NATIVE_DAIMOS_LIBC_ROOT)' \
		CC='$(KCC)'

$(NATIVE_DAIMOS_SYSCALL_OBJ): $(DAIMOS_REPO)/userland/libc/syscall.s $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DAS) -F -C -O $@ $(DAIMOS_REPO)/userland/libc/syscall.s

$(NATIVE_RUNTIME_DIR)/daimos-bootstrap.s: runtime/daimos-bootstrap.c $(KCC)
	mkdir -p $(NATIVE_RUNTIME_DIR) && \
	$(COMPILE_NATIVE) \
		$(DAIMOS_CPP_INCLUDES) -S runtime/daimos-bootstrap.c -o $@

$(NATIVE_RUNTIME_DIR)/daimos-bootstrap.dobj: $(NATIVE_RUNTIME_DIR)/daimos-bootstrap.s
	$(PDP10_DAS) -F -C -O $@ $(NATIVE_RUNTIME_DIR)/daimos-bootstrap.s

$(NATIVE_RUNTIME_DIR)/daimos-driver.s: runtime/daimos-driver.c $(KCC)
	mkdir -p $(NATIVE_RUNTIME_DIR) && \
	$(COMPILE_NATIVE) \
		$(DAIMOS_CPP_INCLUDES) -S runtime/daimos-driver.c -o $@

$(NATIVE_RUNTIME_DIR)/daimos-driver.dobj: $(NATIVE_RUNTIME_DIR)/daimos-driver.s
	$(PDP10_DAS) -F -C -O $@ $(NATIVE_RUNTIME_DIR)/daimos-driver.s

$(NATIVE_RUNTIME_DIR)/daimos-chain.s: runtime/daimos-chain.c $(KCC)
	mkdir -p $(NATIVE_RUNTIME_DIR) && \
	$(COMPILE_NATIVE) \
		$(DAIMOS_CPP_INCLUDES) -S runtime/daimos-chain.c -o $@

$(NATIVE_RUNTIME_DIR)/daimos-chain.dobj: $(NATIVE_RUNTIME_DIR)/daimos-chain.s
	$(PDP10_DAS) -F -C -O $@ $(NATIVE_RUNTIME_DIR)/daimos-chain.s

$(NATIVE_RUNTIME_DIR)/daimos-path.s: runtime/daimos-path.c $(KCC)
	mkdir -p $(NATIVE_RUNTIME_DIR) && \
	$(COMPILE_NATIVE) \
		$(DAIMOS_CPP_INCLUDES) -S runtime/daimos-path.c -o $@

$(NATIVE_RUNTIME_DIR)/daimos-path.dobj: $(NATIVE_RUNTIME_DIR)/daimos-path.s
	$(PDP10_DAS) -F -C -O $@ $(NATIVE_RUNTIME_DIR)/daimos-path.s

$(NATIVE_DRIVER_DXR): $(NATIVE_RUNTIME_DIR)/daimos-driver.dobj $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(@:.dxr=.map) \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_RUNTIME_DIR)/daimos-driver.dobj $(NATIVE_DAIMOS_LIBC)

$(NATIVE_RUNTIME_DIR)/crt0.dobj: runtime/daimos-crt0.s
	mkdir -p $(NATIVE_RUNTIME_DIR) && $(PDP10_DAS) -F -C -O $@ runtime/daimos-crt0.s

$(NATIVE_RUNTIME_DIR)/syscall-helpers.dobj: $(DAIMOS_REPO)/userland/libc/syscall_helpers.s
	mkdir -p $(NATIVE_RUNTIME_DIR) && $(PDP10_DAS) -F -C -O $@ $(DAIMOS_REPO)/userland/libc/syscall_helpers.s

$(NATIVE_KCPP_DXR): $(NATIVE_KCPP_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KCPP.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KCPP_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KPARSE_DXR): $(NATIVE_KPARSE_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KPARSE.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KPARSE_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KGEN_DXR): $(NATIVE_KGEN_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KGEN.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KGEN_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KOPT_DXR): $(NATIVE_KOPT_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KOPT.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KOPT_OBJS) $(NATIVE_DAIMOS_LIBC)


# Native source dependencies; shared recipes vary only by phase.

# Ordinary native sources share their suffixes with the host source inventory.
# The KIR reader and writer are native-only in the split bootstrap.
NATIVE_PLAIN_ASMS = $(NATIVE_ASMS) $(NATIVE_BUILD_DIR)/cckirread.s \
    $(NATIVE_BUILD_DIR)/cckirwrite.s

$(NATIVE_PLAIN_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    $(COMPILE_NATIVE) -S "$$name.c" -o "$@"

# CPP assembly variant.
NATIVE_CPP_ASMS = \
	$(NATIVE_BUILD_DIR)/ccppout.s $(NATIVE_BUILD_DIR)/cc-cpp.s $(NATIVE_BUILD_DIR)/ccout-cpp.s $(NATIVE_BUILD_DIR)/ccerr-cpp.s $(NATIVE_BUILD_DIR)/ccsym-cpp.s \
	$(NATIVE_BUILD_DIR)/ccdata-cpp.s

$(NATIVE_CPP_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-cpp}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_CPP=1 -S "$$name.c" -o "$@"

# CORE assembly variant.
NATIVE_CORE_ASMS = \
	$(NATIVE_BUILD_DIR)/ccppin.s $(NATIVE_BUILD_DIR)/ccdata-core.s $(NATIVE_BUILD_DIR)/ccerr-core.s $(NATIVE_BUILD_DIR)/ccout-core.s $(NATIVE_BUILD_DIR)/ccgen-core.s \
	$(NATIVE_BUILD_DIR)/cc-core.s

$(NATIVE_CORE_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-core}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_CORE=1 -S "$$name.c" -o "$@"

# GEN assembly variant.
NATIVE_GEN_ASMS = \
	$(NATIVE_BUILD_DIR)/cckgen-gen.s $(NATIVE_BUILD_DIR)/ccdata-gen.s $(NATIVE_BUILD_DIR)/cccode-gen.s $(NATIVE_BUILD_DIR)/ccnode-gen.s $(NATIVE_BUILD_DIR)/ccsym-gen.s \
	$(NATIVE_BUILD_DIR)/ccerr-gen.s $(NATIVE_BUILD_DIR)/ccevalgen.s $(NATIVE_BUILD_DIR)/cctype-gen.s $(NATIVE_BUILD_DIR)/ccgen1-gen.s $(NATIVE_BUILD_DIR)/ccgen-gen.s \
	$(NATIVE_BUILD_DIR)/ccppin-gen.s $(NATIVE_BUILD_DIR)/cckpout-gen.s $(NATIVE_BUILD_DIR)/cckpwrite-gen.s

$(NATIVE_GEN_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-gen}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_GEN=1 -S "$$name.c" -o "$@"

# PARSE assembly variant.
NATIVE_PARSE_ASMS = \
	$(NATIVE_BUILD_DIR)/cc-parse.s $(NATIVE_BUILD_DIR)/ccerr-parse.s $(NATIVE_BUILD_DIR)/ccdata-parse.s $(NATIVE_BUILD_DIR)/ccbind-parse.s

$(NATIVE_PARSE_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-parse}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_PARSE=1 -S "$$name.c" -o "$@"

# OPT assembly variant.
NATIVE_OPT_ASMS = \
	$(NATIVE_BUILD_DIR)/cckopt-opt.s $(NATIVE_BUILD_DIR)/ccdata-opt.s $(NATIVE_BUILD_DIR)/ccout-opt.s $(NATIVE_BUILD_DIR)/ccerr-opt.s $(NATIVE_BUILD_DIR)/cckpread-opt.s

$(NATIVE_OPT_ASMS): $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-opt}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_OPT=1 -S "$$name.c" -o "$@"


# Sources scanned by host makedepend (ordinary KCC and native-only files).
HOST_SOURCE_FILES = \
	cc.c ccasmb.c cccreg.c cccse.c cccode.c ccdata.c ccdbug.c \
	ccdecl.c ccerr.c cceval.c ccgen.c ccgen1.c ccgen2.c ccgswi.c \
	ccjskp.c cclex.c ccnode.c ccout.c ccoututil.c ccpp.c ccsrc.c \
	ccreg.c ccstmt.c ccsym.c cctype.c ccopt.c ccvla.c
NATIVE_SOURCE_FILES = $(HOST_SOURCE_FILES) cckirread.c cckirwrite.c \
    ccppout.c ccppin.c cckgen.c ccevalgen.c cckpout.c cckpwrite.c \
    ccbind.c cckopt.c cckpread.c

# target:source:phase triples for variant-specific preprocessing dependencies.
NATIVE_VARIANTS = \
	cc-cpp:cc.c:CPP cc-core:cc.c:CORE cc-parse:cc.c:PARSE cccode-gen:cccode.c:GEN ccdata-cpp:ccdata.c:CPP ccdata-core:ccdata.c:CORE ccdata-gen:ccdata.c:GEN \
	ccdata-parse:ccdata.c:PARSE ccdata-opt:ccdata.c:OPT ccerr-cpp:ccerr.c:CPP ccerr-core:ccerr.c:CORE ccerr-gen:ccerr.c:GEN ccerr-parse:ccerr.c:PARSE ccerr-opt:ccerr.c:OPT \
	ccgen-core:ccgen.c:CORE ccgen-gen:ccgen.c:GEN ccgen1-gen:ccgen1.c:GEN ccnode-gen:ccnode.c:GEN ccout-cpp:ccout.c:CPP ccout-core:ccout.c:CORE ccout-opt:ccout.c:OPT \
	ccsym-cpp:ccsym.c:CPP ccsym-gen:ccsym.c:GEN cctype-gen:cctype.c:GEN ccppout:ccppout.c:CPP ccppin:ccppin.c:CORE ccppin-gen:ccppin.c:GEN cckgen-gen:cckgen.c:GEN \
	ccevalgen:ccevalgen.c:GEN cckpout-gen:cckpout.c:GEN cckpwrite-gen:cckpwrite.c:GEN ccbind-parse:ccbind.c:PARSE cckopt-opt:cckopt.c:OPT cckpread-opt:cckpread.c:OPT

# Generate exact source/header dependencies with the host-side X.Org scanner.
# Both make implementations use an explicit first stage before parsing depend.mk.
# makedepend is a standard host command, installed independently from KCC.
# Supply MAKEDEPEND=/path/to/makedepend when it is not on PATH.

# The dependency file is regenerated before each user-requested build.
# The second make invocation loads it; this avoids nonportable makefile remaking.
depend:
	@mkdir -p $(HOST_BUILD_DIR)
	@set -e; output=$(HOST_BUILD_DIR)/depend.mk; temp=$$output.tmp; \
	    host_inc=`$(CC) -print-file-name=include`; \
	    $(MAKEDEPEND) -f- -p$(HOST_BUILD_DIR)/ -o.o \
	        -I$$host_inc -I. $(HOST_SOURCE_FILES) > $$temp; \
	    $(MAKEDEPEND) -f- -p$(HOST_BUILD_DIR)/ -o.s \
	        -I$$host_inc -I. $(HOST_SOURCE_FILES) >> $$temp; \
	    $(MAKEDEPEND) -f- -p$(NATIVE_BUILD_DIR)/ -o.s \
	        -Yself/include -I. -Iself/include -D__COMPILER_KCC__=1 -DHOST_DAIMOS=1 -DHOST_UNIX=0 \
	        $(NATIVE_SOURCE_FILES) >> $$temp; \
	    for item in $(NATIVE_VARIANTS); do \
	      target=$${item%%:*}; rest=$${item#*:}; source=$${rest%%:*}; phase=$${rest##*:}; \
	      $(MAKEDEPEND) -f- -p$(NATIVE_BUILD_DIR)/ -o.s \
	          -Yself/include -I. -Iself/include -D__COMPILER_KCC__=1 -DHOST_DAIMOS=1 -DHOST_UNIX=0 \
	          -DKCC_PHASE_$$phase=1 $$source | \
	          sed "s@^$(NATIVE_BUILD_DIR)/$${source%.c}\.s:@$(NATIVE_BUILD_DIR)/$$target.s:@" >> $$temp; \
	    done; \
	    for source in $(HOST_SOURCE_FILES); do \
	      stem=$${source%.c}; \
	      printf '%s: %s\n' "$(HOST_BUILD_DIR)/$$stem.o $(HOST_BUILD_DIR)/$$stem.s" "$$source" >> $$temp; \
	    done; \
	    for source in $(NATIVE_SOURCE_FILES); do \
	      stem=$${source%.c}; \
	      printf '%s: %s\n' "$(NATIVE_BUILD_DIR)/$$stem.s" "$$source" >> $$temp; \
	    done; \
	    for item in $(NATIVE_VARIANTS); do \
	      target=$${item%%:*}; rest=$${item#*:}; source=$${rest%%:*}; \
	      printf '%s: %s\n' "$(NATIVE_BUILD_DIR)/$$target.s" "$$source" >> $$temp; \
	    done; \
	    mv $$temp $$output

-include $(HOST_BUILD_DIR)/depend.mk

# Explicit dependencies with common GNU/BSD make recipes.

$(OBJS):
	@mkdir -p $(HOST_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.o}; \
	    $(CC) $(CFLAGS) -c "$$name.c" -o "$@"

$(ASMS): $(KCC)
	@mkdir -p $(HOST_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    $(KCC) $(KCC_SELF_FLAGS) -S "$$name.c" -o "$@"

runtime: $(RUNTIME)

# Host installation is independent of the cross-toolchain search prefix.
install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(KCC) $(DESTDIR)$(BINDIR)/kcc
	$(MAKE) install-runtime

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(DESTDIR)$(KCCLIBDIR)/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	@for f in $(RUNTIME); do \
	    $(RM) "$(DESTDIR)$(KCCLIBDIR)/$${f##*/}"; \
	done

clean:
	$(RM) -r $(HOST_BUILD_DIR) $(NATIVE_BUILD_DIR)

.PHONY: depend host-built native-built kcc all asm self-asm native-asm native-objects native-kcpp-objects \
	native-kcc1-objects native-kparse-objects native-kgen-objects native-kopt-objects native-phase-objects native-phase-dxrs native-driver native runtime install install-runtime \
	uninstall clean
