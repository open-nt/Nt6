// MIT License - see LICENSE
#include <QtCore/qsignalmapper.h>

/*!
    \class QSignalMapper
    \brief The QSignalMapper class bundles signals from identifiable senders.

    A mapper collects the signals of several objects and re-emits each as a
    signal that carries an identifier of the sender: an integer, a string or an
    object. It lets one slot serve many senders, which is handy when the
    senders are created in a loop:

    \code
    QSignalMapper mapper;
    for (int i = 0; i < 3; ++i) {
        QTimer *timer = new QTimer(&mapper);
        mapper.setMapping(timer, i);
        QObject::connect(timer, &QTimer::timeout, &mapper, qOverload<>(&QSignalMapper::map));
    }
    QObject::connect(&mapper, &QSignalMapper::mappedInt, [](int i) { qDebug() << "timer" << i; });
    \endcode

    Connect each sender's signal to map(), which finds the sender with
    QObject::sender(), or to the map() overload that takes it explicitly.
    A sender can have an integer, a string and an object mapping at the same
    time; one signal is emitted for each. A mapping is dropped when its sender
    is destroyed.

    \differences
    The Qt 5 signals \c mapped(int), \c mapped(const QString &) and
    \c mapped(QObject *) were removed in Qt 6, and are not provided here
    either; there is no \c mappedWidget().
    \enddifferences

    \sa QObject::sender()
*/

/*!
    Constructs a mapper with the given \a parent.
*/
QSignalMapper::QSignalMapper(QObject *parent) : QObject(parent) {}

/*!
    Destroys the mapper.
*/
QSignalMapper::~QSignalMapper() = default;

void QSignalMapper::watch(QObject *sender) {
    if (m_ints.contains(sender) || m_strings.contains(sender) || m_objects.contains(sender))
        return;
    QObject::connect(sender, &QObject::destroyed, this, [this](QObject *gone) { removeMappings(gone); });
}

/*!
    Adds a mapping so that when map() is called by \a sender, mappedInt() is
    emitted with \a id. Replaces an earlier integer mapping of \a sender.

    \sa mapping(), removeMappings()
*/
void QSignalMapper::setMapping(QObject *sender, int id) {
    if (!sender)
        return;
    watch(sender);
    m_ints.insert(sender, id);
}

/*!
    \overload

    Adds a mapping so that when map() is called by \a sender, mappedString() is
    emitted with \a text.
*/
void QSignalMapper::setMapping(QObject *sender, const QString &text) {
    if (!sender)
        return;
    watch(sender);
    m_strings.insert(sender, text);
}

/*!
    \overload

    Adds a mapping so that when map() is called by \a sender, mappedObject() is
    emitted with \a object.
*/
void QSignalMapper::setMapping(QObject *sender, QObject *object) {
    if (!sender)
        return;
    watch(sender);
    m_objects.insert(sender, object);
}

/*!
    Removes all mappings of \a sender.

    \sa setMapping()
*/
void QSignalMapper::removeMappings(QObject *sender) {
    m_ints.remove(sender);
    m_strings.remove(sender);
    m_objects.remove(sender);
}

/*!
    Returns the sender that has the integer mapping \a id, or \c nullptr if
    there is none.

    \sa setMapping()
*/
QObject *QSignalMapper::mapping(int id) const {
    for (auto it = m_ints.cbegin(); it != m_ints.cend(); ++it)
        if (it.value() == id)
            return it.key();
    return nullptr;
}

/*!
    \overload

    Returns the sender that has the string mapping \a text, or \c nullptr if
    there is none.
*/
QObject *QSignalMapper::mapping(const QString &text) const {
    for (auto it = m_strings.cbegin(); it != m_strings.cend(); ++it)
        if (it.value() == text)
            return it.key();
    return nullptr;
}

/*!
    \overload

    Returns the sender that has the object mapping \a object, or \c nullptr if
    there is none.
*/
QObject *QSignalMapper::mapping(QObject *object) const {
    for (auto it = m_objects.cbegin(); it != m_objects.cend(); ++it)
        if (it.value() == object)
            return it.key();
    return nullptr;
}

/*!
    Emits the signals that correspond to the mappings of the sender of the
    signal that called this slot, as found by QObject::sender(). Does nothing
    if the sender has no mapping.

    \sa setMapping()
*/
void QSignalMapper::map() { map(sender()); }

/*!
    \overload

    Emits the signals that correspond to the mappings of \a sender, in the
    order mappedInt(), mappedString(), mappedObject().
*/
void QSignalMapper::map(QObject *sender) {
    if (!sender)
        return;
    const auto id = m_ints.constFind(sender);
    const auto text = m_strings.constFind(sender);
    const auto object = m_objects.constFind(sender);
    // Copy first: a slot may change the mappings.
    const bool hasId = id != m_ints.cend(), hasText = text != m_strings.cend(), hasObject = object != m_objects.cend();
    const int i = hasId ? id.value() : 0;
    const QString s = hasText ? text.value() : QString();
    QObject *o = hasObject ? object.value() : nullptr;
    if (hasId)
        mappedInt(i);
    if (hasText)
        mappedString(s);
    if (hasObject)
        mappedObject(o);
}

#include "moc_qsignalmapper.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
