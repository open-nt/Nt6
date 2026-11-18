// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

#include <QtCore/qstringlist.h>
#include <QtCore/qpointer.h>
#include <QtCore/qabstractitemmodel.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QAbstractItemModel;
class QStyleOptionHeader;

class QHeaderView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int offset READ offset WRITE setOffset)
    Q_PROPERTY(int defaultSectionSize READ defaultSectionSize WRITE setDefaultSectionSize)
    Q_PROPERTY(int minimumSectionSize READ minimumSectionSize WRITE setMinimumSectionSize)
    Q_PROPERTY(bool stretchLastSection READ stretchLastSection WRITE setStretchLastSection)
    Q_PROPERTY(bool sectionsClickable READ sectionsClickable WRITE setSectionsClickable)
    Q_PROPERTY(bool showSortIndicator READ isSortIndicatorShown WRITE setSortIndicatorShown)
    Q_PROPERTY(bool highlightSections READ highlightSections WRITE setHighlightSections)
    Q_PROPERTY(bool cascadingSectionResizes READ cascadingSectionResizes WRITE setCascadingSectionResizes)
    Q_PROPERTY(int maximumSectionSize READ maximumSectionSize WRITE setMaximumSectionSize)
    Q_PROPERTY(Qt::Alignment defaultAlignment READ defaultAlignment WRITE setDefaultAlignment)
    Q_PROPERTY(bool sortIndicatorClearable READ isSortIndicatorClearable WRITE setSortIndicatorClearable NOTIFY
                   sortIndicatorClearableChanged)
public:
    enum ResizeMode { Interactive, Stretch, Custom, ResizeToContents, Fixed = Custom };
    Q_ENUM(ResizeMode)

    explicit QHeaderView(Qt::Orientation orientation, QWidget *parent = nullptr);
    ~QHeaderView() override;

    Qt::Orientation orientation() const { return m_orientation; }
    int count() const { return static_cast<int>(m_sizes.size()); }
    int length() const;
    int offset() const { return m_offset; }
    void setOffset(int offset);
    int defaultSectionSize() const { return m_defaultSize; }
    void setDefaultSectionSize(int size);
    int minimumSectionSize() const { return m_minimumSize; }
    void setMinimumSectionSize(int size);
    int sectionSize(int logicalIndex) const;
    void resizeSection(int logicalIndex, int size);
    int sectionPosition(int logicalIndex) const;
    int logicalIndexAt(int position) const;
    int logicalIndex(int visualIndex) const;
    int visualIndex(int logicalIndex) const;
    void moveSection(int from, int to);
    bool sectionsMovable() const { return m_movable; }
    void setSectionsMovable(bool movable) { m_movable = movable; }
    bool sectionsMoved() const;
    QByteArray saveState() const;
    bool restoreState(const QByteArray &state);
    bool isSectionHidden(int logicalIndex) const;
    void setSectionHidden(int logicalIndex, bool hide);
    int hiddenSectionCount() const;
    bool stretchLastSection() const { return m_stretchLast; }
    void setStretchLastSection(bool stretch);
    void setSectionResizeMode(QHeaderView::ResizeMode mode);
    void setSectionResizeMode(int logicalIndex, QHeaderView::ResizeMode mode);
    QHeaderView::ResizeMode sectionResizeMode(int logicalIndex) const;
    int stretchSectionCount() const;
    bool sectionsClickable() const { return m_clickable; }
    void setSectionsClickable(bool clickable);
    bool isSortIndicatorShown() const { return m_sortShown; }
    void setSortIndicatorShown(bool show);
    void setSortIndicator(int logicalIndex, Qt::SortOrder order);
    int sortIndicatorSection() const { return m_sortSection; }
    Qt::SortOrder sortIndicatorOrder() const { return m_sortOrder; }

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;
    int maximumSectionSize() const { return m_maximumSize; }
    void setMaximumSectionSize(int size);
    void resetDefaultSectionSize();
    Qt::Alignment defaultAlignment() const { return m_alignment; }
    void setDefaultAlignment(Qt::Alignment alignment);
    bool highlightSections() const { return m_highlight; }
    void setHighlightSections(bool highlight) { m_highlight = highlight; }
    bool cascadingSectionResizes() const { return m_cascading; }
    void setCascadingSectionResizes(bool enable) { m_cascading = enable; }
    bool isFirstSectionMovable() const { return m_firstMovable; }
    void setFirstSectionMovable(bool movable) { m_firstMovable = movable; }
    bool isSortIndicatorClearable() const { return m_sortClearable; }
    void setSortIndicatorClearable(bool clearable);
    int resizeContentsPrecision() const { return m_precision; }
    void setResizeContentsPrecision(int precision) { m_precision = precision; }
    void resizeSections(QHeaderView::ResizeMode mode);
    int sectionSizeHint(int logicalIndex) const;
    int sectionViewportPosition(int logicalIndex) const;
    int visualIndexAt(int position) const;
    void swapSections(int first, int second);
    bool sectionsHidden() const { return hiddenSectionCount() > 0; }
    QSize sizeHint() const override;
public slots:
    void hideSection(int logicalIndex) { setSectionHidden(logicalIndex, true); }
    void showSection(int logicalIndex) { setSectionHidden(logicalIndex, false); }
    void headerDataChanged(Qt::Orientation orientation, int logicalFirst, int logicalLast);
    void setOffsetToSectionPosition(int visualSectionNumber);
    void setOffsetToLastSection();
    void updateSection(int logicalIndex);
    void resizeSections();

signals:
    /*!
        This signal is emitted when the user clicks the section \a logicalIndex, if the sections are clickable.
    */
    void sectionClicked(int logicalIndex) { emitSignal(&QHeaderView::sectionClicked, logicalIndex); }
    /*!
        \since 6.0

        This signal is emitted when the section \a logicalIndex moves from the visual index \a oldVisualIndex to
        \a newVisualIndex.
    */
    void sectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex) {
        emitSignal(&QHeaderView::sectionMoved, logicalIndex, oldVisualIndex, newVisualIndex);
    }
    /*!
        This signal is emitted when the section that shows the sort indicator, or the order that it shows,
        changes: \a logicalIndex is the section and \a order the order. A view sorts its model by it.

        \sa setSortIndicator()
    */
    void sortIndicatorChanged(int logicalIndex, Qt::SortOrder order) {
        emitSignal(&QHeaderView::sortIndicatorChanged, logicalIndex, order);
    }
    /*!
        This signal is emitted when the section \a logicalIndex changes its size from \a oldSize to \a newSize,
        whether the user dragged its edge or resizeSection() was called.
    */
    void sectionResized(int logicalIndex, int oldSize, int newSize) {
        emitSignal(&QHeaderView::sectionResized, logicalIndex, oldSize, newSize);
    }
    /*!
        \since 6.0
        This signal is emitted when the user presses the mouse on the section \a logicalIndex.
    */
    void sectionPressed(int logicalIndex) { emitSignal(&QHeaderView::sectionPressed, logicalIndex); }
    /*!
        \since 6.0
        This signal is emitted when the user double-clicks the section \a logicalIndex.
    */
    void sectionDoubleClicked(int logicalIndex) { emitSignal(&QHeaderView::sectionDoubleClicked, logicalIndex); }
    /*!
        \since 6.0
        This signal is emitted when the mouse moves onto the section \a logicalIndex with the button down.
    */
    void sectionEntered(int logicalIndex) { emitSignal(&QHeaderView::sectionEntered, logicalIndex); }
    /*!
        \since 6.0
        This signal is emitted when the user double-clicks the edge after the section \a logicalIndex.
    */
    void sectionHandleDoubleClicked(int logicalIndex) {
        emitSignal(&QHeaderView::sectionHandleDoubleClicked, logicalIndex);
    }
    /*!
        \since 6.0
        This signal is emitted when the number of sections changes from \a oldCount to \a newCount.
    */
    void sectionCountChanged(int oldCount, int newCount) {
        emitSignal(&QHeaderView::sectionCountChanged, oldCount, newCount);
    }
    /*!
        \since 6.0
        This signal is emitted when the size the header needs changes.
    */
    void geometriesChanged() { emitSignal(&QHeaderView::geometriesChanged); }
    /*!
        \since 6.0
        This signal is emitted when whether the sort indicator can be cleared changes to \a clearable.
    */
    void sortIndicatorClearableChanged(bool clearable) {
        emitSignal(&QHeaderView::sortIndicatorClearableChanged, clearable);
    }

protected:
    virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const;
    virtual QSize sectionSizeFromContents(int logicalIndex) const;
    void initStyleOption(QStyleOptionHeader *option) const;
    void initStyleOptionForIndex(QStyleOptionHeader *option, int logicalIndex) const;
    void initialize();
    void initializeSections();
    void sectionsInserted(const QModelIndex &parent, int logicalFirst, int logicalLast);
    void sectionsAboutToBeRemoved(const QModelIndex &parent, int logicalFirst, int logicalLast);
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool event(QEvent *event) override;

private:
    friend class QTableView;
    friend class QTableWidget;
    friend class QTreeView;
    void setCount(int count);
    void insertSections(int logicalIndex, int count);
    void removeSections(int logicalIndex, int count);
    void setLabels(const QStringList &labels);
    QString labelAt(int logicalIndex) const;
    int storedSize(int logicalIndex) const;
    int lastVisible() const;
    int edgeAt(const QPoint &pos) const;
    // The sizes of all sections, the stretched ones included; 0 for hidden sections.
    std::vector<int> effectiveSizes() const;
    // The sections whose size follows their contents, which the view sizes when it lays its items out.
    std::vector<int> contentsSections() const;

    Qt::Orientation m_orientation;
    std::vector<int> m_sizes;
    std::vector<char> m_hidden; // parallel to m_sizes
    std::vector<char> m_modes;  // parallel to m_sizes: a ResizeMode each
    std::vector<int> m_order;   // the logical index of each visual index
    bool m_movable = false;
    bool m_moving = false; // the pressed section is being dragged to another place
    int m_pressPosition = 0;
    ResizeMode m_defaultMode = Interactive;
    QStringList m_labels;
    QStringList m_toolTips; // set by QTableWidget from its header items
    int m_offset = 0;
    int m_defaultSize = 100;
    int m_minimumSize = 20;
    bool m_stretchLast = false;
    bool m_clickable = false;
    bool m_sortShown = false;
    int m_sortSection = -1;
    Qt::SortOrder m_sortOrder = Qt::AscendingOrder;
    int m_pressed = -1;
    int m_resizing = -1;
    int m_resizeStart = 0;
    int m_resizeOriginal = 0;
    int m_maximumSize = 1048575;
    Qt::Alignment m_alignment = Qt::AlignCenter;
    bool m_highlight = false;
    bool m_cascading = false;
    bool m_firstMovable = true;
    bool m_sortClearable = false;
    int m_precision = 1000;
    int m_entered = -1; // the section the mouse is over while the button is down
    QPointer<QAbstractItemModel> m_model;
    QList<QMetaObject::Connection> m_modelConnections;
    void readModel();
};
