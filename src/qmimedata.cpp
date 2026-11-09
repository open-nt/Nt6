// MIT License - see LICENSE
#include <QtCore/qmimedata.h>

#include <algorithm>

namespace {
const char kText[] = "text/plain";
const char kHtml[] = "text/html";
const char kUris[] = "text/uri-list";
const char kImage[] = "application/x-qt-image";
const char kColor[] = "application/x-color";
} // namespace

/*!
    Constructs an empty QMimeData.
*/
QMimeData::QMimeData() = default;

/*!
    Destroys the QMimeData.
*/
QMimeData::~QMimeData() = default;

/*!
    Returns the URLs of the \c{text/uri-list} format. Lines starting with
    \c{#} are comments.

    \sa setUrls(), hasUrls()
*/
QList<QUrl> QMimeData::urls() const {
    QList<QUrl> r;
    const QVariant v = retrieveData(QString::fromLatin1(kUris), QMetaType());
    if (v.typeId() == QMetaType::QUrl)
        return {v.toUrl()};
    for (const QByteArray &line : v.toByteArray().split('\n')) {
        const QByteArray t = line.trimmed();
        if (!t.isEmpty() && !t.startsWith('#'))
            r.append(QUrl(QString::fromUtf8(t)));
    }
    return r;
}

/*!
    Sets the \c{text/uri-list} format to \a urls, one URL per line.
*/
void QMimeData::setUrls(const QList<QUrl> &urls) {
    QByteArray list;
    for (const QUrl &u : urls) {
        list.append(u.toEncoded());
        list.append("\r\n");
    }
    setData(QString::fromLatin1(kUris), list);
}

/*!
    Returns \c true if the object has the \c{text/uri-list} format;
    otherwise returns \c false.
*/
bool QMimeData::hasUrls() const { return hasFormat(QString::fromLatin1(kUris)); }

/*!
    Returns the plain text of the \c{text/plain} format, or an empty string.
*/
QString QMimeData::text() const { return QString::fromUtf8(retrieveData(QString::fromLatin1(kText), QMetaType()).toByteArray()); }

/*!
    Sets the \c{text/plain} format to \a text, encoded as UTF-8.
*/
void QMimeData::setText(const QString &text) { setData(QString::fromLatin1(kText), text.toUtf8()); }

/*!
    Returns \c true if the object has the \c{text/plain} format; otherwise
    returns \c false. A URL list also counts as text.
*/
bool QMimeData::hasText() const { return hasFormat(QString::fromLatin1(kText)) || hasUrls(); }

/*!
    Returns the HTML of the \c{text/html} format, or an empty string.
*/
QString QMimeData::html() const { return QString::fromUtf8(retrieveData(QString::fromLatin1(kHtml), QMetaType()).toByteArray()); }

/*!
    Sets the \c{text/html} format to \a html, encoded as UTF-8.
*/
void QMimeData::setHtml(const QString &html) { setData(QString::fromLatin1(kHtml), html.toUtf8()); }

/*!
    Returns \c true if the object has the \c{text/html} format; otherwise
    returns \c false.
*/
bool QMimeData::hasHtml() const { return hasFormat(QString::fromLatin1(kHtml)); }

/*!
    Returns the image stored with setImageData(), or an invalid QVariant.
*/
QVariant QMimeData::imageData() const { return retrieveData(QString::fromLatin1(kImage), QMetaType()); }

/*!
    Stores the image \a image.
*/
void QMimeData::setImageData(const QVariant &image) {
    removeFormat(QString::fromLatin1(kImage));
    m_data.emplace_back(QString::fromLatin1(kImage), image);
}

/*!
    Returns \c true if an image is stored; otherwise returns \c false.
*/
bool QMimeData::hasImage() const { return hasFormat(QString::fromLatin1(kImage)); }

/*!
    Returns the color stored with setColorData(), or an invalid QVariant.
*/
QVariant QMimeData::colorData() const { return retrieveData(QString::fromLatin1(kColor), QMetaType()); }

/*!
    Stores the color \a color.
*/
void QMimeData::setColorData(const QVariant &color) {
    removeFormat(QString::fromLatin1(kColor));
    m_data.emplace_back(QString::fromLatin1(kColor), color);
}

/*!
    Returns \c true if a color is stored; otherwise returns \c false.
*/
bool QMimeData::hasColor() const { return hasFormat(QString::fromLatin1(kColor)); }

/*!
    Returns the data of the format \a mimeType, or an empty byte array.

    \sa setData(), hasFormat()
*/
QByteArray QMimeData::data(const QString &mimeType) const { return retrieveData(mimeType, QMetaType::fromType<QByteArray>()).toByteArray(); }

/*!
    Sets the data of the format \a mimeType to \a data, replacing any
    earlier data of that format.
*/
void QMimeData::setData(const QString &mimeType, const QByteArray &data) {
    for (auto &entry : m_data) {
        if (entry.first == mimeType) {
            entry.second = data;
            return;
        }
    }
    m_data.emplace_back(mimeType, data);
}

/*!
    Removes the format \a mimeType.
*/
void QMimeData::removeFormat(const QString &mimeType) {
    m_data.erase(std::remove_if(m_data.begin(), m_data.end(), [&](const auto &e) { return e.first == mimeType; }), m_data.end());
}

/*!
    Returns \c true if the object has the format \a mimeType; otherwise
    returns \c false.
*/
bool QMimeData::hasFormat(const QString &mimeType) const {
    return std::any_of(m_data.begin(), m_data.end(), [&](const auto &e) { return e.first == mimeType; });
}

/*!
    Returns the formats the object holds, in the order they were set.
*/
QStringList QMimeData::formats() const {
    QStringList r;
    for (const auto &e : m_data)
        r.append(e.first);
    return r;
}

/*!
    Removes all formats.
*/
void QMimeData::clear() { m_data.clear(); }

/*!
    Returns the data of the format \a mimeType. \a preferredType is a hint
    for the type of the result. Reimplement it to provide data on demand.
*/
QVariant QMimeData::retrieveData(const QString &mimeType, QMetaType preferredType) const {
    Q_UNUSED(preferredType)
    for (const auto &e : m_data)
        if (e.first == mimeType)
            return e.second;
    return QVariant();
}

#include "moc_qmimedata.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
