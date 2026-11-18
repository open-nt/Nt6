// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtGui/qguiapplication.h>
#include <QtGui/qpalette.h>

class QStyle;
class QWidget;

class QApplication : public QGuiApplication {
    Q_OBJECT
    Q_PROPERTY(QString styleSheet READ styleSheet WRITE setStyleSheet)
    Q_PROPERTY(bool autoSipEnabled READ autoSipEnabled WRITE setAutoSipEnabled)
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
    static int cursorFlashTime();
    static void setCursorFlashTime(int msecs);
    static int doubleClickInterval();
    static void setDoubleClickInterval(int ms);
    static int keyboardInputInterval();
    static void setKeyboardInputInterval(int ms);
    static int wheelScrollLines();
    static void setWheelScrollLines(int lines);
    static bool isEffectEnabled(Qt::UIEffect effect);
    static void setEffectEnabled(Qt::UIEffect effect, bool enable = true);
    static Qt::NavigationMode navigationMode();
    static void setNavigationMode(Qt::NavigationMode mode);
    static QWidget *activePopupWidget();
    static QWidget *topLevelAt(const QPoint &point);
    static QWidget *topLevelAt(int x, int y) { return topLevelAt(QPoint(x, y)); }
    static void beep();
    QString styleSheet() const { return m_styleSheet; }
    bool autoSipEnabled() const { return m_autoSip; }

public slots:
    static void closeAllWindows();
    static void aboutQt();
    void setStyleSheet(const QString &sheet);
    void setAutoSipEnabled(const bool enabled) { m_autoSip = enabled; }

signals:
    /*!
        \since 6.0
        This signal is emitted when the keyboard focus moves from \a old to \a now, either of which may be
        \c nullptr: within the active window, or to another window.
    */
    void focusChanged(QWidget *old, QWidget *now) { emitSignal(&QApplication::focusChanged, old, now); }

private:
    QString m_styleSheet;
    bool m_autoSip = true;
};

#undef qApp
#define qApp QApplication::instance()
