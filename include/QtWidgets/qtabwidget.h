// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

#include <vector>

class QTabBarWidget;

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
    int count() const { return static_cast<int>(m_tabs.size()); }
    int currentIndex() const { return m_current; }
    QWidget *currentWidget() const { return widget(m_current); }
    QWidget *widget(int index) const;
    int indexOf(const QWidget *page) const;
    QString tabText(int index) const;
    void setTabText(int index, const QString &text);
    bool isTabEnabled(int index) const;
    void setTabEnabled(int index, bool enabled);

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

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    friend class QTabBarWidget;
    struct Tab {
        QWidget *page;
        QString text;
        bool enabled;
    };

    void layoutChildren();
    void pageGone(QObject *page);
    void select(int index);
    bool focusIn(const QWidget *page) const;

    std::vector<Tab> m_tabs;
    int m_current = -1;
    QTabBarWidget *m_bar = nullptr;
};
