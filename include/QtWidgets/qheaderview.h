// MIT License - see LICENSE
#pragma once

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
public:
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
    bool stretchLastSection() const { return m_stretchLast; }
    void setStretchLastSection(bool stretch);
    bool sectionsClickable() const { return m_clickable; }
    void setSectionsClickable(bool clickable);

    QSize sizeHint() const override;

signals:
    /*!
        This signal is emitted when the user clicks the section \a logicalIndex, if the sections are clickable.
    */
    void sectionClicked(int logicalIndex) { emitSignal(&QHeaderView::sectionClicked, logicalIndex); }
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

private:
    friend class QTableWidget;
    void setCount(int count);
    void insertSections(int logicalIndex, int count);
    void removeSections(int logicalIndex, int count);
    void setLabels(const QStringList &labels);
    QString labelAt(int logicalIndex) const;
    int storedSize(int logicalIndex) const;
    int edgeAt(const QPoint &pos) const;

    Qt::Orientation m_orientation;
    std::vector<int> m_sizes;
    QStringList m_labels;
    int m_offset = 0;
    int m_defaultSize = 100;
    int m_minimumSize = 20;
    bool m_stretchLast = false;
    bool m_clickable = false;
    int m_pressed = -1;
    int m_resizing = -1;
    int m_resizeStart = 0;
    int m_resizeOriginal = 0;
};
