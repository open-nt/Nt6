# NDoc

NDoc generates Qt-style HTML reference documentation from comments written next to the code. It is a single C++17 file with no dependencies, and like `ntmoc` it is a text scanner rather than a C++ parser.

```sh
ndoc [-o outdir] [--strict] [--require-complete] doc/ndoc.conf
```

| Option | Effect |
|---|---|
| `-o <dir>` | Output directory, overriding `output` in the config |
| `--strict` | Exit 1 on any warning |
| `--require-complete` | Exit 1 if any public API is undocumented |

Every run writes `coverage.txt` to the output directory, listing undocumented public API.

From CMake, `nt_add_docs(<target> CONFIG <file> [OUTPUT <dir>] [STRICT] [REQUIRE_COMPLETE])` in `cmake/NtDoc.cmake` adds a documentation target.

## Writing comments

A `/*! */` comment directly above a declaration or an out-of-class definition documents it. This works in `.h` and `.cpp` files, and for header-only templates:

```cpp
/*!
    Sets the timeout interval to \a msec milliseconds.

    \sa interval()
*/
void QTimer::setInterval(int msec) { ... }
```

A comment that is not next to its declaration names it with a topic command instead.

The syntax is a subset of QDoc's, so a comment reads the same as it would in Qt. `\differences` and `\cmaketarget` are NDoc additions.

### Topics

| Command | Documents |
|---|---|
| `\class Name` | A class or struct |
| `\namespace Name` | A namespace |
| `\fn signature` | A function, given its full signature on one line |
| `\enum Class::Name` | An enum |
| `\typedef Class::Name` | A typedef or alias |
| `\variable Class::Name` | A public variable |
| `\module Name` | A module named in the config; add `\title` |
| `\page file.html` | A free-standing page; add `\title`. `index.html` adds to the landing page |

### Metadata

| Command | Meaning |
|---|---|
| `\brief text` | One-line summary, for classes, modules and pages |
| `\title text` | Title of a module or page |
| `\since version` | When the entity was introduced |
| `\sa a, b, {Page Title}` | See-also links |
| `\inmodule Name` | Overrides the module inferred from the header's directory |
| `\inheaderfile Name` | Overrides the header shown on the class page |
| `\cmaketarget Target` | CMake target of a module, shown on its class pages |
| `\overload` | Marks an overload; its parameters need not all be described |
| `\reimp` | Marks a reimplementation |
| `\internal` | Hides the entity and excuses it from coverage |
| `\threadsafe` | Adds a thread-safety note |
| `\differences` ... `\enddifferences` | The "Differences from Qt" section of a class page |

### Body

| Command | Output |
|---|---|
| `\a name` | A parameter, checked against the declaration |
| `\c code`, `\c{some code}` | Inline code |
| `\e text`, `\b text` | Emphasis, bold |
| `\l target`, `\l{target}{label}` | A link to a class, member, page, section or URL |
| `\code` ... `\endcode` | A code block |
| `\snippet file tag` | Code between `//! [tag]` markers in a file under `snippets` |
| `\image file alt text` | An image from a directory listed under `images` |
| `\section1 Title`, `\section2 Title` | Headings; `\section1` also joins the contents list |
| `\target name` | An anchor for `\l` |
| `\list` ... `\li` ... `\endlist` | A list; `\list 1` is numbered |
| `\table`, `\header`, `\row`, `\li`, `\endtable` | A table |
| `\value Name text`, `\omitvalue Name` | An enum value; every value must be accounted for |
| `\note text`, `\warning text` | A callout |

Class names, `Class::member` and `function()` are linked automatically when they resolve.

## Configuration

Paths are relative to the config file.

`github` adds a *Source* link with the GitHub logo (from Font Awesome Free, CC BY 4.0) to the header of every page.

```ini
project = Nt
# Either a literal `version = 1.0.0`, or the first line of a file:
version_file = ../VERSION
description = One line for the landing page.
footer = Text for every page.
logo = ../assets/nt-logo.svg
github = https://github.com/open-nt/Nt6
output = html
sources = ../include ../src
snippets = ../examples
images = ../assets/brand
module NtCore = ../include/QtCore
```

`sources` are scanned recursively for `.h` and `.cpp` files, skipping `*_p.h`. A class joins the module whose directory holds its header.

## Limitations

- Global functions, global typedefs and macros cannot be documented yet.
- Template specialisations are skipped.
- Both branches of an `#if` are read.
- Output is HTML only.
- `\fn` signatures must fit on one line.
- Stale pages are not removed from the output directory.
