// MIT License - see LICENSE
#pragma once

#include <QtCore/qdebug.h>
#include <QtCore/qstring.h>
#include <QtCore/qurl.h>

class QQmlError {
public:
    QQmlError() = default;

    bool isValid() const;
    QUrl url() const;
    void setUrl(const QUrl &url);
    QString description() const;
    void setDescription(const QString &description);
    int line() const;
    void setLine(int line);
    int column() const;
    void setColumn(int column);
    QtMsgType messageType() const;
    void setMessageType(QtMsgType messageType);
    QString toString() const;

    friend bool operator==(const QQmlError &a, const QQmlError &b) {
        return a.m_url == b.m_url && a.m_description == b.m_description && a.m_line == b.m_line &&
               a.m_column == b.m_column;
    }
    friend bool operator!=(const QQmlError &a, const QQmlError &b) { return !(a == b); }

private:
    QUrl m_url;
    QString m_description;
    int m_line = -1;
    int m_column = -1;
    QtMsgType m_type = QtWarningMsg;
};

QDebug operator<<(QDebug debug, const QQmlError &error);
