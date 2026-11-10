// MIT License - see LICENSE
//
// Reads data files the two ways Nt offers in place of Qt resources: files shipped next to the
// program and found through a QDir search path, and a file compiled into the program by
// embed.cmake. Prints both and exits with 0 if they were found.
#include <QBuffer>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

#include <cstddef>
#include <cstdio>

//! [declare]
// Defined by the source that embed.cmake generates from data/banner.txt.
extern const unsigned char banner_txt[];
extern const std::size_t banner_txt_size;
//! [declare]

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    //! [search-path]
    // "data:" means: the data directory next to the program, then the installed data directories.
    QStringList dirs{QCoreApplication::applicationDirPath() + "/data"};
    dirs += QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
    QDir::setSearchPaths("data", dirs);

    QFile file("data:greeting.txt");
    if (!file.open(QIODevice::ReadOnly)) {
        std::fprintf(stderr, "greeting.txt not found in %s\n", dirs.join(", ").toUtf8().constData());
        return 1;
    }
    const QByteArray greeting = file.readAll();
    //! [search-path]

    //! [embedded]
    // The bytes stay in the program; fromRawData() refers to them, and QBuffer reads them like a file.
    QBuffer banner;
    banner.setData(
        QByteArray::fromRawData(reinterpret_cast<const char *>(banner_txt), static_cast<qsizetype>(banner_txt_size)));
    banner.open(QIODevice::ReadOnly);
    const QByteArray firstLine = banner.readLine();
    //! [embedded]

    std::fputs(greeting.constData(), stdout);
    std::fputs(firstLine.constData(), stdout);
    return greeting.isEmpty() || firstLine.isEmpty() ? 1 : 0;
}
