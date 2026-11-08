// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qabstractscrollarea.h>

class QAbstractItemView : public QAbstractScrollArea {
    Q_OBJECT
    Q_PROPERTY(SelectionMode selectionMode READ selectionMode WRITE setSelectionMode)
    Q_PROPERTY(bool alternatingRowColors READ alternatingRowColors WRITE setAlternatingRowColors)
public:
    enum SelectionMode { NoSelection, SingleSelection, MultiSelection, ExtendedSelection };
    Q_ENUM(SelectionMode)

    explicit QAbstractItemView(QWidget *parent = nullptr);
    ~QAbstractItemView() override;

    SelectionMode selectionMode() const { return m_selectionMode; }
    void setSelectionMode(SelectionMode mode);
    bool alternatingRowColors() const { return m_alternating; }
    void setAlternatingRowColors(bool enable);

protected:
    virtual void selectionModeChanged();

private:
    SelectionMode m_selectionMode = SingleSelection;
    bool m_alternating = false;
};
