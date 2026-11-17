// MIT License - see LICENSE
#include <QtGui/qclipboard.h>

#include "qpa/qplatformclipboard_p.h"
#include "qpa/qplatformintegration_p.h"
#include "qwindow_p.h"

#include <QtCore/qbuffer.h>
#include <QtCore/qmimedata.h>
#include <QtCore/qpointer.h>
#include <QtGui/qguiapplication.h>

namespace {

QPlatformClipboard *platformClipboard() {
    QPlatformIntegration *integration = QtPrivate::GuiAccess::integration();
    return integration ? integration->clipboard() : nullptr;
}

} // namespace

QPlatformClipboard::QPlatformClipboard() = default;

QPlatformClipboard::~QPlatformClipboard() = default;

QMimeData *QPlatformClipboard::mimeData(QClipboard::Mode mode) { return m_data[mode].get(); }

void QPlatformClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode) {
    m_data[mode].reset(data);
    emitChanged(mode);
}

bool QPlatformClipboard::supportsMode(QClipboard::Mode mode) const { return mode == QClipboard::Clipboard; }

bool QPlatformClipboard::ownsMode(QClipboard::Mode mode) const { return m_data[mode] != nullptr; }

void QPlatformClipboard::emitChanged(QClipboard::Mode mode) {
    if (qGuiApp)
        QGuiApplication::clipboard()->emitChanged(mode);
}

QPlatformClipboard *QPlatformIntegration::clipboard() const {
    static QPlatformClipboard *inProcess = new QPlatformClipboard; // never destroyed: data can outlive the app
    return inProcess;
}

/*!
    \class QClipboard
    \inmodule NtGui
    \since 6.0
    \brief The QClipboard class provides access to the window system clipboard.

    The clipboard is QGuiApplication::clipboard(). Put text, an image or data of any MIME type on it, and read
    what this or another program put there:

    \code
    QClipboard *clipboard = QGuiApplication::clipboard();
    clipboard->setText(path);
    ...
    if (clipboard->mimeData()->hasFormat("application/x-shape"))
        paste(clipboard->mimeData()->data("application/x-shape"));
    \endcode

    On X11 the clipboard is shared with the other programs, and so is the Selection, the text the user last
    selected, which the middle mouse button pastes. Data from another program is read when it is asked for. On the
    other platforms the clipboard holds data for this program only.

    \differences
    On X11, data of more than the largest request of the X server, a few megabytes, is not sent (the INCR protocol
    is missing), and reading from another program waits up to a second for it. dataChanged() is emitted when this
    program changes the clipboard and when another program takes it over from this one, but not when one other
    program replaces the data of another. There is no find buffer.
    \enddifferences

    \sa QMimeData, QGuiApplication::clipboard()
*/

/*!
    \enum QClipboard::Mode

    This enum names the clipboards.

    \value Clipboard The global clipboard: copy and paste.
    \value Selection The text the user last selected, on X11.
    \value FindBuffer The text to find, on macOS; Nt has none.
    \omitvalue LastMode
*/

QClipboard::QClipboard(QObject *parent) : QObject(parent) {}

QClipboard::~QClipboard() = default;

void QClipboard::emitChanged(QClipboard::Mode mode) {
    emit changed(mode);
    if (mode == Clipboard)
        emit dataChanged();
    else if (mode == Selection)
        emit selectionChanged();
    else
        emit findBufferChanged();
}

/*!
    Clears the clipboard \a mode.
*/
void QClipboard::clear(QClipboard::Mode mode) { setMimeData(nullptr, mode); }

/*!
    Returns \c true if there is a Selection clipboard, as on X11.
*/
bool QClipboard::supportsSelection() const {
    QPlatformClipboard *p = platformClipboard();
    return p && p->supportsMode(Selection);
}

/*!
    Returns \c false: Nt has no find buffer.
*/
bool QClipboard::supportsFindBuffer() const { return false; }

/*!
    Returns \c true if the Selection holds data of this program.
*/
bool QClipboard::ownsSelection() const {
    QPlatformClipboard *p = platformClipboard();
    return p && p->ownsMode(Selection);
}

/*!
    Returns \c true if the global clipboard holds data of this program.
*/
bool QClipboard::ownsClipboard() const {
    QPlatformClipboard *p = platformClipboard();
    return p && p->ownsMode(Clipboard);
}

/*!
    Returns \c false: Nt has no find buffer.
*/
bool QClipboard::ownsFindBuffer() const { return false; }

/*!
    Returns the text of the clipboard \a mode, or an empty string if it holds none.
*/
QString QClipboard::text(QClipboard::Mode mode) const {
    const QMimeData *data = mimeData(mode);
    return data ? data->text() : QString();
}

/*!
    \overload

    Returns the text of the clipboard \a mode in the subtype \a subtype, such as \c plain or \c html; an empty
    \a subtype is set to the first text subtype that the clipboard has.
*/
QString QClipboard::text(QString &subtype, QClipboard::Mode mode) const {
    const QMimeData *data = mimeData(mode);
    if (!data)
        return QString();
    if (subtype.isEmpty()) {
        for (const QString &format : data->formats()) {
            if (format.startsWith(QLatin1String("text/"))) {
                subtype = format.mid(5).section(QLatin1Char(';'), 0, 0);
                break;
            }
        }
    }
    if (subtype == QLatin1String("plain"))
        return data->text();
    return QString::fromUtf8(data->data(QStringLiteral("text/") + subtype));
}

/*!
    Puts \a text on the clipboard \a mode.
*/
void QClipboard::setText(const QString &text, QClipboard::Mode mode) {
    auto *data = new QMimeData;
    data->setText(text);
    setMimeData(data, mode);
}

/*!
    Returns the data of the clipboard \a mode, or \c nullptr if it holds none. The clipboard keeps the data, which
    is valid until the clipboard changes.
*/
const QMimeData *QClipboard::mimeData(QClipboard::Mode mode) const {
    QPlatformClipboard *p = platformClipboard();
    return p ? p->mimeData(mode) : nullptr;
}

/*!
    Puts \a data on the clipboard \a mode, and takes ownership of it. \c nullptr clears the clipboard.
*/
void QClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode) {
    QPlatformClipboard *p = platformClipboard();
    if (!p || !p->supportsMode(mode)) {
        delete data;
        return;
    }
    p->setMimeData(data, mode);
}

/*!
    Returns the image of the clipboard \a mode, read from its \c image/png data if it has no QImage; a null image
    if it holds none.
*/
QImage QClipboard::image(QClipboard::Mode mode) const {
    const QMimeData *data = mimeData(mode);
    if (!data)
        return QImage();
    if (data->hasImage()) {
        const QVariant v = data->imageData();
        if (v.canConvert<QImage>())
            return v.value<QImage>();
    }
    for (const char *type : {"image/png", "image/bmp", "image/jpeg"}) {
        const QByteArray bytes = data->data(QString::fromLatin1(type));
        if (!bytes.isEmpty())
            return QImage::fromData(bytes);
    }
    return QImage();
}

/*!
    Returns the image of the clipboard \a mode as a pixmap.
*/
QPixmap QClipboard::pixmap(QClipboard::Mode mode) const { return QPixmap::fromImage(image(mode)); }

/*!
    Puts \a image on the clipboard \a mode, as a QImage and as \c image/png data for other programs.
*/
void QClipboard::setImage(const QImage &image, QClipboard::Mode mode) {
    auto *data = new QMimeData;
    data->setImageData(QVariant::fromValue(image));
    QByteArray png;
    QBuffer buffer(&png);
    if (buffer.open(QIODevice::WriteOnly) && image.save(&buffer, "PNG"))
        data->setData(QStringLiteral("image/png"), png);
    setMimeData(data, mode);
}

/*!
    Puts \a pixmap on the clipboard \a mode, as setImage() does.
*/
void QClipboard::setPixmap(const QPixmap &pixmap, QClipboard::Mode mode) { setImage(pixmap.toImage(), mode); }

/*!
    \since 6.0

    Returns the clipboard of the application.
*/
QClipboard *QGuiApplication::clipboard() {
    static QPointer<QClipboard> clipboard; // a child of the application, made again for a new one
    if (!clipboard)
        clipboard = new QClipboard(qGuiApp);
    return clipboard.data();
}

#include "moc_qclipboard.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
