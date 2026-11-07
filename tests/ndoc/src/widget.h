#pragma once

#include "base.h"

/*!
    \namespace Fx
    \brief The Fx namespace holds fixture enums.
*/
namespace Fx {

/*!
    \enum Fx::Mode
    \brief Unused on enums.

    Selects a mode.

    \value Fast Finishes quickly.
    \value Slow Finishes slowly.
*/
enum Mode { Fast, Slow = 4 };

} // namespace Fx

/*!
    \class Widget
    \brief The Widget class is documented entirely in its header.
    \since 1.1

    See \l{Guide Page}{the guide}, Base and show().

    \snippet demo.cpp use

    \list
    \li One item
    \li Item with \c code
    \endlist

    \differences
    This fixture differs from everything.
    \enddifferences

    \sa Base, show(), {Guide Page}
*/
template <typename T>
class Widget : public Base {
public:
    /*!
        Describes a colour.

        \value Red The colour red.
        \value Blue The colour blue.
    */
    enum Colour { Red, Blue };

    /*!
        Builds a widget owned by \a parent.
    */
    explicit Widget(Base *parent = nullptr) : Base(parent) { int x{1}; (void)x; }

    /*!
        Shows the widget, for \a msec milliseconds.
    */
    void show(int msec = 10) const;

    /*!
        \overload

        Shows the widget with a \a title.
    */
    void show(const char *title = "a, b") const;

    /*!
        Compares with \a other.
    */
    bool operator==(const Widget &other) const noexcept { return this == &other; }

    /*!
        Calls \a callback.
    */
    void each(void (*callback)(int));

    /*!
        Pads to \a width with \a fill, limited to \a limit.
    */
    int pad(int width, char32_t fill = U' ', int limit = 1'000) const;

    /*!
        Runs after the literals above, so it must still be found.
    */
    void afterLiterals();

    int notDocumented() const;

public slots:
    /*!
        Refreshes the widget.
    */
    void refresh();

signals:
    /*!
        This signal is emitted when \a value changes.
    */
    void changed(int value);

protected:
    /*!
        \reimp
    */
    void handle(int code) override;

private:
    int m_hidden = 0;
};
