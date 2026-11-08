// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtWidgets/qframe.h>

#include <memory>
#include <vector>

namespace QtPrivate {
class SplitterHandle;
}

class QSplitter : public QFrame {
    Q_OBJECT
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(bool childrenCollapsible READ childrenCollapsible WRITE setChildrenCollapsible)
    Q_PROPERTY(int handleWidth READ handleWidth WRITE setHandleWidth)
public:
    explicit QSplitter(QWidget *parent = nullptr);
    explicit QSplitter(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QSplitter() override;

    void addWidget(QWidget *widget);
    void insertWidget(int index, QWidget *widget);
    int indexOf(const QWidget *widget) const;
    QWidget *widget(int index) const;
    int count() const { return static_cast<int>(m_items.size()); }

    Qt::Orientation orientation() const { return m_orientation; }
    void setOrientation(Qt::Orientation orientation);
    QList<int> sizes() const;
    void setSizes(const QList<int> &list);
    void setStretchFactor(int index, int stretch);
    bool childrenCollapsible() const { return m_collapsible; }
    void setChildrenCollapsible(bool collapse);
    void setCollapsible(int index, bool collapse);
    bool isCollapsible(int index) const;
    int handleWidth() const;
    void setHandleWidth(int width);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /*!
        This signal is emitted when the user moves the handle with the given \a index (the first handle is 1) to
        \a pos, a position in the splitter along its orientation.
    */
    void splitterMoved(int pos, int index) { emitSignal(&QSplitter::splitterMoved, pos, index); }

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QtPrivate::SplitterHandle;
    struct Item {
        QWidget *widget;
        int size;
        int stretch;
        int collapsible; // -1 follows childrenCollapsible()
    };
    void init();
    void removeItem(QObject *widget);
    int available() const;
    int minimumOf(const Item &item) const;
    bool canCollapse(const Item &item) const;
    void fit();
    void layoutChildren();
    void moveHandle(int index, int pos);
    int startOf(int index) const;

    std::vector<Item> m_items;
    std::vector<QtPrivate::SplitterHandle *> m_handles; // m_handles[i] is between item i and item i + 1
    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_handleWidth = -1;
    bool m_collapsible = true;
    bool m_sized = false;
    bool m_shown = false;
    bool m_laying = false;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
