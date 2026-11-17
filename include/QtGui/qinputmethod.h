// MIT License - see LICENSE
#pragma once

#include <QtCore/qlocale.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qobject.h>
#include <QtCore/qrect.h>

class QInputMethod : public QObject {
    Q_OBJECT
    Q_PROPERTY(QRectF keyboardRectangle READ keyboardRectangle NOTIFY keyboardRectangleChanged)
    Q_PROPERTY(bool visible READ isVisible NOTIFY visibleChanged)
    Q_PROPERTY(bool animating READ isAnimating NOTIFY animatingChanged)
public:
    enum Action { Click, ContextMenu };
    Q_ENUM(Action)

    QRectF keyboardRectangle() const { return QRectF(); }
    QRectF inputItemRectangle() const { return m_itemRect; }
    void setInputItemRectangle(const QRectF &rect) { m_itemRect = rect; }
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible);
    bool isAnimating() const { return false; }
    QLocale locale() const { return QLocale(); }
    Qt::LayoutDirection inputDirection() const { return QLocale().textDirection(); }

public slots:
    void show() { setVisible(true); }
    void hide() { setVisible(false); }
    void update(Qt::InputMethodQueries queries);
    void reset() {}
    void commit() {}
    void invokeAction(QInputMethod::Action a, int cursorPosition);

signals:
    /*!
        This signal is emitted when the input panel is shown or hidden.
    */
    void visibleChanged() { emitSignal(&QInputMethod::visibleChanged); }
    /*!
        This signal is emitted when the rectangle of the input panel changes; Nt never emits it.
    */
    void keyboardRectangleChanged() { emitSignal(&QInputMethod::keyboardRectangleChanged); }
    /*!
        This signal is emitted when the input panel starts or stops moving; Nt never emits it.
    */
    void animatingChanged() { emitSignal(&QInputMethod::animatingChanged); }

private:
    friend class QGuiApplication;
    explicit QInputMethod(QObject *parent);
    QRectF m_itemRect;
    bool m_visible = false;
};
