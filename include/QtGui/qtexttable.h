// MIT License - see LICENSE
#pragma once

#include <QtGui/qtextobject.h>

class QTextTable : public QTextFrame {
    Q_OBJECT
public:
    explicit QTextTable(QTextDocument *doc);
    ~QTextTable() override;

    int rows() const { return m_rows; }
    int columns() const { return m_columns; }
    void setFormat(const QTextTableFormat &format);
    QTextTableFormat format() const { return QTextObject::format().toTableFormat(); }

private:
    friend class QTextDocumentPrivate;
    int m_rows = 0;
    int m_columns = 0;
};
