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
public:
    explicit QLayout(QWidget *parent = nullptr);
    ~QLayout() override;

    int spacing() const;
    void setSpacing(int spacing);
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

private:
    friend class QBoxLayout;
    friend class QWidget;
    void clearPendingRequest();
    // Makes the widgets of the layout, and of the layouts in it, children of the widget it manages.
    void reparentChildWidgets();
    struct Data;
    std::unique_ptr<Data> m_d;
    QRect m_rect;
};
