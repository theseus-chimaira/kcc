# C-SIX Source Storage Format

Date: 2026-10-06

## Purpose

C-SIX is the reversible representation used when C source is stored in
DAIMOS native SIXBIT/S6REC text.  It is a storage encoding only.  KCPP must
decode C-SIX before C translation phase 1, so trigraph replacement,
backslash-newline splicing, comments, string literals, character constants,
identifiers, and preprocessing directives all see the original logical ASCII
source.

KCPP accepts both ordinary ASCII source and C-SIX/S6REC source.  Includes use
the same auto-detection and decoder as the primary source file.

## Plain ASCII word storage

DAIMOS regular files are 36-bit word streams.  Plain ASCII source may be
stored as five 7-bit characters per word, from the high end of the word
downward.  The remaining low bit is spare and is ignored by the source
reader.  Thus the five character fields begin at bit positions 29, 22, 15,
8, and 1 respectively.  A zero character terminates padding at the end of a
file word.

This is the same five-character, 35-bit packing used by DAS `.ASCII`/`.ASCIZ`.

## C-SIX letter rule

Native SIXBIT stores ASCII 040 through 0137 with 040 subtracted.  C-SIX uses
unescaped SIXBIT letters for logical lowercase because lowercase dominates C
source.  Uppercase is escaped with `@`.

```
logical a..z    -> stored SIXBIT A..Z
logical A..Z    -> stored @A..@Z
logical @       -> stored @@
```

The decoder therefore maps an unescaped stored `A` through `Z` to logical
`a` through `z`, while `@A` through `@Z` reconstruct uppercase.

## Characters outside SIXBIT

The printable ASCII characters outside the native SIXBIT range that matter
to C source are encoded with `@` escapes derived from the corresponding C
trigraph suffix where practical:

```
logical `       -> @'
logical {       -> @<
logical |       -> @!
logical }       -> @>
logical ~       -> @-
```

These are C-SIX storage escapes, not C trigraphs.  They are decoded before
KCPP performs normal trigraph processing.

Backslash is directly representable in SIXBIT.  Consequently ordinary C
escape spellings such as `\\n`, `\\t`, `\\101`, `\\x41`, and `\\u1234`
need no special storage rule beyond normal C-SIX letter case conversion.

## C digraphs and trigraphs

KCPP continues to implement ordinary C trigraphs.  C digraphs are also valid
source spellings where supported by the language frontend.  In particular,
`<%` and `%>` are useful human-written alternatives for `{` and `}`.

The C-SIX storage converter does not rewrite source tokens into digraphs or
trigraphs.  It preserves the original source bytes through C-SIX escapes.
For example a literal `{` is stored as `@<`; a programmer-written `<%`
remains `<%`.

## Whitespace and records

S6REC record boundaries represent logical newlines.  CRLF input is normalized
to one newline.  Horizontal TAB may be expanded by the host converter to the
next eight-column stop before C-SIX encoding; KCPP treats the resulting spaces
as ordinary source whitespace.  Other nonprinting control characters are
rejected by the C-SIX host converter unless a later format revision assigns
them an explicit escape.

## Detection

KCPP source input is automatic:

1. A structurally valid S6REC text stream is read as C-SIX and decoded.
2. Otherwise the source is read as ordinary ASCII.  Native DAIMOS ASCII files
   use the five-7-bit-characters-per-word representation above; hosted KCPP
   also accepts ordinary host byte-stream ASCII.

Detection must be strict enough that arbitrary ASCII is not misclassified as
S6REC merely because its first bytes resemble a record header.

## Errors

The following are malformed C-SIX and must be rejected rather than silently
changed:

- `@` at end of record/file;
- `@` followed by an unassigned escape character;
- malformed S6REC framing;
- characters outside the accepted ASCII/C-SIX source repertoire.

The source decoder is syntax-blind.  The same rules apply inside comments,
strings, character constants, macro bodies, identifiers, and include files.
