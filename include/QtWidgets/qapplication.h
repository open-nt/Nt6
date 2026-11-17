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
    static QStyle *setStyle(const QString &style);
    static QPalette palette();
    static void setPalette(const QPalette &palette);
    static QList<QWidget *> topLevelWidgets();
    static QList<QWidget *> allWidgets();
    static QWidget *activeWindow();
    static QWidget *activeModalWidget();
    static QWidget *focusWidget();
    static QWidget *widgetAt(const QPoint &point);
    static void alert(QWidget *widget, int msec = 0);
    static int startDragDistance();
    static void setStartDragDistance(int l);
    static int startDragTime();
    static void setStartDragTime(int ms);

public slots:
    static void closeAllWindows();
};

#undef qApp
#define qApp QApplication::instance()
