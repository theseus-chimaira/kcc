CC ?= cc
KCC ?= build/kcc
KCC_SELF_FLAGS ?= -P=stdc+kcc -DHOST_UNIX=1 -Iself/include/ -Hself/include/
PDP10_DAS ?= $(PDP10_PREFIX)/bin/das
PDP10_DLINK ?= $(PDP10_PREFIX)/bin/dlink
DAIMOS_REPO ?= ../DAIMOS
NATIVE_BUILD_DIR ?= build-native
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
DAIMOS_CPP_INCLUDES = -I$(DAIMOS_REPO)/userland/libc \
	-I$(DAIMOS_REPO)/system/kernel/boot -I$(DAIMOS_REPO)/system/kernel/core \
	-I$(DAIMOS_REPO)/system/kernel/drivers -I$(DAIMOS_REPO)/system/kernel/fs \
	-I$(DAIMOS_REPO)/system/kernel/mm -I$(DAIMOS_REPO)/system/kernel/modules \
	-I$(DAIMOS_REPO)/system/kernel/proc -I$(DAIMOS_REPO)/system/kernel/storage \
	-I$(PDP10_PREFIX)/include
RUNTIMEDIR = runtime
.SUFFIXES:
.SUFFIXES: .s .dobj
.s.dobj:
	$(ASSEMBLE_NATIVE)

ASSEMBLE_NATIVE = $(PDP10_DAS) -F -C -O $@ $<
COMPILE_NATIVE = $(KCC) $(NATIVE_KCCFLAGS) $(NATIVE_CPPFLAGS)
KCC_ROOT != pwd
HOST_BUILD_DIR ?= build
KCC_ABS ?= $(KCC_ROOT)/$(HOST_BUILD_DIR)/kcc
DAIMOS_LIBC_SRCS = $(DAIMOS_REPO)/userland/libc/Makefile $(DAIMOS_REPO)/userland/libc/crt0.s $(DAIMOS_REPO)/userland/libc/logevent.c $(DAIMOS_REPO)/userland/libc/memcpy.s $(DAIMOS_REPO)/userland/libc/memmove.s $(DAIMOS_REPO)/userland/libc/process_ctype.c $(DAIMOS_REPO)/userland/libc/stat_time.c $(DAIMOS_REPO)/userland/libc/stdlib.c $(DAIMOS_REPO)/userland/libc/string.c $(DAIMOS_REPO)/userland/libc/syscall.s $(DAIMOS_REPO)/userland/libc/syscall_helpers.s $(DAIMOS_REPO)/userland/libc/text.c $(DAIMOS_REPO)/userland/libc/u.c

SRCS = \
	cc.c ccasmb.c cccreg.c cccse.c cccode.c ccdata.c ccdbug.c ccdecl.c \
	ccerr.c cceval.c ccgen.c ccgen1.c ccgen2.c ccgswi.c ccjskp.c cclex.c \
	ccnode.c ccout.c ccoututil.c ccpp.c ccsrc.c ccreg.c ccstmt.c ccsym.c cctype.c ccopt.c \
	ccvla.c
OBJS = \
	$(HOST_BUILD_DIR)/cc.o $(HOST_BUILD_DIR)/ccasmb.o $(HOST_BUILD_DIR)/cccreg.o $(HOST_BUILD_DIR)/cccse.o $(HOST_BUILD_DIR)/cccode.o \
	$(HOST_BUILD_DIR)/ccdata.o $(HOST_BUILD_DIR)/ccdbug.o $(HOST_BUILD_DIR)/ccdecl.o $(HOST_BUILD_DIR)/ccerr.o $(HOST_BUILD_DIR)/cceval.o \
	$(HOST_BUILD_DIR)/ccgen.o $(HOST_BUILD_DIR)/ccgen1.o $(HOST_BUILD_DIR)/ccgen2.o $(HOST_BUILD_DIR)/ccgswi.o $(HOST_BUILD_DIR)/ccjskp.o \
	$(HOST_BUILD_DIR)/cclex.o $(HOST_BUILD_DIR)/ccnode.o $(HOST_BUILD_DIR)/ccout.o $(HOST_BUILD_DIR)/ccoututil.o $(HOST_BUILD_DIR)/ccpp.o \
	$(HOST_BUILD_DIR)/ccsrc.o $(HOST_BUILD_DIR)/ccreg.o $(HOST_BUILD_DIR)/ccstmt.o $(HOST_BUILD_DIR)/ccsym.o $(HOST_BUILD_DIR)/cctype.o \
	$(HOST_BUILD_DIR)/ccopt.o $(HOST_BUILD_DIR)/ccvla.o
ASMS = \
	$(HOST_BUILD_DIR)/cc.s $(HOST_BUILD_DIR)/ccasmb.s $(HOST_BUILD_DIR)/cccreg.s $(HOST_BUILD_DIR)/cccse.s $(HOST_BUILD_DIR)/cccode.s \
	$(HOST_BUILD_DIR)/ccdata.s $(HOST_BUILD_DIR)/ccdbug.s $(HOST_BUILD_DIR)/ccdecl.s $(HOST_BUILD_DIR)/ccerr.s $(HOST_BUILD_DIR)/cceval.s \
	$(HOST_BUILD_DIR)/ccgen.s $(HOST_BUILD_DIR)/ccgen1.s $(HOST_BUILD_DIR)/ccgen2.s $(HOST_BUILD_DIR)/ccgswi.s $(HOST_BUILD_DIR)/ccjskp.s \
	$(HOST_BUILD_DIR)/cclex.s $(HOST_BUILD_DIR)/ccnode.s $(HOST_BUILD_DIR)/ccout.s $(HOST_BUILD_DIR)/ccoututil.s $(HOST_BUILD_DIR)/ccpp.s \
	$(HOST_BUILD_DIR)/ccsrc.s $(HOST_BUILD_DIR)/ccreg.s $(HOST_BUILD_DIR)/ccstmt.s $(HOST_BUILD_DIR)/ccsym.s $(HOST_BUILD_DIR)/cctype.s \
	$(HOST_BUILD_DIR)/ccopt.s $(HOST_BUILD_DIR)/ccvla.s
NATIVE_ASMS = \
	$(NATIVE_BUILD_DIR)/cc.s $(NATIVE_BUILD_DIR)/ccasmb.s $(NATIVE_BUILD_DIR)/cccreg.s $(NATIVE_BUILD_DIR)/cccse.s $(NATIVE_BUILD_DIR)/cccode.s \
	$(NATIVE_BUILD_DIR)/ccdata.s $(NATIVE_BUILD_DIR)/ccdbug.s $(NATIVE_BUILD_DIR)/ccdecl.s $(NATIVE_BUILD_DIR)/ccerr.s $(NATIVE_BUILD_DIR)/cceval.s \
	$(NATIVE_BUILD_DIR)/ccgen.s $(NATIVE_BUILD_DIR)/ccgen1.s $(NATIVE_BUILD_DIR)/ccgen2.s $(NATIVE_BUILD_DIR)/ccgswi.s $(NATIVE_BUILD_DIR)/ccjskp.s \
	$(NATIVE_BUILD_DIR)/cclex.s $(NATIVE_BUILD_DIR)/ccnode.s $(NATIVE_BUILD_DIR)/ccout.s $(NATIVE_BUILD_DIR)/ccoututil.s $(NATIVE_BUILD_DIR)/ccpp.s \
	$(NATIVE_BUILD_DIR)/ccsrc.s $(NATIVE_BUILD_DIR)/ccreg.s $(NATIVE_BUILD_DIR)/ccstmt.s $(NATIVE_BUILD_DIR)/ccsym.s $(NATIVE_BUILD_DIR)/cctype.s \
	$(NATIVE_BUILD_DIR)/ccopt.s $(NATIVE_BUILD_DIR)/ccvla.s
NATIVE_OBJS = $(NATIVE_ASMS:.s=.dobj)
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
NATIVE_HEADERS = \
	c-env.h cc.h ccchar.h cccode.h ccerr.h \
	ccgen.h cckir.h cckpcode.h cclex.h ccnode.h \
	ccparm.h ccphase.h ccreg.h ccsite.h ccsrc.h \
	ccsym.h cctoks.h ccvla.h kcchst.h self/include/ctype.h \
	self/include/errno.h self/include/limits.h self/include/muuo.h self/include/stdarg.h self/include/stddef.h \
	self/include/stdio.h self/include/stdlib.h self/include/string.h self/include/time.h
NATIVE_PHASE_ASMS = \
	$(NATIVE_BUILD_DIR)/ccppout.s $(NATIVE_BUILD_DIR)/ccppin.s \
	$(NATIVE_BUILD_DIR)/cc-cpp.s $(NATIVE_BUILD_DIR)/ccout-cpp.s $(NATIVE_BUILD_DIR)/ccerr-cpp.s \
	$(NATIVE_BUILD_DIR)/ccsym-cpp.s $(NATIVE_BUILD_DIR)/ccdata-cpp.s \
	$(NATIVE_BUILD_DIR)/ccdata-core.s $(NATIVE_BUILD_DIR)/ccerr-core.s \
	$(NATIVE_BUILD_DIR)/ccout-core.s $(NATIVE_BUILD_DIR)/ccgen-core.s $(NATIVE_BUILD_DIR)/cc-core.s \
	$(NATIVE_BUILD_DIR)/cckgen-gen.s $(NATIVE_BUILD_DIR)/ccdata-gen.s $(NATIVE_BUILD_DIR)/cccode-gen.s \
	$(NATIVE_BUILD_DIR)/ccnode-gen.s $(NATIVE_BUILD_DIR)/ccsym-gen.s $(NATIVE_BUILD_DIR)/ccerr-gen.s \
	$(NATIVE_BUILD_DIR)/ccevalgen.s $(NATIVE_BUILD_DIR)/cctype-gen.s $(NATIVE_BUILD_DIR)/ccgen1-gen.s \
	$(NATIVE_BUILD_DIR)/ccgen-gen.s $(NATIVE_BUILD_DIR)/ccppin-gen.s $(NATIVE_BUILD_DIR)/cckpout-gen.s \
	$(NATIVE_BUILD_DIR)/cckpwrite-gen.s $(NATIVE_BUILD_DIR)/cc-parse.s $(NATIVE_BUILD_DIR)/ccerr-parse.s \
	$(NATIVE_BUILD_DIR)/ccdata-parse.s $(NATIVE_BUILD_DIR)/ccbind-parse.s \
	$(NATIVE_BUILD_DIR)/cckopt-opt.s $(NATIVE_BUILD_DIR)/ccdata-opt.s \
	$(NATIVE_BUILD_DIR)/ccout-opt.s $(NATIVE_BUILD_DIR)/ccerr-opt.s $(NATIVE_BUILD_DIR)/cckpread-opt.s

NATIVE_KCPP_OBJS = \
	$(NATIVE_BUILD_DIR)/cc-cpp.dobj $(NATIVE_BUILD_DIR)/ccasmb.dobj \
	$(NATIVE_BUILD_DIR)/ccdata-cpp.dobj $(NATIVE_BUILD_DIR)/ccerr-cpp.dobj $(NATIVE_BUILD_DIR)/ccout-cpp.dobj \
	$(NATIVE_BUILD_DIR)/ccpp.dobj $(NATIVE_BUILD_DIR)/ccppout.dobj \
	$(NATIVE_BUILD_DIR)/ccsym-cpp.dobj $(NATIVE_BUILD_DIR)/ccsrc.dobj

NATIVE_KCC1_OBJS = \
	$(NATIVE_BUILD_DIR)/cc-core.dobj $(NATIVE_BUILD_DIR)/ccasmb.dobj \
	$(NATIVE_BUILD_DIR)/cccreg.dobj $(NATIVE_BUILD_DIR)/cccse.dobj \
	$(NATIVE_BUILD_DIR)/cccode.dobj $(NATIVE_BUILD_DIR)/ccdata-core.dobj \
	$(NATIVE_BUILD_DIR)/ccdbug.dobj $(NATIVE_BUILD_DIR)/ccdecl.dobj \
	$(NATIVE_BUILD_DIR)/ccerr-core.dobj $(NATIVE_BUILD_DIR)/cceval.dobj \
	$(NATIVE_BUILD_DIR)/ccgen-core.dobj $(NATIVE_BUILD_DIR)/ccgen1.dobj \
	$(NATIVE_BUILD_DIR)/ccgen2.dobj $(NATIVE_BUILD_DIR)/ccgswi.dobj \
	$(NATIVE_BUILD_DIR)/ccjskp.dobj $(NATIVE_BUILD_DIR)/cclex.dobj \
	$(NATIVE_BUILD_DIR)/ccnode.dobj $(NATIVE_BUILD_DIR)/ccout-core.dobj \
	$(NATIVE_BUILD_DIR)/ccreg.dobj $(NATIVE_BUILD_DIR)/ccstmt.dobj \
	$(NATIVE_BUILD_DIR)/ccsym.dobj $(NATIVE_BUILD_DIR)/cctype.dobj \
	$(NATIVE_BUILD_DIR)/ccopt.dobj $(NATIVE_BUILD_DIR)/ccoututil.dobj \
	$(NATIVE_BUILD_DIR)/ccppin.dobj $(NATIVE_BUILD_DIR)/ccvla.dobj

NATIVE_KPARSE_OBJS = \
	$(NATIVE_BUILD_DIR)/cc-parse.dobj $(NATIVE_RUNTIME_DIR)/daimos-chain.dobj $(NATIVE_RUNTIME_DIR)/daimos-path.dobj \
	$(NATIVE_BUILD_DIR)/ccasmb.dobj \
	$(NATIVE_BUILD_DIR)/ccdata-parse.dobj $(NATIVE_BUILD_DIR)/ccbind-parse.dobj $(NATIVE_BUILD_DIR)/ccdbug.dobj \
	$(NATIVE_BUILD_DIR)/ccdecl.dobj $(NATIVE_BUILD_DIR)/ccerr-parse.dobj \
	$(NATIVE_BUILD_DIR)/cceval.dobj $(NATIVE_BUILD_DIR)/cclex.dobj \
	$(NATIVE_BUILD_DIR)/ccnode.dobj $(NATIVE_BUILD_DIR)/ccppin.dobj \
	$(NATIVE_BUILD_DIR)/ccstmt.dobj $(NATIVE_BUILD_DIR)/ccsym.dobj \
	$(NATIVE_BUILD_DIR)/cctype.dobj $(NATIVE_BUILD_DIR)/ccoututil.dobj \
	$(NATIVE_BUILD_DIR)/cckirwrite.dobj $(NATIVE_BUILD_DIR)/ccvla.dobj

NATIVE_KGEN_OBJS = \
	$(NATIVE_BUILD_DIR)/cckgen-gen.dobj $(NATIVE_RUNTIME_DIR)/daimos-chain.dobj $(NATIVE_RUNTIME_DIR)/daimos-path.dobj \
	$(NATIVE_BUILD_DIR)/cccreg.dobj $(NATIVE_BUILD_DIR)/cccse.dobj \
	$(NATIVE_BUILD_DIR)/cccode-gen.dobj $(NATIVE_BUILD_DIR)/ccdata-gen.dobj \
	$(NATIVE_BUILD_DIR)/ccdbug.dobj $(NATIVE_BUILD_DIR)/ccerr-gen.dobj \
	$(NATIVE_BUILD_DIR)/ccevalgen.dobj \
	$(NATIVE_BUILD_DIR)/ccgen-gen.dobj $(NATIVE_BUILD_DIR)/ccgen1-gen.dobj \
	$(NATIVE_BUILD_DIR)/ccgen2.dobj $(NATIVE_BUILD_DIR)/ccgswi.dobj \
	$(NATIVE_BUILD_DIR)/ccjskp.dobj \
	$(NATIVE_BUILD_DIR)/ccnode-gen.dobj $(NATIVE_BUILD_DIR)/ccreg.dobj \
	$(NATIVE_BUILD_DIR)/ccsym-gen.dobj \
	$(NATIVE_BUILD_DIR)/cctype-gen.dobj $(NATIVE_BUILD_DIR)/ccopt.dobj \
	$(NATIVE_BUILD_DIR)/ccoututil.dobj $(NATIVE_BUILD_DIR)/cckpout-gen.dobj \
	$(NATIVE_BUILD_DIR)/cckpwrite-gen.dobj $(NATIVE_BUILD_DIR)/cckirread.dobj \
	$(NATIVE_BUILD_DIR)/ccvla.dobj

NATIVE_KOPT_OBJS = \
	$(NATIVE_BUILD_DIR)/cckopt-opt.dobj $(NATIVE_RUNTIME_DIR)/daimos-path.dobj $(NATIVE_BUILD_DIR)/cckpread-opt.dobj $(NATIVE_BUILD_DIR)/ccout-opt.dobj \
	$(NATIVE_BUILD_DIR)/ccoututil.dobj $(NATIVE_BUILD_DIR)/ccdata-opt.dobj \
	$(NATIVE_BUILD_DIR)/ccerr-opt.dobj $(NATIVE_BUILD_DIR)/ccasmb.dobj

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

all: $(HOST_BUILD_DIR)/kcc runtime

kcc: $(HOST_BUILD_DIR)/kcc

$(HOST_BUILD_DIR):
	mkdir -p $@

.DELETE_ON_ERROR:

$(HOST_BUILD_DIR)/kcc: $(OBJS)
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
native: $(HOST_BUILD_DIR)/kcc $(NATIVE_PHASE_DXRS) $(NATIVE_DRIVER_DXR) \
	$(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_BUILD_DIR):
	mkdir -p $@

$(NATIVE_RUNTIME_DIR):
	mkdir -p $@

$(NATIVE_DAIMOS_LIBC): $(KCC) $(DAIMOS_LIBC_SRCS)
	$(MAKE) -C $(DAIMOS_REPO)/userland/libc build \
		PDP10_PREFIX='$(PDP10_PREFIX)' BUILD_ROOT='$(NATIVE_DAIMOS_LIBC_ROOT)' \
		CC='$(KCC_ABS)'

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

$(NATIVE_ASMS) $(NATIVE_PHASE_ASMS): $(NATIVE_HEADERS)

include mk/native.mk

runtime: $(RUNTIME)

install: all
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(HOST_BUILD_DIR)/kcc $(DESTDIR)$(BINDIR)/kcc
	cmp $(HOST_BUILD_DIR)/kcc $(DESTDIR)$(BINDIR)/kcc
	$(MAKE) install-runtime

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(DESTDIR)$(KCCLIBDIR)/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	@for f in $(RUNTIME); do \
		$(RM) "$(DESTDIR)$(KCCLIBDIR)/$${f##*/}"; \
	done

include mk/host.mk

clean:
	$(RM) -r $(HOST_BUILD_DIR) $(NATIVE_BUILD_DIR)

.PHONY: kcc all asm self-asm native-asm native-objects native-kcpp-objects \
	native-kcc1-objects native-kparse-objects native-kgen-objects native-kopt-objects native-phase-objects native-phase-dxrs native-driver native runtime install install-runtime \
	uninstall clean
