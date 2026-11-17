// MIT License - see LICENSE
#pragma once

#include "qpa/qplatformclipboard_p.h"

#include <QtCore/qbytearray.h>
#include <QtCore/qstringlist.h>

#include <xcb/xcb.h>

class QXcbConnection;

// The X11 clipboard: CLIPBOARD for QClipboard::Clipboard and PRIMARY for QClipboard::Selection. This program owns
// a selection while it holds data there, and answers the requests of other programs from a hidden window; data of
// another program is read when it is asked for.
class QXcbClipboard : public QPlatformClipboard {
public:
    explicit QXcbClipboard(QXcbConnection *connection);
    ~QXcbClipboard() override;

    QMimeData *mimeData(QClipboard::Mode mode) override;
    void setMimeData(QMimeData *data, QClipboard::Mode mode) override;
    bool supportsMode(QClipboard::Mode mode) const override;
    bool ownsMode(QClipboard::Mode mode) const override;

    // Returns true if `event` was a selection event for the clipboard, which handles it.
    bool handleEvent(xcb_generic_event_t *event);

    // Reads `target` of the selection `selection`, waiting for the owner; false if it gives nothing in time.
    bool fetch(xcb_atom_t selection, xcb_atom_t target, QByteArray *data, xcb_atom_t *type);
    QString atomName(xcb_atom_t atom);

private:
    xcb_atom_t selectionAtom(QClipboard::Mode mode) const;
    void answer(const xcb_selection_request_event_t *request);
    QByteArray dataFor(const QMimeData *data, xcb_atom_t target, xcb_atom_t *type);

    QXcbConnection *m_connection;
    xcb_window_t m_window = 0; // owns the selections and receives what is read
    xcb_atom_t m_clipboard = 0;
    xcb_atom_t m_targets = 0;
    xcb_atom_t m_text = 0;
    xcb_atom_t m_property = 0;
    bool m_owner[QClipboard::LastMode + 1] = {};
    std::unique_ptr<QMimeData> m_foreign[QClipboard::LastMode + 1]; // data of another program
};
