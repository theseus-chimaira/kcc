# KCC for PDP-10

KCC is a C compiler targeting the PDP-10 family. This tree is the maintained
DAIMOS-oriented KCC port and is intended to remain useful on small and
historical PDP-10 systems while interoperating with the modern PDP-10 toolchain.

The compiler supports PDP-6, KA10, KI10, KL10, and KS10 target selection, plus
target variants needed by supported operating environments. Generated source
uses the PDP-10 assembler toolchain installed below `PDP10_PREFIX`.

## Project layout

- `cc*.c`, `cc*.h`: compiler front end, optimizer, register allocator, and code
  generator.
- `runtime/`: target runtime assembly and compiler-private helper fragments.
- `TODO.md`: remaining implementation, audit, compatibility, and optimization
  work plus design constraints which must be preserved.
- `LICENSE`: project license.

Compiler regression tests do not live in this repository. They are maintained
in `pdp10-c-testkit`, under `tests/kcc-regression`, so KCC, GCC, and shared
PDP-10 C behavior are tested from one test project.

## Build

Set `PDP10_PREFIX` to the installed PDP-10 toolchain prefix and run:

```sh
make
```

The host compiler defaults to `gcc` and can be overridden with `CC`. The build
requires a C99-capable host compiler; this is a host-build requirement and does
not change the language modes accepted by KCC itself.

Useful targets are:

```text
make                 build KCC and verify runtime source presence
make asm             self-compile compiler sources to PDP-10 assembly
make install         install KCC and its runtime below PDP10_PREFIX
make install-runtime install only the runtime assembly
make clean           remove host objects, self-compiled assembly, and KCC
make source-tar      create a source archive from committed files
```

Tests are run from `pdp10-c-testkit`, not through this Makefile.

## PDP-10 C ABI

The canonical DAIMOS PDP-10 C ABI uses four argument registers and four return
registers. AC0 through AC7 are call-clobbered; the remaining ABI-preserved
register state is restored by normal KCC-generated callees. KCC accepts its
historical `-r7` setting; incompatible register-ABI selections are rejected.

For a normal call, the first four argument words are passed in AC1 through AC4.
Additional words are passed on the stack. Multiword arguments consume
consecutive argument positions. A hidden structure-result pointer occupies
AC1, shifting user arguments by one word.

Scalar results use AC1. Multiword and aggregate return conventions use the
canonical register/result rules shared with the DAIMOS PDP-10 GCC port. Code
which mixes old stack-only KCC objects with the canonical ABI is unsupported;
rebuild all C objects and C-callable assembly when migrating an old system.

The DAIMOS process entry contract supplies `argc` in AC1 and `argv` in AC2.
The startup code passes those values directly to `main`; its result is returned
through AC1 to `exit`.

Compiler-private runtime helpers may use internal conventions when they are not
externally C-callable. Such helpers do not alter the public C ABI.

## Compatibility policy

KCC and PDP-10 GCC should share source and ABI behavior where that is practical,
but KCC is not to be redesigned merely to imitate GCC. Prefer, in order:

1. correct test classification and shared headers;
2. a well-defined common language and ABI subset;
3. explicit adapters where native representations differ;
4. small compiler improvements which are independently useful;
5. compiler redesign only when required for correctness, not compatibility
   cosmetics.

Compiler bugs exposed by valid source must be fixed in the compiler rather than
hidden by source-side workarounds.

## Testing

Use the separate `pdp10-c-testkit` repository. The imported KCC regression
suite can be run with its dedicated KCC regression target, while the normal
matrix compares KCC, GCC, and other supported compilers where tests are
semantically comparable.
