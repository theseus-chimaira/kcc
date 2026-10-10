# DAIMOS-target bootstrap, native phase objects, runtime, and linking.
# Complete host-side bootstrap for DAIMOS: four native compiler phases,
# driver, startup/runtime objects and native libc archive.  These files
# are consumed by DAIMOS's boot-image staging at NATIVE_BUILD_DIR.  This
# requires DAIMOS_REPO's libc to provide malloc/free and the native heap ABI;
# an older DAIMOS checkout will fail at link time instead of shipping broken
# executables.
# Keep the individual targets for iterative development; "make native"
# must never leave an apparently prepared but incomplete bootstrap tree.
native: depend
	$(MAKE) $(KCC) $(NATIVE_PHASE_DXRS) $(NATIVE_DRIVER_DXR) \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)

# Bootstrap DXRs are installed separately from the Unix-hosted compiler.
# DAIMOS reads these stable paths instead of private build-tree artifacts.
install-native: native
	$(INSTALL) -d $(NATIVE_BOOTSTRAP_DIR)
	$(INSTALL) -m 555 $(NATIVE_DRIVER_DXR) $(NATIVE_BOOTSTRAP_DIR)/KCC.dxr
	$(INSTALL) -m 555 $(NATIVE_KCPP_DXR) $(NATIVE_BOOTSTRAP_DIR)/KCPP.dxr
	$(INSTALL) -m 555 $(NATIVE_KPARSE_DXR) $(NATIVE_BOOTSTRAP_DIR)/KPARSE.dxr
	$(INSTALL) -m 555 $(NATIVE_KGEN_DXR) $(NATIVE_BOOTSTRAP_DIR)/KGEN.dxr
	$(INSTALL) -m 555 $(NATIVE_KOPT_DXR) $(NATIVE_BOOTSTRAP_DIR)/KOPT.dxr
	$(INSTALL) -d $(NATIVE_BOOTSTRAP_DIR)/runtime $(NATIVE_BOOTSTRAP_DIR)/daimos-libc/libc
	$(INSTALL) -m 444 $(NATIVE_RUNTIME_DIR)/crt0.dobj $(NATIVE_BOOTSTRAP_DIR)/runtime/crt0-v1.dobj
	$(INSTALL) -m 444 $(NATIVE_RUNTIME_DIR)/daimos-bootstrap.dobj $(NATIVE_BOOTSTRAP_DIR)/runtime/daimos-bootstrap-v1.dobj
	$(INSTALL) -m 444 $(NATIVE_RUNTIME_DIR)/syscall-helpers.dobj $(NATIVE_BOOTSTRAP_DIR)/runtime/syscall-helpers-v1.dobj
	$(INSTALL) -m 444 $(NATIVE_DAIMOS_SYSCALL_OBJ) $(NATIVE_BOOTSTRAP_DIR)/daimos-libc/libc/syscall.dobj
	$(INSTALL) -m 444 $(NATIVE_DAIMOS_LIBC) $(NATIVE_BOOTSTRAP_DIR)/daimos-libc/libc/libc.a

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
	@echo "LINK $@"
	@$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(@:.dxr=.map) \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_RUNTIME_DIR)/daimos-driver.dobj $(NATIVE_DAIMOS_LIBC)

$(NATIVE_RUNTIME_DIR)/crt0.dobj: runtime/daimos-crt0.s
	mkdir -p $(NATIVE_RUNTIME_DIR) && $(PDP10_DAS) -F -C -O $@ runtime/daimos-crt0.s

$(NATIVE_RUNTIME_DIR)/syscall-helpers.dobj: $(DAIMOS_REPO)/userland/libc/syscall_helpers.s
	mkdir -p $(NATIVE_RUNTIME_DIR) && $(PDP10_DAS) -F -C -O $@ $(DAIMOS_REPO)/userland/libc/syscall_helpers.s

$(NATIVE_KCPP_DXR): $(NATIVE_KCPP_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	@echo "LINK $@"
	@$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KCPP.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KCPP_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KPARSE_DXR): $(NATIVE_KPARSE_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	@echo "LINK $@"
	@$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KPARSE.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KPARSE_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KGEN_DXR): $(NATIVE_KGEN_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	@echo "LINK $@"
	@$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KGEN.map \
		$(NATIVE_LINK_RUNTIME) $(NATIVE_KGEN_OBJS) $(NATIVE_DAIMOS_LIBC)

$(NATIVE_KOPT_DXR): $(NATIVE_KOPT_OBJS) $(NATIVE_LINK_RUNTIME) $(NATIVE_DAIMOS_LIBC)
	@echo "LINK $@"
	@$(PDP10_DLINK) --daimos-uuo-relax -b 020 -o $@ -M $(NATIVE_BUILD_DIR)/KOPT.map \
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
