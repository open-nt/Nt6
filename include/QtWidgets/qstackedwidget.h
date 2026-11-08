// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qframe.h>

#include <memory>

class QStackedLayout;

class QStackedWidget : public QFrame {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentChanged)
    Q_PROPERTY(int count READ count)
public:
    explicit QStackedWidget(QWidget *parent = nullptr);
    ~QStackedWidget() override;

    int addWidget(QWidget *widget);
    int insertWidget(int index, QWidget *widget);
    void removeWidget(QWidget *widget);
    QWidget *widget(int index) const;
    int indexOf(const QWidget *widget) const;
    QWidget *currentWidget() const;
    int currentIndex() const;
    int count() const;

public slots:
    void setCurrentIndex(int index);
    void setCurrentWidget(QWidget *widget);

signals:
    /*!
        This signal is emitted when the current page changes, with its \a index, or -1 if there is none.
    */
    void currentChanged(int index) { emitSignal(&QStackedWidget::currentChanged, index); }
    /*!
        This signal is emitted when the page that was at \a index is removed.
    */
    void widgetRemoved(int index) { emitSignal(&QStackedWidget::widgetRemoved, index); }

private:
    QStackedLayout *m_layout;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
