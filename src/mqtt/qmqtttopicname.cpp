// MIT License - see LICENSE
#include <QtMqtt/qmqtttopicfilter.h>
#include <QtMqtt/qmqtttopicname.h>

/*!
    \class QMqttTopicName
    \brief The QMqttTopicName class is the name of a topic that a message is published to.
    \since 6.0

    A topic name is a path of levels separated by \c{/}, such as \c{sensors/board1/temperature}.
    Unlike a QMqttTopicFilter it holds no wildcards.

    \differences
    Objects of this class are plain values, not implicitly shared.
    \enddifferences

    \sa QMqttTopicFilter
*/

/*!
    Constructs a topic name from \a name.
*/
QMqttTopicName::QMqttTopicName(const QString &name) : m_name(name) {}

/*!
    \overload

    Constructs a topic name from \a name.
*/
QMqttTopicName::QMqttTopicName(const QLatin1String &name) : m_name(QString(name)) {}

/*!
    \fn void QMqttTopicName::swap(QMqttTopicName &other)

    Swaps this topic name with \a other.
*/

/*!
    Returns the name.
*/
QString QMqttTopicName::name() const { return m_name; }

/*!
    Sets the name to \a name.
*/
void QMqttTopicName::setName(const QString &name) { m_name = name; }

/*!
    Returns \c true if the name can be published to: it is not empty, has at most 65535 bytes of UTF-8 and holds no
    wildcard (\c{+} or \c{#}) and no null character.
*/
bool QMqttTopicName::isValid() const {
    if (m_name.isEmpty() || m_name.toUtf8().size() > 65535)
        return false;
    return !m_name.contains(QChar(u'+')) && !m_name.contains(QChar(u'#')) && !m_name.contains(QChar(u'\0'));
}

/*!
    Returns the number of levels of the name, which is one more than its number of \c{/}.
*/
int QMqttTopicName::levelCount() const {
    return m_name.isEmpty() ? 0 : static_cast<int>(m_name.count(QChar(u'/'))) + 1;
}

/*!
    Returns the levels of the name, which may be empty strings.
*/
QStringList QMqttTopicName::levels() const { return m_name.split(QChar(u'/')); }

// The hash of a QMqttTopicName, for QHash and QSet.
size_t qHash(const QMqttTopicName &name, size_t seed) noexcept { return qHash(name.name(), seed); }

/*!
    \class QMqttTopicFilter
    \brief The QMqttTopicFilter class is a pattern of topic names that a subscription receives.
    \since 6.0

    A filter is a topic name in which a level may be the wildcard \c{+}, which matches any one
    level, and whose last level may be \c{#}, which matches any number of levels, none included:
    \c{sensors/+/temperature} matches \c{sensors/board1/temperature}, and \c{sensors/#} matches it
    and \c{sensors} too. A filter of the form \c{$share/<name>/<filter>} is a shared subscription.

    \differences
    Objects of this class are plain values, not implicitly shared. Shared subscriptions need a
    broker that offers them under MQTT 3.1.1, as Mosquitto does.
    \enddifferences

    \sa QMqttTopicName, QMqttClient::subscribe()
*/

/*!
    \enum QMqttTopicFilter::MatchOption

    Options of match().

    \value NoMatchOption Wildcards also match names that start with \c{$}, as in Qt.
    \value WildcardsDontMatchDollarTopicMatchOption A wildcard at the start of the filter does not match a name that
           starts with \c{$}, as the MQTT standard has it for topics such as \c{$SYS}.
*/

/*!
    Constructs a topic filter from \a filter.
*/
QMqttTopicFilter::QMqttTopicFilter(const QString &filter) : m_filter(filter) {}

/*!
    \overload

    Constructs a topic filter from \a filter.
*/
QMqttTopicFilter::QMqttTopicFilter(const QLatin1String &filter) : m_filter(QString(filter)) {}

/*!
    \fn void QMqttTopicFilter::swap(QMqttTopicFilter &other)

    Swaps this filter with \a other.
*/

/*!
    Returns the filter.
*/
QString QMqttTopicFilter::filter() const { return m_filter; }

/*!
    Sets the filter to \a filter.
*/
void QMqttTopicFilter::setFilter(const QString &filter) { m_filter = filter; }

/*!
    Returns the name of the shared subscription of a filter \c{$share/<name>/<filter>}, or an empty string.
*/
QString QMqttTopicFilter::sharedSubscriptionName() const {
    if (!m_filter.startsWith(QStringLiteral("$share/")))
        return QString();
    const qsizetype end = m_filter.indexOf(QChar(u'/'), 7);
    return end < 0 ? QString() : m_filter.mid(7, end - 7);
}

/*!
    Returns \c true if the filter can be subscribed to: it is not empty, has at most 65535 bytes of UTF-8 and no null
    character, \c{+} only stands as a whole level, and \c{#} only as the whole last level.
*/
bool QMqttTopicFilter::isValid() const {
    if (m_filter.isEmpty() || m_filter.toUtf8().size() > 65535 || m_filter.contains(QChar(u'\0')))
        return false;
    const QStringList levels = m_filter.split(QChar(u'/'));
    for (qsizetype i = 0; i < levels.size(); ++i) {
        const QString &level = levels[i];
        if (level.contains(QChar(u'#')) && (level.size() != 1 || i != levels.size() - 1))
            return false;
        if (level.contains(QChar(u'+')) && level.size() != 1)
            return false;
    }
    if (m_filter.startsWith(QStringLiteral("$share/"))) // $share/<name>/<a filter of at least one level>
        return levels.size() >= 3 && !levels[1].isEmpty() && !levels[1].contains(QChar(u'+')) &&
               !levels[1].contains(QChar(u'#'));
    return true;
}

/*!
    Returns \c true if the filter matches the topic \a name, with the given \a matchOptions. An invalid filter or name
    matches nothing. The filter of a shared subscription matches as the filter that follows its name.
*/
bool QMqttTopicFilter::match(const QMqttTopicName &name, MatchOptions matchOptions) const {
    if (!isValid() || !name.isValid())
        return false;
    QStringList filterLevels = m_filter.split(QChar(u'/'));
    if (m_filter.startsWith(QStringLiteral("$share/")))
        filterLevels = filterLevels.mid(2);
    const QStringList nameLevels = name.levels();
    if (matchOptions.testFlag(WildcardsDontMatchDollarTopicMatchOption) && name.name().startsWith(QChar(u'$')) &&
        (filterLevels.first() == QStringLiteral("+") || filterLevels.first() == QStringLiteral("#")))
        return false;
    qsizetype i = 0;
    for (; i < filterLevels.size(); ++i) {
        const QString &level = filterLevels[i];
        if (level == QStringLiteral("#"))
            return true; // matches the parent level too: "a/#" matches "a"
        if (i >= nameLevels.size())
            return false;
        if (level != QStringLiteral("+") && level != nameLevels[i])
            return false;
    }
    return i == nameLevels.size();
}

// The hash of a QMqttTopicFilter, for QHash and QSet.
size_t qHash(const QMqttTopicFilter &filter, size_t seed) noexcept { return qHash(filter.filter(), seed); }
