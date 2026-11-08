// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

#include <memory>
#include <vector>

class QStatusBar : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool sizeGripEnabled READ isSizeGripEnabled WRITE setSizeGripEnabled)
public:
    explicit QStatusBar(QWidget *parent = nullptr);
    ~QStatusBar() override;

    void addWidget(QWidget *widget, int stretch = 0);
    void insertWidget(int index, QWidget *widget, int stretch = 0);
    void addPermanentWidget(QWidget *widget, int stretch = 0);
    void insertPermanentWidget(int index, QWidget *widget, int stretch = 0);
    void removeWidget(QWidget *widget);
    QString currentMessage() const { return m_message; }
    bool isSizeGripEnabled() const { return m_sizeGrip; }
    void setSizeGripEnabled(bool enabled) { m_sizeGrip = enabled; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void showMessage(const QString &text, int timeout = 0);
    void clearMessage();

signals:
    /*!
        This signal is emitted when the message changes to \a message, which is empty when it is cleared.
    */
    void messageChanged(const QString &message) { emitSignal(&QStatusBar::messageChanged, message); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void timerEvent(QTimerEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    struct Entry {
        QWidget *widget;
        int stretch;
        bool permanent;
        bool hiddenByUs;
    };
    void insertEntry(int index, QWidget *widget, int stretch, bool permanent);
    void layoutEntries();
    void entryDestroyed(QObject *widget);

    std::vector<Entry> m_entries;
    QString m_message;
    int m_timer = 0;
    bool m_sizeGrip = false;
    std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
