# KCC TODO

## Language implementation

- [ ] Variable length arrays.
- [ ] Hexadecimal floating constants.
- [ ] `_Noreturn`.
- [ ] `_Generic`.
- [ ] `_Alignof`.
- [ ] `_Alignas`.
- [ ] Anonymous structs and unions.
- [ ] Preprocessor `#elifdef`, `#elifndef`, and `#warning`.
- [ ] `__VA_OPT__`.

## Language audit

Tests belong in `pdp10-c-testkit`, not in this repository.

- [ ] Audit C89 functionality systematically.
- [ ] Audit `inline` semantics and code generation.
- [ ] Audit `typeof` semantics.

## PDP-10 target completion

- [ ] Finish KL10 support. The current KL target is usable but is not yet a
  complete model of the KL instruction set, addressing modes, and profitable
  KL-specific code generation.
- [ ] Audit profitable `EXTEND` generation for KL10/KS10 where it reduces code
  size or runtime without materially increasing compiler RAM.

## KCC/GCC compatibility audit

KCC should interoperate with PDP-10 GCC where useful without replacing KCC's
language model or native semantics simply to mimic GCC.

- [ ] Keep testkit type metadata honest. Never compare KCC and GCC cells whose
  source types or widths do not describe equivalent operations.
- [ ] Continue exact-width semantic probes for truncation, sign extension,
  aggregate layout, and array stride.
- [ ] Classify GNU attributes explicitly as supported, harmless, emulated, or
  unsupported; do not silently erase attributes with ABI/semantic meaning.
- [ ] Detect compiler-conditioned source branches that compile materially
  different programs and exclude them from optimization comparisons.
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
