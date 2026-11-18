// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtCore/qobject.h>
#include <QtWidgets/qlayoutitem.h>

#include <memory>

class QWidget;

class QLayout : public QObject, public QLayoutItem {
    Q_OBJECT
    Q_PROPERTY(int spacing READ spacing WRITE setSpacing)
    Q_PROPERTY(SizeConstraint sizeConstraint READ sizeConstraint WRITE setSizeConstraint)
public:
    enum SizeConstraint {
        SetDefaultConstraint,
        SetNoConstraint,
        SetMinimumSize,
        SetFixedSize,
        SetMaximumSize,
        SetMinMaxSize
    };
    Q_ENUM(SizeConstraint)

    explicit QLayout(QWidget *parent = nullptr);
    ~QLayout() override;

    int spacing() const;
    void setSpacing(int spacing);
    SizeConstraint sizeConstraint() const;
    void setSizeConstraint(SizeConstraint constraint);
    SizeConstraint horizontalSizeConstraint() const;
    void setHorizontalSizeConstraint(SizeConstraint constraint);
    SizeConstraint verticalSizeConstraint() const;
    void setVerticalSizeConstraint(SizeConstraint constraint);
    void setSizeConstraints(SizeConstraint horizontal, SizeConstraint vertical);
    bool isEnabled() const;
    void setEnabled(bool enable);
    void getContentsMargins(int *left, int *top, int *right, int *bottom) const;
    void unsetContentsMargins();
    QLayoutItem *replaceWidget(QWidget *from, QWidget *to, Qt::FindChildOptions options = Qt::FindChildrenRecursively);
    QSize totalSizeHint() const;
    QSize totalMinimumSize() const;
    QSize totalMaximumSize() const;
    int totalHeightForWidth(int w) const;
    int totalMinimumHeightForWidth(int w) const;
    QSizePolicy::ControlTypes controlTypes() const override;
    static QSize closestAcceptableSize(const QWidget *widget, const QSize &size);
    QWidget *menuBar() const;
    void setMenuBar(QWidget *widget);
    void setContentsMargins(int left, int top, int right, int bottom);
    void setContentsMargins(const QMargins &margins);
    QMargins contentsMargins() const;
    QRect contentsRect() const;

    void addWidget(QWidget *widget);
    virtual void addItem(QLayoutItem *item) = 0;
    virtual void removeItem(QLayoutItem *item);
    void removeWidget(QWidget *widget);
    virtual QLayoutItem *itemAt(int index) const = 0;
    virtual QLayoutItem *takeAt(int index) = 0;
    virtual int count() const = 0;
    int indexOf(const QWidget *widget) const;
    QWidget *parentWidget() const;

    bool activate();
    void update();
    void invalidate() override;
    QRect geometry() const override { return m_rect; }
    void setGeometry(const QRect &rect) override;
    bool isEmpty() const override;
    QLayout *layout() override { return this; }
    QSize maximumSize() const override;
    Qt::Orientations expandingDirections() const override;

protected:
    // Makes `widget` a child of the widget that the layout manages.
    void addChildWidget(QWidget *widget);
    void addChildLayout(QLayout *l);
    bool adoptLayout(QLayout *layout);
    QRect alignmentRect(const QRect &r) const;
    void widgetEvent(QEvent *event);

private:
    friend class QBoxLayout;
    friend class QGridLayout;
    friend class QFormLayout;
    friend class QStackedLayout;
    friend class QWidget;
    void clearPendingRequest();
    // Makes the widgets of the layout, and of the layouts in it, children of the widget it manages.
    void reparentChildWidgets();
    // Puts `item` in the place of the item at `index` and returns that one, or null if the layout cannot.
    virtual QLayoutItem *replaceAt(int index, QLayoutItem *item);
    QSize addTotalMargins(const QSize &size) const;
    struct Data;
    std::unique_ptr<Data> m_d;
    QRect m_rect;
};
