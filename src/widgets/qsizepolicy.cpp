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
    \fn bool QSizePolicy::hasHeightForWidth() const
    \since 6.0

    Returns \c true if the height of the widget depends on its width. Nt layouts do not use it.
*/

/*!
    \fn void QSizePolicy::setHeightForWidth(bool b)
    \since 6.0

    Sets whether the height of the widget depends on its width to \a b.
*/

/*!
    \fn bool QSizePolicy::hasWidthForHeight() const
    \since 6.0

    Returns \c true if the width of the widget depends on its height. Nt layouts do not use it.
*/

/*!
    \fn void QSizePolicy::setWidthForHeight(bool b)
    \since 6.0

    Sets whether the width of the widget depends on its height to \a b.
*/

/*!
    \fn bool QSizePolicy::retainSizeWhenHidden() const
    \since 6.0

    Returns \c true if a layout keeps the room of the widget while it is hidden. Nt layouts do not use it.
*/

/*!
    \fn void QSizePolicy::setRetainSizeWhenHidden(bool retainSize)
    \since 6.0

    Sets whether a layout keeps the room of the widget while it is hidden to \a retainSize.
*/

/*!
    \enum QSizePolicy::ControlType
    \since 6.0

    The kind of control a widget is, which a style may use to space widgets in a layout.

    \value DefaultType No particular kind.
    \value ButtonBox A QDialogButtonBox.
    \value CheckBox A QCheckBox.
    \value ComboBox A QComboBox.
    \value Frame A QFrame.
    \value GroupBox A QGroupBox.
    \value Label A QLabel.
    \value Line A QFrame drawn as a line.
    \value LineEdit A QLineEdit.
    \value PushButton A QPushButton.
    \value RadioButton A QRadioButton.
    \value Slider A QAbstractSlider.
    \value SpinBox A QAbstractSpinBox.
    \value TabWidget A QTabWidget.
    \value ToolButton A QToolButton.
*/

/*!
    \fn QSizePolicy::ControlType QSizePolicy::controlType() const
    \since 6.0

    Returns the kind of control of the policy, DefaultType by default.
*/

/*!
    \fn void QSizePolicy::setControlType(ControlType type)
    \since 6.0

    Sets the kind of control to \a type. Nt layouts space all widgets the same, so this is only kept.
*/

/*!
    \fn QSizePolicy::QSizePolicy(Policy horizontal, Policy vertical, ControlType type)
    \since 6.0

    Constructs a size policy with the \a horizontal and \a vertical policies for a control of \a type.
*/

/*!
    \fn void QSizePolicy::transpose()
    \since 6.0

    Swaps the horizontal and vertical parts of the policy.
*/

/*!
    \fn QSizePolicy QSizePolicy::transposed() const
    \since 6.0

    Returns the policy with its horizontal and vertical parts swapped.
*/

/*!
    \fn Qt::Orientations QSizePolicy::expandingDirections() const

    Returns the directions in which the widget wants to expand: those whose policy has the ExpandFlag.
*/
