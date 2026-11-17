// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>

#include <QtCore/qstringlist.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QHeaderView : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int offset READ offset WRITE setOffset)
    Q_PROPERTY(int defaultSectionSize READ defaultSectionSize WRITE setDefaultSectionSize)
    Q_PROPERTY(int minimumSectionSize READ minimumSectionSize WRITE setMinimumSectionSize)
    Q_PROPERTY(bool stretchLastSection READ stretchLastSection WRITE setStretchLastSection)
    Q_PROPERTY(bool sectionsClickable READ sectionsClickable WRITE setSectionsClickable)
    Q_PROPERTY(bool showSortIndicator READ isSortIndicatorShown WRITE setSortIndicatorShown)
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

    QSize sizeHint() const override;

public slots:
    void hideSection(int logicalIndex) { setSectionHidden(logicalIndex, true); }
    void showSection(int logicalIndex) { setSectionHidden(logicalIndex, false); }

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

protected:
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
};
