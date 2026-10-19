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
	runtime/libc/kcc-libc-strspn-v1.s runtime/libc/p10bytes-v2.s
FREESTANDING_LIBC_ARCHIVE = $(HOST_BUILD_DIR)/libp10c-v2.a

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

install-freestanding-libc: freestanding-libc
	$(INSTALL) -d $(DESTDIR)$(FREESTANDING_LIBC_DIR)
	$(INSTALL) -m 644 runtime/libc/kcc-libc-*.s runtime/libc/p10bytes-v2.s \
		$(FREESTANDING_LIBC_STRING) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/
	$(INSTALL) -m 644 $(FREESTANDING_LIBC_ARCHIVE) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/

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
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memcmp-v1.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/kcc-libc-memcmp-v2.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/p10bytes-v2.s
	$(RM) $(DESTDIR)$(FREESTANDING_LIBC_DIR)/libp10c-v2.a
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
	install-runtime install-headers freestanding-libc \
	install-freestanding-libc uninstall clean
