// MIT License - see LICENSE
#include "qxcbclipboard_p.h"

#include "qxcbconnection_p.h"

#include <QtCore/qbuffer.h>
#include <QtCore/qelapsedtimer.h>
#include <QtCore/qmimedata.h>
#include <QtGui/qimage.h>

#include <poll.h>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr int kTimeoutMs = 1000;

// The formats that mean text, from the best.
bool isTextTarget(const QString &name) {
    return name == QLatin1String("UTF8_STRING") || name == QLatin1String("STRING") || name == QLatin1String("TEXT") ||
           name.startsWith(QLatin1String("text/plain"));
}

// The data of another program, read from its selection format by format when it is asked for.
class ForeignMimeData : public QMimeData {
public:
    ForeignMimeData(QXcbClipboard *clipboard, QXcbConnection *connection, xcb_atom_t selection)
        : m_clipboard(clipboard), m_connection(connection), m_selection(selection) {}

    QStringList formats() const override {
        if (!m_formatsKnown) {
            m_formatsKnown = true;
            QByteArray bytes;
            xcb_atom_t type = 0;
            if (m_clipboard->fetch(m_selection, atom("TARGETS"), &bytes, &type)) {
                const auto count = static_cast<std::size_t>(bytes.size()) / sizeof(xcb_atom_t);
                std::vector<xcb_atom_t> targets(count);
                if (count)
                    std::memcpy(targets.data(), bytes.constData(), count * sizeof(xcb_atom_t));
                for (xcb_atom_t target : targets) {
                    const QString name = m_clipboard->atomName(target);
                    const QString format = isTextTarget(name) ? QStringLiteral("text/plain") : name;
                    if (format.contains(QLatin1Char('/')) && !m_formats.contains(format))
                        m_formats.append(format);
                }
            }
        }
        return m_formats;
    }

    bool hasFormat(const QString &mimeType) const override { return formats().contains(mimeType); }

protected:
    QVariant retrieveData(const QString &mimeType, QMetaType preferredType) const override {
        (void)preferredType;
        if (!hasFormat(mimeType))
            return QVariant();
        QByteArray bytes;
        xcb_atom_t type = 0;
        if (mimeType == QLatin1String("text/plain")) {
            for (const char *target : {"UTF8_STRING", "text/plain;charset=utf-8", "STRING", "TEXT"})
                if (m_clipboard->fetch(m_selection, atom(target), &bytes, &type))
                    return bytes;
            return QVariant();
        }
        if (m_clipboard->fetch(m_selection, atom(mimeType.toLatin1().constData()), &bytes, &type))
            return bytes;
        return QVariant();
    }

private:
    xcb_atom_t atom(const char *name) const { return m_connection->atom(name); }

    QXcbClipboard *m_clipboard;
    QXcbConnection *m_connection;
    xcb_atom_t m_selection;
    mutable bool m_formatsKnown = false;
    mutable QStringList m_formats;
};

} // namespace

QXcbClipboard::QXcbClipboard(QXcbConnection *connection) : m_connection(connection) {
    xcb_connection_t *c = connection->xcb();
    m_window = xcb_generate_id(c);
    const uint32_t mask = XCB_CW_EVENT_MASK;
    const uint32_t values[] = {XCB_EVENT_MASK_PROPERTY_CHANGE};
    xcb_create_window(c, XCB_COPY_FROM_PARENT, m_window, connection->rootWindow(), -10, -10, 1, 1, 0,
                      XCB_WINDOW_CLASS_INPUT_ONLY, XCB_COPY_FROM_PARENT, mask, values);
    m_clipboard = connection->atom("CLIPBOARD");
    m_targets = connection->atom("TARGETS");
    m_text = connection->atom("UTF8_STRING");
    m_property = connection->atom("_NT_SELECTION");
    connection->flush();
}

QXcbClipboard::~QXcbClipboard() {
    if (m_window)
        xcb_destroy_window(m_connection->xcb(), m_window);
}

xcb_atom_t QXcbClipboard::selectionAtom(QClipboard::Mode mode) const {
    return mode == QClipboard::Selection ? static_cast<xcb_atom_t>(XCB_ATOM_PRIMARY) : m_clipboard;
}

bool QXcbClipboard::supportsMode(QClipboard::Mode mode) const {
    return mode == QClipboard::Clipboard || mode == QClipboard::Selection;
}

bool QXcbClipboard::ownsMode(QClipboard::Mode mode) const { return supportsMode(mode) && m_owner[mode]; }

QMimeData *QXcbClipboard::mimeData(QClipboard::Mode mode) {
    if (!supportsMode(mode))
        return nullptr;
    if (m_owner[mode])
        return m_data[mode].get();
    // Another program, or nobody, owns the selection: a fresh reader each time, as the owner may have changed.
    m_foreign[mode] = std::make_unique<ForeignMimeData>(this, m_connection, selectionAtom(mode));
    return m_foreign[mode].get();
}

void QXcbClipboard::setMimeData(QMimeData *data, QClipboard::Mode mode) {
    if (!supportsMode(mode)) {
        delete data;
        return;
    }
    m_data[mode].reset(data);
    m_owner[mode] = data != nullptr;
    xcb_set_selection_owner(m_connection->xcb(), data ? m_window : static_cast<xcb_window_t>(XCB_NONE),
                            selectionAtom(mode), XCB_CURRENT_TIME);
    m_connection->flush();
    emitChanged(mode);
}

QString QXcbClipboard::atomName(xcb_atom_t atom) {
    xcb_connection_t *c = m_connection->xcb();
    xcb_get_atom_name_reply_t *reply = xcb_get_atom_name_reply(c, xcb_get_atom_name(c, atom), nullptr);
    if (!reply)
        return QString();
    const QString name = QString::fromLatin1(xcb_get_atom_name_name(reply), xcb_get_atom_name_name_length(reply));
    std::free(reply);
    return name;
}

QByteArray QXcbClipboard::dataFor(const QMimeData *data, xcb_atom_t target, xcb_atom_t *type) {
    const QString name = atomName(target);
    *type = target;
    if (isTextTarget(name)) {
        if (!data->hasText())
            return QByteArray();
        *type = name == QLatin1String("STRING") ? static_cast<xcb_atom_t>(XCB_ATOM_STRING) : m_text;
        return name == QLatin1String("STRING") ? data->text().toLatin1() : data->text().toUtf8();
    }
    if (name == QLatin1String("image/png") && !data->hasFormat(name) && data->hasImage()) {
        QByteArray png;
        QBuffer buffer(&png);
        if (buffer.open(QIODevice::WriteOnly))
            data->imageData().value<QImage>().save(&buffer, "PNG");
        return png;
    }
    return data->hasFormat(name) ? data->data(name) : QByteArray();
}

void QXcbClipboard::answer(const xcb_selection_request_event_t *request) {
    xcb_connection_t *c = m_connection->xcb();
    QClipboard::Mode mode = request->selection == XCB_ATOM_PRIMARY ? QClipboard::Selection : QClipboard::Clipboard;
    const QMimeData *data = m_owner[mode] ? m_data[mode].get() : nullptr;
    // An old client gives no property: the target names it then.
    const xcb_atom_t property = request->property == XCB_NONE ? request->target : request->property;
    bool ok = false;
    if (data && request->target == m_targets) {
        std::vector<xcb_atom_t> targets{m_targets};
        if (data->hasText())
            for (const char *t : {"UTF8_STRING", "STRING", "TEXT", "text/plain;charset=utf-8"})
                targets.push_back(m_connection->atom(t));
        for (const QString &format : data->formats())
            if (format != QLatin1String("text/plain"))
                targets.push_back(m_connection->atom(format.toLatin1().constData()));
        if (data->hasImage() && !data->hasFormat(QStringLiteral("image/png")))
            targets.push_back(m_connection->atom("image/png"));
        xcb_change_property(c, XCB_PROP_MODE_REPLACE, request->requestor, property, XCB_ATOM_ATOM, 32,
                            static_cast<uint32_t>(targets.size()), targets.data());
        ok = true;
    } else if (data) {
        xcb_atom_t type = 0;
        const QByteArray bytes = dataFor(data, request->target, &type);
        if (!bytes.isEmpty() && static_cast<std::size_t>(bytes.size()) < m_connection->maxRequestBytes() - 64) {
            xcb_change_property(c, XCB_PROP_MODE_REPLACE, request->requestor, property, type, 8,
                                static_cast<uint32_t>(bytes.size()), bytes.constData());
            ok = true;
        }
    }
    xcb_selection_notify_event_t notify;
    std::memset(&notify, 0, sizeof notify);
    notify.response_type = XCB_SELECTION_NOTIFY;
    notify.time = request->time;
    notify.requestor = request->requestor;
    notify.selection = request->selection;
    notify.target = request->target;
    notify.property = ok ? property : static_cast<xcb_atom_t>(XCB_NONE);
    xcb_send_event(c, 0, request->requestor, XCB_EVENT_MASK_NO_EVENT, reinterpret_cast<const char *>(&notify));
    m_connection->flush();
}

bool QXcbClipboard::handleEvent(xcb_generic_event_t *event) {
    switch (event->response_type & ~0x80) {
    case XCB_SELECTION_REQUEST: {
        const auto *request = reinterpret_cast<const xcb_selection_request_event_t *>(event);
        if (request->owner != m_window)
            return false;
        answer(request);
        return true;
    }
    case XCB_SELECTION_CLEAR: {
        const auto *clear = reinterpret_cast<const xcb_selection_clear_event_t *>(event);
        if (clear->owner != m_window)
            return false;
        // Another program took the selection over.
        const QClipboard::Mode mode = clear->selection == XCB_ATOM_PRIMARY ? QClipboard::Selection : QClipboard::Clipboard;
        m_owner[mode] = false;
        m_data[mode].reset();
        emitChanged(mode);
        return true;
    }
    default:
        return false;
    }
}

bool QXcbClipboard::fetch(xcb_atom_t selection, xcb_atom_t target, QByteArray *data, xcb_atom_t *type) {
    xcb_connection_t *c = m_connection->xcb();
    xcb_delete_property(c, m_window, m_property);
    xcb_convert_selection(c, m_window, selection, target, m_property, XCB_CURRENT_TIME);
    xcb_flush(c);
    QElapsedTimer timer;
    timer.start();
    const int fd = xcb_get_file_descriptor(c);
    while (timer.elapsed() < kTimeoutMs) {
        xcb_generic_event_t *e = xcb_poll_for_event(c);
        if (!e) {
            if (xcb_connection_has_error(c))
                return false;
            pollfd p{fd, POLLIN, 0};
            ::poll(&p, 1, static_cast<int>(std::max<qint64>(1, kTimeoutMs - timer.elapsed())));
            continue;
        }
        const auto kind = static_cast<uint8_t>(e->response_type & ~0x80);
        if (kind == XCB_SELECTION_NOTIFY) {
            const auto *notify = reinterpret_cast<const xcb_selection_notify_event_t *>(e);
            if (notify->requestor == m_window && notify->selection == selection) {
                const bool given = notify->property != XCB_NONE;
                std::free(e);
                if (!given)
                    return false;
                xcb_get_property_reply_t *reply = xcb_get_property_reply(
                    c, xcb_get_property(c, 1, m_window, m_property, XCB_GET_PROPERTY_TYPE_ANY, 0, 0x1fffffff),
                    nullptr);
                if (!reply)
                    return false;
                const bool incremental = reply->type == m_connection->atom("INCR");
                if (!incremental) {
                    *type = reply->type;
                    // xcb_get_property_value_length() already returns bytes, not items
                    *data = QByteArray(static_cast<const char *>(xcb_get_property_value(reply)),
                                       xcb_get_property_value_length(reply));
                }
                std::free(reply);
                return !incremental;
            }
        }
        // Requests for this program's own selections are answered while it waits, so that two Nt programs that
        // read from each other at the same time do not wait for each other.
        if (kind == XCB_SELECTION_REQUEST && handleEvent(e)) {
            std::free(e);
            continue;
        }
        m_connection->defer(e);
    }
    return false;
}
