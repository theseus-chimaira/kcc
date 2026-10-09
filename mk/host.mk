# Explicit dependencies with common GNU/BSD make recipes.

$(OBJS):
	@mkdir -p $(HOST_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.o}; \
	    $(CC) $(CFLAGS) -c "$$name.c" -o "$@"

$(ASMS): $(HOST_BUILD_DIR)/kcc
	@mkdir -p $(HOST_BUILD_DIR)
	@name="$@"; name=$${name##*/}; name=$${name%.s}; \
	    $(KCC) $(KCC_SELF_FLAGS) -S "$$name.c" -o "$@"

# Source prerequisites remain explicit for correct incremental rebuilds.
$(HOST_BUILD_DIR)/cc.o: cc.c
$(HOST_BUILD_DIR)/cc.s: cc.c
$(HOST_BUILD_DIR)/ccasmb.o: ccasmb.c
$(HOST_BUILD_DIR)/ccasmb.s: ccasmb.c
$(HOST_BUILD_DIR)/cccreg.o: cccreg.c
$(HOST_BUILD_DIR)/cccreg.s: cccreg.c
$(HOST_BUILD_DIR)/cccse.o: cccse.c
$(HOST_BUILD_DIR)/cccse.s: cccse.c
$(HOST_BUILD_DIR)/cccode.o: cccode.c
$(HOST_BUILD_DIR)/cccode.s: cccode.c
$(HOST_BUILD_DIR)/ccdata.o: ccdata.c
$(HOST_BUILD_DIR)/ccdata.s: ccdata.c
$(HOST_BUILD_DIR)/ccdbug.o: ccdbug.c
$(HOST_BUILD_DIR)/ccdbug.s: ccdbug.c
$(HOST_BUILD_DIR)/ccdecl.o: ccdecl.c
$(HOST_BUILD_DIR)/ccdecl.s: ccdecl.c
$(HOST_BUILD_DIR)/ccerr.o: ccerr.c
$(HOST_BUILD_DIR)/ccerr.s: ccerr.c
$(HOST_BUILD_DIR)/cceval.o: cceval.c
$(HOST_BUILD_DIR)/cceval.s: cceval.c
$(HOST_BUILD_DIR)/ccgen.o: ccgen.c
$(HOST_BUILD_DIR)/ccgen.s: ccgen.c
$(HOST_BUILD_DIR)/ccgen1.o: ccgen1.c
$(HOST_BUILD_DIR)/ccgen1.s: ccgen1.c
$(HOST_BUILD_DIR)/ccgen2.o: ccgen2.c
$(HOST_BUILD_DIR)/ccgen2.s: ccgen2.c
$(HOST_BUILD_DIR)/ccgswi.o: ccgswi.c
$(HOST_BUILD_DIR)/ccgswi.s: ccgswi.c
$(HOST_BUILD_DIR)/ccjskp.o: ccjskp.c
$(HOST_BUILD_DIR)/ccjskp.s: ccjskp.c
$(HOST_BUILD_DIR)/cclex.o: cclex.c
$(HOST_BUILD_DIR)/cclex.s: cclex.c
$(HOST_BUILD_DIR)/ccnode.o: ccnode.c
$(HOST_BUILD_DIR)/ccnode.s: ccnode.c
$(HOST_BUILD_DIR)/ccout.o: ccout.c
$(HOST_BUILD_DIR)/ccout.s: ccout.c
$(HOST_BUILD_DIR)/ccoututil.o: ccoututil.c
$(HOST_BUILD_DIR)/ccoututil.s: ccoututil.c
$(HOST_BUILD_DIR)/ccpp.o: ccpp.c
$(HOST_BUILD_DIR)/ccpp.s: ccpp.c
$(HOST_BUILD_DIR)/ccsrc.o: ccsrc.c
$(HOST_BUILD_DIR)/ccsrc.s: ccsrc.c
$(HOST_BUILD_DIR)/ccreg.o: ccreg.c
$(HOST_BUILD_DIR)/ccreg.s: ccreg.c
$(HOST_BUILD_DIR)/ccstmt.o: ccstmt.c
$(HOST_BUILD_DIR)/ccstmt.s: ccstmt.c
$(HOST_BUILD_DIR)/ccsym.o: ccsym.c
$(HOST_BUILD_DIR)/ccsym.s: ccsym.c
$(HOST_BUILD_DIR)/cctype.o: cctype.c
$(HOST_BUILD_DIR)/cctype.s: cctype.c
$(HOST_BUILD_DIR)/ccopt.o: ccopt.c
$(HOST_BUILD_DIR)/ccopt.s: ccopt.c
$(HOST_BUILD_DIR)/ccvla.o: ccvla.c
$(HOST_BUILD_DIR)/ccvla.s: ccvla.c
