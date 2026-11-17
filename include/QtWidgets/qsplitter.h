// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtWidgets/qframe.h>

#include <memory>
#include <vector>

class QSplitterHandle;

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
    QSplitterHandle *handle(int index) const;
    QWidget *replaceWidget(int index, QWidget *widget);
    bool opaqueResize() const { return m_opaque; }
    void setOpaqueResize(bool opaque = true) { m_opaque = opaque; }
    QByteArray saveState() const;
    bool restoreState(const QByteArray &state);
    void refresh();

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /*!
        This signal is emitted when the user moves the handle with the given \a index (the first handle is 1) to
        \a pos, a position in the splitter along its orientation.
    */
    void splitterMoved(int pos, int index) { emitSignal(&QSplitter::splitterMoved, pos, index); }

protected:
    virtual QSplitterHandle *createHandle();
    int closestLegalPosition(int pos, int index);
    void moveSplitter(int pos, int index);
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QSplitterHandle;
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
    std::vector<QSplitterHandle *> m_handles; // m_handles[i] is between item i and item i + 1
    bool m_opaque = true;
    Qt::Orientation m_orientation = Qt::Horizontal;
    int m_handleWidth = -1;
    bool m_collapsible = true;
    bool m_sized = false;
    bool m_shown = false;
    bool m_laying = false;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};

class QSplitterHandle : public QWidget {
    Q_OBJECT
public:
    explicit QSplitterHandle(Qt::Orientation orientation, QSplitter *parent);
    ~QSplitterHandle() override;

    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }
    bool opaqueResize() const;
    QSplitter *splitter() const { return m_splitter; }

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void moveSplitter(int pos);
    int closestLegalPosition(int pos);

private:
    int index() const; // the widget that the handle is after
    QSplitter *m_splitter;
    Qt::Orientation m_orientation;
    int m_grab = 0;
    bool m_dragging = false;
};
