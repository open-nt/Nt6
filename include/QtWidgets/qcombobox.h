// MIT License - see LICENSE
#pragma once

#include <QtGui/qicon.h>

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qpointer.h>
#include <QtCore/qmap.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qwidget.h>

class QAbstractItemDelegate;
class QAbstractItemView;
class QComboPopup;

class QLineEdit;

class QComboBox : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(QString currentText READ currentText WRITE setCurrentText NOTIFY currentTextChanged USER true)
    Q_PROPERTY(int count READ count)
    Q_PROPERTY(int maxVisibleItems READ maxVisibleItems WRITE setMaxVisibleItems)
    Q_PROPERTY(bool editable READ isEditable WRITE setEditable)
    Q_PROPERTY(InsertPolicy insertPolicy READ insertPolicy WRITE setInsertPolicy)
    Q_PROPERTY(int modelColumn READ modelColumn WRITE setModelColumn)
public:
    enum InsertPolicy {
        NoInsert,
        InsertAtTop,
        InsertAtCurrent,
        InsertAtBottom,
        InsertAfterCurrent,
        InsertBeforeCurrent,
        InsertAlphabetically
    };
    Q_ENUM(InsertPolicy)

    explicit QComboBox(QWidget *parent = nullptr);
    ~QComboBox() override;

    int count() const;
    QString itemText(int index) const;
    void setItemText(int index, const QString &text);
    int currentIndex() const { return m_current; }
    QString currentText() const;
    bool isEditable() const { return m_edit != nullptr; }
    void setEditable(bool editable);
    QLineEdit *lineEdit() const { return m_edit; }
    InsertPolicy insertPolicy() const { return m_insertPolicy; }
    void setInsertPolicy(InsertPolicy policy) { m_insertPolicy = policy; }
    int findText(const QString &text,
                 Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchExactly | Qt::MatchCaseSensitive)) const;
    QString placeholderText() const { return m_placeholder; }
    void setPlaceholderText(const QString &placeholderText);
    QAbstractItemView *view() const;
    QAbstractItemDelegate *itemDelegate() const;
    void setItemDelegate(QAbstractItemDelegate *delegate);
    int findData(const QVariant &data, int role = Qt::UserRole,
                 Qt::MatchFlags flags = Qt::MatchFlags(Qt::MatchExactly | Qt::MatchCaseSensitive)) const;
    QVariant itemData(int index, int role = Qt::UserRole) const;
    void setItemData(int index, const QVariant &value, int role = Qt::UserRole);
    QVariant currentData(int role = Qt::UserRole) const { return itemData(m_current, role); }
    int maxVisibleItems() const { return m_maxVisible; }
    void setMaxVisibleItems(int count);
    bool isPopupVisible() const;

    void addItem(const QString &text, const QVariant &userData = QVariant());
    void addItem(const QIcon &icon, const QString &text, const QVariant &userData = QVariant());
    void insertItem(int index, const QIcon &icon, const QString &text, const QVariant &userData = QVariant());
    QIcon itemIcon(int index) const;
    void setItemIcon(int index, const QIcon &icon);
    QSize iconSize() const;
    void setIconSize(const QSize &size);
    void addItems(const QStringList &texts);
    void insertItem(int index, const QString &text, const QVariant &userData = QVariant());
    void removeItem(int index);

    QAbstractItemModel *model() const { return m_model; }
    void setModel(QAbstractItemModel *model);
    int modelColumn() const { return m_modelColumn; }
    void setModelColumn(int visibleColumn);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentText(const QString &text);
    void setEditText(const QString &text);
    void clearEditText() { setEditText(QString()); }
    void clear();
    virtual void showPopup();
    virtual void hidePopup();

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
    /*!
        \since 6.0
        This signal is emitted when the text in the line edit of an editable combo box changes, to \a text.
    */
    void editTextChanged(const QString &text) { emitSignal(&QComboBox::editTextChanged, text); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    friend class QComboPopup;
    void choose(int index);
    QRect editRect() const;
    void commitEdit();
    QModelIndex itemIndex(int row) const;
    bool isItemEnabled(int row) const;
    int nextEnabled(int from, int step) const; // the first enabled item from `from` on, going by `step`; -1 if none
    void connectModel();
    void announceCurrent(); // emits the signals for a new current item
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);
    void itemsChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);
    void modelWasReset();

    QAbstractItemModel *m_model = nullptr; // a QStandardItemModel of the combo box, or the one setModel() set
    QList<QMetaObject::Connection> m_modelConnections;
    int m_modelColumn = 0;
    QString m_announcedText; // the text of the current item when currentTextChanged() was last emitted
    int m_current = -1;
    int m_maxVisible = 10;
    bool m_down = false;
    QPointer<QWidget> m_popup;
    QLineEdit *m_edit = nullptr; // a child, while the combo box is editable
    InsertPolicy m_insertPolicy = InsertAtBottom;
    QSize m_iconSize; // invalid: the small icon size of the style
    QString m_placeholder;
    QPointer<QAbstractItemDelegate> m_delegate;
    mutable QAbstractItemView *m_view = nullptr; // made by view(): a hidden list view that holds settings
};
