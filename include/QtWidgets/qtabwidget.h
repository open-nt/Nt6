// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QTabBar;

class QTabWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
public:
    explicit QTabWidget(QWidget *parent = nullptr);
    ~QTabWidget() override;

    int addTab(QWidget *page, const QString &label);
    int insertTab(int index, QWidget *page, const QString &label);
    void removeTab(int index);
    void clear();
    int count() const { return static_cast<int>(m_pages.size()); }
    int currentIndex() const;
    QWidget *currentWidget() const { return widget(currentIndex()); }
    QWidget *widget(int index) const;
    int indexOf(const QWidget *page) const;
    QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    bool isTabEnabled(int index) const;
    void setTabEnabled(int index, bool enabled);
    QString tabToolTip(int index) const;
    void setTabToolTip(int index, const QString &tip);
    bool tabsClosable() const;
    void setTabsClosable(bool closable);
    bool isMovable() const;
    void setMovable(bool movable);
    QTabBar *tabBar() const { return m_bar; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget *page);

signals:
    /*!
        This signal is emitted when the current tab changes to the one at \a index, which is -1 if there are no
        tabs left.
    */
    void currentChanged(int index) { emitSignal(&QTabWidget::currentChanged, index); }
    /*!
        This signal is emitted when the user clicks the close button of the tab at \a index.
    */
    void tabCloseRequested(int index) { emitSignal(&QTabWidget::tabCloseRequested, index); }
    /*!
        This signal is emitted when the user presses on the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarClicked(int index) { emitSignal(&QTabWidget::tabBarClicked, index); }
    /*!
        This signal is emitted when the user double-clicks the tab at \a index, or -1 for the empty part of the bar.
    */
    void tabBarDoubleClicked(int index) { emitSignal(&QTabWidget::tabBarDoubleClicked, index); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void layoutChildren();
    void pageGone(QObject *page);
    void select(int index);
    bool focusIn(const QWidget *page) const;

    std::vector<QWidget *> m_pages;
    QPointer<QWidget> m_shown; // the page that is shown
    QTabBar *m_bar = nullptr;
};
