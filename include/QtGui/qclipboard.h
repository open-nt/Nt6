// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtGui/qimage.h>
#include <QtGui/qpixmap.h>

class QMimeData;

class QClipboard : public QObject {
    Q_OBJECT
public:
    enum Mode { Clipboard, Selection, FindBuffer, LastMode = FindBuffer };

    void clear(QClipboard::Mode mode = Clipboard);
    bool supportsSelection() const;
    bool supportsFindBuffer() const;
    bool ownsSelection() const;
    bool ownsClipboard() const;
    bool ownsFindBuffer() const;

    QString text(QClipboard::Mode mode = Clipboard) const;
    QString text(QString &subtype, QClipboard::Mode mode = Clipboard) const;
    void setText(const QString &text, QClipboard::Mode mode = Clipboard);

    const QMimeData *mimeData(QClipboard::Mode mode = Clipboard) const;
    void setMimeData(QMimeData *data, QClipboard::Mode mode = Clipboard);

    QImage image(QClipboard::Mode mode = Clipboard) const;
    QPixmap pixmap(QClipboard::Mode mode = Clipboard) const;
    void setImage(const QImage &image, QClipboard::Mode mode = Clipboard);
    void setPixmap(const QPixmap &pixmap, QClipboard::Mode mode = Clipboard);

signals:
    /*!
        This signal is emitted when the data of the clipboard \a mode changes.
    */
    void changed(QClipboard::Mode mode) { emitSignal(&QClipboard::changed, mode); }
    /*!
        This signal is emitted when the data of the global clipboard changes.
    */
    void dataChanged() { emitSignal(&QClipboard::dataChanged); }
    /*!
        This signal is emitted when the selection, the text the user last selected on X11, changes.
    */
    void selectionChanged() { emitSignal(&QClipboard::selectionChanged); }
    /*!
        This signal is emitted when the find buffer changes. Nt has none, so it is never emitted.
    */
    void findBufferChanged() { emitSignal(&QClipboard::findBufferChanged); }

private:
    friend class QGuiApplication;
    friend class QPlatformClipboard;
    explicit QClipboard(QObject *parent = nullptr);
    ~QClipboard() override;
    void emitChanged(QClipboard::Mode mode);
};
