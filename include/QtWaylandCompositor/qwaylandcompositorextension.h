// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>

struct wl_interface;

class QWaylandCompositor;
class QWaylandCompositorExtension;

class QWaylandObject : public QObject {
    Q_OBJECT
public:
    ~QWaylandObject() override;

    QWaylandCompositorExtension *extension(const QByteArray &name);
    QWaylandCompositorExtension *extension(const wl_interface *interface);
    QList<QWaylandCompositorExtension *> extensions() const;
    void addExtension(QWaylandCompositorExtension *extension);
    void removeExtension(QWaylandCompositorExtension *extension);

protected:
    explicit QWaylandObject(QObject *parent = nullptr);

private:
    QList<QWaylandCompositorExtension *> m_extensions;
};

class QWaylandCompositorExtension : public QWaylandObject {
    Q_OBJECT
public:
    QWaylandCompositorExtension();
    explicit QWaylandCompositorExtension(QWaylandObject *container);
    ~QWaylandCompositorExtension() override;

    /*!
        Returns the object the extension belongs to, or \c nullptr.
    */
    QWaylandObject *extensionContainer() const { return m_container; }
    void setExtensionContainer(QWaylandObject *container);

    virtual void initialize();
    /*!
        Returns \c true if initialize() has run.
    */
    bool isInitialized() const { return m_initialized; }

    /*!
        Returns the protocol interface the extension implements, as an opaque pointer. Nt has no libwayland, so
        the default is \c nullptr; an extension that overrides this returns its own marker.
    */
    virtual const wl_interface *extensionInterface() const { return nullptr; }
    /*!
        Returns the name of the protocol interface the extension implements, such as \c "xdg_wm_base".
    */
    virtual QByteArray extensionInterfaceName() const = 0;

protected:
    bool event(QEvent *event) override;

private:
    QWaylandObject *m_container = nullptr;
    bool m_initialized = false;
};

template <typename T> class QWaylandCompositorExtensionTemplate : public QWaylandCompositorExtension {
public:
    QWaylandCompositorExtensionTemplate() = default;
    explicit QWaylandCompositorExtensionTemplate(QWaylandObject *container) : QWaylandCompositorExtension(container) {}

    static T *findIn(QWaylandObject *container) {
        if (!container)
            return nullptr;
        return qobject_cast<T *>(container->extension(T::interfaceName()));
    }

    QByteArray extensionInterfaceName() const override { return T::interfaceName(); }
};
