// MIT License - see LICENSE
//
// Guide to shipping data files with an Nt program, in place of Qt resources, for the NDoc
// documentation. This file holds only a documentation comment and is not part of the library.

/*!
    \page resources.html
    \title Resource Files
    \brief How to ship data files, such as text, settings and translations, with an Nt program.

    In Qt, files listed in a \c .qrc file are compiled into the program by \c rcc, and the program
    opens them with paths that start with \c{:/}, such as \c{":/i18n/app_de.qm"}. Nt has no resource
    compiler and no QResource class yet, so \c qt_add_resources() does not exist and a path that
    starts with \c{:} is an ordinary relative file name. This page shows what to do instead.

    \note If you are porting from Qt, search your code for \c{":/"} and \c{"qrc:"}: each of those
    paths needs one of the approaches below.

    \section1 Choosing an Approach

    \table
    \header
        \li You have
        \li Use
    \row
        \li Data files that go with the program: templates, default settings, translations
        \li \l{Files Next to the Program}{Ship the files and find them through a search path}
    \row
        \li A few small files that must always be there, even if the program is copied alone
        \li \l{Files Inside the Program}{Compile them into the program}
    \row
        \li Files the user creates or changes
        \li The writable locations of QStandardPaths, such as \c AppDataLocation and \c AppConfigLocation
    \endtable

    The \c resources example, in \c{examples/resources}, does the first two. Build it on its own
    against an installed Nt:

    \code
    cmake -S examples/resources -B build-resources
    cmake --build build-resources
    build-resources/resources
    \endcode

    \section1 Files Next to the Program

    Keep the files in a directory of your project, for example \c{data/}, copy it next to the
    executable when you build, and install it with the program:

    \code
    add_custom_command(TARGET myapp POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:myapp>/data")
    include(GNUInstallDirs)
    install(DIRECTORY data/ DESTINATION "${CMAKE_INSTALL_DATADIR}/myapp")
    \endcode

    At run time, register a search path prefix with QDir::setSearchPaths(). A path that starts
    with the prefix and a colon, such as \c{"data:greeting.txt"}, then refers to the first
    directory of the list that has the file. This is the closest match to \c{":/greeting.txt"} in
    Qt, and keeps the rest of the code free of directory names:

    \snippet resources/main.cpp search-path

    QCoreApplication::applicationDirPath() finds the files while you develop, and
    QStandardPaths::standardLocations() with \c AppDataLocation finds them once installed:
    \c{/usr/share/myapp}, \c{/usr/local/share/myapp} and the user's \c{~/.local/share/myapp}. The
    application sub-directory is the file name of the executable.

    The prefix is understood by QFile::open(), QFile::exists(), QFileInfo, QDir, and the classes
    that open files through them, such as QTranslator. It must be at least two characters long, so
    that it is not mistaken for a drive letter.

    \section2 Translations

    Ship the \c .qm files made by \c lrelease in the same way and load them through a prefix:

    \code
    QDir::setSearchPaths("i18n", {QCoreApplication::applicationDirPath() + "/translations"});
    QTranslator translator;
    if (translator.load(QLocale(), "myapp", "_", "i18n:"))
        QCoreApplication::installTranslator(&translator);
    \endcode

    \section1 Files Inside the Program

    For a small file that must never be missing, compile its bytes into the program. The example's
    \c embed.cmake turns a file into a C++ array at build time, and rebuilds it when the file
    changes:

    \code
    include(embed.cmake)
    nt_example_embed(myapp banner_txt data/banner.txt)
    \endcode

    Copy \c embed.cmake into your project; it is part of the example, not of the Nt package. Declare
    the array where you use it, and read it through QByteArray::fromRawData(), which does not copy
    the bytes, or through a QBuffer, which reads them like a file:

    \snippet resources/main.cpp declare
    \snippet resources/main.cpp embedded

    Embedded data cannot be changed or replaced without rebuilding, and it makes the program
    larger, so keep it for files of a few kilobytes to a few megabytes.

    \section1 Images and Icons

    Nt GUI cannot decode image files such as PNG or SVG yet, so images cannot be loaded from a file
    or from embedded bytes with QImage. Draw simple graphics with QPainter instead, as the Nt logo
    in the \c gallery example is drawn from its outlines.

    \section1 What Is Not There Yet

    \list
        \li The resource system: \c rcc, \c .qrc files, QResource, \c{:/} and \c{qrc:} paths.
        \li Image file formats for QImage, QPixmap and QIcon.
    \endlist

    \sa {Installing Nt}, QDir::setSearchPaths(), QStandardPaths, QBuffer
*/
