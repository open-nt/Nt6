// MIT License - see LICENSE
//
// Brand guide page for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page brand.html
    \title Nt Brand Guide
    \brief How to use the Nt name and logo, including the Nt Development logo and version names.

    This guide covers the Nt logo, its colour variations, the secondary Nt
    Development logo and how version names such as Nt 6 are written. The
    artwork lives in \c{assets/brand/} in the source tree and is MIT licensed
    like the rest of Nt.

    \note Nt is a clean-room implementation of the Qt 6 API. It is not
    affiliated with The Qt Company, and the Nt logo and name must never be
    presented as, or mistaken for, the Qt logo or name.

    \section1 Primary Logo

    The primary logo is the Nt mark: a teal hexagonal "nut" holding a large
    capital N and a smaller lowercase t that share a baseline. The letters are
    set in Titillium Web Semibold (see \l{Typeface}). It stands for the Nt libraries, the build tools and the project itself, in documentation,
    packages, slides and web pages.

    \image nt-logo-variations.svg Nt logo in teal, black on white and white on black

    \section2 Colour Variations

    \table
    \header
        \li Version
        \li Use
        \li File
    \row
        \li Teal
        \li The default. Use it on white or very light backgrounds.
        \li \c{assets/nt-logo.svg}
    \row
        \li Black on white
        \li Letterheads, printing in one colour, and any place the brand colours
            are not available.
        \li \c{assets/brand/nt-logo-black.svg}
    \row
        \li White on black
        \li On top of photographs and other busy or colourful surfaces.
        \li \c{assets/brand/nt-logo-white.svg}
    \endtable

    \section1 Secondary Logo: Nt Development

    The Nt Development logo is the Nt mark followed by the word
    \e Development. Use it only where it must be told apart from released,
    packaged builds of Nt: development snapshots, nightly builds, preview
    documentation and contributor material. In every other place use the
    primary logo.

    \image nt-development-logo-variations.svg Nt Development logo in neon on pine, pine on neon, black on white and white on black

    \table
    \header
        \li Version
        \li Use
        \li File
    \row
        \li Neon on pine
        \li The default. Place it on a pine background.
        \li \c{assets/brand/nt-development-logo-neon.svg}
    \row
        \li Pine on neon
        \li Place it on a neon background.
        \li \c{assets/brand/nt-development-logo-pine.svg}
    \row
        \li Black on white
        \li One-colour printing, or where the brand colours are not available.
        \li \c{assets/brand/nt-development-logo-black.svg}
    \row
        \li White on black
        \li Photographs and other colourful surfaces.
        \li \c{assets/brand/nt-development-logo-white.svg}
    \endtable

    The files include their background rectangle and an even margin on every
    side, so that they can be placed as they are. The word Development is set in
    Titillium Web Semibold and, like the letters of the mark, is converted to
    outlines, so the files do not need the font installed and are ready for
    print.

    \section1 Typeface

    Nt uses Titillium Web Semibold for the letters of the mark, the word
    Development and headings in brand material. Titillium Web is licensed under
    the SIL Open Font License 1.1, a copy of which is in
    \c{assets/brand/OFL-TitilliumWeb.txt}. It is a squared, geometric sans with
    a lowercase t that is clearly shorter than the capital N; that difference in
    height is part of the mark.

    \section1 Colours

    \table
    \header
        \li Name
        \li Hex
        \li Use
    \row
        \li Nut Teal
        \li \c{#13977F}
        \li The hexagon of the primary logo, and links
    \row
        \li Nut Neon
        \li \c{#3FD9B4}
        \li The mark on dark backgrounds, and the background of the pine variation
    \row
        \li Pine
        \li \c{#0B2E27}
        \li Background of the neon variation, and text on neon
    \row
        \li Black
        \li \c{#000000}
        \li One-colour logos
    \row
        \li White
        \li \c{#FFFFFF}
        \li The letters in the mark, and one-colour logos on dark backgrounds
    \endtable

    The white letters have a contrast ratio of 3.6:1 against Nut Teal, above the
    3:1 that WCAG asks for graphical objects. Do not use a lighter teal behind
    white letters. In the one-colour marks the letters are cut out of the
    hexagon, so they show whatever is behind them and the marks can be placed on
    any background.

    \section1 Size and Clear Space

    \list
    \li Keep a margin around the logo of at least one quarter of the height of
        the mark. Keep other text, images and page edges out of it.
    \li Do not show the mark smaller than 16 px (or 5 mm) high. Do not show the
        Nt Development logo smaller than 32 px (or 10 mm) high.
    \li Scale the logo in proportion. Prefer SVG; where a bitmap is needed,
        export it at twice the displayed size.
    \endlist

    \section1 Names and Versions

    \table
    \header
        \li Write
        \li When
    \row
        \li \b{Nt}
        \li The project, in running text. It is pronounced "nut".
    \row
        \li \b{Nt 6}
        \li A version of Nt in running text and headings, with a space, as in
            "Qt 6". The number is the Qt major version whose API Nt implements.
            It is not Nt's own release number, which is the \c version in
            \c{doc/ndoc.conf}.
    \row
        \li \b{Nt6}
        \li Code, paths, repository and package names, without a space, as in
            \c{Qt6}.
    \row
        \li \b{Nt Development}
        \li A development build of Nt, shown with the Nt Development logo. Write
            "Nt 6 Development" when the version matters, and keep Nt 6 and
            Development in the same style.
    \row
        \li \b{Nt Core}, \b{Nt Network}
        \li Modules. Their classes keep Qt's names, such as \c QObject.
    \endtable

    The logo is the same for every version. Do not put the version number in the
    mark; write it next to the logo in text instead.

    \section1 Do and Don't

    \list
    \li Do use the supplied files, unchanged.
    \li Do use the black or white version when the colour version would be hard
        to see.
    \li Don't stretch, rotate, outline, shadow or recolour the logo.
    \li Don't redraw the letters, enlarge the t to match the N, or set the logo
        in another typeface.
    \li Don't use the Nt Development logo for a released version of Nt.
    \li Don't place the logo on a background that makes it hard to read.
    \li Don't combine the logo with the Qt logo, or use Qt's green, its cut-corner
        shape or lettering that makes Nt look like a Qt product.
    \endlist

    \sa {Signals and Slots}
*/
