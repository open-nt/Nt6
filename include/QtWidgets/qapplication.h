// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpalette.h>

class QStyle;
class QWidget;

class QApplication : public QGuiApplication {
    Q_OBJECT
public:
    QApplication(int &argc, char **argv);
    ~QApplication() override;

    static QApplication *instance();
    static QStyle *style();
    static void setStyle(QStyle *style);
    static QPalette palette();
    static void setPalette(const QPalette &palette);
    static QList<QWidget *> topLevelWidgets();
    static QList<QWidget *> allWidgets();
    static QWidget *activeWindow();
    static QWidget *focusWidget();
    static QWidget *widgetAt(const QPoint &point);

public slots:
    static void closeAllWindows();
};

#undef qApp
#define qApp QApplication::instance()
