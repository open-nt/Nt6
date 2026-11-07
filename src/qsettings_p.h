// MIT License - see LICENSE
#pragma once

#include <QtCore/qsettings.h>

namespace NtSettings {

// Reads the INI format written by QSettings into map. Returns false if a line was malformed; the
// settings read before and after it are kept.
bool readIniFile(QIODevice &device, QSettings::SettingsMap &map);

// Writes map in the INI format. Sections and keys are sorted, so the output is deterministic.
bool writeIniFile(QIODevice &device, const QSettings::SettingsMap &map);

// The text form of a setting's value, as it is stored in an INI file after the escaping of
// special characters: plain text for strings and numbers, @Point(), @Variant() and so on.
QString variantToString(const QVariant &v);
QVariant stringToVariant(const QString &s);

} // namespace NtSettings
