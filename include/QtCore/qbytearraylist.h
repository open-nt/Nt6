// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>

class QByteArrayView;

/*!
    \class QByteArrayList
    \brief The QByteArrayList class provides a list of byte arrays.

    QByteArrayList is another name of QList<QByteArray>, with join() added
    to what QList has. Lists are usually produced by QByteArray::split() and
    combined back with join():

    \code
    QByteArrayList fields = QByteArray("a,b,c").split(',');
    fields << "d";
    QByteArray csv = fields.join(',');   // "a,b,c,d"
    \endcode

    \differences
    None.
    \enddifferences

    \sa QByteArray, QList, QStringList
*/
template <> struct QListSpecialMethods<QByteArray> {
    /*!
        Returns the byte arrays of the list joined into one, with nothing
        between them.
    */
    QByteArray join() const {
        QByteArray r;
        for (const QByteArray &a : self())
            r += a;
        return r;
    }
    /*!
        \overload

        Returns the byte arrays of the list joined into one, with the bytes of
        \a sep between each two of them.
    */
    QByteArray join(QByteArrayView sep) const;
    /*!
        \overload

        Returns the byte arrays of the list joined into one, with the byte
        \a sep between each two of them.
    */
    QByteArray join(char sep) const {
        QByteArray r;
        bool first = true;
        for (const QByteArray &a : self()) {
            if (!first)
                r += sep;
            r += a;
            first = false;
        }
        return r;
    }

protected:
    /*! \internal */
    ~QListSpecialMethods() = default;
    // The names that QList brings into its scope, where QStringList has overloads; see qlist.h.
    void contains() const = delete;
    void indexOf() const = delete;
    void lastIndexOf() const = delete;

private:
    const QList<QByteArray> &self() const { return *static_cast<const QList<QByteArray> *>(this); }
};
