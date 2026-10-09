# Native source dependencies; shared recipes vary only by phase.

# PLAIN assembly variant.
$(NATIVE_BUILD_DIR)/cc.s $(NATIVE_BUILD_DIR)/ccasmb.s $(NATIVE_BUILD_DIR)/cccreg.s $(NATIVE_BUILD_DIR)/cccse.s $(NATIVE_BUILD_DIR)/cccode.s $(NATIVE_BUILD_DIR)/ccdata.s $(NATIVE_BUILD_DIR)/ccdbug.s $(NATIVE_BUILD_DIR)/ccdecl.s $(NATIVE_BUILD_DIR)/ccerr.s $(NATIVE_BUILD_DIR)/cceval.s $(NATIVE_BUILD_DIR)/ccgen.s $(NATIVE_BUILD_DIR)/ccgen1.s $(NATIVE_BUILD_DIR)/ccgen2.s $(NATIVE_BUILD_DIR)/ccgswi.s $(NATIVE_BUILD_DIR)/ccjskp.s $(NATIVE_BUILD_DIR)/cclex.s $(NATIVE_BUILD_DIR)/ccnode.s $(NATIVE_BUILD_DIR)/ccout.s $(NATIVE_BUILD_DIR)/ccoututil.s $(NATIVE_BUILD_DIR)/ccpp.s $(NATIVE_BUILD_DIR)/ccsrc.s $(NATIVE_BUILD_DIR)/ccreg.s $(NATIVE_BUILD_DIR)/ccstmt.s $(NATIVE_BUILD_DIR)/ccsym.s $(NATIVE_BUILD_DIR)/cctype.s $(NATIVE_BUILD_DIR)/ccopt.s $(NATIVE_BUILD_DIR)/ccvla.s $(NATIVE_BUILD_DIR)/cckirread.s $(NATIVE_BUILD_DIR)/cckirwrite.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    $(COMPILE_NATIVE) -S "$$name.c" -o "$@"

# CPP assembly variant.
$(NATIVE_BUILD_DIR)/ccppout.s $(NATIVE_BUILD_DIR)/cc-cpp.s $(NATIVE_BUILD_DIR)/ccout-cpp.s $(NATIVE_BUILD_DIR)/ccerr-cpp.s $(NATIVE_BUILD_DIR)/ccsym-cpp.s $(NATIVE_BUILD_DIR)/ccdata-cpp.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-cpp}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_CPP=1 -S "$$name.c" -o "$@"

# CORE assembly variant.
$(NATIVE_BUILD_DIR)/ccppin.s $(NATIVE_BUILD_DIR)/ccdata-core.s $(NATIVE_BUILD_DIR)/ccerr-core.s $(NATIVE_BUILD_DIR)/ccout-core.s $(NATIVE_BUILD_DIR)/ccgen-core.s $(NATIVE_BUILD_DIR)/cc-core.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-core}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_CORE=1 -S "$$name.c" -o "$@"

# GEN assembly variant.
$(NATIVE_BUILD_DIR)/cckgen-gen.s $(NATIVE_BUILD_DIR)/ccdata-gen.s $(NATIVE_BUILD_DIR)/cccode-gen.s $(NATIVE_BUILD_DIR)/ccnode-gen.s $(NATIVE_BUILD_DIR)/ccsym-gen.s $(NATIVE_BUILD_DIR)/ccerr-gen.s $(NATIVE_BUILD_DIR)/ccevalgen.s $(NATIVE_BUILD_DIR)/cctype-gen.s $(NATIVE_BUILD_DIR)/ccgen1-gen.s $(NATIVE_BUILD_DIR)/ccgen-gen.s $(NATIVE_BUILD_DIR)/ccppin-gen.s $(NATIVE_BUILD_DIR)/cckpout-gen.s $(NATIVE_BUILD_DIR)/cckpwrite-gen.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-gen}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_GEN=1 -S "$$name.c" -o "$@"

# PARSE assembly variant.
$(NATIVE_BUILD_DIR)/cc-parse.s $(NATIVE_BUILD_DIR)/ccerr-parse.s $(NATIVE_BUILD_DIR)/ccdata-parse.s $(NATIVE_BUILD_DIR)/ccbind-parse.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-parse}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_PARSE=1 -S "$$name.c" -o "$@"

# OPT assembly variant.
$(NATIVE_BUILD_DIR)/cckopt-opt.s $(NATIVE_BUILD_DIR)/ccdata-opt.s $(NATIVE_BUILD_DIR)/ccout-opt.s $(NATIVE_BUILD_DIR)/ccerr-opt.s $(NATIVE_BUILD_DIR)/cckpread-opt.s: $(KCC)
	@mkdir -p $(NATIVE_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    name=$${name%-opt}; \
	    $(COMPILE_NATIVE) -DKCC_PHASE_OPT=1 -S "$$name.c" -o "$@"

# Per-target source prerequisites, including phase variants.
$(NATIVE_BUILD_DIR)/cc.s: cc.c
$(NATIVE_BUILD_DIR)/ccasmb.s: ccasmb.c
$(NATIVE_BUILD_DIR)/cccreg.s: cccreg.c
$(NATIVE_BUILD_DIR)/cccse.s: cccse.c
$(NATIVE_BUILD_DIR)/cccode.s: cccode.c
$(NATIVE_BUILD_DIR)/ccdata.s: ccdata.c
$(NATIVE_BUILD_DIR)/ccdbug.s: ccdbug.c
$(NATIVE_BUILD_DIR)/ccdecl.s: ccdecl.c
$(NATIVE_BUILD_DIR)/ccerr.s: ccerr.c
$(NATIVE_BUILD_DIR)/cceval.s: cceval.c
$(NATIVE_BUILD_DIR)/ccgen.s: ccgen.c
$(NATIVE_BUILD_DIR)/ccgen1.s: ccgen1.c
$(NATIVE_BUILD_DIR)/ccgen2.s: ccgen2.c
$(NATIVE_BUILD_DIR)/ccgswi.s: ccgswi.c
$(NATIVE_BUILD_DIR)/ccjskp.s: ccjskp.c
$(NATIVE_BUILD_DIR)/cclex.s: cclex.c
$(NATIVE_BUILD_DIR)/ccnode.s: ccnode.c
$(NATIVE_BUILD_DIR)/ccout.s: ccout.c
$(NATIVE_BUILD_DIR)/ccoututil.s: ccoututil.c
$(NATIVE_BUILD_DIR)/ccpp.s: ccpp.c
$(NATIVE_BUILD_DIR)/ccsrc.s: ccsrc.c
$(NATIVE_BUILD_DIR)/ccreg.s: ccreg.c
$(NATIVE_BUILD_DIR)/ccstmt.s: ccstmt.c
$(NATIVE_BUILD_DIR)/ccsym.s: ccsym.c
$(NATIVE_BUILD_DIR)/cctype.s: cctype.c
$(NATIVE_BUILD_DIR)/ccopt.s: ccopt.c
$(NATIVE_BUILD_DIR)/ccvla.s: ccvla.c
$(NATIVE_BUILD_DIR)/cckirread.s: cckirread.c
$(NATIVE_BUILD_DIR)/cckirwrite.s: cckirwrite.c
$(NATIVE_BUILD_DIR)/ccppout.s: ccppout.c
$(NATIVE_BUILD_DIR)/cc-cpp.s: cc.c
$(NATIVE_BUILD_DIR)/ccout-cpp.s: ccout.c
$(NATIVE_BUILD_DIR)/ccerr-cpp.s: ccerr.c
$(NATIVE_BUILD_DIR)/ccsym-cpp.s: ccsym.c
$(NATIVE_BUILD_DIR)/ccdata-cpp.s: ccdata.c
$(NATIVE_BUILD_DIR)/ccppin.s: ccppin.c
$(NATIVE_BUILD_DIR)/ccdata-core.s: ccdata.c
$(NATIVE_BUILD_DIR)/ccerr-core.s: ccerr.c
$(NATIVE_BUILD_DIR)/ccout-core.s: ccout.c
$(NATIVE_BUILD_DIR)/ccgen-core.s: ccgen.c
$(NATIVE_BUILD_DIR)/cc-core.s: cc.c
$(NATIVE_BUILD_DIR)/cckgen-gen.s: cckgen.c
$(NATIVE_BUILD_DIR)/ccdata-gen.s: ccdata.c
$(NATIVE_BUILD_DIR)/cccode-gen.s: cccode.c
$(NATIVE_BUILD_DIR)/ccnode-gen.s: ccnode.c
$(NATIVE_BUILD_DIR)/ccsym-gen.s: ccsym.c
$(NATIVE_BUILD_DIR)/ccerr-gen.s: ccerr.c
$(NATIVE_BUILD_DIR)/ccevalgen.s: ccevalgen.c
$(NATIVE_BUILD_DIR)/cctype-gen.s: cctype.c
$(NATIVE_BUILD_DIR)/ccgen1-gen.s: ccgen1.c
$(NATIVE_BUILD_DIR)/ccgen-gen.s: ccgen.c
$(NATIVE_BUILD_DIR)/ccppin-gen.s: ccppin.c
$(NATIVE_BUILD_DIR)/cckpout-gen.s: cckpout.c
$(NATIVE_BUILD_DIR)/cckpwrite-gen.s: cckpwrite.c
$(NATIVE_BUILD_DIR)/cc-parse.s: cc.c
$(NATIVE_BUILD_DIR)/ccerr-parse.s: ccerr.c
$(NATIVE_BUILD_DIR)/ccdata-parse.s: ccdata.c
$(NATIVE_BUILD_DIR)/ccbind-parse.s: ccbind.c
$(NATIVE_BUILD_DIR)/cckopt-opt.s: cckopt.c
$(NATIVE_BUILD_DIR)/ccdata-opt.s: ccdata.c
$(NATIVE_BUILD_DIR)/ccout-opt.s: ccout.c
$(NATIVE_BUILD_DIR)/ccerr-opt.s: ccerr.c
$(NATIVE_BUILD_DIR)/cckpread-opt.s: cckpread.c
