// MIT License - see LICENSE
#pragma once

#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>

namespace QtPrivate {
struct GuiAccess;
}

class QStyleHints : public QObject {
    Q_OBJECT
    Q_PROPERTY(Qt::ColorScheme colorScheme READ colorScheme WRITE setColorScheme NOTIFY colorSchemeChanged)
public:
    ~QStyleHints() override;

    Qt::ColorScheme colorScheme() const;
    void setColorScheme(Qt::ColorScheme scheme);
    void unsetColorScheme();
    int startDragDistance() const { return m_dragDistance; }
    void setStartDragDistance(int startDragDistance) { m_dragDistance = startDragDistance; }
    int startDragTime() const { return m_dragTime; }
    void setStartDragTime(int startDragTime) { m_dragTime = startDragTime; }
    int cursorFlashTime() const { return m_cursorFlashTime; }
    void setCursorFlashTime(int cursorFlashTime) { m_cursorFlashTime = cursorFlashTime; }
    int mouseDoubleClickInterval() const { return m_doubleClickInterval; }
    void setMouseDoubleClickInterval(int mouseDoubleClickInterval) { m_doubleClickInterval = mouseDoubleClickInterval; }
    int keyboardInputInterval() const { return m_keyboardInputInterval; }
    void setKeyboardInputInterval(int keyboardInputInterval) { m_keyboardInputInterval = keyboardInputInterval; }
    int wheelScrollLines() const { return m_wheelScrollLines; }
    void setWheelScrollLines(int scrollLines) { m_wheelScrollLines = scrollLines; }

signals:
    /*!
        This signal is emitted when colorScheme() changes to \a colorScheme: the desktop
        switches between its light and dark appearance, or setColorScheme() is called.
    */
    void colorSchemeChanged(Qt::ColorScheme colorScheme) { emitSignal(&QStyleHints::colorSchemeChanged, colorScheme); }

private:
    friend struct QtPrivate::GuiAccess;
    friend class QGuiApplication;
    QStyleHints();

    void setPlatformColorScheme(Qt::ColorScheme scheme);

    Qt::ColorScheme m_platform = Qt::ColorScheme::Unknown;
    Qt::ColorScheme m_override = Qt::ColorScheme::Unknown;
    int m_dragDistance = 10;
    int m_dragTime = 500;
    int m_cursorFlashTime = 1000;
    int m_doubleClickInterval = 400;
    int m_keyboardInputInterval = 400;
    int m_wheelScrollLines = 3;
};
