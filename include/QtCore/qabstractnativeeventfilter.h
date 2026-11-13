// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

class QByteArray;

class QAbstractNativeEventFilter {
public:
    QAbstractNativeEventFilter();
    virtual ~QAbstractNativeEventFilter();
    QAbstractNativeEventFilter(const QAbstractNativeEventFilter &) = delete;
    QAbstractNativeEventFilter &operator=(const QAbstractNativeEventFilter &) = delete;
    QAbstractNativeEventFilter(QAbstractNativeEventFilter &&) = delete;
    QAbstractNativeEventFilter &operator=(QAbstractNativeEventFilter &&) = delete;

    virtual bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr *result) = 0;
};
