// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtWidgets/qsizepolicy.h>

class QLayout;
class QSpacerItem;
class QWidget;

// The largest size a widget or layout can have; Qt's QWIDGETSIZE_MAX.
constexpr int QLAYOUTSIZE_MAX = (1 << 24) - 1;

class QLayoutItem {
public:
    explicit QLayoutItem(Qt::Alignment alignment = Qt::Alignment()) : m_alignment(alignment) {}
    virtual ~QLayoutItem();

    virtual QSize sizeHint() const = 0;
    virtual QSize minimumSize() const = 0;
    virtual QSize maximumSize() const = 0;
    virtual Qt::Orientations expandingDirections() const = 0;
    virtual void setGeometry(const QRect &rect) = 0;
    virtual QRect geometry() const = 0;
    virtual bool isEmpty() const = 0;
    virtual void invalidate() {}
    virtual QWidget *widget() const { return nullptr; }
    virtual QLayout *layout() { return nullptr; }
    virtual QSpacerItem *spacerItem() { return nullptr; }

    Qt::Alignment alignment() const { return m_alignment; }
    void setAlignment(Qt::Alignment alignment) { m_alignment = alignment; }

private:
    Qt::Alignment m_alignment;
};

class QSpacerItem : public QLayoutItem {
public:
    QSpacerItem(int w, int h, QSizePolicy::Policy hPolicy = QSizePolicy::Minimum,
                QSizePolicy::Policy vPolicy = QSizePolicy::Minimum);

    QSize sizeHint() const override { return m_size; }
    QSize minimumSize() const override;
    QSize maximumSize() const override;
    Qt::Orientations expandingDirections() const override { return m_policy.expandingDirections(); }
    void setGeometry(const QRect &rect) override { m_rect = rect; }
    QRect geometry() const override { return m_rect; }
    bool isEmpty() const override { return true; }
    QSpacerItem *spacerItem() override { return this; }
    QSizePolicy sizePolicy() const { return m_policy; }

private:
    QSize m_size;
    QSizePolicy m_policy;
    QRect m_rect;
};

class QWidgetItem : public QLayoutItem {
public:
    explicit QWidgetItem(QWidget *widget) : m_widget(widget) {}

    QSize sizeHint() const override;
    QSize minimumSize() const override;
    QSize maximumSize() const override;
    Qt::Orientations expandingDirections() const override;
    void setGeometry(const QRect &rect) override;
    QRect geometry() const override;
    bool isEmpty() const override;
    QWidget *widget() const override { return m_widget; }

private:
    QWidget *m_widget;
};
