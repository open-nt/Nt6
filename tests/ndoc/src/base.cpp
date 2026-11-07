#include "base.h"

/*!
    \module FixtureCore
    \title Fixture Core Classes
    \brief Classes used by the NDoc tests.
    \cmaketarget Fx::Core
*/

/*!
    \page guide.html
    \title Guide Page
    \brief A page written in a source file.

    \section1 First Section

    Text for Widget.

    \target custom-anchor
    \table
    \header
        \li Name
    \row
        \li Value
    \endtable
*/

/*!
    \class Base
    \brief The Base class is documented from a source file.
*/

/*!
    Constructs a base with the given \a parent.
*/
Base::Base(Base *parent) { (void)parent; }

/*!
    Destroys the base.
*/
Base::~Base() = default;

/*!
    Returns the value.
*/
int Base::value() const { return 1; }

/*!
    \fn void Base::handle(int code)

    Handles \a code.
*/
void Base::handle(int) {}
