// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qcursor.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#include <QtGui/qpaintdevice.h>
#include <QtGui/qpalette.h>
#include <QtGui/qregion.h>
#include <QtWidgets/qsizepolicy.h>

#include <memory>

class QAction;
class QActionEvent;
class QCloseEvent;
class QEnterEvent;
class QFocusEvent;
class QHideEvent;
class QKeyEvent;
class QLayout;
class QMouseEvent;
class QMoveEvent;
class QPaintEvent;
class QResizeEvent;
class QShowEvent;
class QStyle;
class QWheelEvent;
class QWidgetWindow;
class QWindow;

namespace QtPrivate {
struct WidgetAccess;
}

class QWidget : public QObject, public QPaintDevice {
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible)
    Q_PROPERTY(QString windowTitle READ windowTitle WRITE setWindowTitle NOTIFY windowTitleChanged)
    Q_PROPERTY(QRect geometry READ geometry)
    Q_PROPERTY(int x READ x)
    Q_PROPERTY(int y READ y)
    Q_PROPERTY(int width READ width)
    Q_PROPERTY(int height READ height)
    Q_PROPERTY(bool autoFillBackground READ autoFillBackground WRITE setAutoFillBackground)
    Q_PROPERTY(bool mouseTracking READ hasMouseTracking WRITE setMouseTracking)
    Q_PROPERTY(QString toolTip READ toolTip WRITE setToolTip)
    Q_PROPERTY(bool windowModified READ isWindowModified WRITE setWindowModified)
    Q_PROPERTY(bool updatesEnabled READ updatesEnabled WRITE setUpdatesEnabled)
public:
    explicit QWidget(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~QWidget() override;

    QPaintEngine *paintEngine() const override;

    // Geometry
    QRect geometry() const;
    QRect rect() const { return QRect(0, 0, width(), height()); }
    QPoint pos() const { return geometry().topLeft(); }
    QSize size() const { return geometry().size(); }
    int x() const { return geometry().x(); }
    int y() const { return geometry().y(); }
    int width() const { return geometry().width(); }
    int height() const { return geometry().height(); }
    void setGeometry(const QRect &rect);
    void setGeometry(int x, int y, int w, int h) { setGeometry(QRect(x, y, w, h)); }
    void move(const QPoint &pos);
    void move(int x, int y) { move(QPoint(x, y)); }
    void resize(const QSize &size);
    void resize(int w, int h) { resize(QSize(w, h)); }
    QSize minimumSize() const;
    QSize maximumSize() const;
    void setMinimumSize(const QSize &size);
    void setMinimumSize(int minw, int minh) { setMinimumSize(QSize(minw, minh)); }
    void setMaximumSize(const QSize &size);
    void setMaximumSize(int maxw, int maxh) { setMaximumSize(QSize(maxw, maxh)); }
    void setFixedSize(const QSize &size);
    void setFixedSize(int w, int h) { setFixedSize(QSize(w, h)); }
    void setMinimumWidth(int minw);
    void setMinimumHeight(int minh);
    void setMaximumWidth(int maxw);
    void setMaximumHeight(int maxh);
    void setFixedWidth(int w);
    void setFixedHeight(int h);
    virtual QSize sizeHint() const;
    virtual QSize minimumSizeHint() const;
    QSizePolicy sizePolicy() const;
    void setSizePolicy(QSizePolicy policy);
    void setSizePolicy(QSizePolicy::Policy horizontal, QSizePolicy::Policy vertical) {
        setSizePolicy(QSizePolicy(horizontal, vertical));
    }
    void adjustSize();
    void updateGeometry();
    QPoint mapToGlobal(const QPoint &pos) const;
    QPoint mapFromGlobal(const QPoint &pos) const;
    QPoint mapToParent(const QPoint &pos) const { return pos + this->pos(); }
    QPoint mapFromParent(const QPoint &pos) const { return pos - this->pos(); }
    QPoint mapTo(const QWidget *parent, const QPoint &pos) const;
    QPoint mapFrom(const QWidget *parent, const QPoint &pos) const;
    QWidget *childAt(const QPoint &pos) const;
    QWidget *childAt(int x, int y) const { return childAt(QPoint(x, y)); }

    // Hierarchy and windows
    QWidget *parentWidget() const;
    void setParent(QWidget *parent);
    void setParent(QWidget *parent, Qt::WindowFlags f);
    bool isWindow() const;
    Qt::WindowFlags windowFlags() const;
    void setWindowFlags(Qt::WindowFlags type);
    Qt::WindowType windowType() const;
    Qt::WindowModality windowModality() const;
    void setWindowModality(Qt::WindowModality windowModality);
    bool isModal() const { return windowModality() != Qt::NonModal; }
    QWidget *window() const;
    QWindow *windowHandle() const;
    QString windowTitle() const;
    bool isActiveWindow() const;
    void activateWindow();

    // State
    bool isVisible() const;
    bool isHidden() const;
    bool isEnabled() const;
    void setAttribute(Qt::WidgetAttribute attribute, bool on = true);
    bool testAttribute(Qt::WidgetAttribute attribute) const;
    bool updatesEnabled() const;
    void setUpdatesEnabled(bool enable);
    bool isWindowModified() const;

    // Look
    const QPalette &palette() const;
    void setPalette(const QPalette &palette);
    const QFont &font() const;
    void setFont(const QFont &font);
    QFontMetrics fontMetrics() const { return QFontMetrics(font()); }
    QStyle *style() const;
    void setStyle(QStyle *style);
    bool autoFillBackground() const;
    void setAutoFillBackground(bool enabled);

    // Focus and mouse
    Qt::FocusPolicy focusPolicy() const;
    void setFocusPolicy(Qt::FocusPolicy policy);
    bool hasFocus() const;
    QWidget *focusWidget() const;
    bool focusNextChild() { return focusNextPrevChild(true); }
    bool focusPreviousChild() { return focusNextPrevChild(false); }
    void setMouseTracking(bool enable);
    QCursor cursor() const;
    void setCursor(const QCursor &cursor);
    void unsetCursor();
    bool hasMouseTracking() const;
    bool underMouse() const;

    // Actions
    QList<QAction *> actions() const;
    void addAction(QAction *action);
    void addActions(const QList<QAction *> &actions);
    void insertAction(QAction *before, QAction *action);
    void insertActions(QAction *before, const QList<QAction *> &actions);
    void removeAction(QAction *action);

    // Tool tip
    QString toolTip() const;
    void setToolTip(const QString &text);

    // Rendering
    void render(QPaintDevice *target, const QPoint &targetOffset = QPoint(), const QRegion &sourceRegion = QRegion());

    // Layout
    QLayout *layout() const;
    void setLayout(QLayout *layout);

public slots:
    virtual void setVisible(bool visible);
    void show();
    void hide();
    bool close();
    void raise();
    void lower();
    void setEnabled(bool enabled);
    void setDisabled(bool disable) { setEnabled(!disable); }
    void setWindowTitle(const QString &title);
    void setWindowModified(bool modified);
    void setFocus();
    void clearFocus();
    void update();
    void repaint();

public:
    void update(const QRect &rect);
    void update(const QRegion &region);
    void repaint(const QRect &rect);

signals:
    /*!
        This signal is emitted when the window title changes to \a title.
    */
    void windowTitleChanged(const QString &title) { emitSignal(&QWidget::windowTitleChanged, title); }

protected:
    bool event(QEvent *event) override;
    virtual void paintEvent(QPaintEvent *event);
    virtual void resizeEvent(QResizeEvent *event);
    virtual void moveEvent(QMoveEvent *event);
    virtual void showEvent(QShowEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void mousePressEvent(QMouseEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseDoubleClickEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void wheelEvent(QWheelEvent *event);
    virtual void keyPressEvent(QKeyEvent *event);
    virtual void keyReleaseEvent(QKeyEvent *event);
    virtual void focusInEvent(QFocusEvent *event);
    virtual void focusOutEvent(QFocusEvent *event);
    virtual void enterEvent(QEnterEvent *event);
    virtual void leaveEvent(QEvent *event);
    virtual void changeEvent(QEvent *event);
    virtual void actionEvent(QActionEvent *event);
    virtual bool focusNextPrevChild(bool next);
    int metric(PaintDeviceMetric metric) const override;
    QPaintDevice *redirected(QPoint *offset) const override;
    bool systemClip(QRegion *region) const override;

private:
    friend struct QtPrivate::WidgetAccess;
    friend class QWidgetWindow;
    friend class QLayout;
    friend class QApplication;
    struct Data;
    std::unique_ptr<Data> m_d;
};
