# Contributing to Nt

Thank you for helping. Nt is a clean-room, MIT-licensed implementation of the Qt 6 API, and it grows one class at a
time, so small contributions are welcome: a missing function, a test, a fix to the documentation.

## Where to start

- Issues labelled [`good first issue`](https://github.com/open-nt/Nt6/labels/good%20first%20issue) are small and
  self-contained; [`help wanted`](https://github.com/open-nt/Nt6/labels/help%20wanted) ones are larger.
- The "not implemented yet" lists in the [README](README.md#status) show what is missing in each module.
- Something you need is not there? Open a [missing API issue](https://github.com/open-nt/Nt6/issues/new/choose), or
  ask in [Discussions](https://github.com/open-nt/Nt6/discussions) first if you are not sure.

Say on the issue that you are working on it, so that two people do not write the same class.

## The clean-room rule

Nt must stay free of Qt's code and text. This is what makes the MIT license possible, so it is not negotiable:

- **Do not copy or translate Qt source code**, and do not write Nt code with Qt's implementation open beside you. If
  you have recently worked on the implementation of a Qt class, please pick a different class in Nt.
- **Do not copy or reword Qt's documentation.** Qt's documentation text is not MIT licensed. Write the comments from
  what Nt's code does.
- **Work from the public API and from behaviour**: the class and function signatures, public standards (RFCs, the
  D-Bus specification, Unicode data), and what a test program observes.
- **Other third-party code** needs a license compatible with MIT and a note of where it came from in the pull request.

By opening a pull request you confirm that the contribution is your own work, that it follows these rules, and that
you license it under the [MIT License](LICENSE).

## Build and test

```sh
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

Nt needs a C++17 compiler, CMake 3.16 or newer and a POSIX system. Development happens on Linux first; keep
platform-specific code isolated so that the other platforms can follow.

## Before you commit: `./check.sh`

`./check.sh` is the quality gate, and CI runs the same script, so a pull request that passes it locally passes in CI.

```sh
./check.sh                  # everything, including the sanitizer builds
./check.sh --quick          # skips the sanitizer builds
./check.sh --fix            # reformats the lines you changed with clang-format
./check.sh --install-hook   # runs it as a git pre-commit hook
```

It checks file hygiene, a `-Werror` build, every test, complete documentation, clang-format, clang-tidy and cppcheck,
then runs the tests again under AddressSanitizer, UBSan and ThreadSanitizer. Fix what it finds; do not suppress a
warning to get it green. Where a suppression is justified, keep it narrow and give the reason.

## What a change needs

- **Tests** in `tests/` for new behaviour and for every bug fixed.
- **Documentation** for every public class and member, in `/*! ... */` comments above the declaration or definition.
  The syntax is a subset of QDoc and is described in [tools/ndoc/README.md](tools/ndoc/README.md). Each class has a
  `\differences` block that says how it differs from Qt.
- **The license line.** Every `.h` and `.cpp` file starts with `// MIT License - see LICENSE`.
- **CMake.** A new `src/**/*.cpp` is listed once in `CMakeLists.txt`. A header with `Q_OBJECT` or `Q_GADGET` gets a
  generated `moc_<header>.cpp`, which the source file includes at its end.
- **The README.** Move the class from "not implemented yet" to the table of its module.

[AGENTS.md](AGENTS.md) has the same rules in more detail, with a documentation example.

## Pull requests

Keep a pull request to one subject, describe what it adds and how you tested it, and link the issue it closes. A
class that is only partly implemented is fine when the documentation says what is missing.

## Reporting bugs and security problems

Use the [bug report form](https://github.com/open-nt/Nt6/issues/new/choose) for bugs, with a small program that shows
the problem. Report security problems privately, as [SECURITY.md](SECURITY.md) describes.

## Conduct

Everyone taking part is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
