// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtCore/qtimer.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qwidget.h>

#include <type_traits>
#include <vector>

class QMenuBar;
class QStyleOptionMenuItem;

class QMenu : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QIcon icon READ icon WRITE setIcon)
    Q_PROPERTY(bool tearOffEnabled READ isTearOffEnabled WRITE setTearOffEnabled)
    Q_PROPERTY(bool separatorsCollapsible READ separatorsCollapsible WRITE setSeparatorsCollapsible)
    Q_PROPERTY(bool toolTipsVisible READ toolTipsVisible WRITE setToolTipsVisible)
public:
    explicit QMenu(QWidget *parent = nullptr);
    explicit QMenu(const QString &title, QWidget *parent = nullptr);
    ~QMenu() override;

    using QWidget::addAction;
    QAction *addAction(const QString &text);
    template <typename Functor> QAction *addAction(const QString &text, Functor functor) {
        QAction *action = addAction(text);
        QObject::connect(action, &QAction::triggered, action, std::move(functor));
        return action;
    }
    QAction *addAction(const QIcon &icon, const QString &text);
    template <typename Functor> QAction *addAction(const QIcon &icon, const QString &text, Functor functor) {
        QAction *action = addAction(icon, text);
        QObject::connect(action, &QAction::triggered, action, std::move(functor));
        return action;
    }
    QAction *addAction(const QString &text, const QObject *receiver, const char *member,
                       const QKeySequence &shortcut = QKeySequence());
    template <typename Receiver, typename Functor,
              typename = std::enable_if_t<std::is_base_of_v<QObject, Receiver> &&
                                          !std::is_convertible_v<Functor, const char *>>>
    QAction *addAction(const QString &text, const Receiver *receiver, Functor functor,
                       const QKeySequence &shortcut = QKeySequence()) {
        QAction *action = addAction(text);
        action->setShortcut(shortcut);
        QObject::connect(action, &QAction::triggered, receiver, std::move(functor));
        return action;
    }
    QAction *addAction(const QString &text, const QKeySequence &shortcut);
    QAction *addAction(const QIcon &icon, const QString &text, const QKeySequence &shortcut);
    QAction *addAction(const QString &text, const QKeySequence &shortcut, const QObject *receiver, const char *member,
                       Qt::ConnectionType type = Qt::AutoConnection);
    template <typename Receiver, typename Functor,
              typename = std::enable_if_t<std::is_base_of_v<QObject, Receiver> &&
                                          !std::is_convertible_v<Functor, const char *>>>
    QAction *addAction(const QString &text, const QKeySequence &shortcut, const Receiver *receiver, Functor functor,
                       Qt::ConnectionType type = Qt::AutoConnection) {
        QAction *action = addAction(text, shortcut);
        QObject::connect(action, &QAction::triggered, receiver, std::move(functor), type);
        return action;
    }
    template <typename Functor, typename = std::enable_if_t<!std::is_convertible_v<Functor, const char *> &&
                                                            !std::is_pointer_v<Functor>>>
    QAction *addAction(const QString &text, const QKeySequence &shortcut, Functor functor) {
        QAction *action = addAction(text, shortcut);
        QObject::connect(action, &QAction::triggered, action, std::move(functor));
        return action;
    }
    QAction *addMenu(QMenu *menu);
    QMenu *addMenu(const QString &title);
    QMenu *addMenu(const QIcon &icon, const QString &title);
    QAction *addSeparator();
    QAction *insertMenu(QAction *before, QMenu *menu);
    QAction *insertSeparator(QAction *before);
    QAction *addSection(const QString &text);
    QAction *addSection(const QIcon &icon, const QString &text);
    QAction *insertSection(QAction *before, const QString &text);
    QAction *insertSection(QAction *before, const QIcon &icon, const QString &text);
    QAction *defaultAction() const { return m_default.data(); }
    void setDefaultAction(QAction *act);
    QIcon icon() const;
    void setIcon(const QIcon &icon);
    bool separatorsCollapsible() const { return m_collapsible; }
    void setSeparatorsCollapsible(bool collapse);
    bool toolTipsVisible() const { return m_toolTipsVisible; }
    void setToolTipsVisible(bool visible) { m_toolTipsVisible = visible; }
    bool isTearOffEnabled() const { return m_tearOff; }
    void setTearOffEnabled(bool enable) { m_tearOff = enable; }
    bool isTearOffMenuVisible() const { return false; }
    void showTearOffMenu();
    void showTearOffMenu(const QPoint &pos);
    void hideTearOffMenu();
    void setNoReplayFor(QWidget *widget);
    static QMenu *menuInAction(const QAction *action);
    void clear();
    bool isEmpty() const;
    QString title() const { return m_title; }
    void setTitle(const QString &title);
    QAction *menuAction() const { return m_menuAction; }

    QAction *activeAction() const { return m_active.data(); }
    void setActiveAction(QAction *action);
    QAction *actionAt(const QPoint &point) const;
    QRect actionGeometry(QAction *action) const;

    void popup(const QPoint &pos, QAction *at = nullptr);
    QAction *exec();
    QAction *exec(const QPoint &pos, QAction *at = nullptr);

    QSize sizeHint() const override;

signals:
    /*!
        This signal is emitted when an action of the menu, or of a menu that is open below it, is triggered by the
        user, with that \a action. It comes after the menu has closed.
    */
    void triggered(QAction *action) { emitSignal(&QMenu::triggered, action); }
    /*!
        This signal is emitted when the user moves to an action of the menu, with that \a action. The action is
        hovered too.
    */
    void hovered(QAction *action) { emitSignal(&QMenu::hovered, action); }
    /*!
        This signal is emitted just before the menu is shown.
    */
    void aboutToShow() { emitSignal(&QMenu::aboutToShow); }
    /*!
        This signal is emitted just after the menu has been hidden.
    */
    void aboutToHide() { emitSignal(&QMenu::aboutToHide); }

protected:
    int columnCount() const { return 1; }
    void initStyleOption(QStyleOptionMenuItem *option, const QAction *action) const;
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void actionEvent(QActionEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QMenuBar;
    struct Item {
        QAction *action;
        QRect rect;
    };

    void init();
    void layoutItems() const;
    void showAt(QWidget *host, const QPoint &position, QAction *at, int leftEdge = -1);
    QWidget *host() const;
    void setCurrent(QAction *action, bool openSubMenu);
    void openSubMenu(QAction *action, bool selectFirst);
    void closeSubMenu();
    void activateItem(QAction *action);
    void closeAll();
    QMenu *rootMenu();
    QAction *step(QAction *from, int direction) const;
    QAction *mnemonicTarget(int key, bool *several) const;
    bool isSubMenuAction(const QAction *action) const;
    QMenu *openMenu() const;
    QMenuBar *menuBar() const;

    QString m_title;
    QAction *m_menuAction = nullptr;
    QPointer<QAction> m_active;
    QPointer<QAction> m_triggered;
    QPointer<QWidget> m_open;
    QPointer<QWidget> m_previousFocus;
    QPointer<QWidget> m_bar;
    QTimer m_subMenuTimer;
    QPointer<QAction> m_pendingSubMenu;
    mutable std::vector<Item> m_items;
    mutable QSize m_size;
    mutable bool m_dirty = true;
    bool m_closing = false;
    QPointer<QAction> m_default;
    bool m_collapsible = true;
    bool m_toolTipsVisible = false;
    bool m_tearOff = false;
};
