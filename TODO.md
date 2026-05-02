# KCC TODO

## Language implementation

- [x] Variable length arrays.
- [x] Hexadecimal floating constants.
- [x] `_Noreturn`.
- [x] `_Generic`.
- [x] `_Alignof`.
- [x] `_Alignas`.
- [x] Anonymous structs and unions.
- [x] Preprocessor `#elifdef`, `#elifndef`, and `#warning`.
- [x] `__VA_OPT__`.

## Language audit

Tests belong in `pdp10-c-testkit`, not in this repository.

- [x] Audit C89 functionality systematically. Explicit `-Pc89`, `-Pc99`,
  `-Pgnu89`, and `-Pgnu99` language profiles enforce the standard/extension
  boundary; `-Pstrict` is an alias for `-Pc89`.

## PDP-10 target completion

- [ ] Finish KL10 support. The current KL target is usable but is not yet a
  complete model of the KL instruction set, addressing modes, and profitable
  KL-specific code generation.
- [ ] Audit profitable `EXTEND` generation for KL10/KS10 where it reduces code
  size or runtime without materially increasing compiler RAM.

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
- [ ] Audit direct KCC/GCC interoperability for argument assignment, return
  registers, preserved registers, stack cleanup, aggregate returns, aggregate
  and bit-field layout, pointer representations, floating formats, symbol
  spelling/significance, runtime helpers, assembler directives, relocation,
  and object conventions.
- [ ] Use explicit adapters for interfaces whose native representations differ;
  do not change KCC `long`, byte pointers, or the default ABI merely to match a
  GCC internal machine mode.
- [ ] Keep runtime-result equivalence separate from code-size/performance
  comparisons when built-ins or compatibility helpers differ.
- [ ] Treat mixed variadic calls as unsupported unless direct ABI compatibility
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

## Design constraints

- Correct compiler behavior has priority over compatibility workarounds.
- Preserve the canonical four-argument/four-return-register PDP-10 C ABI.
- Preserve historical KCC behavior unless a change is required for correctness
  or is a small, independently useful improvement.
- Prefer lower compiler/runtime RAM consumption first, execution speed second,
  and disk size third.
- Avoid feature creep and target-specific complexity without demonstrated use.
