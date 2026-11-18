// MIT License - see LICENSE
#include <QtGui/qstylehints.h>

/*!
    \class QStyleHints
    \brief The QStyleHints class contains platform specific hints and settings.

    The hints describe how the desktop wants applications to look. The application has one
    QStyleHints, which QGuiApplication::styleHints() returns. Its colorScheme() says whether the
    desktop uses a light or a dark appearance, and the default style follows it:

    \code
    QApplication app(argc, argv);
    if (app.styleHints()->colorScheme() == Qt::ColorScheme::Dark)
        qDebug() << "the desktop is dark";
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged, [](Qt::ColorScheme scheme) {
        qDebug() << "the desktop changed to" << (scheme == Qt::ColorScheme::Dark ? "dark" : "light");
    });
    \endcode

    On the \c xcb platform the scheme comes from the \c org.freedesktop.appearance \c color-scheme
    setting of the desktop portal (the "dark style" preference of GNOME, KDE and others), which is
    read over D-Bus when the application starts and followed as it changes.

    \differences
    The only hint is the color scheme. The other hints of Qt, such as the double click interval,
    the key repeat rate and the wheel scroll lines, are not available.
    \enddifferences

    \sa QGuiApplication::styleHints(), Qt::ColorScheme
*/

QStyleHints::QStyleHints() = default;

/*!
    Destroys the hints.
*/
QStyleHints::~QStyleHints() = default;

/*!
    Returns the color scheme of the application: the one that setColorScheme() set, or else the one that
    the desktop reports. It is Qt::ColorScheme::Unknown if the desktop does not say.

    \sa setColorScheme(), unsetColorScheme()
*/
Qt::ColorScheme QStyleHints::colorScheme() const {
    return m_override != Qt::ColorScheme::Unknown ? m_override : m_platform;
}

/*!
    Makes \a scheme the color scheme of the application, whatever the desktop says. Setting
    Qt::ColorScheme::Unknown is the same as calling unsetColorScheme().

    \sa unsetColorScheme(), colorScheme()
*/
void QStyleHints::setColorScheme(Qt::ColorScheme scheme) {
    const Qt::ColorScheme before = colorScheme();
    m_override = scheme;
    if (colorScheme() != before)
        colorSchemeChanged(colorScheme());
}

/*!
    Removes the color scheme that setColorScheme() set, so that the scheme of the desktop applies
    again.

    \sa setColorScheme()
*/
void QStyleHints::unsetColorScheme() { setColorScheme(Qt::ColorScheme::Unknown); }

void QStyleHints::setPlatformColorScheme(Qt::ColorScheme scheme) {
    const Qt::ColorScheme before = colorScheme();
    m_platform = scheme;
    if (colorScheme() != before)
        colorSchemeChanged(colorScheme());
}

/*!
    \fn int QStyleHints::startDragDistance() const
    \since 6.0

    Returns how many pixels the mouse must move with a button held before a drag starts: 10 by default.
*/

/*!
    \fn void QStyleHints::setStartDragDistance(int startDragDistance)
    \since 6.0

    Sets how many pixels the mouse must move before a drag starts to \a startDragDistance.
*/

/*!
    \fn int QStyleHints::startDragTime() const
    \since 6.0

    Returns how many milliseconds a button must be held before a drag starts even if the mouse does not move: 500
    by default.
*/

/*!
    \fn void QStyleHints::setStartDragTime(int startDragTime)
    \since 6.0

    Sets how long a button must be held before a drag starts to \a startDragTime milliseconds.
*/

/*!
    \fn int QStyleHints::cursorFlashTime() const
    \since 6.0

    Returns how many milliseconds a blink of the text cursor takes, 1000 by default.
*/

/*!
    \fn void QStyleHints::setCursorFlashTime(int cursorFlashTime)
    \since 6.0

    Sets how long a blink of the text cursor takes to \a cursorFlashTime milliseconds. The cursors of the Nt
    editors do not blink, so the value is only kept.
*/

/*!
    \fn int QStyleHints::mouseDoubleClickInterval() const
    \since 6.0

    Returns how many milliseconds may pass between the clicks of a double click, 400 by default.
*/

/*!
    \fn void QStyleHints::setMouseDoubleClickInterval(int mouseDoubleClickInterval)
    \since 6.0

    Sets how long may pass between the clicks of a double click to \a mouseDoubleClickInterval milliseconds.
*/

/*!
    \fn int QStyleHints::keyboardInputInterval() const
    \since 6.0

    Returns how many milliseconds may pass between key presses that a view takes as one search, 400 by default.
*/

/*!
    \fn void QStyleHints::setKeyboardInputInterval(int keyboardInputInterval)
    \since 6.0

    Sets how long may pass between the key presses of one search to \a keyboardInputInterval milliseconds.
*/

/*!
    \fn int QStyleHints::wheelScrollLines() const
    \since 6.0

    Returns how many lines a notch of the mouse wheel scrolls, 3 by default.
*/

/*!
    \fn void QStyleHints::setWheelScrollLines(int scrollLines)
    \since 6.0

    Sets how many lines a notch of the mouse wheel scrolls to \a scrollLines.
*/

#include "moc_qstylehints.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
