# KCC Multiphase Self-Hosting Design

Date: 2026-10-06

## Goal

Split native DAIMOS KCC into small sequential processes so PDP-6/PDP-10
self-hosting does not require the complete compiler backend resident at once.
The split must preserve KCC's generated DAS/GAS assembly byte-for-byte.

The working resource target is that every native compiler phase fit within
96K PDP-10 words including its image, BSS, stack, libc/runtime support, and
normal compilation working state.  This is a target rather than permission to
change compiler semantics: correctness and byte identity take precedence over
crossing the 96K threshold.

The resulting toolchain should be:

```
        C source
           |
           v
        +------+
        | KCPP |
        +------+
           |
          KPT4
           |
           v
      +---------+
      | KPARSE  |
      +---------+
           |
          KIR1
           |
           v
        +------+
        | KGEN |
        +------+
           |
        KPCODE1
           |
           v
        +------+
        | KOPT |
        +------+
           |
      DAS/GAS assembly
           |
           v
          DAS
           |
          DOBJ
           |
           v
         DLINK
```

KCPP already exists and is not redesigned by this work.

## Non-goals

- Do not change generated assembly in order to make a phase boundary easier.
- Do not introduce a new optimizer algorithm.
- Do not introduce userspace overlays or a new kernel ABI.
- Do not serialize host or PDP-10 process pointers.
- Do not dump raw C structures as an on-disk ABI.
- Do not split inside expression generation or live virtual-register
  allocation merely to make individual source objects smaller.
- Do not combine DAS or DLINK into KCC.  They remain separate programs.

## Acceptance invariants

Every accepted phase boundary must satisfy all of the following before the
next split is attempted:

1. All 24 KCC self-hosting translation units produce byte-identical DAS/GAS
   assembly through the split and unsplit paths.
2. The existing KCC regression suite remains green.
3. The native phase object set builds with KCC and assembles with DAS.
4. Intermediate files are versioned and reject incompatible readers.
5. A reader reconstructs all state needed by the existing downstream code;
   downstream algorithms are not rewritten simply to accommodate the split.
6. Native phase image/text/data/BSS sizes are measured after each accepted
   split.  Runtime, stack, and dynamic high-water measurements are added once
   native libc permits executable phase links.

## Why these boundaries

### Existing parser/backend boundary

The main compiler loop already performs:

```
n = extdef();
gencode(n);
nodeinit();
```

This means one complete external definition has a finite lifetime between
`extdef()` and `nodeinit()`.  That is the natural KPARSE/KGEN boundary.  KGEN
can continue to call the existing `gencode()` on a reconstructed typed tree.

### Existing PCODE boundary

KCC already lowers generated code into the bounded `PCODE` representation.
PCODE uses real PDP-10 register numbers rather than live `VREG` objects.  This
is the natural KGEN/KOPT boundary.  The first implementation preserves all
existing incremental optimization in KGEN and serializes only the PCODE that
would otherwise be handed to final output.  Pure PCODE passes are migrated to
KOPT only after that boundary is byte-identical.

### Rejected boundary: inside CCGEN2

Splitting inside expression generation would require serializing live VREGs,
stack state, partially emitted PCODE, labels, and expression temporaries.
That is both larger and less stable than either proposed IR and would make
byte identity harder to prove.

## KPCODE1 format

KPCODE1 is a word-oriented, versioned PDP-10 intermediate stream.  It is not
the in-memory `struct PCODE` layout.

The file begins with a fixed header containing:

- magic and version (`KPCODE1`);
- target CPU and architecture;
- target feature flags which affect output;
- assembler dialect identifier;
- compilation flags required by KOPT;
- module identity.

Records then describe final-output state in original emission order:

- function/module boundaries;
- labels and symbol references;
- PCODE instructions;
- literal/string data;
- segment changes and declarations;
- final module state needed by `outdone()`.

Pointers are replaced by compact stable IDs.  Symbols referenced by PCODE are
assigned IDs in first-observed order.  The symbol record contains only fields
needed by the downstream optimizer/output path; KPCODE1 does not reproduce the
frontend symbol table unnecessarily.

Integer and PDP-10 target-word values are stored as 36-bit words.  Strings are
length-prefixed packed words.  The representation must be streamable; KOPT
must not need the complete translation unit in memory.

## KGEN/KOPT migration sequence

The optimizer split is deliberately incremental.

### KOPT stage A: output boundary only

KGEN retains current generation, register allocation, and all current
peephole/CSE/jump transformations.  At the point where `flushcode()` would
call `realcode()`, KGEN writes the already-optimized instruction and associated
output records to KPCODE1 instead.

KOPT reads KPCODE1 and runs the existing output path.  No optimization moves
in this step.  This isolates serialization correctness.

Acceptance: all 24 KCC translation units byte-identical.

### KOPT stage B: migrate pure final PCODE passes

Move only passes whose complete semantic input is represented in KPCODE1.
Candidate order:

1. final jump/skip cleanup;
2. local PCODE peephole transforms;
3. PCODE CSE/rewrite helpers;
4. remaining output-local `cccode` transforms.

After each group moves, rerun the byte-identity gate and measure both KGEN and
KOPT.  Any pass whose result feeds live VREG selection or changes a value
returned to CCGEN remains in KGEN.

CCOUT belongs in KOPT once the KPCODE boundary is stable.

## KIR1 format

KIR1 carries exactly the typed parser result needed to invoke existing
`gencode()` in another process.  It is also versioned and word-oriented.

KIR1 must preserve identity and discovery order for objects where KCC can
observe those properties.  Raw addresses are never serialized.

### Stable IDs

Assign monotonically increasing IDs, in original allocation/discovery order,
to:

- TYPE objects;
- SYMBOL objects;
- NODE objects;
- string literals and other separately allocated payloads where identity is
  significant.

References in records use those IDs.  KGEN reconstructs objects in ID order
so pointer sharing and traversal order match the unsplit compiler.

### TYPE records

Record all fields consumed by code generation, including:

- `Tspec`;
- `Tbytes`;
- `Tflag`;
- size/prototype value;
- subtype/tag reference;
- prototype-chain references.

Interned type sharing must remain shared after reconstruction.  VLA types,
which intentionally have identity beyond structural equality, retain distinct
IDs.

### SYMBOL records

Record all fields visible to code generation or final output, including as
applicable:

- name;
- class;
- flags;
- assigned register;
- type ID;
- value/offset;
- member/prototype/list links;
- reference/initialization/usage counts;
- linkage/static-label identity.

Do not serialize preprocessor-only macro state into KIR1.

### NODE records

Record:

- opcode;
- flags;
- assigned register count/state already fixed by the parser;
- source line state required by generated diagnostics;
- type ID;
- child NODE IDs;
- SYMBOL IDs;
- integer/floating/string constants;
- opcode-specific auxiliary values.

The reader must reconstruct the same tree graph passed by `extdef()` to
`gencode()` today.

## Module and function streaming

KIR1 is external-definition oriented rather than a dump of one enormous
translation-unit graph.

Records are ordered as:

```
MODULE_BEGIN
GLOBAL_STATE / GLOBAL_STATE_DELTA
EXTDEF
GLOBAL_STATE_DELTA
EXTDEF
...
TENTATIVE_DEF
...
MODULE_END
```

An EXTDEF carries the typed NODE graph and the symbol/type objects reachable
from that definition.  Persistent global objects are introduced once and then
updated by deltas when parser-visible fields change.  Function-local objects
may be released by KGEN after `gencode()` completes for that definition.

This preserves KCC's existing one-external-definition-at-a-time lifetime and
keeps both phases bounded.

## Ordering and exact-output requirements

The following must remain deterministic and identical to the unsplit compiler:

- symbol discovery order;
- type creation/interning order;
- static/local label numbering;
- string/literal order;
- tentative-definition order;
- external declaration order;
- target configuration;
- optimizer configuration;
- segment transitions;
- source-line values which affect emitted diagnostics or null-pointer checks.

If byte comparison finds a difference, fix serialization/reconstruction.  Do
not normalize, reorder, or teach the emitter to accept a different order.

## Process/resource strategy

The reason for phase splitting is peak RAM, not disk size.  Intermediate-file
compactness matters only after phase memory and execution speed.

For each phase report:

- text words;
- initialized data words;
- BSS words;
- object total;
- native linked image once available;
- stack high-water mark once runnable under DAIMOS;
- heap/dynamic high-water mark once measurable;
- maximum total user words.

The desired end state is every compiler phase at or below 96K total user
words under a representative KCC self-build.  A phase which remains above 96K
becomes the next optimization target.  Do not shrink already-small phases at
the expense of clarity or runtime speed.

## Implementation order

1. Add a permanent integrated-vs-split 24-TU byte-identity regression.
2. Define KPCODE1 writer/reader and KOPT stage A with no optimizer migration.
3. Measure KGEN/KOPT native object closures.
4. Migrate pure PCODE passes one group at a time with byte-identity gates.
5. Define KIR1 stable-ID writer/reader at `extdef()`/`gencode()`.
6. Split KPARSE from KGEN without changing `gencode()` semantics.
7. Measure KPARSE/KGEN/KOPT object closures and identify any phase above 96K.
8. Only then perform phase-local dead-code/table reductions where measurement
   says they are needed.
9. Link native phase DXRs when the minimal DAIMOS libc/runtime surface is
   ready, and measure actual image+BSS+stack+heap against the 96K target.

## Failure policy

An experimental split is retained only when its underlying boundary is sound.
If a prototype differs from integrated output, first determine whether the
format omitted necessary state or the reader reconstructed it incorrectly.
Do not discard the architectural boundary merely because the first serializer
implementation is wrong.

If a boundary fundamentally requires large live state from both sides, record
that measurement and keep the affected logic in one phase rather than adding a
workaround or weakening byte-identity acceptance.
