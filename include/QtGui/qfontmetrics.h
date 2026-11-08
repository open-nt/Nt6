// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qfont.h>

#include <memory>

class QFontEngine;

class QFontMetrics {
public:
    explicit QFontMetrics(const QFont &font);
    QFontMetrics(const QFontMetrics &other);
    QFontMetrics &operator=(const QFontMetrics &other);
    ~QFontMetrics();

    int ascent() const;
    int descent() const;
    int height() const;
    int leading() const;
    int lineSpacing() const;
    int xHeight() const;
    int averageCharWidth() const;
    bool inFont(QChar ch) const;

    int horizontalAdvance(const QString &text, int length = -1) const;
    int horizontalAdvance(QChar ch) const;
    QRect boundingRect(const QString &text) const;
    QRect boundingRect(const QRect &rect, int flags, const QString &text) const;
    QRect boundingRect(int x, int y, int w, int h, int flags, const QString &text) const {
        return boundingRect(QRect(x, y, w, h), flags, text);
    }
    QSize size(int flags, const QString &text) const;
    QString elidedText(const QString &text, Qt::TextElideMode mode, int width, int flags = 0) const;

private:
    std::shared_ptr<QFontEngine> m_engine;
};
