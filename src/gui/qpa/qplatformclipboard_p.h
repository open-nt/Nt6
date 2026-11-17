// MIT License - see LICENSE
#pragma once

#include <QtGui/qclipboard.h>

#include <memory>

class QMimeData;

// Where QClipboard keeps its data. The default keeps it in the process, which is all a platform without a window
// system needs; a platform with one, such as xcb, shares it with the other programs.
class QPlatformClipboard {
public:
    QPlatformClipboard();
    virtual ~QPlatformClipboard();
    QPlatformClipboard(const QPlatformClipboard &) = delete;
    QPlatformClipboard &operator=(const QPlatformClipboard &) = delete;

    // The data of `mode`, or null if there is none. Owned by the platform clipboard.
    virtual QMimeData *mimeData(QClipboard::Mode mode);
    // Takes ownership of `data` (null clears) and makes it the data of `mode`.
    virtual void setMimeData(QMimeData *data, QClipboard::Mode mode);
    virtual bool supportsMode(QClipboard::Mode mode) const;
    virtual bool ownsMode(QClipboard::Mode mode) const;

    // Tells QClipboard, and through it the application, that the data of `mode` changed.
    static void emitChanged(QClipboard::Mode mode);

protected:
    std::unique_ptr<QMimeData> m_data[QClipboard::LastMode + 1];
};
