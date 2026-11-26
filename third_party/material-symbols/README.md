# Material Symbols

The icons in `icons/` are **Material Symbols** by Google. They are **not** part of what is MIT licensed in this
repository: they are licensed under the **Apache License, Version 2.0**, whose text is in [LICENSE](LICENSE).

- Source: <https://github.com/google/material-design-icons>
- Commit: `10ecc14790612ca8fd164e3f4ffe9c16fc50d5b7`
- Files: `symbols/web/<name>/materialsymbolsoutlined/<name>_24px.svg` and `<name>_fill1_24px.svg`
- Changes: none. The files are exactly as published. The upstream repository had no `NOTICE` file at that commit.

A selection of 76 symbols, outlined and filled, is included, not the whole set.

## The package

These icons are the data of **Nt Material Symbols** (`Nt::MaterialSymbols`), an optional library of its own. No other
part of Nt uses or links to it. Its C++ code (`include/QtMaterialSymbols`, `src/materialsymbols`) is MIT licensed
like the rest of Nt; the icons that the build compiles into the library stay under Apache 2.0. Configure with
`-DNT_BUILD_MATERIAL_SYMBOLS=OFF` to leave it out. It is installed as the CMake component `MaterialSymbols`, and
packaged on its own as `nt6-material-symbols`.

A program that links with it distributes the icons, and so must do what section 4 of the Apache License asks:
give its users a copy of the license, keep the attribution, and say so if it changes the files.

## Adding or updating icons

Take the files, unchanged, from a commit of the upstream repository, put them in `icons/` with their names, and
record the commit above. The build picks up every `.svg` file in the folder.

"Material Symbols" and "Google" are names of Google LLC, used here to say where the icons come from. Nt is not
affiliated with or endorsed by Google, and the Apache License grants no right to its trademarks.
