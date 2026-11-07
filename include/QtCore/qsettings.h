// MIT License - see LICENSE
#pragma once

#include <QtCore/qmap.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>

#include <memory>

class QIODevice;

class QSettings : public QObject {
    Q_OBJECT
public:
    /*!
        \enum QSettings::Status

        The status of a QSettings object after the last operation.

        \value NoError No error occurred.
        \value AccessError An access error occurred: a file could not be opened, read or written, the
               organization name is missing, or the format is not usable.
        \value FormatError A format error occurred, for instance a malformed INI file or a custom
               format function that failed.
    */
    enum Status { NoError = 0, AccessError, FormatError };
    Q_ENUM(Status)

    /*!
        \enum QSettings::Format

        The storage format of the settings.

        \value NativeFormat The platform's native format. On Linux this is the INI format with the
               file extension \c{.conf}.
        \value IniFormat The INI format, with the file extension \c{.ini}.
        \value Registry32Format The 32-bit Windows registry. Not available: using it makes
               status() return \c AccessError.
        \value Registry64Format The 64-bit Windows registry. Not available: using it makes
               status() return \c AccessError.
        \value InvalidFormat The value returned by registerFormat() when no format can be registered;
               a QSettings object with this format cannot read or write.
        \value CustomFormat1 The first format registered with registerFormat().
        \value CustomFormat2 The second format registered with registerFormat().
        \value CustomFormat3 The third format registered with registerFormat().
        \value CustomFormat4 The fourth format registered with registerFormat().
        \value CustomFormat5 The fifth format registered with registerFormat().
        \value CustomFormat6 The sixth format registered with registerFormat().
        \value CustomFormat7 The seventh format registered with registerFormat().
        \value CustomFormat8 The eighth format registered with registerFormat().
        \value CustomFormat9 The ninth format registered with registerFormat().
        \value CustomFormat10 The tenth format registered with registerFormat().
        \value CustomFormat11 The eleventh format registered with registerFormat().
        \value CustomFormat12 The twelfth format registered with registerFormat().
        \value CustomFormat13 The thirteenth format registered with registerFormat().
        \value CustomFormat14 The fourteenth format registered with registerFormat().
        \value CustomFormat15 The fifteenth format registered with registerFormat().
        \value CustomFormat16 The sixteenth format registered with registerFormat().
    */
    enum Format {
        NativeFormat,
        IniFormat,
        Registry32Format,
        Registry64Format,
        InvalidFormat = 16,
        CustomFormat1,
        CustomFormat2,
        CustomFormat3,
        CustomFormat4,
        CustomFormat5,
        CustomFormat6,
        CustomFormat7,
        CustomFormat8,
        CustomFormat9,
        CustomFormat10,
        CustomFormat11,
        CustomFormat12,
        CustomFormat13,
        CustomFormat14,
        CustomFormat15,
        CustomFormat16
    };
    Q_ENUM(Format)

    /*!
        \enum QSettings::Scope

        Whether the settings belong to the current user or to all users of the system.

        \value UserScope Store the settings in the current user's configuration directory.
        \value SystemScope Store the settings in the system-wide configuration directory.
    */
    enum Scope { UserScope, SystemScope };
    Q_ENUM(Scope)

    /*!
        \typedef QSettings::SettingsMap

        The type of the map that a custom format reads and writes: every setting by its full key
        (the group names and the key joined with \c{/}).
    */
    using SettingsMap = QMap<QString, QVariant>;

    /*!
        \typedef QSettings::ReadFunc

        A function that reads a custom format from \a device into \a map and returns \c true on
        success.
    */
    using ReadFunc = bool (*)(QIODevice &device, SettingsMap &map);

    /*!
        \typedef QSettings::WriteFunc

        A function that writes \a map to \a device in a custom format and returns \c true on
        success.
    */
    using WriteFunc = bool (*)(QIODevice &device, const SettingsMap &map);

    explicit QSettings(const QString &organization, const QString &application = QString(), QObject *parent = nullptr);
    QSettings(Scope scope, const QString &organization, const QString &application = QString(),
              QObject *parent = nullptr);
    QSettings(Format format, Scope scope, const QString &organization, const QString &application = QString(),
              QObject *parent = nullptr);
    QSettings(const QString &fileName, Format format, QObject *parent = nullptr);
    explicit QSettings(QObject *parent = nullptr);
    explicit QSettings(Scope scope, QObject *parent = nullptr);
    ~QSettings() override;

    void clear();
    void sync();
    Status status() const;
    bool isAtomicSyncRequired() const;
    void setAtomicSyncRequired(bool enable);

    void beginGroup(const QString &prefix);
    void endGroup();
    QString group() const;

    int beginReadArray(const QString &prefix);
    void beginWriteArray(const QString &prefix, int size = -1);
    void endArray();
    void setArrayIndex(int i);

    QStringList allKeys() const;
    QStringList childKeys() const;
    QStringList childGroups() const;
    bool isWritable() const;

    void setValue(const QString &key, const QVariant &value);
    QVariant value(const QString &key) const;
    QVariant value(const QString &key, const QVariant &defaultValue) const;

    void remove(const QString &key);
    bool contains(const QString &key) const;

    void setFallbacksEnabled(bool b);
    bool fallbacksEnabled() const;

    QString fileName() const;
    Format format() const;
    Scope scope() const;
    QString organizationName() const;
    QString applicationName() const;

    static void setDefaultFormat(Format format);
    static Format defaultFormat();
    static void setPath(Format format, Scope scope, const QString &path);
    static Format registerFormat(const QString &extension, ReadFunc readFunc, WriteFunc writeFunc,
                                 Qt::CaseSensitivity caseSensitivity = Qt::CaseSensitive);

protected:
    bool event(QEvent *event) override;

private:
    struct Private;
    std::unique_ptr<Private> p;
};
