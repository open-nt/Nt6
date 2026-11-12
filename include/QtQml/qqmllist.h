// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmetatype.h>

class QObject;

/*!
    \class QQmlListProperty
    \brief The QQmlListProperty class is a list of objects that QML documents fill.

    A property of this type takes the objects declared in it, or assigned to it as a list. The usual way is
    to give it a QList that the object owns:

    \code
    class Shelf : public QObject {
        Q_OBJECT
        Q_PROPERTY(QQmlListProperty<QObject> books READ books)
        Q_CLASSINFO("DefaultProperty", "books")
    public:
        QQmlListProperty<QObject> books() { return QQmlListProperty<QObject>(this, &m_books); }
    private:
        QList<QObject *> m_books;
    };
    \endcode

    Functions can do the work instead of a QList, for lists that need to react to changes.

    \differences
    Nt fills list properties of any \c QQmlListProperty<T> through \c{QQmlListProperty<QObject>}, so
    \c T must be a QObject subclass.
    \enddifferences

    \sa {Using Nt QML}
*/
template <typename T> class QQmlListProperty {
public:
    /*!
        \typedef QQmlListProperty::AppendFunction
        A function that appends an object to the list.
    */
    using AppendFunction = void (*)(QQmlListProperty<T> *, T *);
    /*!
        \typedef QQmlListProperty::CountFunction
        A function that returns the number of objects in the list.
    */
    using CountFunction = qsizetype (*)(QQmlListProperty<T> *);
    /*!
        \typedef QQmlListProperty::AtFunction
        A function that returns the object at an index of the list.
    */
    using AtFunction = T *(*)(QQmlListProperty<T> *, qsizetype);
    /*!
        \typedef QQmlListProperty::ClearFunction
        A function that empties the list.
    */
    using ClearFunction = void (*)(QQmlListProperty<T> *);
    /*!
        \typedef QQmlListProperty::ReplaceFunction
        A function that replaces the object at an index of the list.
    */
    using ReplaceFunction = void (*)(QQmlListProperty<T> *, qsizetype, T *);
    /*!
        \typedef QQmlListProperty::RemoveLastFunction
        A function that removes the last object of the list.
    */
    using RemoveLastFunction = void (*)(QQmlListProperty<T> *);

    /*!
        Constructs an invalid list property.
    */
    QQmlListProperty() = default;
    /*!
        Constructs a list property of \a o backed by \a list, which \a o must outlive.
    */
    QQmlListProperty(QObject *o, QList<T *> *list)
        : object(o), data(list), append(appendList), count(countList), at(atList), clear(clearList),
          replace(replaceList), removeLast(removeLastList) {}
    /*!
        Constructs a list property of \a o with the user data \a d and the functions \a a (append),
        \a c (count), \a t (at) and \a cl (clear).
    */
    QQmlListProperty(QObject *o, void *d, AppendFunction a, CountFunction c, AtFunction t, ClearFunction cl)
        : object(o), data(d), append(a), count(c), at(t), clear(cl) {}
    /*!
        Constructs a list property of \a o with the user data \a d and the functions \a a (append),
        \a c (count), \a t (at), \a cl (clear), \a r (replace) and \a rl (remove last).
    */
    QQmlListProperty(QObject *o, void *d, AppendFunction a, CountFunction c, AtFunction t, ClearFunction cl,
                     ReplaceFunction r, RemoveLastFunction rl)
        : object(o), data(d), append(a), count(c), at(t), clear(cl), replace(r), removeLast(rl) {}
    /*!
        Constructs a read-only list property of \a o with the user data \a d and the functions \a c (count)
        and \a t (at).
    */
    QQmlListProperty(QObject *o, void *d, CountFunction c, AtFunction t) : object(o), data(d), count(c), at(t) {}

    /*!
        Returns \c true if this list property and \a o have the same object, data and functions.
    */
    bool operator==(const QQmlListProperty &o) const {
        return object == o.object && data == o.data && append == o.append && count == o.count && at == o.at &&
               clear == o.clear && replace == o.replace && removeLast == o.removeLast;
    }

    /*!
        The object that has the list.
    */
    QObject *object = nullptr;
    /*!
        The user data of the functions; the QList for the QList constructor.
    */
    void *data = nullptr;
    /*!
        The function that appends, or \c nullptr if the list is read-only.
    */
    AppendFunction append = nullptr;
    /*!
        The function that counts.
    */
    CountFunction count = nullptr;
    /*!
        The function that returns an object.
    */
    AtFunction at = nullptr;
    /*!
        The function that empties the list, or \c nullptr.
    */
    ClearFunction clear = nullptr;
    /*!
        The function that replaces an object, or \c nullptr.
    */
    ReplaceFunction replace = nullptr;
    /*!
        The function that removes the last object, or \c nullptr.
    */
    RemoveLastFunction removeLast = nullptr;

private:
    static void appendList(QQmlListProperty *p, T *v) { static_cast<QList<T *> *>(p->data)->append(v); }
    static qsizetype countList(QQmlListProperty *p) { return static_cast<QList<T *> *>(p->data)->size(); }
    static T *atList(QQmlListProperty *p, qsizetype i) { return static_cast<QList<T *> *>(p->data)->at(i); }
    static void clearList(QQmlListProperty *p) { static_cast<QList<T *> *>(p->data)->clear(); }
    static void replaceList(QQmlListProperty *p, qsizetype i, T *v) { (*static_cast<QList<T *> *>(p->data))[i] = v; }
    static void removeLastList(QQmlListProperty *p) { static_cast<QList<T *> *>(p->data)->removeLast(); }
};
