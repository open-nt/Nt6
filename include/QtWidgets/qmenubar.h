// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QMenu;

class QMenuBar : public QWidget {
    Q_OBJECT
public:
    explicit QMenuBar(QWidget *parent = nullptr);
    ~QMenuBar() override;

    using QWidget::addAction;
    QAction *addAction(const QString &text);
    template <typename Functor> QAction *addAction(const QString &text, Functor functor) {
        QAction *action = addAction(text);
        QObject::connect(action, &QAction::triggered, action, std::move(functor));
        return action;
    }
    QAction *addMenu(QMenu *menu);
    QMenu *addMenu(const QString &title);
    QAction *insertMenu(QAction *before, QMenu *menu);
    void clear();

    QAction *activeAction() const { return m_active.data(); }
    void setActiveAction(QAction *action);
    QAction *actionAt(const QPoint &point) const;
    QRect actionGeometry(QAction *action) const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    /*!
        This signal is emitted when the user triggers an action of the menu bar or of one of its menus, with that
        \a action.
    */
    void triggered(QAction *action) { emitSignal(&QMenuBar::triggered, action); }
    /*!
        This signal is emitted when the user moves to an action of the menu bar or of one of its menus, with that
        \a action.
    */
    void hovered(QAction *action) { emitSignal(&QMenuBar::hovered, action); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void actionEvent(QActionEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QMenu;
    friend struct QtPrivate::WidgetAccess;
    struct Item {
        QAction *action;
        QRect rect;
    };

    void layoutItems() const;
    QMenu *openMenu() const;
    void popupMenu(QAction *action, bool selectFirst);
    void closeMenu();
    void menuClosed(QMenu *menu);
    void navigate(QMenu *from, int direction);
    bool activateMnemonic(int key);

    QPointer<QAction> m_active;
    QPointer<QWidget> m_open;
    mutable std::vector<Item> m_items;
    mutable QSize m_size;
    mutable bool m_dirty = true;
};
