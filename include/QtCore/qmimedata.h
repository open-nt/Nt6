// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>

#include <utility>
#include <vector>

/*!
    \class QMimeData
    \brief The QMimeData class provides a container for data that records information about its MIME type.

    A QMimeData holds data in several formats, each named by a MIME type,
    as used by the clipboard, drag and drop and
    QAbstractItemModel::mimeData(). Convenience functions handle plain text
    (\c{text/plain}), HTML (\c{text/html}) and URL lists
    (\c{text/uri-list}):

    \code
    auto *data = new QMimeData;
    data->setText(u"hello"_s);
    data->setData(u"application/x-mine"_s, payload);
    \endcode

    \differences
    Images and colors are stored as QVariant values under the formats
    \c{application/x-qt-image} and \c{application/x-color}; Nt has no
    conversion of images to and from encoded image formats.
    \enddifferences

    \sa QAbstractItemModel::mimeData()
*/
class QMimeData : public QObject {
    Q_OBJECT
public:
    QMimeData();
    ~QMimeData() override;

    QList<QUrl> urls() const;
    void setUrls(const QList<QUrl> &urls);
    bool hasUrls() const;
    QString text() const;
    void setText(const QString &text);
    bool hasText() const;
    QString html() const;
    void setHtml(const QString &html);
    bool hasHtml() const;
    QVariant imageData() const;
    void setImageData(const QVariant &image);
    bool hasImage() const;
    QVariant colorData() const;
    void setColorData(const QVariant &color);
    bool hasColor() const;

    QByteArray data(const QString &mimeType) const;
    void setData(const QString &mimeType, const QByteArray &data);
    void removeFormat(const QString &mimeType);
    virtual bool hasFormat(const QString &mimeType) const;
    virtual QStringList formats() const;
    void clear();

protected:
    virtual QVariant retrieveData(const QString &mimeType, QMetaType preferredType) const;

private:
    std::vector<std::pair<QString, QVariant>> m_data;
};
