// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>

class QFileSelector : public QObject {
    Q_OBJECT
public:
    explicit QFileSelector(QObject *parent = nullptr);
    ~QFileSelector() override;

    QString select(const QString &filePath) const;
    QUrl select(const QUrl &filePath) const;

    QStringList extraSelectors() const;
    void setExtraSelectors(const QStringList &list);

    QStringList allSelectors() const;

private:
    QStringList m_extra;
};
