// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qstringlist.h>

/*!
    \class QFactoryInterface
    \brief The QFactoryInterface class is the base of the interfaces of plugins that make objects by key.

    A plugin interface that makes objects of several kinds derives from it and lists the kinds with keys().

    \differences
    None.
    \enddifferences

    \sa QPluginLoader
*/
struct QFactoryInterface {
    /*!
        Destroys the interface.
    */
    virtual ~QFactoryInterface() = default;
    /*!
        \fn QStringList QFactoryInterface::keys() const
        Returns the keys of the kinds of objects that the plugin makes.
    */
    virtual QStringList keys() const = 0;
};

Q_DECLARE_INTERFACE(QFactoryInterface, "org.qt-project.Qt.QFactoryInterface")
