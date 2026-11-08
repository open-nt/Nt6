// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>

class QWidget;

class QToolTip {
public:
    static void showText(const QPoint &pos, const QString &text, QWidget *w = nullptr, const QRect &rect = QRect());
    static void hideText();
    static bool isVisible();
    static QString text();

private:
    QToolTip() = delete;
};
