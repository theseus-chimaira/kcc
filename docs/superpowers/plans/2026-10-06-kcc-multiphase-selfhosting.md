# KCC Multiphase Self-Hosting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Split native DAIMOS KCC into KCPP, KPARSE, KGEN, and KOPT processes while preserving byte-identical PDP-6 DAS/GAS assembly and driving every compiler phase toward a 96K-word total working-set target.

**Architecture:** Keep the existing compiler algorithms authoritative and introduce two pointer-free, versioned stream boundaries around them. First split final PCODE/output into `KGEN -> KPCODE1 -> KOPT`; after that is byte-identical and measured, split the existing `extdef() -> gencode()` boundary into `KPARSE -> KIR1 -> KGEN`. Migrate only optimizer passes whose complete semantic input is present in KPCODE1, one group at a time.

**Tech Stack:** C99 KCC sources, PDP-6 KCC backend, DAS/DOBJ, POSIX shell regression scripts in `pdp10-testkit`, versioned word-oriented intermediate files.

**Spec:** `docs/superpowers/specs/2026-10-06-kcc-multiphase-selfhosting-design.md`

## Global Constraints

- All new C code is C99 and must compile with both the host C99 compiler and KCC.
- Generated PDP-6 DAS/GAS assembly must remain byte-for-byte identical to the integrated compiler after every accepted phase split or optimizer migration.
- KCPP remains the existing KPT4 phase and is not redesigned.
- Do not serialize process pointers or raw C structure layouts.
- KIR1 and KPCODE1 are versioned, pointer-free, word-oriented formats with explicit magic/version rejection.
- Do not split inside live expression generation or VREG allocation.
- DAS and DLINK remain separate external programs.
- Correctness outranks RAM reduction; RAM outranks execution speed; disk size is secondary.
- Target each native compiler phase at or below 96K PDP-10 words total working set once linked/runnable; before native linking, report text/data/BSS/object totals separately.
- Use `$TMPDIR`, versionized names, and cumulative shar archives. Every shar ends with a git commit using `Theseus <theseus@chimaira.net>`.
- Tests belong in `pdp10-testkit`; `STATUS.MD` is current state and `TODO.MD` contains open work only.

## Review Focus

- A KPCODE1 reader must reject wrong magic/version and truncated records rather than emitting partial assembly; Task 2 adds malformed-stream tests.
- Symbol IDs must preserve first-observed order and distinguish same-spelling local/static identities; Tasks 2 and 6 add duplicate-name identity tests.
- KIR1 must preserve TYPE sharing while keeping identity-sensitive VLA TYPE objects distinct; Task 6 adds shared-type/VLA identity tests.
- Module-final output state (`outdone()`, tentative definitions, runtime helper references, segment state) must survive both boundaries in original order; Tasks 3 and 7 add module-final parity cases.
- `-O` and `-n` must both preserve integrated-vs-split byte identity, so optimizer migration cannot accidentally become mandatory for correctness; Tasks 1, 4, and 5 exercise both modes.

---

### Task 1: Permanent 24-TU phase byte-identity regression

**Files:**
- Create: `../pdp10-testkit/tests/kcc-regression/kcc-phase-byte-identity-20261006-v1.sh`
- Modify: `../pdp10-testkit/Makefile` or the existing KCC regression manifest/runner entry that enumerates KCC tests

**Interfaces:**
- Consumes: `KCC_SOURCE` pointing at the KCC checkout; `PDP10_PREFIX` for DAS and installed support tools; `HOSTCC` defaulting to `cc`.
- Produces: one regression command that builds the integrated DAIMOS host surrogate plus available split phase surrogates and compares all 24 KCC source outputs byte-for-byte under both optimized and `-n` compilation.

- [ ] **Step 1: Write the phase byte-identity regression script**

The script must:

```sh
: "${TMPDIR:?TMPDIR must be set}"
: "${KCC_SOURCE:?KCC_SOURCE must point to the KCC tree}"
: "${PDP10_PREFIX:?PDP10_PREFIX must be set}"
HOSTCC=${HOSTCC:-cc}
```

Build a versionized integrated DAIMOS compiler surrogate from current KCC sources, build each split phase executable that exists, compile the 24 `SRCS` translation units with `-Pgnu99 -x=pdp6 -m=gas -DHOST_DAIMOS=1 -DHOST_UNIX=0 -Iself/include/ -Hself/include/`, and use `cmp` on assembly outputs. Repeat with `-n` replacing `-O`. Fail on the first mismatch and print the source file and phase path.

- [ ] **Step 2: Run the regression before adding new phases**

Run from `pdp10-testkit`:

```sh
TMPDIR="$HOME/tmp" KCC_SOURCE="$HOME/git/kcc" PDP10_PREFIX="$HOME/cross" \
  sh tests/kcc-regression/kcc-phase-byte-identity-20261006-v1.sh
```

Expected: PASS for integrated versus existing `KCPP -> KPT4 -> KCC1`; new KGEN/KOPT/KPARSE comparisons are skipped only because their executables do not yet exist.

- [ ] **Step 3: Register the test in the normal KCC regression runner**

Use the existing testkit registration style; do not add a KCC-local duplicate test.

- [ ] **Step 4: Run the registered regression**

Expected: PASS in both `-O` and `-n` modes.

- [ ] **Step 5: Commit testkit checkpoint and create cumulative shar**

Commit message:

```text
tests: gate KCC phase byte identity
```

Shar name: `$TMPDIR/pdp10-testkit-kcc-phase-identity-20261006-v1.shar`.

---

### Task 2: KPCODE1 format primitives and malformed-stream tests

**Files:**
- Create: `cckpcode.h`
- Create: `cckpwrite.c`
- Create: `cckpread.c`
- Modify: `c-env.h`
- Test: `../pdp10-testkit/tests/kcc-regression/kcc-kpcode-format-20261006-v1.sh`

**Interfaces:**
- Produces: `int kpcode_write_header(FILE *fp);`, `int kpcode_write_pcode(FILE *fp, const PCODE *p);`, `int kpcode_write_symbol(FILE *fp, unsigned int id, const SYMBOL *s);`, `int kpcode_write_record(FILE *fp, unsigned int kind, const INT *words, unsigned int nwords);`, `int kpcode_read_header(FILE *fp);`, `int kpcode_read_record(FILE *fp, unsigned int *kind, INT *words, unsigned int cap, unsigned int *nwords);`.
- Produces phase macros: `KCC_PHASE_GEN` and `KCC_PHASE_OPT`; exactly one of CPP/CORE/GEN/OPT may be nonzero at this stage. Task 6 adds PARSE to the same exclusivity check when KIR1 is introduced.

- [ ] **Step 1: Add failing KPCODE1 format tests**

The shell regression builds a small host test program against `cckpwrite.c`/`cckpread.c` and asserts:

- a valid header round-trips;
- wrong magic fails;
- version `2` fails against the KPCODE1 reader;
- truncated record fails;
- a 36-bit negative/positive value round-trips without host-width loss;
- two symbol records with the same spelling but different IDs remain distinct.

- [ ] **Step 2: Run the format test and verify failure**

Expected: FAIL because `cckpcode.h`, writer, and reader do not exist.

- [ ] **Step 3: Define KPCODE1 constants and record kinds in `cckpcode.h`**

Use a fixed magic/version and explicit record kinds for at least:

```c
KPCODE_REC_MODULE_BEGIN
KPCODE_REC_SYMBOL
KPCODE_REC_PCODE
KPCODE_REC_LABEL
KPCODE_REC_SEGMENT
KPCODE_REC_TEXT
KPCODE_REC_MODULE_END
```

IDs start at 1; ID 0 means null/no symbol. Store target words as masked 36-bit values, never host pointers.

- [ ] **Step 4: Implement writer and reader primitives**

Keep the routines streaming and bounded. Reject payload lengths larger than the fixed caller-provided buffer rather than allocating an unbounded record buffer.

- [ ] **Step 5: Run KPCODE1 format regression**

Expected: PASS.

- [ ] **Step 6: Build ordinary host KCC and native phase objects**

```sh
make -j1 kcc
make -j1 native-phase-objects KCC="$PWD/kcc" PDP10_PREFIX="$HOME/cross"
```

Expected: PASS; no existing phase output changes yet.

- [ ] **Step 7: Commit KPCODE1 primitives and cumulative shar**

Commit message: `native: add KPCODE1 stream primitives`.

---

### Task 3: KGEN/KOPT stage A at the existing CCOUT boundary

**Files:**
- Create: `cckpout.c`
- Create: `cckpin.c`
- Modify: `cc.c`
- Modify: `ccout.c`
- Modify: `cccode.c`
- Modify: `ccdata.c`
- Modify: `ccerr.c`
- Modify: `Makefile`
- Test: `../pdp10-testkit/tests/kcc-regression/kcc-phase-byte-identity-20261006-v1.sh`

**Interfaces:**
- `KGEN` consumes KPT4 and produces KPCODE1.
- `KOPT` consumes KPCODE1 and produces assembly.
- `cckpout.c` implements the CCOUT entry points used by KGEN as KPCODE1 event writers, including `realcode(PCODE *)`, `outlab(SYMBOL *)`, segment transitions, declarations/text/literal output, and module begin/end state.
- `cckpin.c` replays KPCODE1 records through the unmodified real CCOUT implementation in original record order.

- [ ] **Step 1: Extend the permanent identity test to require `kgen-v1` and `kopt-v1` when present**

For each of the 24 TUs compare:

```text
integrated.s == KCPP|KCC1.s == KCPP|KGEN|KOPT.s
```

under `-O` and `-n`.

Also add synthetic module-final cases that exercise a tentative definition, code/data/BSS segment transitions, string/literal emission, and at least one runtime-helper reference so `outdone()` state is compared even when the 24 self-hosting files do not cover a particular record kind.

- [ ] **Step 2: Run the regression and verify it fails once empty phase stubs are exposed**

Expected: FAIL because KGEN/KOPT do not yet preserve output events.

- [ ] **Step 3: Add `KCC_PHASE_GEN` driver behavior in `cc.c`**

KGEN opens KPT4 exactly as current KCC1 does, calls the existing parser/code generator, but directs final-output entry points to KPCODE1. It must not invoke DAS or DLINK.

- [ ] **Step 4: Implement KGEN-side CCOUT event serialization in `cckpout.c`**

Do not serialize rendered assembler text as the primary representation. `realcode()` records PCODE plus stable symbol IDs; label/segment/declaration/literal calls get explicit record types. Text records are permitted only for output operations whose existing CCOUT contract is intrinsically raw text and have no lower-level structured representation.

- [ ] **Step 5: Implement KOPT replay in `cckpin.c` and KOPT driver path**

KOPT validates KPCODE1 target CPU/architecture/dialect flags, reconstructs minimal downstream SYMBOL objects by stable ID, and calls the existing CCOUT routines in stream order. No optimizer code moves in stage A.

- [ ] **Step 6: Add Makefile host and native phase targets**

Add versioned native object lists `NATIVE_KGEN_OBJS` and `NATIVE_KOPT_OBJS`; keep `native-kcc1-objects` until the new path passes the full gate.

- [ ] **Step 7: Run all 24 byte-identity tests**

Expected: exact PASS in `-O` and `-n`.

- [ ] **Step 8: Run existing targeted compiler regressions**

At minimum run `kcc-null-pointer-static-init-20261005-v1.sh`, `kcc-absolute-pointer-store-20261005-v1.sh`, and the normal KCC regression group.

- [ ] **Step 9: Commit stage-A boundary and cumulative shar**

Commit message: `native: split KGEN and KOPT output phase`.

---

### Task 4: Measure KGEN/KOPT and move final-output-only code

**Files:**
- Modify: `Makefile`
- Modify: `STATUS.MD`
- Modify: `TODO.MD`
- Modify only as measurement requires: `ccout.c`, `ccdata.c`, `ccerr.c`, `ccasmb.c`

**Interfaces:**
- Produces: reproducible text/data/BSS totals for KCPP, transitional KCC1, KGEN, and KOPT from DAS DOBJ headers.

- [ ] **Step 1: Clean-build all native phase objects**

Delete only `build-native-v1` through the project clean target or a fresh versionized build directory, then build all phase objects with KCC and DAS.

- [ ] **Step 2: Record exact per-phase object closures**

Report text, data, BSS, total, and octal total. Explicitly compare KGEN and KOPT to the 96K pre-link target, noting that libc/stack/heap are still absent.

- [ ] **Step 3: Remove CCOUT and assembler-driver objects from KGEN where the stage-A writer makes them unnecessary**

Only remove objects proven unused by symbol-closure inspection. Keep generic filename helpers if still referenced; do not introduce a workaround merely to drop `ccasmb`.

- [ ] **Step 4: Re-run the 24-TU byte gate and measure again**

Expected: byte-identical outputs, KGEN closure smaller than stage-A baseline.

- [ ] **Step 5: Update `STATUS.MD` and `TODO.MD` with measured values**

Do not claim the 96K runtime goal until linked image+BSS+stack+heap are measured.

- [ ] **Step 6: Commit measurement/closure checkpoint and cumulative shar**

Commit message: `native: trim KGEN output closure`.

---

### Task 5: Migrate pure PCODE optimizer groups to KOPT

**Files:**
- Modify: `cccode.c`
- Modify: `ccopt.c`
- Modify: `cccse.c`
- Modify: `ccjskp.c`
- Modify: `cccreg.c`
- Modify: `cckpout.c`
- Modify: `cckpin.c`
- Modify: `Makefile`
- Test: `../pdp10-testkit/tests/kcc-regression/kcc-phase-byte-identity-20261006-v1.sh`

**Interfaces:**
- Consumes: KPCODE1 carrying every field required by the migrated pass.
- Produces: smaller `NATIVE_KGEN_OBJS`, correspondingly larger KOPT closure, identical assembly.

- [ ] **Step 1: Move final jump/skip cleanup only**

Select functions whose complete inputs are PCODE, stable labels/symbol IDs, target flags, and optimizer flags. Move the KGEN serialization point upstream of this pass so KOPT receives the exact pre-pass PCODE block and runs the existing pass before CCOUT. Add any missing KPCODE1 metadata before moving the code.

- [ ] **Step 2: Run `-O`/`-n` byte gate and measure KGEN/KOPT**

Expected: PASS. Commit only if KGEN shrinks and no KOPT correctness dependency reaches back into live VREG/code-generation state.

- [ ] **Step 3: Move local PCODE peephole transforms**

Advance the serialization boundary upstream only across transforms that operate wholly on a completed bounded PCODE block. Keep any transform in KGEN if its result is consumed immediately by `ccgen1/ccgen2`, changes a VREG result, or affects subsequent instruction generation.

- [ ] **Step 4: Run byte gate and measure again**

Expected: PASS.

- [ ] **Step 5: Move PCODE CSE/rewrite helpers that are proven output-local**

Do not move `folddiv()` or other generation-time routines merely because their implementation resides in an optimizer object.

- [ ] **Step 6: Run byte gate and measure again**

Expected: PASS.

- [ ] **Step 7: Move remaining output-local `cccode` transforms if closure analysis proves them separable**

Stop migration when the next candidate requires live VREG/parser/codegen state; record the boundary instead of serializing that live state.

- [ ] **Step 8: Final PCODE migration verification**

Run normal KCC regressions plus the 24-TU `-O`/`-n` gate and record final KGEN/KOPT object totals.

- [ ] **Step 9: Commit each accepted optimizer group separately with cumulative shar**

Use versionized commit subjects such as `native: move final jump cleanup to KOPT`; each patch is based on the previous accepted checkpoint.

---

### Task 6: KIR1 stable-ID graph format

**Files:**
- Create: `cckir.h`
- Create: `cckirwrite.c`
- Create: `cckirread.c`
- Modify: `c-env.h`
- Test: `../pdp10-testkit/tests/kcc-regression/kcc-kir-format-20261006-v1.sh`

**Interfaces:**
- Produces: `int kir_write_header(FILE *fp);`, `int kir_write_extdef(FILE *fp, NODE *root);`, `int kir_write_tentative(FILE *fp, NODE *root);`, `int kir_write_module_end(FILE *fp, int mainflg);`, `int kir_read_header(FILE *fp);`, `int kir_read_next(FILE *fp, unsigned int *kind, NODE **root);`.
- Adds phase macro `KCC_PHASE_PARSE` and extends the phase exclusivity check to CPP/CORE/GEN/OPT/PARSE; `KCC_PHASE_GEN` is the KIR1 consumer once Task 7 is complete.

- [ ] **Step 1: Write failing KIR1 graph-format tests**

Tests must cover:

- wrong magic/version/truncation rejection;
- two NODE references to one TYPE reconstruct to the same TYPE pointer;
- two identity-distinct VLA TYPE objects with identical structural fields reconstruct as distinct pointers;
- two same-spelling SYMBOL objects with different stable IDs remain distinct;
- a cyclic/member-linked struct/union symbol graph reconstructs without pointer serialization;
- string/integer/double constant payloads round-trip.

- [ ] **Step 2: Run and verify failure**

Expected: FAIL because KIR1 support does not exist.

- [ ] **Step 3: Define KIR1 record schema and stable-ID maps**

Use monotonically increasing IDs in first-discovery/allocation traversal order. ID 0 is null. TYPE, SYMBOL, NODE, and separately allocated payloads have independent ID spaces.

- [ ] **Step 4: Implement graph writer with reachable-object discovery**

Serialize only fields consumed by downstream generation/output plus persistent global deltas required by later extdefs. Do not include preprocessor macro bodies.

- [ ] **Step 5: Implement graph reader in ID order**

Allocate all referenced objects before resolving links so cycles work without special cases. Reconstruct interned sharing from IDs; do not re-intern and accidentally merge identity-sensitive VLA types.

- [ ] **Step 6: Run KIR1 format tests**

Expected: PASS.

- [ ] **Step 7: Build host/native objects and commit KIR1 primitives**

Commit message: `native: add KIR1 typed graph stream`.

---

### Task 7: Split KPARSE from KGEN at `extdef() -> gencode()`

**Files:**
- Modify: `cc.c`
- Modify: `ccdecl.c`
- Modify: `ccsym.c`
- Modify: `ccnode.c`
- Modify: `ccgen.c`
- Modify: `ccdata.c`
- Modify: `ccerr.c`
- Modify: `Makefile`
- Modify: `cckirwrite.c`
- Modify: `cckirread.c`
- Test: `../pdp10-testkit/tests/kcc-regression/kcc-phase-byte-identity-20261006-v1.sh`

**Interfaces:**
- KPARSE consumes KPT4 and writes KIR1 records in extdef order.
- KGEN consumes KIR1, reconstructs one external definition at a time, calls the existing `gencode(NODE *)`, then releases function-local reconstructed state before reading the next extdef.

- [ ] **Step 1: Extend the byte-identity regression to require KPARSE/KGEN/KOPT**

Compare integrated output to:

```text
KCPP -> KPARSE -> KGEN -> KOPT
```

under both `-O` and `-n`.

- [ ] **Step 2: Implement KPARSE driver loop by replacing only `gencode(n)` with `kir_write_extdef()`**

Preserve existing `nodeinit()`, extdef order, tentative-definition order, `mainsymp()`, module pragma handling, and parser diagnostics.

Add a focused synthetic parity input containing forward declarations, completed struct/union tags, a VLA, static locals, duplicate local names in distinct scopes, tentative definitions, and string literals; compare its integrated and four-phase assembly in addition to the 24-TU gate.

- [ ] **Step 3: Emit persistent global-state deltas after each extdef**

Track fields whose later generation can observe mutation (`Srefs`, linkage/class, assigned register metadata, type completion/member links, label/static identity). Emit each persistent object once and emit explicit update records when one of these fields changes.

- [ ] **Step 4: Implement KGEN KIR1 loop**

For every EXTDEF record, reconstruct the graph and call the unchanged `gencode(root)`. Process tentative definitions and final module state in the same order as the integrated main loop.

- [ ] **Step 5: Run 24-TU byte gate**

On any mismatch, inspect object ordering/identity and omitted state first. Do not normalize output or modify code generation to accommodate the split.

- [ ] **Step 6: Run full KCC regression group**

Expected: PASS.

- [ ] **Step 7: Commit parser/generator split and cumulative shar**

Commit message: `native: split KPARSE and KGEN`.

---

### Task 8: Native closure measurement against the 96K target

**Files:**
- Modify: `Makefile`
- Modify: `STATUS.MD`
- Modify: `TODO.MD`

**Interfaces:**
- Produces native object sets `NATIVE_KPARSE_OBJS`, `NATIVE_KGEN_OBJS`, `NATIVE_KOPT_OBJS` plus the existing `NATIVE_KCPP_OBJS`.

- [ ] **Step 1: Clean-build all four phase closures with KCC and DAS**

Expected: all DOBJ sets build without monolithic KCC1-only objects leaking into unrelated phases.

- [ ] **Step 2: Measure text/data/BSS/object totals for KCPP/KPARSE/KGEN/KOPT**

Report decimal and octal totals. Mark every phase above 96K object+BSS as immediate optimization work; phases below 96K still need later libc/stack/heap headroom.

- [ ] **Step 3: Inspect the largest object contributors only in phases that exceed the target or lack credible runtime margin**

Do not proactively shrink already-small KCPP/KOPT merely for symmetry.

- [ ] **Step 4: Perform phase-local dead-code/table reductions if measurement requires them**

Each reduction gets its own byte-identity/regression gate. Do not alter shared compiler semantics for a phase-local size win.

- [ ] **Step 5: Update `STATUS.MD` and `TODO.MD` with final measured closures and remaining runtime-link work**

Replace the obsolete monolithic KCC1 target wording with the four-phase state only after the new pipeline passes.

- [ ] **Step 6: Commit measurement checkpoint and cumulative shar**

Commit message: `native: measure multiphase KCC closures`.

---

### Task 9: Final documentation and runtime-measurement handoff

**Files:**
- Modify after implementation is complete: `README.MD`
- Modify after implementation is complete: `STATUS.MD`
- Modify after implementation is complete: `TODO.MD`
- Modify after implementation is complete: `../pdp10-doc/KERNEL/SELFHOSTING.MD`
- Inspect/update if needed: `../DAIMOS/PROJECT-STATE.MD`

**Interfaces:**
- Produces the documented native self-hosting compiler pipeline and the exact remaining blocker to a true 96K-per-phase runtime claim.

- [ ] **Step 1: Run the final 24-TU `-O`/`-n` byte-identity gate and normal KCC regressions**

Expected: PASS.

- [ ] **Step 2: Run a clean native object build for every phase**

Expected: PASS with measured closures matching `STATUS.MD`.

- [ ] **Step 3: Update KCC usage/state docs**

Document KCPP/KPARSE/KGEN/KOPT names, KPT4/KIR1/KPCODE1 interfaces, current object totals, and the fact that DAS/DLINK remain separate.

- [ ] **Step 4: Update `pdp10-doc` only now that the phase-split task is finished**

Record the completed architecture and explicitly separate object-size evidence from the future linked/runtime high-water measurement.

- [ ] **Step 5: Inspect DAIMOS `PROJECT-STATE.MD` and update only if the self-hosting architecture/current-order summary is materially stale**

- [ ] **Step 6: Create final cumulative shar archives for every modified repository**

Each shar is based on the last accepted patch in that repository and ends with its repository's final git commit.

- [ ] **Step 7: Commit and push all completed repositories**

Use `Theseus <theseus@chimaira.net>` and verify clean working trees plus matching remote heads.
