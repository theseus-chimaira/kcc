# Unix-hosted cross-compiler, host runtime, and host installation.
include common.mk
include native.mk
# Default host build is the Unix-hosted PDP-6/PDP-10 cross-compiler.
all: cross

cross: depend
	$(MAKE) $(KCC) runtime

$(HOST_BUILD_DIR):
	mkdir -p $@

.DELETE_ON_ERROR:

$(KCC): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^

$(HOST_BUILD_DIR)/ccgen.o $(HOST_BUILD_DIR)/ccgen1.o $(HOST_BUILD_DIR)/ccgen2.o: cc.h ccgen.h

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
install: cross
	$(INSTALL) -d $(DESTDIR)$(BINDIR)
	$(INSTALL) -m 755 $(KCC) $(DESTDIR)$(BINDIR)/kcc
	$(MAKE) install-runtime
	$(MAKE) install-headers
	$(MAKE) install-freestanding-libc

# Freestanding C library primitives use only the common PDP-6 instruction
# subset and the DAIMOS/KCC C ABI. No SYS calls or kernel state are needed.
# Keep these separately selectable so linkers do not pull unwanted routines
# into memory-constrained programs.
FREESTANDING_LIBC_DIR = $(KCCLIBDIR)/freestanding
FREESTANDING_LIBC_STRING = $(HOST_BUILD_DIR)/kcc-libc-string-v1.s
PDP10_DARC ?= $(PDP10_PREFIX)/bin/darc
FREESTANDING_LIBC_SOURCES = runtime/libc/kcc-libc-memcpy-v1.s \
	runtime/libc/kcc-libc-memmove-v1.s \
	runtime/libc/kcc-libc-memset-v1.s \
	runtime/libc/kcc-libc-memcmp-v2.s \
	runtime/libc/kcc-libc-strlen-v1.s \
	runtime/libc/kcc-libc-strcmp-v1.s \
	runtime/libc/kcc-libc-strncmp-v1.s \
	runtime/libc/kcc-libc-strstr-v1.s \
	runtime/libc/kcc-libc-strpbrk-v1.s \
	runtime/libc/kcc-libc-strspn-v1.s \
	runtime/libc/kcc-libc-dimode-float-pdp6-v1.s \
	runtime/libc/p10bytes-v2.s
FREESTANDING_LIBC_ARCHIVE = $(HOST_BUILD_DIR)/libp10c-v2.a
FREESTANDING_CTYPE_SOURCE = runtime/libc/kcc-libc-ctype-c-locale-v1.s
FREESTANDING_CTYPE_ARCHIVE = $(HOST_BUILD_DIR)/libp10ctype-c-locale-v1.a
FREESTANDING_FP36_SOURCE = runtime/libc/kcc-libc-fp36-pdp6-v1.s
FREESTANDING_FP36_ARCHIVE = $(HOST_BUILD_DIR)/libp10fp36-pdp6-v1.a
FREESTANDING_SIGNED_DF_SOURCE = runtime/libc/kcc-libc-fp36-signed-df-pdp6-v1.s
FREESTANDING_SIGNED_DF_ARCHIVE = $(HOST_BUILD_DIR)/libp10fp36-signed-df-pdp6-v1.a
# Same portable source as GCC, separately compiled for KCC's va_list ABI.
FREESTANDING_STDIO_SOURCE = runtime/libc/p10stdio-format-v2.c
FREESTANDING_STDIO_ASM = $(HOST_BUILD_DIR)/p10stdio-format-v2.s
FREESTANDING_STDIO_ARCHIVE = $(HOST_BUILD_DIR)/libp10stdio-v2.a
FREESTANDING_STDIO_V3_SOURCE = runtime/libc/p10stdio-format-v3.c
FREESTANDING_STDIO_V3_ASM = $(HOST_BUILD_DIR)/p10stdio-format-v3.s
FREESTANDING_STDIO_V3_ARCHIVE = $(HOST_BUILD_DIR)/libp10stdio-v3.a
GCC_FREESTANDING_CTYPE_DIR = $(PDP10_PREFIX)/lib/gcc-lib/pdp10-dec-none/3.2/freestanding

$(FREESTANDING_LIBC_STRING): runtime/libc/kcc-libc-string-v1.c $(KCC)
	$(INSTALL) -d $(HOST_BUILD_DIR)
	$(KCC) -S -v=nostats -x=pdp6 -Iself/include/ -o $@ $<

$(FREESTANDING_LIBC_ARCHIVE): $(FREESTANDING_LIBC_SOURCES) $(FREESTANDING_LIBC_STRING)
	@mkdir -p $(HOST_BUILD_DIR)/libp10c-v2
	@set -e; set --; \
	for source in $(FREESTANDING_LIBC_SOURCES) $(FREESTANDING_LIBC_STRING); do \
	    stem=$${source##*/}; stem=$${stem%.s}; \
	    obj="$(HOST_BUILD_DIR)/libp10c-v2/$$stem.dobj"; \
	    $(PDP10_DAS) -F -C -O "$$obj" "$$source"; \
	    set -- "$$@" "$$obj"; \
	done; \
	$(PDP10_DARC) -o "$@" "$$@"

freestanding-libc: $(FREESTANDING_LIBC_ARCHIVE)

# C-locale ctype is an independently selectable compiler library.  Keep
# libp10c-v2.a unchanged so its established source/ABI fingerprint and
# historical regression evidence are not invalidated by new services.
$(FREESTANDING_CTYPE_ARCHIVE): $(FREESTANDING_CTYPE_SOURCE)
	@mkdir -p $(HOST_BUILD_DIR)/libp10ctype-c-locale-v1
	$(PDP10_DAS) -F -C -O $(HOST_BUILD_DIR)/libp10ctype-c-locale-v1/kcc-libc-ctype-c-locale-v1.dobj $(FREESTANDING_CTYPE_SOURCE)
	$(PDP10_DARC) -o $@ $(HOST_BUILD_DIR)/libp10ctype-c-locale-v1/kcc-libc-ctype-c-locale-v1.dobj

freestanding-ctype: $(FREESTANDING_CTYPE_ARCHIVE)

# Separate compiler-owned conversion archive.  Do not change the
# already SHA-frozen core libp10c-v2.a or add compiler test stubs.
$(FREESTANDING_FP36_ARCHIVE): $(FREESTANDING_FP36_SOURCE)
	@mkdir -p $(HOST_BUILD_DIR)/libp10fp36-pdp6-v1
	$(PDP10_DAS) -F -C -O $(HOST_BUILD_DIR)/libp10fp36-pdp6-v1/kcc-libc-fp36-pdp6-v1.dobj $(FREESTANDING_FP36_SOURCE)
	$(PDP10_DARC) -o $@ $(HOST_BUILD_DIR)/libp10fp36-pdp6-v1/kcc-libc-fp36-pdp6-v1.dobj

freestanding-fp36: $(FREESTANDING_FP36_ARCHIVE)

# Dedicated signed D-format conversion member.  Keep the earlier FP36
# archive byte-identical to its verified v1 evidence.  These two
# directions share the exact unsigned D-format helpers in that archive.
$(FREESTANDING_SIGNED_DF_ARCHIVE): $(FREESTANDING_SIGNED_DF_SOURCE)
	@mkdir -p $(HOST_BUILD_DIR)/libp10fp36-signed-df-pdp6-v1
	$(PDP10_DAS) -F -C -O $(HOST_BUILD_DIR)/libp10fp36-signed-df-pdp6-v1/kcc-libc-fp36-signed-df-pdp6-v1.dobj $(FREESTANDING_SIGNED_DF_SOURCE)
	$(PDP10_DARC) -o $@ $(HOST_BUILD_DIR)/libp10fp36-signed-df-pdp6-v1/kcc-libc-fp36-signed-df-pdp6-v1.dobj

freestanding-signed-df: $(FREESTANDING_SIGNED_DF_ARCHIVE)

$(FREESTANDING_STDIO_ASM): $(FREESTANDING_STDIO_SOURCE) $(KCC) self/include/stdarg.h self/include/stddef.h self/include/limits.h
	$(KCC) -S -v=nostats -x=pdp6 -m=gas -Iself/include/ -o $@ $<

$(FREESTANDING_STDIO_ARCHIVE): $(FREESTANDING_STDIO_ASM)
	$(PDP10_DAS) -F -C -O $(HOST_BUILD_DIR)/p10stdio-format-v2.dobj $(FREESTANDING_STDIO_ASM)
	$(PDP10_DARC) -o $@ $(HOST_BUILD_DIR)/p10stdio-format-v2.dobj

freestanding-stdio: $(FREESTANDING_STDIO_ARCHIVE)

$(FREESTANDING_STDIO_V3_ASM): $(FREESTANDING_STDIO_V3_SOURCE) $(FREESTANDING_STDIO_SOURCE) runtime/libc/p10stdio-float-v3.h $(KCC) self/include/stdarg.h self/include/stddef.h self/include/limits.h
	$(KCC) -S -v=nostats -x=pdp6 -m=gas -Iself/include/ -o $@ $<

$(FREESTANDING_STDIO_V3_ARCHIVE): $(FREESTANDING_STDIO_V3_ASM)
	$(PDP10_DAS) -F -C -O $(HOST_BUILD_DIR)/p10stdio-format-v3.dobj $(FREESTANDING_STDIO_V3_ASM)
	$(PDP10_DARC) -o $@ $(HOST_BUILD_DIR)/p10stdio-format-v3.dobj

freestanding-stdio-v3: $(FREESTANDING_STDIO_V3_ARCHIVE)

install-freestanding-libc: freestanding-libc freestanding-ctype freestanding-fp36 freestanding-signed-df freestanding-stdio freestanding-stdio-v3
	$(INSTALL) -d $(DESTDIR)$(FREESTANDING_LIBC_DIR)
	$(INSTALL) -m 644 runtime/libc/kcc-libc-*.s runtime/libc/p10bytes-v2.s \
		$(FREESTANDING_LIBC_STRING) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/
	$(INSTALL) -m 644 $(FREESTANDING_LIBC_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/
	$(INSTALL) -m 644 $(FREESTANDING_CTYPE_SOURCE) $(FREESTANDING_CTYPE_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/
	$(INSTALL) -m 644 $(FREESTANDING_FP36_SOURCE) $(FREESTANDING_FP36_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/
	$(INSTALL) -m 644 $(FREESTANDING_SIGNED_DF_SOURCE) $(FREESTANDING_SIGNED_DF_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/

	$(INSTALL) -m 644 $(FREESTANDING_STDIO_ASM) $(FREESTANDING_STDIO_ARCHIVE) $(FREESTANDING_STDIO_V3_ASM) $(FREESTANDING_STDIO_V3_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/

# GCC and KCC use the same PDP-10 C ABI for these two freestanding
# entry points.  Install the exact same assembly/objects for GCC, not
# a separate reimplementation or a test-local implementation.
install-gcc-freestanding-ctype: freestanding-ctype
	$(INSTALL) -d $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)
	$(INSTALL) -m 644 $(FREESTANDING_CTYPE_SOURCE) $(FREESTANDING_CTYPE_ARCHIVE) $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)/

install-gcc-freestanding-fp36: freestanding-fp36
	$(INSTALL) -d $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)
	$(INSTALL) -m 644 $(FREESTANDING_FP36_SOURCE) $(FREESTANDING_FP36_ARCHIVE) $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)/

install-gcc-freestanding-signed-df: freestanding-signed-df
	$(INSTALL) -d $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)
	$(INSTALL) -m 644 $(FREESTANDING_SIGNED_DF_SOURCE) $(FREESTANDING_SIGNED_DF_ARCHIVE) $(DESTDIR)$(GCC_FREESTANDING_CTYPE_DIR)/

install-kcc-freestanding-signed-df: freestanding-signed-df
	$(INSTALL) -d $(DESTDIR)$(FREESTANDING_LIBC_DIR)
	$(INSTALL) -m 644 $(FREESTANDING_SIGNED_DF_SOURCE) $(FREESTANDING_SIGNED_DF_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/

install-runtime: runtime
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)
	$(INSTALL) -m 644 $(RUNTIME) $(DESTDIR)$(KCCLIBDIR)/

# KCC's target headers belong to the same installed toolchain as the
# compiler and runtime.  In particular, stdarg.h is ABI-specific; selecting
# an unrelated host compiler's headers would be incorrect.
install-headers:
	$(INSTALL) -d $(DESTDIR)$(KCCLIBDIR)/include/sys
	$(INSTALL) -m 644 self/include/*.h $(DESTDIR)$(KCCLIBDIR)/include/
	$(INSTALL) -m 644 self/include/sys/*.h $(DESTDIR)$(KCCLIBDIR)/include/sys/

uninstall:
	$(RM) $(DESTDIR)$(BINDIR)/kcc
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memcpy-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memmove-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memset-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strlen-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strcmp-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strncmp-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strstr-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strpbrk-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-strspn-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-dimode-float-pdp6-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memcmp-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memcmp-v2.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/p10bytes-v2.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/p10stdio-format-v2.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10stdio-v2.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/p10stdio-format-v3.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10stdio-v3.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10c-v2.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-ctype-c-locale-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10ctype-c-locale-v1.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-fp36-pdp6-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10fp36-pdp6-v1.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-fp36-signed-df-pdp6-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10fp36-signed-df-pdp6-v1.a
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-string-v1.s
	@for f in $(RUNTIME); do \
	    $(RM) "$(DESTDIR)$(KCCLIBDIR)/$${f##*/}"; \
	done
	@for f in self/include/*.h; do \
	    $(RM) "$(DESTDIR)$(KCCLIBDIR)/include/$${f##*/}"; \
	done
	@for f in self/include/sys/*.h; do \
	    $(RM) "$(DESTDIR)$(KCCLIBDIR)/include/sys/$${f##*/}"; \
	done

clean:
	$(RM) -r $(HOST_BUILD_DIR) $(NATIVE_BUILD_DIR)

.PHONY: all cross native depend runtime install install-native \
	install-runtime install-headers freestanding-libc freestanding-ctype freestanding-fp36 freestanding-signed-df freestanding-stdio freestanding-stdio-v3 install-gcc-freestanding-ctype install-gcc-freestanding-fp36 install-gcc-freestanding-signed-df install-kcc-freestanding-signed-df \
	install-freestanding-libc uninstall clean
