// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qmargins.h>
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
#include <QtGui/qpixmap.h>
#include <QtGui/qregion.h>
#include <QtGui/qwindowdefs.h>
#include <QtWidgets/qsizepolicy.h>

#include <memory>

// The largest width or height of a widget.
#define QWIDGETSIZE_MAX ((1 << 24) - 1)

class QAction;
class QGraphicsEffect;
class QActionEvent;
class QIcon;
class QContextMenuEvent;
class QDragEnterEvent;
class QDragLeaveEvent;
class QDragMoveEvent;
class QDropEvent;
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
class QScreen;
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
    Q_PROPERTY(Qt::ContextMenuPolicy contextMenuPolicy READ contextMenuPolicy WRITE setContextMenuPolicy)
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
    int minimumWidth() const { return minimumSize().width(); }
    int minimumHeight() const { return minimumSize().height(); }
    int maximumWidth() const { return maximumSize().width(); }
    int maximumHeight() const { return maximumSize().height(); }
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
    QPointF mapToGlobal(const QPointF &pos) const;
    QPoint mapFromGlobal(const QPoint &pos) const;
    QPointF mapFromGlobal(const QPointF &pos) const;
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
    bool isAncestorOf(const QWidget *child) const;
    Qt::WindowFlags windowFlags() const;
    void setWindowFlags(Qt::WindowFlags type);
    void setWindowFlag(Qt::WindowType flag, bool on = true);
    Qt::WindowType windowType() const;
    Qt::WindowModality windowModality() const;
    void setWindowModality(Qt::WindowModality windowModality);
    bool isModal() const { return windowModality() != Qt::NonModal; }
    QWidget *window() const;
    QWindow *windowHandle() const;
    WId winId() const;
    QScreen *screen() const;
    void setScreen(QScreen *screen);
    bool isFullScreen() const;
    bool isMaximized() const;
    bool isMinimized() const;
    Qt::WindowStates windowState() const;
    void setWindowState(Qt::WindowStates state);
    void showFullScreen();
    void showNormal();
    void showMaximized();
    void showMinimized();
    QString windowTitle() const;
    QIcon windowIcon() const;
    void setWindowIcon(const QIcon &icon);
    bool isActiveWindow() const;
    void activateWindow();

    // State
    bool isVisible() const;
    bool isHidden() const;
    bool isVisibleTo(const QWidget *ancestor) const;
    bool acceptDrops() const;
    void setAcceptDrops(bool on);
    Qt::ContextMenuPolicy contextMenuPolicy() const;
    void setContextMenuPolicy(Qt::ContextMenuPolicy policy);
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
    QString styleSheet() const;
    void setStyleSheet(const QString &styleSheet);
    void setCursor(const QCursor &cursor);
    void unsetCursor();
    bool hasMouseTracking() const;
    bool underMouse() const;
    void grabMouse();
    void grabMouse(const QCursor &cursor);
    void releaseMouse();
    void grabKeyboard();
    void releaseKeyboard();
    static QWidget *mouseGrabber();
    static QWidget *keyboardGrabber();
    Qt::InputMethodHints inputMethodHints() const;
    void setInputMethodHints(Qt::InputMethodHints hints);

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
    QString statusTip() const;
    void setStatusTip(const QString &text);
    QString whatsThis() const;
    void setWhatsThis(const QString &text);
    QString accessibleName() const;
    void setAccessibleName(const QString &name);
    QString accessibleDescription() const;
    void setAccessibleDescription(const QString &description);

    // Rendering
    void render(QPaintDevice *target, const QPoint &targetOffset = QPoint(), const QRegion &sourceRegion = QRegion());
    enum RenderFlag { DrawWindowBackground = 0x1, DrawChildren = 0x2, IgnoreMask = 0x4 };
    Q_DECLARE_FLAGS(RenderFlags, RenderFlag)
    void render(QPainter *painter, const QPoint &targetOffset = QPoint(), const QRegion &sourceRegion = QRegion(),
                RenderFlags renderFlags = RenderFlags(DrawWindowBackground) | DrawChildren);

    QPixmap grab(const QRect &rectangle = QRect(QPoint(0, 0), QSize(-1, -1)));
    QGraphicsEffect *graphicsEffect() const;
    void setGraphicsEffect(QGraphicsEffect *effect);

    // Layout
    QLayout *layout() const;
    void setLayout(QLayout *layout);
    void setContentsMargins(int left, int top, int right, int bottom);
    void setContentsMargins(const QMargins &margins);
    QMargins contentsMargins() const;
    QRect contentsRect() const;

public slots:
    virtual void setVisible(bool visible);
    void ensurePolished() const;
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
    /*!
        \since 6.0
        This signal is emitted when the user asks for a context menu of a widget whose context menu policy is
        Qt::CustomContextMenu. \a pos is where, in the widget, or in the viewport of a scroll area such as an item
        view; map it with mapToGlobal() to open a menu there.
    */
    void customContextMenuRequested(const QPoint &pos) { emitSignal(&QWidget::customContextMenuRequested, pos); }

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
    virtual void contextMenuEvent(QContextMenuEvent *event);
    virtual void dragEnterEvent(QDragEnterEvent *event);
    virtual void dragMoveEvent(QDragMoveEvent *event);
    virtual void dragLeaveEvent(QDragLeaveEvent *event);
    virtual void dropEvent(QDropEvent *event);
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

Q_DECLARE_OPERATORS_FOR_FLAGS(QWidget::RenderFlags)
