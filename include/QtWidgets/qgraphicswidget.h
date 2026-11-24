// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qkeysequence.h>
#include <QtGui/qaction.h>
#include <QtGui/qfont.h>
#include <QtGui/qpalette.h>
#include <QtWidgets/qgraphicsitem.h>
#include <QtWidgets/qgraphicslayout.h>
#include <QtWidgets/qgraphicslayoutitem.h>
#include <QtWidgets/qstyle.h>

#include <bitset>

class QCloseEvent;
class QHideEvent;
class QShowEvent;
class QStyleOptionGraphicsItem;
class QGraphicsSceneMoveEvent;
class QStyleOption;
class QGraphicsSceneResizeEvent;

class QGraphicsWidget : public QGraphicsObject, public QGraphicsLayoutItem {
    Q_OBJECT
    Q_PROPERTY(QSizeF size READ size WRITE resize NOTIFY geometryChanged)
    Q_PROPERTY(QRectF geometry READ geometry WRITE setGeometry NOTIFY geometryChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont)
    Q_PROPERTY(QPalette palette READ palette WRITE setPalette)
    Q_PROPERTY(bool autoFillBackground READ autoFillBackground WRITE setAutoFillBackground)
    Q_INTERFACES(QGraphicsItem QGraphicsLayoutItem)
public:
    enum { Type = 11 };

    explicit QGraphicsWidget(QGraphicsItem *parent = nullptr, Qt::WindowFlags wFlags = Qt::WindowFlags());
    ~QGraphicsWidget() override;

    QGraphicsLayout *layout() const { return m_layout; }
    void setLayout(QGraphicsLayout *layout);
    void adjustSize();

    QSizeF size() const { return QGraphicsLayoutItem::geometry().size(); }
    void resize(const QSizeF &size);
    void resize(qreal w, qreal h) { resize(QSizeF(w, h)); }
    QRectF rect() const { return QRectF(QPointF(0, 0), size()); }
    void setGeometry(const QRectF &rect) override;
    void setGeometry(qreal x, qreal y, qreal w, qreal h) { setGeometry(QRectF(x, y, w, h)); }
    QRectF geometry() const { return QRectF(pos(), size()); }

    void setContentsMargins(qreal left, qreal top, qreal right, qreal bottom);
    void getContentsMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const override;

    QFont font() const { return m_font; }
    void setFont(const QFont &font);
    QPalette palette() const { return m_palette; }
    void setPalette(const QPalette &palette);
    bool autoFillBackground() const { return m_autoFill; }
    void setAutoFillBackground(bool enabled);

    Qt::WindowFlags windowFlags() const { return m_windowFlags; }
    void setWindowFlags(Qt::WindowFlags wFlags) { m_windowFlags = wFlags; }
    Qt::WindowType windowType() const { return static_cast<Qt::WindowType>(int(m_windowFlags & Qt::WindowType_Mask)); }
    QString windowTitle() const { return m_windowTitle; }
    void setWindowTitle(const QString &title) { m_windowTitle = title; }
    bool isActiveWindow() const;

    Qt::FocusPolicy focusPolicy() const { return m_focusPolicy; }
    void setFocusPolicy(Qt::FocusPolicy policy) { m_focusPolicy = policy; }
    QGraphicsWidget *focusWidget() const;
    static void setTabOrder(QGraphicsWidget *first, QGraphicsWidget *second);

    QStyle *style() const;
    void setStyle(QStyle *style) { m_style = style; }
    Qt::LayoutDirection layoutDirection() const { return m_layoutDirection; }
    void setLayoutDirection(Qt::LayoutDirection direction);
    void unsetLayoutDirection();
    void setAttribute(Qt::WidgetAttribute attribute, bool on = true);
    bool testAttribute(Qt::WidgetAttribute attribute) const;
    void setShortcutEnabled(int id, bool enabled = true);
    void setShortcutAutoRepeat(int id, bool enabled = true);
    int grabShortcut(const QKeySequence &sequence, Qt::ShortcutContext context = Qt::WindowShortcut);
    void releaseShortcut(int id);

    void setWindowFrameMargins(qreal left, qreal top, qreal right, qreal bottom);
    void getWindowFrameMargins(qreal *left, qreal *top, qreal *right, qreal *bottom) const;
    void unsetWindowFrameMargins();
    QRectF windowFrameGeometry() const;
    QRectF windowFrameRect() const;
    Qt::WindowFrameSection windowFrameSectionAt(const QPointF &pos) const;

    QList<QAction *> actions() const { return m_actions; }
    void addAction(QAction *action) { insertAction(nullptr, action); }
    void addActions(const QList<QAction *> &actions) { insertActions(nullptr, actions); }
    void insertAction(QAction *before, QAction *action);
    void insertActions(QAction *before, const QList<QAction *> &actions);
    void removeAction(QAction *action);

    void updateGeometry() override;
    QRectF boundingRect() const override;
    QPainterPath shape() const override;
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) override;
    int type() const override;

public slots:
    bool close();

signals:
    /*!
        This signal is emitted when the geometry of the widget changes.
    */
    void geometryChanged() { emitSignal(&QGraphicsWidget::geometryChanged); }
    /*!
        This signal is emitted when the layout of the widget changes.
    */
    void layoutChanged() { emitSignal(&QGraphicsWidget::layoutChanged); }

protected:
    QSizeF sizeHint(Qt::SizeHint which, const QSizeF &constraint = QSizeF()) const override;
    virtual void resizeEvent(QGraphicsSceneResizeEvent *event);
    virtual void moveEvent(QGraphicsSceneMoveEvent *event);
    virtual void closeEvent(QCloseEvent *event);
    virtual void showEvent(QShowEvent *event);
    virtual void hideEvent(QHideEvent *event);
    virtual void changeEvent(QEvent *event);
    virtual void initStyleOption(QStyleOption *option) const;
    virtual void paintWindowFrame(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr);
    bool event(QEvent *event) override;

private:
    friend class QGraphicsLayout;
    void requestLayout(); // posts a layout request, once until it is handled

    QGraphicsLayout *m_layout = nullptr;
    qreal m_margins[4] = {0, 0, 0, 0};
    QFont m_font;
    QPalette m_palette;
    bool m_autoFill = false;
    bool m_layoutRequested = false;
    QPointF m_lastPos; // for the move events
    bool m_settingGeometry = false;
    Qt::WindowFlags m_windowFlags;
    QString m_windowTitle;
    Qt::FocusPolicy m_focusPolicy = Qt::NoFocus;
    QPointer<QStyle> m_style;
    Qt::LayoutDirection m_layoutDirection = Qt::LeftToRight;
    bool m_layoutDirectionSet = false;
    std::bitset<Qt::WA_AttributeCount> m_attributes;
    qreal m_frameMargins[4] = {0, 0, 0, 0};
    bool m_frameMarginsSet = false;
    QList<QAction *> m_actions;
    int m_nextShortcut = 1;
};
