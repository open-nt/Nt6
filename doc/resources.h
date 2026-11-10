// MIT License - see LICENSE
//
// Overview page about resources for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page resources.html
    \title The Nt Resource System
    \brief Compiling files into a program with ntrcc and reading them with QFile.

    Resources are files, such as icons, translations or default settings,
    that are compiled into the program so that it does not need to find them
    on disk at run time. Nt reads the Qt resource collection files
    (\c .qrc) that Qt projects already have, and compiles them with
    \c ntrcc, its replacement for Qt's \c rcc.

    \section1 Resource Collection Files

    A \c .qrc file is XML. Each \c <qresource> element puts the files it
    lists below its \c prefix; each \c <file> names a file relative to the
    \c .qrc file, and its \c alias attribute gives it another name. A
    directory is added with every file in it, except hidden ones.

    \badcode
    <!DOCTYPE RCC>
    <RCC version="1.0">
        <qresource prefix="/">
            <file>icons/open.svg</file>
            <file alias="config.json">defaults/config-1.2.json</file>
            <file>themes</file>
        </qresource>
        <qresource prefix="/text" lang="fr">
            <file alias="welcome.txt">welcome_fr.txt</file>
        </qresource>
    </RCC>
    \endcode

    \section2 Compression

    \c ntrcc compresses a file when that makes it at least 70% smaller, with
    Zstandard if Nt was built with libzstd, otherwise with zlib. QFile
    decompresses a resource into memory when it opens it, so code that reads
    resources does not change. The attributes of a \c <file> override the
    command line for that file:

    \badcode
    <file compression-algorithm="zlib" compress="9">data/catalog.json</file>
    <file compression-algorithm="none">images/photo.jpg</file>
    <file threshold="20">help/index.html</file>
    \endcode

    \c compression-algorithm is \c zstd, \c zlib, \c best or \c none;
    \c compress is the level (1 to 9 for zlib, 1 to 22 for Zstandard, \c -1
    for the default, \c 0 for none); \c threshold is the percentage a file
    must shrink by. Files that are already compressed, such as PNG or JPEG
    images, rarely pass the threshold and are stored as they are.

    zlib and libzstd are found when Nt is configured; turn them off with
    \c{-DNT_USE_ZLIB=OFF} and \c{-DNT_USE_ZSTD=OFF}. A build without an
    algorithm cannot read resources compressed with it, and its \c ntrcc
    refuses to write them.

    \section2 Locales

    The \c lang attribute makes a variant for one language (\c fr) or one
    language and territory (\c fr_FR). A file is read in the variant that
    best fits the locale: one for the language and territory, then one for
    the language, then one without \c lang.

    \section1 Building with CMake

    The helpers are in \c NtRcc.cmake, which the Nt6 package includes.
    Either list the \c .qrc file among the sources and turn on AUTORCC:

    \code
    add_executable(viewer main.cpp viewer.qrc)
    target_link_libraries(viewer PRIVATE Nt::Core)
    nt_enable_autorcc(viewer)
    \endcode

    or compile it explicitly, as \c qt_add_resources() does:

    \code
    nt_add_resources(viewer_resources viewer.qrc)
    add_executable(viewer main.cpp ${viewer_resources})
    \endcode

    The Qt 6 form names the files in CMake and needs no \c .qrc file. Each
    file is named by its path relative to \c BASE, or by its
    \c QT_RESOURCE_ALIAS source file property:

    \code
    nt_add_resources(viewer icons PREFIX /icons BASE art FILES art/open.svg art/save.svg)
    \endcode

    \c nt_add_binary_resources() writes a binary \c .rcc file instead, to be
    loaded at run time with QResource::registerResource(). Unless Qt's own
    are defined, \c qt_add_resources(), \c qt6_add_resources(),
    \c qt_add_binary_resources() and \c qt6_add_binary_resources() are the
    same functions, and \c Qt6::rcc is \c ntrcc, so \c CMAKE_AUTORCC finds it.

    \section1 Using Resources

    A resource path starts with a colon. QFile opens it for reading,
    QFileInfo describes it, and QDir and QDirIterator list resource
    directories:

    \code
    QFile file(":/config.json");
    if (file.open(QIODevice::ReadOnly))
        settings = QJsonDocument::fromJson(file.readAll());

    for (const QString &name : QDir(":/icons").entryList(QDir::Files))
        addIcon(name);
    \endcode

    Resources cannot be written, removed or renamed. QResource reads the
    bytes of a resource without opening it, and registers \c .rcc files.

    Resources compiled into an executable are registered when the program
    starts. Resources in a static library are registered only if their object
    file is linked, so call \c{Q_INIT_RESOURCE(name)}, with the base name of
    the \c .qrc file, from a function in the global namespace such as
    \c main(). \c{Q_CLEANUP_RESOURCE(name)} undoes it.

    \section1 Search Paths

    QDir::setSearchPaths() gives a list of directories a name prefix, and
    the directories can be resource directories. A name such as
    \c{icons:open.png} then refers to the first \c open.png in the list, so
    files on disk can override resources:

    \code
    QDir::setSearchPaths("icons", QStringList() << QDir::homePath() + "/.viewer/icons" << ":/icons");
    QFile icon("icons:open.png");
    \endcode

    QFile, QFileInfo, QDir and QDirIterator all understand prefixes, and
    QDir::addSearchPath() appends to a list. A file that exists in none of
    the directories is created in the first one, if it can be written.

    \section1 Running ntrcc

    \badcode
    ntrcc [options] <inputs.qrc>...
    \endcode

    \table
    \header \li Option \li Meaning
    \row \li \c{-o}, \c{--output <file>} \li Write to \c <file> instead of the standard output.
    \row \li \c{--name <name>} \li Name the functions \c qInitResources_<name>() and
             \c qCleanupResources_<name>(), used by the macros. The default is the base name
             of the first input.
    \row \li \c{--root <path>} \li Put every resource below \c <path>.
    \row \li \c{--binary} \li Write a binary \c .rcc file instead of C++.
    \row \li \c{--list}, \c{--list-mapping} \li List the files the inputs refer to, with their
             resource paths for \c --list-mapping.
    \row \li \c{-d}, \c{--depfile <file>} \li Also write a Makefile dependency file.
    \row \li \c{--compress-algo <algo>} \li Compress with \c zstd, \c zlib, \c best (the
             default) or \c none.
    \row \li \c{--compress <level>} \li The compression level; \c 0 turns compression off.
    \row \li \c{--threshold <percent>} \li Compress a file only if that saves at least
             \c <percent> of it (70 by default).
    \row \li \c{--no-compress}, \c{--no-zstd} \li Do not compress; do not use Zstandard.
    \endtable

    Single dashes work as well as double ones, as with \c rcc. When
    \c SOURCE_DATE_EPOCH is set, the time stamps of the resources are clamped
    to it, for reproducible builds.

    \section1 Differences from Qt

    Compression needs zlib or libzstd when Nt is built, and the default
    Zstandard level is 14. The compiled format is Nt's own, so \c .rcc
    files from Qt's \c rcc cannot be registered, and Nt's cannot be read by
    Qt. QResource has no search paths of its own, as in Qt 6: use
    QDir::setSearchPaths(). The \c qrc: URL scheme is not understood by
    QFile. Resources and their directories are listed in name
    order. The \c BIG_RESOURCES option and \c OUTPUT_TARGETS of
    \c qt_add_resources() are accepted and have no effect.

    \sa QResource, QFile, QDir
*/
