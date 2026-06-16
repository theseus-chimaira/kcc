# KCC TODO

## KCC/GCC compatibility audit

KCC should interoperate with PDP-10 GCC where useful without replacing KCC's
language model or native semantics simply to mimic GCC.

- [x] Keep testkit type metadata honest. Never compare KCC and GCC cells whose
  source types or widths do not describe equivalent operations.
- [x] Continue exact-width semantic probes for truncation, sign extension,
  aggregate layout, and array stride.
- [x] Classify GNU attributes explicitly as supported, harmless, emulated, or
  unsupported; do not silently erase attributes with ABI/semantic meaning.
- [x] Eliminate compiler-conditioned executable/data branches from direct
  optimization-comparison sources.  Keep only explicitly semantic-only native
  language-model or imported-suite differences, enforced by the testkit audit.
- [x] Audit direct KCC/GCC interoperability for argument assignment, return
  registers, preserved registers, stack cleanup, aggregate returns, aggregate
  and bit-field layout, pointer representations, floating formats, symbol
  spelling/significance, runtime helpers, assembler directives, relocation,
  and object conventions.
- [x] Use explicit adapters for interfaces whose native representations differ;
  do not change KCC `long`, byte pointers, or the default ABI merely to match a
  GCC internal machine mode.
- [x] Keep runtime-result equivalence separate from code-size/performance
  comparisons when built-ins or compatibility helpers differ.
- [x] Treat mixed variadic calls as unsupported unless direct ABI compatibility
  is proven; otherwise use explicit wrappers.

## Optimizer follow-up

The completed local DImode pass found no worthwhile general peephole remaining
without broader allocator or control-flow work. Preserve these stopping rules:

- Do not add allocator complexity merely to eliminate rare pressure-only spills.
- Do not build a local mini-allocator for DImode division/remainder fallbacks.
- Keep generic control-flow and call-boundary spills unless liveness/dataflow
  work proves they can be removed safely.
- Machine-generic assembly improvements should go into the assembler when they
  also benefit GCC and handwritten assembly.

The current DImode division policy intentionally keeps a single general
signed/unsigned operation inline, while repeated same-kind operations may use a
shared helper. Revisit that threshold only with measurements across PDP-6,
KA10, KI10, and KS10.

## Rerun optimization audit
- [x] Re-audit code generation added by the compatibility and language work.
  Packed aggregate copies now stream byte pointers instead of repeatedly
  recomputing them with `ADJBP`; small constant byte-pointer adjustments use
  `IBP` on PDP-6/KA10/KI10 while KS10/KL10 keep native `ADJBP`.  Simple named
  DImode assignments store the live pair directly instead of pushing it,
  reconstructing the destination address, and reloading another pair; KI10 and
  later use `DMOVEM`, while PDP-6/KA10 expand it to two `MOVEM`s.  Constant
  negative arithmetic on ordinary native byte pointers now uses a PDP-6-safe
  whole-word address decrement plus forward `IBP`s on PDP-6/KA10/KI10 instead
  of `%ADJBPH`; KS10/KL10 retain native `ADJBP`.  Representative
  exact-width, DImode ABI, packed-value, aggregate, and mixed-call paths were
  rechecked.  Do not add allocator/control-flow complexity merely to remove the
  remaining one-instruction exact-18 return copy.

## PDP-10 target completion

- [x] Finish the standard-build KL10 section-0 target model.  `-x=kl10`
  uses the native section-0 instruction capabilities already represented by
  KCC; `-x=klx` is rejected unless KCC is built with the historical
  `MULTI_SECTION` support instead of silently emitting section-0 code.
- [x] Audit profitable `EXTEND` generation for KL10/KS10.  The current IR has
  no low-cost common C lowering for CMPS/MOVS/EDIT/CVT, and XBLT primarily
  serves extended-address copies rather than improving section-0 BLT.  Do not
  add new string/dataflow machinery without a measured workload that justifies
  its compiler-RAM cost.


## Design constraints

- Correct compiler behavior has priority over compatibility workarounds.
- Preserve the canonical four-argument/four-return-register PDP-10 C ABI.
- Preserve historical KCC behavior unless a change is required for correctness
  or is a small, independently useful improvement.
- Prefer lower compiler/runtime RAM consumption first, execution speed second,
  and disk size third.
- Avoid feature creep and target-specific complexity without demonstrated use.
