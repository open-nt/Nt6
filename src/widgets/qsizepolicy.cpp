// MIT License - see LICENSE
#include <QtWidgets/qsizepolicy.h>

/*!
    \class QSizePolicy
    \brief The QSizePolicy class is a layout attribute describing horizontal and vertical resizing policy.

    A widget's size policy tells layouts how it may be resized. Each direction has a policy: whether the widget
    can grow beyond its size hint, shrink below it, and whether it wants to take extra space.

    \code
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    \endcode

    \differences
    There is no height-for-width support, \c retainSizeWhenHidden(), \c ControlType or \c transposed().
    The policy is a plain value with the stretch factors, which the box layouts of Nt do not use: use
    QBoxLayout::addWidget()'s stretch argument.
    \enddifferences

    \sa QWidget::setSizePolicy(), QLayout
*/

/*!
    \enum QSizePolicy::PolicyFlag

    These flags make up the policies. A policy is a combination of them.

    \value GrowFlag The widget can grow beyond its size hint if necessary.
    \value ExpandFlag The widget wants to get as much space as possible.
    \value ShrinkFlag The widget can shrink below its size hint if necessary.
    \value IgnoreFlag The size hint is ignored; the widget gets as much space as it can.
*/

/*!
    \enum QSizePolicy::Policy

    This enum describes how a widget can be resized in one direction.

    \value Fixed The size hint is the only size that the widget can have.
    \value Minimum The size hint is the smallest size; the widget can grow.
    \value Maximum The size hint is the largest size; the widget can shrink.
    \value Preferred The size hint is best, but the widget can grow and shrink.
    \value MinimumExpanding The size hint is the smallest size; the widget can grow and wants to.
    \value Expanding The size hint is a sensible size; the widget can grow and shrink and wants to grow.
    \value Ignored The size hint is ignored; the widget gets as much space as it can.
*/

/*!
    \fn QSizePolicy::QSizePolicy()

    Constructs a policy that is Preferred in both directions.
*/

/*!
    \fn QSizePolicy::QSizePolicy(Policy horizontal, Policy vertical)
    \overload

    Constructs a policy with the given \a horizontal and \a vertical policies.
*/

/*!
    \fn QSizePolicy::Policy QSizePolicy::horizontalPolicy() const

    Returns the horizontal policy.

    \sa setHorizontalPolicy()
*/

/*!
    \fn QSizePolicy::Policy QSizePolicy::verticalPolicy() const

    Returns the vertical policy.

    \sa setVerticalPolicy()
*/

/*!
    \fn void QSizePolicy::setHorizontalPolicy(Policy policy)

    Sets the horizontal policy to \a policy.

    \sa horizontalPolicy()
*/

/*!
    \fn void QSizePolicy::setVerticalPolicy(Policy policy)

    Sets the vertical policy to \a policy.

    \sa verticalPolicy()
*/

/*!
    \fn int QSizePolicy::horizontalStretch() const

    Returns the horizontal stretch factor.

    \sa setHorizontalStretch()
*/

/*!
    \fn int QSizePolicy::verticalStretch() const

    Returns the vertical stretch factor.

    \sa setVerticalStretch()
*/

/*!
    \fn void QSizePolicy::setHorizontalStretch(int stretch)

    Sets the horizontal stretch factor to \a stretch.

    \sa horizontalStretch()
*/

/*!
    \fn void QSizePolicy::setVerticalStretch(int stretch)

    Sets the vertical stretch factor to \a stretch.

    \sa verticalStretch()
*/

/*!
    \fn Qt::Orientations QSizePolicy::expandingDirections() const

    Returns the directions in which the widget wants to expand: those whose policy has the ExpandFlag.
*/
