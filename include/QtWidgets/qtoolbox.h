// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qframe.h>

#include <vector>

class QAbstractButton;
class QVBoxLayout;

class QToolBox : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
public:
    explicit QToolBox(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~QToolBox() override;

    int addItem(QWidget *widget, const QString &text) { return insertItem(-1, widget, text); }
    int insertItem(int index, QWidget *widget, const QString &text);
    void removeItem(int index);
    void setItemEnabled(int index, bool enabled);
    bool isItemEnabled(int index) const;
    void setItemText(int index, const QString &text);
    QString itemText(int index) const;
    void setItemToolTip(int index, const QString &toolTip);
    QString itemToolTip(int index) const;
    int currentIndex() const { return m_current; }
    QWidget *currentWidget() const { return widget(m_current); }
    QWidget *widget(int index) const;
    int indexOf(const QWidget *widget) const;
    int count() const { return static_cast<int>(m_items.size()); }

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget *widget);

signals:
    /*!
        This signal is emitted when the current item changes to the one at \a index, which is -1 if there are no items
        left.
    */
    void currentChanged(int index) { emitSignal(&QToolBox::currentChanged, index); }

protected:
    virtual void itemInserted(int index);
    virtual void itemRemoved(int index);

private:
    struct Item {
        QAbstractButton *button;
        QWidget *page;
    };

    void relayout();
    void pageGone(QObject *page);

    std::vector<Item> m_items;
    QVBoxLayout *m_layout = nullptr;
    int m_current = -1;
};
