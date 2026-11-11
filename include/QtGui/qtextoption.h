// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>
#include <QtCore/qnamespace.h>

class QTextOption {
public:
    /*!
        \enum QTextOption::WrapMode

        How the lines of a text are broken.

        \value NoWrap Not at all: a line ends only where the text has a line break.
        \value WordWrap Between words. A word that is longer than a line sticks out.
        \value ManualWrap The same as \c NoWrap.
        \value WrapAnywhere Wherever the line is full, also inside a word.
        \value WrapAtWordBoundaryOrAnywhere Between words, and inside a word only if the word is longer than a line. This is the default.
    */
    enum WrapMode { NoWrap, WordWrap, ManualWrap, WrapAnywhere, WrapAtWordBoundaryOrAnywhere };

    /*!
        Constructs a text option that aligns text to the left and wraps it
        with \c WrapAtWordBoundaryOrAnywhere.
    */
    QTextOption() = default;
    /*!
        Constructs a text option with the alignment \a alignment.
    */
    QTextOption(Qt::Alignment alignment) : m_alignment(alignment) {}

    /*!
        Sets the alignment of the lines to \a alignment.

        \sa alignment()
    */
    void setAlignment(Qt::Alignment alignment) { m_alignment = alignment; }
    /*!
        Returns the alignment of the lines.

        \sa setAlignment()
    */
    Qt::Alignment alignment() const { return m_alignment; }
    /*!
        Sets how lines are broken to \a wrap.

        \sa wrapMode()
    */
    void setWrapMode(WrapMode wrap) { m_wrap = wrap; }
    /*!
        Returns how lines are broken.

        \sa setWrapMode()
    */
    WrapMode wrapMode() const { return m_wrap; }
    /*!
        Sets the distance between tab stops to \a tabStopDistance pixels.
        A distance that is not positive stands for eight spaces of the
        font, which is the default.

        \sa tabStopDistance()
    */
    void setTabStopDistance(qreal tabStopDistance) { m_tab = tabStopDistance; }
    /*!
        Returns the distance between tab stops, or -1 for eight spaces of
        the font.

        \sa setTabStopDistance()
    */
    qreal tabStopDistance() const { return m_tab; }

private:
    Qt::Alignment m_alignment = Qt::AlignLeft;
    WrapMode m_wrap = WrapAtWordBoundaryOrAnywhere;
    qreal m_tab = -1;
};
