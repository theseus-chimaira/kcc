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
	install-runtime install-headers uninstall clean
