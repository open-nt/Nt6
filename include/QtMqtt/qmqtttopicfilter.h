// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qhashfunctions.h>
#include <QtCore/qmetatype.h>
#include <QtCore/qstring.h>
#include <QtMqtt/qmqtttopicname.h>

class QMqttTopicFilter {
public:
    enum MatchOption { NoMatchOption = 0x0000, WildcardsDontMatchDollarTopicMatchOption = 0x0001 };
    Q_DECLARE_FLAGS(MatchOptions, MatchOption)

    QMqttTopicFilter(const QString &filter = QString());
    QMqttTopicFilter(const QLatin1String &filter);

    void swap(QMqttTopicFilter &other) noexcept { m_filter.swap(other.m_filter); }

    QString filter() const;
    void setFilter(const QString &filter);

    QString sharedSubscriptionName() const;

    bool isValid() const;
    bool match(const QMqttTopicName &name, MatchOptions matchOptions = NoMatchOption) const;

    friend bool operator==(const QMqttTopicFilter &a, const QMqttTopicFilter &b) noexcept {
        return a.m_filter == b.m_filter;
    }
    friend bool operator!=(const QMqttTopicFilter &a, const QMqttTopicFilter &b) noexcept {
        return a.m_filter != b.m_filter;
    }
    friend bool operator<(const QMqttTopicFilter &a, const QMqttTopicFilter &b) noexcept {
        return a.m_filter < b.m_filter;
    }

private:
    QString m_filter;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QMqttTopicFilter::MatchOptions)

size_t qHash(const QMqttTopicFilter &filter, size_t seed = 0) noexcept;

Q_DECLARE_METATYPE(QMqttTopicFilter)
