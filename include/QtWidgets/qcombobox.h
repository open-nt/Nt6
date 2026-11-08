// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qwidget.h>

class QComboPopup;

class QComboBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int maxVisibleItems READ maxVisibleItems WRITE setMaxVisibleItems)
public:
    explicit QComboBox(QWidget *parent = nullptr);
    ~QComboBox() override;

    int count() const { return static_cast<int>(m_items.size()); }
    QString itemText(int index) const;
    void setItemText(int index, const QString &text);
    int currentIndex() const { return m_current; }
    QString currentText() const { return itemText(m_current); }
    int findText(const QString &text) const;
    int maxVisibleItems() const { return m_maxVisible; }
    void setMaxVisibleItems(int count);
    bool isPopupVisible() const;

    void addItem(const QString &text);
    void addItems(const QStringList &texts);
    void insertItem(int index, const QString &text);
    void removeItem(int index);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentText(const QString &text);
    void clear();
    void showPopup();
    void hidePopup();

signals:
    /*!
        This signal is emitted when the current item changes to the one at \a index, which is -1 if there is none.
        Its cause can be the user, setCurrentIndex() or a change of the items.
    */
    void currentIndexChanged(int index) { emitSignal(&QComboBox::currentIndexChanged, index); }
    /*!
        This signal is emitted when the text of the current item changes to \a text.
    */
    void currentTextChanged(const QString &text) { emitSignal(&QComboBox::currentTextChanged, text); }
    /*!
        This signal is emitted when the user chooses the item at \a index, also when it is the current one already.
    */
    void activated(int index) { emitSignal(&QComboBox::activated, index); }
    /*!
        This signal is emitted when the user moves over the item at \a index in the popup.
    */
    void highlighted(int index) { emitSignal(&QComboBox::highlighted, index); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QComboPopup;
    void choose(int index);

    QStringList m_items;
    int m_current = -1;
    int m_maxVisible = 10;
    bool m_down = false;
    QPointer<QWidget> m_popup;
};
