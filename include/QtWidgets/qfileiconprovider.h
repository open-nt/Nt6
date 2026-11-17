// MIT License - see LICENSE
#pragma once

#include <QtGui/qabstractfileiconprovider.h>

class QFileIconProvider : public QAbstractFileIconProvider {
public:
    QFileIconProvider();
    ~QFileIconProvider() override;

    QIcon icon(IconType type) const override;
    QIcon icon(const QFileInfo &info) const override;
};
