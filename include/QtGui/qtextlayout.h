// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qpoint.h>
#include <QtCore/qrect.h>
#include <QtCore/qstring.h>
#include <QtGui/qfont.h>
#include <QtGui/qtextformat.h>
#include <QtGui/qtextoption.h>

#include <memory>

class QPaintDevice;
class QPainter;
class QTextLayout;

class QTextLine {
public:
    /*!
        \enum QTextLine::Edge

        The edges of a character, for cursorToX().

        \value Leading The edge where the character starts.
        \value Trailing The edge where it ends.
    */
    enum Edge { Leading, Trailing };

    /*!
        Constructs an invalid line. Valid lines come from
        QTextLayout::createLine() and QTextLayout::lineAt().
    */
    QTextLine() = default;

    bool isValid() const { return m_layout != nullptr; }
    QRectF rect() const;
    qreal x() const;
    qreal y() const;
    qreal width() const;
    qreal ascent() const;
    qreal descent() const;
    qreal height() const;
    qreal leading() const;
    qreal naturalTextWidth() const;
    QRectF naturalTextRect() const;

    void setLineWidth(qreal width);
    void setPosition(const QPointF &pos);
    QPointF position() const;

    int textStart() const;
    int textLength() const;
    int lineNumber() const { return m_index; }

    qreal cursorToX(int cursorPos, Edge edge = Leading) const;
    int xToCursor(qreal x) const;

    void draw(QPainter *painter, const QPointF &position) const;

private:
    friend class QTextLayout;
    QTextLine(const QTextLayout *layout, int index) : m_layout(layout), m_index(index) {}
    const QTextLayout *m_layout = nullptr;
    int m_index = -1;
};

class QTextLayout {
public:
    /*!
        \class QTextLayout::FormatRange
        \brief The QTextLayout::FormatRange structure is used to apply extra formatting information for a specified area in the text layout's content.

        \differences
        None.
        \enddifferences
    */
    struct FormatRange {
        /*! The position in the text where the format starts to apply. */
        int start = 0;
        /*! The number of characters that the format applies to. */
        int length = 0;
        /*! The format. */
        QTextCharFormat format;
    };

    /*!
        \enum QTextLayout::CursorMode

        How far nextCursorPosition() and previousCursorPosition() go.

        \value SkipCharacters To the next character.
        \value SkipWords To the start of the next word.
    */
    enum CursorMode { SkipCharacters, SkipWords };

    QTextLayout();
    explicit QTextLayout(const QString &text);
    QTextLayout(const QString &text, const QFont &font, const QPaintDevice *paintdevice = nullptr);
    ~QTextLayout();
    QTextLayout(const QTextLayout &) = delete;
    QTextLayout &operator=(const QTextLayout &) = delete;

    void setFont(const QFont &font);
    QFont font() const;
    void setText(const QString &string);
    QString text() const;
    void setTextOption(const QTextOption &option);
    const QTextOption &textOption() const;
    void setFormats(const QList<FormatRange> &formats);
    QList<FormatRange> formats() const;
    void clearFormats();

    void beginLayout();
    void endLayout();
    void clearLayout();
    QTextLine createLine();
    int lineCount() const;
    QTextLine lineAt(int i) const;
    QTextLine lineForTextPosition(int pos) const;

    bool isValidCursorPosition(int pos) const;
    int nextCursorPosition(int oldPos, CursorMode mode = SkipCharacters) const;
    int previousCursorPosition(int oldPos, CursorMode mode = SkipCharacters) const;

    QPointF position() const;
    void setPosition(const QPointF &p);
    QRectF boundingRect() const;
    qreal minimumWidth() const;
    qreal maximumWidth() const;

    void draw(QPainter *p, const QPointF &pos, const QList<FormatRange> &selections = QList<FormatRange>(),
              const QRectF &clip = QRectF()) const;

private:
    friend class QTextLine;
    struct Data;
    std::unique_ptr<Data> d;
};
