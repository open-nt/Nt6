// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qcolor.h>
#include <QtGui/qpixmap.h>
#include <QtWidgets/qwidget.h>

class QPainter;

class QSplashScreen : public QWidget {
    Q_OBJECT
public:
    explicit QSplashScreen(const QPixmap &pixmap = QPixmap(), Qt::WindowFlags f = Qt::WindowFlags());
    ~QSplashScreen() override;

    void setPixmap(const QPixmap &pixmap);
    QPixmap pixmap() const { return m_pixmap; }
    void finish(QWidget *mainWin);
    QString message() const { return m_message; }

    QSize sizeHint() const override;

public slots:
    void showMessage(const QString &message, int alignment = Qt::AlignLeft, const QColor &color = Qt::black);
    void clearMessage();

signals:
    /*!
        This signal is emitted when the message changes to \a message.
    */
    void messageChanged(const QString &message) { emitSignal(&QSplashScreen::messageChanged, message); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    virtual void drawContents(QPainter *painter);
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QPixmap m_pixmap;
    QString m_message;
    int m_alignment = Qt::AlignLeft;
    QColor m_color = Qt::black;
    QPointer<QWidget> m_waitFor;
};
