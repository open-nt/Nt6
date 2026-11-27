// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qwidget.h>

class QEventLoop;
class QPushButton;

class QDialog : public QWidget {
    Q_OBJECT
    Q_PROPERTY(bool modal READ isModal WRITE setModal)
    Q_PROPERTY(bool sizeGripEnabled READ isSizeGripEnabled WRITE setSizeGripEnabled)
public:
    enum DialogCode { Rejected, Accepted };
    Q_ENUM(DialogCode)

    explicit QDialog(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
    ~QDialog() override;

    int result() const { return m_result; }
    void setResult(int result) { m_result = result; }
    void setModal(bool modal);
    bool isSizeGripEnabled() const { return m_sizeGrip != nullptr; }
    void setSizeGripEnabled(bool enabled);
    void setVisible(bool visible) override;

public slots:
    virtual void open();
    virtual int exec();
    virtual void done(int r);
    virtual void accept();
    virtual void reject();

signals:
    /*!
        This signal is emitted when the dialog is closed by done(), accept() or reject(), or by the user, with the
        \a result it closed with.

        \sa accepted(), rejected()
    */
    void finished(int result) { emitSignal(&QDialog::finished, result); }
    /*!
        This signal is emitted when the dialog is accepted, before finished().
    */
    void accepted() { emitSignal(&QDialog::accepted); }
    /*!
        This signal is emitted when the dialog is rejected, before finished(). Escape and the close button of the
        window reject the dialog.
    */
    void rejected() { emitSignal(&QDialog::rejected); }

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void adjustPosition(QWidget *w);

private:
    void fitPlatformWidth();
    QPushButton *defaultButton() const;

    QEventLoop *m_loop = nullptr;
    int m_result = 0;
    bool m_positioned = false;
    bool m_resetModality = false;
    QWidget *m_sizeGrip = nullptr; // a QSizeGrip, while it is enabled
};
