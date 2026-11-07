// MIT License - see LICENSE

#include <QtCore/qtemporaryfile.h>

#include <QtCore/qdir.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qtemporarydir.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <utility>
#include <vector>

/*
    Differences from Qt:
    - A template without "XXXXXX" gets ".XXXXXX" (files) or "-XXXXXX"
      (directories) appended.
    - QTemporaryFile::open() always opens the file ReadWrite; the extra
      createNativeFile()/rename() helpers are not provided.
    - QTemporaryDir::path() is absolute even for a relative template.
*/

namespace {

const char kPlaceholder[] = "XXXXXX";

// Splits a template into a mutable path and the length of the part after the
// last XXXXXX in the file name, appending `fallbackSuffix` when there is none.
std::string expandTemplate(const QString &templ, const char *fallbackSuffix, std::size_t &suffixLen) {
    std::string path = templ.toStdString();
    const std::size_t nameStart = path.rfind('/') == std::string::npos ? 0 : path.rfind('/') + 1;
    std::size_t pos = path.rfind(kPlaceholder);
    if (pos == std::string::npos || pos < nameStart) {
        path += fallbackSuffix;
        pos = path.size() - 6;
    }
    suffixLen = path.size() - pos - 6;
    return path;
}

QString defaultFileTemplate() { return QDir::tempPath() + QString::fromUtf8("/qt_temp.XXXXXX"); }

} // namespace

QTemporaryFile::QTemporaryFile() : m_template(defaultFileTemplate()) {}

QTemporaryFile::QTemporaryFile(const QString &templateName) : m_template(templateName) {}

QTemporaryFile::QTemporaryFile(QObject *parent) : QFile(parent), m_template(defaultFileTemplate()) {}

QTemporaryFile::QTemporaryFile(const QString &templateName, QObject *parent) : QFile(parent), m_template(templateName) {}

QTemporaryFile::~QTemporaryFile() {
    close();
    if (m_autoRemove && !m_name.isEmpty())
        ::unlink(m_name.toUtf8().constData());
}

bool QTemporaryFile::open(OpenMode mode) {
    if (isOpen()) {
        setFileError(OpenError, QString::fromUtf8("file already open"));
        return false;
    }
    const OpenMode keep = mode & (Append | Truncate | Text | Unbuffered);
    const OpenMode effective = OpenMode(ReadWrite) | keep;
    if (!m_name.isEmpty())
        return openPath(m_name, effective);

    std::size_t suffixLen = 0;
    std::string path = expandTemplate(m_template.isEmpty() ? defaultFileTemplate() : m_template, ".XXXXXX", suffixLen);
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');
    const int fd = ::mkostemps(buf.data(), static_cast<int>(suffixLen), O_CLOEXEC);
    if (fd < 0) {
        setFileError(OpenError, QString::fromUtf8(std::strerror(errno)));
        return false;
    }
    m_name = QString::fromUtf8(buf.data());
    m_fd = fd;
    m_error = NoError;
    QIODevice::open(effective);
    return true;
}

QTemporaryDir::QTemporaryDir() { create(QDir::tempPath() + QString::fromUtf8("/qt_temp-XXXXXX")); }

QTemporaryDir::QTemporaryDir(const QString &templatePath) {
    create(templatePath.isEmpty() ? QDir::tempPath() + QString::fromUtf8("/qt_temp-XXXXXX") : templatePath);
}

QTemporaryDir::QTemporaryDir(QTemporaryDir &&other) noexcept
    : m_path(std::move(other.m_path)), m_error(std::move(other.m_error)), m_autoRemove(other.m_autoRemove) {
    other.m_path.clear();
    other.m_autoRemove = false;
}

QTemporaryDir &QTemporaryDir::operator=(QTemporaryDir &&other) noexcept {
    if (this != &other) {
        if (m_autoRemove && isValid())
            remove();
        m_path = std::move(other.m_path);
        m_error = std::move(other.m_error);
        m_autoRemove = other.m_autoRemove;
        other.m_path.clear();
        other.m_autoRemove = false;
    }
    return *this;
}

QTemporaryDir::~QTemporaryDir() {
    if (m_autoRemove && isValid())
        remove();
}

void QTemporaryDir::create(const QString &templatePath) {
    m_path.clear();
    m_error.clear();
    std::size_t suffixLen = 0;
    const std::string path = expandTemplate(templatePath, "-XXXXXX", suffixLen);
    std::vector<char> buf(path.begin(), path.end());
    buf.push_back('\0');

    bool ok = false;
    if (suffixLen == 0) {
        ok = ::mkdtemp(buf.data()) != nullptr;
    } else {
        // mkdtemp() needs the placeholder at the very end, so fill it in by hand.
        static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
        std::random_device rd;
        const std::size_t pos = path.size() - suffixLen - 6;
        for (int attempt = 0; attempt < 100 && !ok; ++attempt) {
            for (std::size_t i = 0; i < 6; ++i)
                buf[pos + i] = alphabet[rd() % (sizeof alphabet - 1)];
            ok = ::mkdir(buf.data(), 0700) == 0;
            if (!ok && errno != EEXIST)
                break;
        }
    }
    if (!ok) {
        m_error = QString::fromUtf8("Failed to create a temporary directory: ") + QString::fromUtf8(std::strerror(errno));
        return;
    }
    m_path = QFileInfo(QString::fromUtf8(buf.data())).absoluteFilePath();
}

bool QTemporaryDir::remove() {
    if (!isValid())
        return false;
    return QDir(m_path).removeRecursively();
}

QString QTemporaryDir::filePath(const QString &fileName) const {
    if (!isValid() || QDir::isAbsolutePath(fileName))
        return QString();
    return m_path + QChar(u'/') + fileName;
}
