// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qdatetime.h>
#include <QtCore/qlist.h>
#include <QtCore/qlocale.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qstring.h>

#include <climits>
#include <memory>

struct QTimeZonePrivate;

/*!
    \class QTimeZone
    \since 5.2
    \brief The QTimeZone class identifies how a time representation relates to UTC.

    A QTimeZone is UTC, a fixed offset from UTC, local time, or a zone of the
    IANA time zone database such as \c{"Europe/Berlin"}, with its history of
    offset changes and daylight-saving rules. Use it with QDateTime to
    represent and convert times in that zone:

    \code
    const QTimeZone berlin("Europe/Berlin");
    QDateTime meeting(QDate(2024, 7, 1), QTime(15, 0), berlin);
    meeting.offsetFromUtc();                      // 7200
    meeting.toTimeZone(QTimeZone("America/New_York")).time(); // 09:00
    \endcode

    \differences
    Zones are read from the TZif files of the system's time zone database
    (\c /usr/share/zoneinfo, or the directory in \c TZDIR), including the
    POSIX rule that continues each zone after its last transition. There is
    no ICU or CLDR data, so displayName() gives abbreviations and offsets,
    not localized names such as "Central European Standard Time", and there
    are no Windows zone ID conversions. availableTimeZoneIds() lists the zones
    of \c tzdata.zi or \c zone1970.tab.

    On Zephyr the system time zone is UTC, and zones other than UTC and fixed
    offsets need a time zone database on a mounted file system, named by \c
    TZDIR. See \l{Nt on Zephyr}.
    \enddifferences

    \sa QDateTime
*/
class QTimeZone {
public:
    /*!
        \enum QTimeZone::Initialization
        \since 6.5

        \value LocalTime The local time of the system.
        \value UTC Coordinated Universal Time.
    */
    enum Initialization { LocalTime, UTC };

    /*!
        \enum QTimeZone::TimeType

        \value StandardTime The standard time of the zone.
        \value DaylightTime The daylight-saving time of the zone.
        \value GenericTime The time of the zone without regard to daylight saving.
    */
    enum TimeType { StandardTime = 0, DaylightTime = 1, GenericTime = 2 };

    /*!
        \enum QTimeZone::NameType

        \value DefaultName The abbreviation if one is known, otherwise the offset name.
        \value LongName The long name. Nt has no localized names, so this is the
               custom name of the zone or else its IANA ID.
        \value ShortName The abbreviation, such as "CEST".
        \value OffsetName The offset, such as "UTC+02:00".
    */
    enum NameType { DefaultName = 0, LongName = 1, ShortName = 2, OffsetName = 3 };

    /*!
        The smallest supported offset from UTC, -16 hours.
    */
    static constexpr int MinUtcOffsetSecs = -16 * 3600;
    /*!
        The largest supported offset from UTC, +16 hours.
    */
    static constexpr int MaxUtcOffsetSecs = +16 * 3600;

    /*!
        \class QTimeZone::OffsetData
        \brief The OffsetData class describes the offsets of a zone at an instant.

        \differences
        None.
        \enddifferences
    */
    struct OffsetData {
        /*!
            The abbreviation in effect, such as "CEST".
        */
        QString abbreviation;
        /*!
            The instant the data applies to, in UTC.
        */
        QDateTime atUtc;
        /*!
            The total offset from UTC in seconds.
        */
        int offsetFromUtc = INT_MIN;
        /*!
            The offset of standard time from UTC in seconds.
        */
        int standardTimeOffset = INT_MIN;
        /*!
            The additional daylight-saving offset in seconds, or 0.
        */
        int daylightTimeOffset = INT_MIN;
    };
    /*! \typedef QTimeZone::OffsetDataList

        A list of OffsetData, as returned by transitions(). */
    using OffsetDataList = QList<OffsetData>;

    QTimeZone() noexcept;
    QTimeZone(Initialization spec) noexcept;
    explicit QTimeZone(const QByteArray &ianaId);
    explicit QTimeZone(int offsetSeconds);
    QTimeZone(const QByteArray &zoneId, int offsetSeconds, const QString &name, const QString &abbreviation,
              QLocale::Territory territory = QLocale::AnyTerritory, const QString &comment = QString());
    QTimeZone(const QTimeZone &other) noexcept;
    QTimeZone(QTimeZone &&other) noexcept;
    ~QTimeZone();
    QTimeZone &operator=(const QTimeZone &other);
    QTimeZone &operator=(QTimeZone &&other) noexcept;
    /*!
        Swaps this zone with \a other.
    */
    void swap(QTimeZone &other) noexcept { d.swap(other.d); }

    bool isValid() const;
    QByteArray id() const;
    QLocale::Territory territory() const;
    /*!
        Same as territory().
    */
    QLocale::Country country() const { return territory(); }
    QString comment() const;
    QString displayName(const QDateTime &atDateTime, NameType nameType = DefaultName,
                        const QLocale &locale = QLocale()) const;
    QString displayName(TimeType timeType, NameType nameType = DefaultName, const QLocale &locale = QLocale()) const;
    QString abbreviation(const QDateTime &atDateTime) const;

    int offsetFromUtc(const QDateTime &atDateTime) const;
    int standardTimeOffset(const QDateTime &atDateTime) const;
    int daylightTimeOffset(const QDateTime &atDateTime) const;
    bool hasDaylightTime() const;
    bool isDaylightTime(const QDateTime &atDateTime) const;
    OffsetData offsetData(const QDateTime &forDateTime) const;
    bool hasTransitions() const;
    OffsetData nextTransition(const QDateTime &afterDateTime) const;
    OffsetData previousTransition(const QDateTime &beforeDateTime) const;
    OffsetDataList transitions(const QDateTime &fromDateTime, const QDateTime &toDateTime) const;

    Qt::TimeSpec timeSpec() const noexcept;
    int fixedSecondsAheadOfUtc() const noexcept;
    bool isUtcOrFixedOffset() const noexcept;

    static QTimeZone fromSecondsAheadOfUtc(int offset);
    static QTimeZone utc();
    static QTimeZone systemTimeZone();
    static QByteArray systemTimeZoneId();
    static bool isTimeZoneIdAvailable(const QByteArray &ianaId);
    static QList<QByteArray> availableTimeZoneIds();
    static QList<QByteArray> availableTimeZoneIds(QLocale::Territory territory);
    static QList<QByteArray> availableTimeZoneIds(int offsetSeconds);

    friend bool operator==(const QTimeZone &a, const QTimeZone &b) noexcept;
    friend bool operator!=(const QTimeZone &a, const QTimeZone &b) noexcept { return !(a == b); }

private:
    friend class QDateTime;
    explicit QTimeZone(std::shared_ptr<const QTimeZonePrivate> data) noexcept;
    std::shared_ptr<const QTimeZonePrivate> d;
};

std::size_t qHash(const QTimeZone &key, std::size_t seed = 0) noexcept;
