// MIT License - see LICENSE
#pragma once

#include <QtCore/qhashfunctions.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>

class QMqttTopicName {
public:
    QMqttTopicName(const QString &name = QString());
    QMqttTopicName(const QLatin1String &name);

    void swap(QMqttTopicName &other) noexcept { m_name.swap(other.m_name); }

    QString name() const;
    void setName(const QString &name);

    bool isValid() const;
    int levelCount() const;
    QStringList levels() const;

    friend bool operator==(const QMqttTopicName &a, const QMqttTopicName &b) noexcept { return a.m_name == b.m_name; }
    friend bool operator!=(const QMqttTopicName &a, const QMqttTopicName &b) noexcept { return a.m_name != b.m_name; }
    friend bool operator<(const QMqttTopicName &a, const QMqttTopicName &b) noexcept { return a.m_name < b.m_name; }

private:
    QString m_name;
};

size_t qHash(const QMqttTopicName &name, size_t seed = 0) noexcept;

Q_DECLARE_METATYPE(QMqttTopicName)
