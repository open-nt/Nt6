// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qfont.h>
#include <QtGui/qpalette.h>

class QWidget;

class QToolTip {
public:
    static void showText(const QPoint &pos, const QString &text, QWidget *w = nullptr, const QRect &rect = QRect());
    static void hideText();
    static bool isVisible();
    static QString text();
    static QFont font();
    static void setFont(const QFont &font);
    static QPalette palette();
    static void setPalette(const QPalette &palette);

private:
    QToolTip() = delete;
};
