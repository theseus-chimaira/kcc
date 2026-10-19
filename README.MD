# KCC

KCC IS THE PDP-10 C COMPILER USED BY THE DAIMOS TOOLCHAIN.

THE SEPARATELY INSTALLED FREESTANDING `libp10c-v2.a` PROVIDES 9-BIT
STRING SEARCHES `strstr`, `strpbrk`, AND `strspn` AS INDEPENDENT
PDP-6-COMPATIBLE ASSEMBLER MEMBERS. THESE ROUTINES REQUIRE NO OS
SERVICES, STACK WORDS, OR WRITABLE LIBRARY DATA. LINK ONLY THE MEMBERS
YOUR PROGRAM USES. THE MATCHING GCC LIBRARY IS BUILT FROM IDENTICAL
ASSEMBLER SOURCES.

THE COMPILER TARGETS THE PDP-6, KA10, KI10, KL10, AND KS10 FAMILIES AND
SUPPORTS TARGET-SPECIFIC CODE GENERATION NEEDED BY THE SUPPORTED PDP-10
ENVIRONMENTS.

THE PROJECT FAVORS CORRECT CODE GENERATION, SMALL COMPILER AND RUN-TIME MEMORY
USE, AND COMPATIBILITY WITH THE COMMON PDP-10 C ABI.

## TREE

COMPILER SOURCES ARE IN THE PROJECT ROOT.

RUNTIME CONTAINS TARGET-SPECIFIC ASSEMBLY HELPERS USED BY GENERATED CODE.

LICENSE CONTAINS THE PROJECT LICENSE.

README.MD DESCRIBES THE PROJECT.

TODO.MD CONTAINS OPEN WORK ONLY.

STATUS.MD IS THE SOLE DOCUMENT FOR CURRENT IMPLEMENTATION STATE.

COMPILER REGRESSION TESTS BELONG IN PDP-C-TESTKIT, NOT IN THIS REPOSITORY.

## BUILD

THE HOST BUILD REQUIRES A C99 COMPILER.

DAIMOS_REPO DEFAULTS TO $(HOME)/git/DAIMOS AND CAN BE OVERRIDDEN WITH AN
ABSOLUTE PATH (FOR EXAMPLE DAIMOS_REPO=/SRV/SRC/DAIMOS). KCC NO LONGER
ASSUMES THAT DAIMOS IS A SIBLING CHECKOUT.

BUILD KCC WITH GNU MAKE OR BSD BMAKE:

    MAKE

THE SINGLE MAKEFILE SUPPORTS GNU MAKE AND BSD MAKE. IT USES AN
INSTALLED HOST MAKEDEPEND PROGRAM (FOUND THROUGH PATH, OR OVERRIDDEN
WITH MAKEDEPEND=/PATH/TO/MAKEDEPEND) TO GENERATE BUILD/DEPEND.MK.
MAKEDEPEND IS INSTALLED INDEPENDENTLY OF KCC.

THE HOST COMPILER AND OBJECTS ARE WRITTEN TO BUILD/KCC AND BUILD/*.O.
THE NATIVE BOOTSTRAP AND PHASE EXECUTABLES ARE WRITTEN TO BUILD-NATIVE/.
ALL GENERATED FILENAMES OMIT HISTORICAL -V1 SUFFIXES.

BUILD THE DAIMOS NATIVE BOOTSTRAP WITH:

    MAKE NATIVE PDP10_PREFIX=/PATH

THE DAIMOS LIBC SUBBUILD USES THE SAME MAKE IMPLEMENTATION AS KCC.
GNU MAKE AND BSD MAKE ARE BOTH SUPPORTED THROUGH THE NATIVE TARGET.

BUILD PDP-10 ASSEMBLY FROM THE COMPILER SOURCES WITH:

    MAKE ASM

INSTALL HOST KCC, ITS RUNTIME SUPPORT, AND TARGET HEADERS UNDER PREFIX WITH:

    MAKE INSTALL PREFIX=/PATH

PDP10_PREFIX SELECTS THE CROSS-TOOLCHAIN (DAS, DLINK, AND TARGET
HEADERS) AND DEFAULTS TO $(HOME)/GIT/LOCAL. IT DOES NOT CONTROL
WHERE HOST KCC OR ITS RUNTIME FILES ARE INSTALLED.
THE DEFAULT HOST RUNTIME DIRECTORY REMAINS /USR/LOCAL/LIB/KCC, AS
CURRENTLY REFERENCED BY CCSITE.H.

TARGET HEADERS, INCLUDING STDARG.H, ARE INSTALLED UNDER
`$(KCCLIBDIR)/include` (BY DEFAULT `/usr/local/lib/kcc/include`).
FOR A NONDEFAULT PREFIX, USE THE INSTALLED INCLUDE DIRECTORY WITH
`-I/path/lib/kcc/include`; PDP10-TESTKIT FINDS IT FROM PDP10_PREFIX
OR AN EXPLICIT KCC_INCLUDE_DIR OVERRIDE.

KCC SUPPORTS GNU `#include_next <header>` AND `#include_next "header"`.
SEARCH CONTINUES AFTER THE DIRECTORY WHICH PROVIDED THE CURRENT HEADER,
AND EXCLUDED DIRECTIVES HAVE NO EFFECT. THE HEADER PREPROCESSOR SUPPORT
DOES NOT CHANGE THE ABI OF C VARIADIC CALLS.

## FREESTANDING C LIBRARY

THE `INSTALL` TARGET ALSO INSTALLS OS-INDEPENDENT PDP-6 STRING/MEMORY
ROUTINES UNDER `$(KCCLIBDIR)/freestanding`, INCLUDING `libp10c-v2.a`.
THE ARCHIVE PROVIDES THE STANDARD 9-BIT C-BYTE OPERATIONS WITHOUT ANY
DAIMOS SYSTEM CALLS. IT IS SEPARATE FROM KCC'S COMPILER RUNTIME HELPERS.

`<p10bytes-v2.h>` ADDS A TARGET-SPECIFIC RAW BYTE-POINTER INTERFACE:
`p10bcopy_v2`, `p10bfill_v2`, AND `p10bcmp_v2`. THEY ACCEPT RAW PDP-10
ONE-WORD POINTER DESCRIPTORS, NOT INTERCHANGEABLE C `char *` POINTERS.
THEY SUPPORT BYTE SIZES 1 THROUGH 36 VIA `LDB`/`DPB`/`IBP`, WITH
WHOLE-WORD PATHS FOR MATCHING ALIGNED 6-, 9-, 18-, AND 36-BIT POINTERS.
THE CALLING CONVENTION USES AC1–AC3; SCRATCH REGISTERS ARE CALL-CLOBBERED.

THE ARCHIVE MUST BE LINKED EXPLICITLY WHERE A FREESTANDING PROGRAM
REQUIRES IT; IT IS NOT AUTOMATICALLY ADDED TO ALL TARGET EXECUTABLES.

## TARGET SELECTION

KCC SUPPORTS PDP-6, KA10, KI10, KL10, AND KS10 TARGET SELECTION.

THE PDP-6 TARGET IS THE PORTABLE BASE FOR DAIMOS CODE THAT MUST RUN ON THE
EARLIEST SUPPORTED PROCESSOR.

KL10 NON-ZERO-SECTION CODE REQUIRES A COMPILER BUILT WITH MULTI_SECTION
SUPPORT.  THE NORMAL BUILD DOES NOT SILENTLY SUBSTITUTE SECTION-ZERO CODE FOR
THAT TARGET.

## ABI

THE DAIMOS PDP-10 C ABI USES FOUR ARGUMENT REGISTERS AND FOUR RETURN REGISTERS.
AC0 THROUGH AC7 ARE CALL-CLOBBERED.

THE FIRST FOUR ARGUMENT WORDS USE AC1 THROUGH AC4.  ADDITIONAL ARGUMENT WORDS
USE THE STACK.  MULTIWORD ARGUMENTS CONSUME CONSECUTIVE ARGUMENT POSITIONS.

SCALAR RESULTS USE AC1.  MULTIWORD AND AGGREGATE RESULTS FOLLOW THE COMMON
PDP-10 ABI RULES USED BY DAIMOS C TOOLCHAINS.

THE DAIMOS PROCESS ENTRY CONTRACT SUPPLIES ARGC IN AC1 AND ARGV IN AC2.

OLD STACK-ONLY KCC OBJECTS ARE NOT ABI-COMPATIBLE WITH THE CURRENT REGISTER
ABI.  REBUILD C OBJECTS AND C-CALLABLE ASSEMBLY WHEN MOVING OLD SOFTWARE TO
THE CURRENT ABI.

## POLICY

VALID SOURCE THAT EXPOSES A COMPILER OR CODE-GENERATOR BUG MUST BE FIXED IN
KCC.  DO NOT HIDE COMPILER DEFECTS WITH SOURCE-SIDE WORKAROUNDS.

PREFER, IN ORDER:

1. CORRECTNESS.
2. LOWER COMPILER AND RUN-TIME RAM USE.
3. EXECUTION SPEED.
4. SMALLER DISK SIZE.

AVOID FEATURE CREEP AND TARGET-SPECIFIC COMPLEXITY WITHOUT A DEMONSTRATED NEED.

## Shared KCC Makefiles (2026-10-09)

`Makefile` selects the platform. `cross.mk` provides the host cross-compiler
entry point and includes `common.mk` and `native.mk` for shared inventories,
dependency scanning, and DAIMOS bootstrap rules. `daimos.mk` contains the on-target incremental
build, with each source and its transitive local headers as prerequisites.
Do not restore the old DAIMOS Python Makefile generator or separate generated
phase Makefiles. Edit the KCC build description in this repository instead.

On Unix:

    make cross
    make native
    make install-native

For DAIMOS, stage the common `Makefile` and `daimos.mk` using DAIMOS's
`scripts/stage-kcc-sources-v1.sh`. Staging changes `PLATFORM ?= cross` into
`PLATFORM ?= daimos`, uppercases file names and text, converts Unix recipe
prefixes into DAIMOS `>`/direct-RUN prefixes and encodes the result as S6REC.
The resulting `/OPTION/SOURCE/KCC/MAKEFILE` includes `DAIMOS.MK`. Native
`MAKE ALL` rebuilds the four phase DXRs and driver; `MAKE INSTALL` installs
them. BOOT runtime objects and libc archive must already be staged. Archive
chunks bound DLINK argument count. Run full native KCC compilation manually;
this refactor has only received host parsing and staging preflight tests.

The on-target graph is intentionally explicit. Changes to phase module lists
must be reflected in `daimos.mk`'s object, archive and link rules. The
`daimos-kcc-make-split-20261009-v1` test in daimos-testkit checks coverage
and local header dependencies so missing object/header rules are detected.

## Native MAKE graph preflight (2026-10-09)

Native `MAKE` direct-execution recipes now resolve bare tool names using the
environment's `PATH` (just as `DSH` does). `daimos.mk` assumes `KCC`, `DAS`,
`DARC`, `DLINK`, `INSTALL`, and `RM` can be found on `PATH`. A missing tool
causes the recipe to fail; absolute executable paths still work. Destination
paths for installed KCC programs remain absolute, independently of the
command names. This retains the direct-run argument capacity needed by the
native compiler's archive and linker recipes.

Native `daimos.mk` defines short variables for the executive directory,
KCC, DAS, DARC, DLINK, INSTALL, and the native KCC installation directory.
The commands remain byte-for-byte identical after variable expansion; the
longest expanded recipe is 174 characters, below native MAKE's 256-character
limit. Changing an executable location requires updating its variable rather
than every compilation or installation recipe. The corresponding regression
test is in `daimos-testkit`.

Native KCC artifacts reside in `BUILD/`, rather than `B/`. Native MAKE's
`.S.DOBJ` suffix rule assembles ordinary and phase-specific `.S` files,
replacing 53 repeated assembler rules. `.C` to `.S` transformations remain
explicit because phase variants require different compiler options. Archive
and link dependencies remain explicit to preserve membership and ordering.
`MAKE CLEAN` uses the system C implementation of `RM -R -F BUILD` to delete
the generated directory recursively, rather than a file-by-file inventory.
The separate optional assembler RM is no longer built or installed.

The on-target `daimos.mk` shares one `KCC_NATIVE_FLAGS` definition across
53 compilation recipes. KCC, DARC, and DLINK use the supported `$@` target
variable for output names. This reduces file size without changing phase
membership, prerequisites, flags, object order, or expanded commands.

The archive rules also use `$^` for the complete ordered object-prerequisite
list. GNU Make and BSD Make both support this automatic variable, and DAIMOS
native MAKE now implements it. DLINK retains explicit object/archive order
because archive resolution needs startup objects first. Cleanup pairs combine
the matching `.S` and `.DOBJ` paths into one `RM` invocation. The native MAKE
implementation retains its 256-character recipe limit; no global limit was
increased. The host archive-membership regression lives in `daimos-testkit`.
All 77 changed recipes match the preceding version after expansion;
the longest expanded KCC recipe is 188 characters, below native MAKE's
256-character expansion limit. The corresponding host regression is in
`daimos-testkit`. An actual SIMH native build is still required to verify
execution with the current native MAKE binary.

The initial on-target graph failed to parse because long cumulative `+=`
assignments exceeded MAKE's bounded 256-character variable value buffer.
Host/cross-only module inventories now reside in `common.mk`; the shared root
`Makefile` is a small platform selector. The full explicit incremental
native graph is in `daimos.mk`, with local header prerequisites and phased
archives. The host testkit verifies coverage against the Unix inventory.
DAIMOS SIMH has passed native `MAKE -Q` (default and ALL), first-object
`MAKE -N`, and `MAKE CLEAN`. A real CC-CPP.S build was attempted but timed
out after reaching KCC and emitting `KCC000006`; native compilation and
incremental rebuild of actual outputs remain pending acceptance gates.
