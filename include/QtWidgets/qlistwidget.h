// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qbrush.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qabstractitemview.h>

#include <vector>

class QListWidget;

class QListWidgetItem {
public:
    QListWidgetItem();
    explicit QListWidgetItem(const QString &text);
    QListWidgetItem(const QString &text, QListWidget *listWidget);
    QListWidgetItem(const QListWidgetItem &) = delete;
    QListWidgetItem &operator=(const QListWidgetItem &) = delete;
    virtual ~QListWidgetItem();

    QListWidget *listWidget() const { return m_list; }
    QString text() const { return m_text; }
    void setText(const QString &text);
    QString toolTip() const { return m_toolTip; }
    void setToolTip(const QString &toolTip) { m_toolTip = toolTip; }
    Qt::Alignment textAlignment() const { return m_alignment; }
    void setTextAlignment(Qt::Alignment alignment);
    QBrush foreground() const { return m_foreground; }
    void setForeground(const QBrush &brush);
    QBrush background() const { return m_background; }
    void setBackground(const QBrush &brush);
    QFont font() const;
    void setFont(const QFont &font);
    Qt::ItemFlags flags() const { return m_flags; }
    void setFlags(Qt::ItemFlags flags);
    bool isSelected() const { return m_selected; }
    void setSelected(bool select);

private:
    friend class QListWidget;
    void changed();

    QListWidget *m_list = nullptr;
    QString m_text;
    QString m_toolTip;
    Qt::Alignment m_alignment = Qt::AlignLeft | Qt::AlignVCenter;
    QBrush m_foreground;
    QBrush m_background;
    QFont m_font;
    bool m_hasFont = false;
    bool m_selected = false;
    Qt::ItemFlags m_flags = Qt::ItemIsSelectable | Qt::ItemIsEnabled;
};

class QListWidget : public QAbstractItemView {
    Q_OBJECT
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int currentRow READ currentRow WRITE setCurrentRow NOTIFY currentRowChanged)
public:
    explicit QListWidget(QWidget *parent = nullptr);
    ~QListWidget() override;

    int count() const { return static_cast<int>(m_items.size()); }
    QListWidgetItem *item(int row) const;
    int row(const QListWidgetItem *item) const;
    void addItem(QListWidgetItem *item);
    void addItem(const QString &text);
    void addItems(const QStringList &labels);
    void insertItem(int row, QListWidgetItem *item);
    void insertItem(int row, const QString &text);
    void insertItems(int row, const QStringList &labels);
    QListWidgetItem *takeItem(int row);
    void sortItems(Qt::SortOrder order = Qt::AscendingOrder);

    QListWidgetItem *currentItem() const;
    void setCurrentItem(QListWidgetItem *item);
    int currentRow() const { return m_current; }
    QList<QListWidgetItem *> selectedItems() const;
    QListWidgetItem *itemAt(const QPoint &pos) const;
    QListWidgetItem *itemAt(int x, int y) const { return itemAt(QPoint(x, y)); }
    QRect visualItemRect(const QListWidgetItem *item) const;
    void scrollToItem(const QListWidgetItem *item);
    int rowHeight() const;

public slots:
    void setCurrentRow(int row);
    void clear();
    void selectAll();

signals:
    /*!
        This signal is emitted when the current item changes from \a previous to \a current, either of which may be
        \c nullptr.
    */
    void currentItemChanged(QListWidgetItem *current, QListWidgetItem *previous) {
        emitSignal(&QListWidget::currentItemChanged, current, previous);
    }
    /*!
        This signal is emitted when the current row changes to \a currentRow, or -1 if there is none.
    */
    void currentRowChanged(int currentRow) { emitSignal(&QListWidget::currentRowChanged, currentRow); }
    /*!
        This signal is emitted when the current item changes, with the \a currentText of the new one, which is empty
        if there is none.
    */
    void currentTextChanged(const QString &currentText) { emitSignal(&QListWidget::currentTextChanged, currentText); }
    /*!
        This signal is emitted when the user clicks \a item.
    */
    void itemClicked(QListWidgetItem *item) { emitSignal(&QListWidget::itemClicked, item); }
    /*!
        This signal is emitted when the user double-clicks \a item.
    */
    void itemDoubleClicked(QListWidgetItem *item) { emitSignal(&QListWidget::itemDoubleClicked, item); }
    /*!
        This signal is emitted when the user activates \a item: double-clicks it, or presses Return or Enter while it
        is the current item.
    */
    void itemActivated(QListWidgetItem *item) { emitSignal(&QListWidget::itemActivated, item); }
    /*!
        This signal is emitted when the selection changes.
    */
    void itemSelectionChanged() { emitSignal(&QListWidget::itemSelectionChanged); }

protected:
    bool viewportEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void changeEvent(QEvent *event) override;
    void selectionModeChanged() override;

private:
    friend class QListWidgetItem;
    void adopt(QListWidgetItem *item);
    void updateScrollBars();
    void itemChanged(QListWidgetItem *item);
    void forget(QListWidgetItem *item);
    bool selectable(int row) const;
    bool enabled(int row) const;
    void moveCurrent(int row);
    void select(int row, Qt::KeyboardModifiers modifiers);
    void setRowSelected(int row, bool selected);
    bool clearSelection(int except);
    void scrollToRow(int row);
    int rowAtY(int y) const;
    void navigate(int target, Qt::KeyboardModifiers modifiers);
    void finishSelection();
    void search(const QString &text);

    std::vector<QListWidgetItem *> m_items;
    int m_current = -1;
    int m_anchor = -1;
    int m_pressed = -1;
    int m_scrollRow = -1;
    bool m_selectionDirty = false;
    bool m_updating = false;
    bool m_updateAgain = false;
};
