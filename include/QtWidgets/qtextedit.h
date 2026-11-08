// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qplaintextedit.h>

class QTextEdit : public QPlainTextEdit {
    Q_OBJECT
    Q_PROPERTY(bool acceptRichText READ acceptRichText WRITE setAcceptRichText)
public:
    explicit QTextEdit(QWidget *parent = nullptr);
    explicit QTextEdit(const QString &text, QWidget *parent = nullptr);
    ~QTextEdit() override;

    bool acceptRichText() const { return m_acceptRichText; }
    void setAcceptRichText(bool accept) { m_acceptRichText = accept; }

public slots:
    void setText(const QString &text);
    void append(const QString &text);

private:
    bool m_acceptRichText = true;
};
