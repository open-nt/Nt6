// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QStyle;

class QStylePlugin : public QObject {
    Q_OBJECT
public:
    explicit QStylePlugin(QObject *parent = nullptr);
    ~QStylePlugin() override;

    virtual QStyle *create(const QString &key) = 0;
};
