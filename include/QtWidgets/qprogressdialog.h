// MIT License - see LICENSE
#pragma once

#include <QtCore/qelapsedtimer.h>
#include <QtCore/qtimer.h>
#include <QtWidgets/qdialog.h>

class QLabel;
class QProgressBar;
class QPushButton;

class QProgressDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(bool wasCanceled READ wasCanceled)
    Q_PROPERTY(int minimum READ minimum WRITE setMinimum)
    Q_PROPERTY(int maximum READ maximum WRITE setMaximum)
    Q_PROPERTY(int value READ value WRITE setValue)
    Q_PROPERTY(bool autoReset READ autoReset WRITE setAutoReset)
    Q_PROPERTY(bool autoClose READ autoClose WRITE setAutoClose)
    Q_PROPERTY(int minimumDuration READ minimumDuration WRITE setMinimumDuration)
    Q_PROPERTY(QString labelText READ labelText WRITE setLabelText)
public:
    explicit QProgressDialog(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    QProgressDialog(const QString &labelText, const QString &cancelButtonText, int minimum, int maximum,
                    QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QProgressDialog() override;

    bool wasCanceled() const { return m_canceled; }
    int minimum() const;
    int maximum() const;
    int value() const;
    QString labelText() const;
    int minimumDuration() const { return m_minimumDuration; }
    void setAutoReset(bool reset) { m_autoReset = reset; }
    bool autoReset() const { return m_autoReset; }
    void setAutoClose(bool close) { m_autoClose = close; }
    bool autoClose() const { return m_autoClose; }

    QSize sizeHint() const override;

public slots:
    void cancel();
    void reset();
    void setMaximum(int maximum);
    void setMinimum(int minimum);
    void setRange(int minimum, int maximum);
    void setValue(int progress);
    void setLabelText(const QString &text);
    void setCancelButtonText(const QString &text);
    void setMinimumDuration(int ms);

signals:
    /*!
        This signal is emitted when the user clicks Cancel, presses Escape or closes the window. It is connected to
        cancel().
    */
    void canceled() { emitSignal(&QProgressDialog::canceled); }

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void init(const QString &labelText, const QString &cancelText, int minimum, int maximum);
    void forceShow();

    QLabel *m_label = nullptr;
    QProgressBar *m_bar = nullptr;
    QPushButton *m_cancel = nullptr;
    QTimer m_forceTimer;
    QElapsedTimer m_started;
    int m_minimumDuration = 4000;
    bool m_canceled = false;
    bool m_autoReset = true;
    bool m_autoClose = true;
    bool m_shownOnce = false;
    bool m_setValueCalled = false;
};
