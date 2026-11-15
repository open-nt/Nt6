// MIT License - see LICENSE
#include <QtWaylandCompositor/qwaylandcompositorextension.h>

#include <QtCore/qcoreapplication.h>
#include <QtCore/qcoreevent.h>

/*!
    \class QWaylandObject
    \brief The QWaylandObject class is the base of the compositor classes that can carry extensions.

    A compositor, an output, a seat or a surface can be given extensions: objects that implement a further
    protocol interface for it, such as QWaylandXdgShell for a QWaylandCompositor. addExtension() attaches one,
    and extension() finds it again by the name of its interface.

    \differences
    Extensions are looked up by interface name; the \c wl_interface overload takes an opaque pointer that only an
    extension of Nt itself can produce, as Nt has no libwayland.
    \enddifferences

    \sa QWaylandCompositorExtension
*/

/*!
    Constructs an object with the given \a parent.
*/
QWaylandObject::QWaylandObject(QObject *parent) : QObject(parent) {}

/*!
    Destroys the object. Its extensions lose their container.
*/
QWaylandObject::~QWaylandObject() {
    for (QWaylandCompositorExtension *e : std::as_const(m_extensions))
        e->setExtensionContainer(nullptr);
}

/*!
    Returns the extension that implements the interface called \a name, or \c nullptr.
*/
QWaylandCompositorExtension *QWaylandObject::extension(const QByteArray &name) {
    for (QWaylandCompositorExtension *e : std::as_const(m_extensions)) {
        if (e->extensionInterfaceName() == name)
            return e;
    }
    return nullptr;
}

/*!
    \overload

    Returns the extension whose extensionInterface() is \a interface, or \c nullptr.
*/
QWaylandCompositorExtension *QWaylandObject::extension(const wl_interface *interface) {
    for (QWaylandCompositorExtension *e : std::as_const(m_extensions)) {
        if (interface && e->extensionInterface() == interface)
            return e;
    }
    return nullptr;
}

/*!
    Returns the extensions of the object.
*/
QList<QWaylandCompositorExtension *> QWaylandObject::extensions() const { return m_extensions; }

/*!
    Adds \a extension to the object. QWaylandCompositorExtension::setExtensionContainer() calls this.
*/
void QWaylandObject::addExtension(QWaylandCompositorExtension *extension) {
    if (!m_extensions.contains(extension))
        m_extensions.append(extension);
}

/*!
    Removes \a extension from the object.
*/
void QWaylandObject::removeExtension(QWaylandCompositorExtension *extension) { m_extensions.removeAll(extension); }

/*!
    \class QWaylandCompositorExtension
    \brief The QWaylandCompositorExtension class is the base of objects that add a protocol interface to a
    compositor object.

    An extension is created with its container (usually a QWaylandCompositor) and initialized once, either by
    initialize() or, when control returns to the event loop after construction, by a posted event. Shell
    protocols such as QWaylandXdgShell are extensions.

    \differences
    extensionInterface() is not pure virtual and returns \c nullptr by default, since there are no \c wl_interface
    structures; extensionInterfaceName() names the interface instead.
    \enddifferences

    \sa QWaylandCompositorExtensionTemplate, QWaylandXdgShell
*/

/*!
    Constructs an extension without a container; call setExtensionContainer() before initialize().
*/
QWaylandCompositorExtension::QWaylandCompositorExtension() : QWaylandObject(nullptr) {
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Constructs an extension of \a container.
*/
QWaylandCompositorExtension::QWaylandCompositorExtension(QWaylandObject *container) : QWaylandObject(container) {
    setExtensionContainer(container);
    QCoreApplication::postEvent(this, new QEvent(QEvent::User));
}

/*!
    Destroys the extension and removes it from its container.
*/
QWaylandCompositorExtension::~QWaylandCompositorExtension() {
    if (m_container)
        m_container->removeExtension(this);
}

/*!
    Makes \a container the object the extension belongs to, replacing the previous one.
*/
void QWaylandCompositorExtension::setExtensionContainer(QWaylandObject *container) {
    if (m_container == container)
        return;
    if (m_container)
        m_container->removeExtension(this);
    m_container = container;
    if (m_container)
        m_container->addExtension(this);
}

/*!
    Initializes the extension: registers its interface with the compositor. Subclasses call the base
    implementation first. Runs once; an extension that is not initialized by hand is initialized when the event
    loop runs after its construction.
*/
void QWaylandCompositorExtension::initialize() { m_initialized = true; }

/*!
    \reimp
*/
bool QWaylandCompositorExtension::event(QEvent *event) {
    if (event->type() == QEvent::User) {
        if (!m_initialized)
            initialize();
        return true;
    }
    return QWaylandObject::event(event);
}

/*!
    \class QWaylandCompositorExtensionTemplate
    \brief The QWaylandCompositorExtensionTemplate class is the convenient base of an extension of class \c T.

    It implements extensionInterfaceName() with \c{T::interfaceName()} and adds findIn(), which looks the
    extension up in a container:

    \code
    QWaylandXdgShell *shell = QWaylandXdgShell::findIn(compositor);
    \endcode

    \differences
    There is no \c checkInterface() and no resource template arguments.
    \enddifferences
*/

/*!
    \fn template <typename T> T *QWaylandCompositorExtensionTemplate<T>::findIn(QWaylandObject *container)

    Returns the extension of class \c T attached to \a container, or \c nullptr.
*/

/*!
    \fn template <typename T> QWaylandCompositorExtensionTemplate<T>::QWaylandCompositorExtensionTemplate()

    Constructs an extension without a container.
*/

/*!
    \fn template <typename T> QWaylandCompositorExtensionTemplate<T>::QWaylandCompositorExtensionTemplate(QWaylandObject *container)

    Constructs an extension of \a container.
*/

/*!
    \fn template <typename T> QByteArray QWaylandCompositorExtensionTemplate<T>::extensionInterfaceName() const
    \reimp
*/

#include "moc_qwaylandcompositorextension.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
