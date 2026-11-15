// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qfont.h>

class QFontDatabase {
public:
    enum WritingSystem { Any, Latin };
    enum SystemFont { GeneralFont, FixedFont, TitleFont, SmallestReadableFont };

    static QStringList families(WritingSystem writingSystem = Any);
    static bool hasFamily(const QString &family);
    static bool isFixedPitch(const QString &family, const QString &style = QString());
    static QFont systemFont(SystemFont type);

    static int addApplicationFont(const QString &fileName);
    static int addApplicationFontFromData(const QByteArray &fontData);
    static QStringList applicationFontFamilies(int id);
    static bool removeApplicationFont(int id);
    static bool removeAllApplicationFonts();
};
