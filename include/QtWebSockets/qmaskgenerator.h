// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>

class QMaskGenerator : public QObject {
    Q_OBJECT
public:
    explicit QMaskGenerator(QObject *parent = nullptr);
    ~QMaskGenerator() override;

    /*!
        Prepares the generator for use, for example by seeding a random
        number generator. Returns \c true on success; a socket does not use
        a generator whose seed() fails.

        \sa nextMask()
    */
    virtual bool seed() = 0;
    /*!
        Returns the next 32-bit masking key. Keys must be unpredictable, so
        that a script cannot choose the bytes that appear on the wire.

        \sa seed()
    */
    virtual quint32 nextMask() = 0;
};
