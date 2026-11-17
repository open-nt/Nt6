// MIT License - see LICENSE
#pragma once

#include <QtCore/qpoint.h>
#include <QtCore/qstring.h>

class QAction;
class QObject;
class QWidget;

class QWhatsThis {
public:
    QWhatsThis() = delete;

    static void enterWhatsThisMode();
    static bool inWhatsThisMode();
    static void leaveWhatsThisMode();
    static void showText(const QPoint &pos, const QString &text, QWidget *w = nullptr);
    static void hideText();
    static QAction *createAction(QObject *parent = nullptr);
};
