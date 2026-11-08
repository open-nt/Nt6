// MIT License - see LICENSE
#include <QtGui/qpalette.h>

namespace {

struct Defaults {
    QRgb role[QPalette::NColorRoles];
};

// The colors of the light theme: Fusion's, for the active group.
Defaults activeDefaults() {
    Defaults d{};
    for (QRgb &c : d.role)
        c = 0xff000000u;
    d.role[QPalette::WindowText] = 0xff000000u;
    d.role[QPalette::Button] = 0xffefefefu;
    d.role[QPalette::Light] = 0xffffffffu;
    d.role[QPalette::Midlight] = 0xffcacacau;
    d.role[QPalette::Dark] = 0xff9f9f9fu;
    d.role[QPalette::Mid] = 0xffb8b8b8u;
    d.role[QPalette::Text] = 0xff000000u;
    d.role[QPalette::BrightText] = 0xffffffffu;
    d.role[QPalette::ButtonText] = 0xff000000u;
    d.role[QPalette::Base] = 0xffffffffu;
    d.role[QPalette::Window] = 0xffefefefu;
    d.role[QPalette::Shadow] = 0xff767676u;
    d.role[QPalette::Highlight] = 0xff308cc6u;
    d.role[QPalette::HighlightedText] = 0xffffffffu;
    d.role[QPalette::Link] = 0xff0000ffu;
    d.role[QPalette::LinkVisited] = 0xffff00ffu;
    d.role[QPalette::AlternateBase] = 0xfff7f7f7u;
    d.role[QPalette::NoRole] = 0xff000000u;
    d.role[QPalette::ToolTipBase] = 0xffffffdcu;
    d.role[QPalette::ToolTipText] = 0xff000000u;
    d.role[QPalette::PlaceholderText] = 0x80000000u;
    return d;
}

} // namespace

/*!
    \class QPalette
    \brief The QPalette class contains color groups for each widget state.

    A palette maps color roles (the window background, text, buttons, selection) to brushes,
    separately for the active, disabled and inactive states of a widget. Widgets draw with the
    roles instead of fixed colors, so one palette changes the look of a whole application.

    \code
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#202020"));
    palette.setColor(QPalette::WindowText, Qt::white);
    widget->setPalette(palette);
    \endcode

    The default palette is a light one. Text in the disabled group is gray.

    \differences
    The palette is a plain value: there is no resolve mask, \c cacheKey(), \c isBrushSet() or
    \c setColorGroup(), no QVariant or QDataStream conversion, and no constructors that take
    nine colors. The inactive group equals the active one until you change it.
    \enddifferences

    \sa QBrush, QColor
*/

/*!
    \enum QPalette::ColorGroup

    This enum names the states that a palette has colors for.

    \value Active The window has the keyboard focus.
    \value Disabled The widget is disabled.
    \value Inactive The window does not have the keyboard focus.
    \value NColorGroups The number of groups.
    \value Current The group that currentColorGroup() names.
    \value All Every group; setBrush() and setColor() accept it.
    \value Normal The same as Active.
*/

/*!
    \enum QPalette::ColorRole

    This enum names the parts of the user interface that have a color.

    \value WindowText The general foreground color.
    \value Button The background of buttons.
    \value Light Lighter than Button; used for bevels.
    \value Midlight Between Button and Light.
    \value Dark Darker than Button; used for bevels.
    \value Mid Between Button and Dark.
    \value Text The foreground on Base, such as the text of a line edit.
    \value BrightText A text color that contrasts with the dark colors.
    \value ButtonText A foreground color for buttons.
    \value Base The background of text entry widgets.
    \value Window The general background color.
    \value Shadow A very dark color for shadows.
    \value Highlight The background of selected items.
    \value HighlightedText The text on a Highlight.
    \value Link The color of hyperlinks.
    \value LinkVisited The color of visited hyperlinks.
    \value AlternateBase The alternate background in lists with alternating rows.
    \value NoRole A role that is not a role.
    \value ToolTipBase The background of tool tips.
    \value ToolTipText The text color of tool tips.
    \value PlaceholderText The color of placeholder text.
    \value NColorRoles The number of roles.
    \value Foreground The same as WindowText.
    \value Background The same as Window.
*/

/*!
    Constructs the default light palette.
*/
QPalette::QPalette() {
    const Defaults d = activeDefaults();
    for (int g = 0; g < NColorGroups; ++g)
        for (int r = 0; r < NColorRoles; ++r)
            m_brushes[g][r] = QBrush(QColor::fromRgba(d.role[r]));
    for (int r : {WindowText, Text, ButtonText})
        m_brushes[Disabled][r] = QBrush(QColor(0xbe, 0xbe, 0xbe));
    m_brushes[Disabled][Highlight] = QBrush(QColor(0x91, 0x91, 0x91));
    m_brushes[Disabled][Base] = QBrush(QColor(0xef, 0xef, 0xef));
    m_brushes[Disabled][PlaceholderText] = QBrush(QColor(0xbe, 0xbe, 0xbe, 0x80));
    for (int g = 0; g < NColorGroups; ++g)
        for (int r = 0; r < NColorRoles; ++r)
            m_brushes[g][r].setStyle(Qt::SolidPattern);
}

/*!
    Constructs a light palette whose Button color is \a button; the other colors of the roles
    that depend on it are made from it.
*/
QPalette::QPalette(const QColor &button) : QPalette(button, button) {}

/*!
    \overload

    Constructs a light palette whose Button color is \a button and whose Window color is \a window.
*/
QPalette::QPalette(const QColor &button, const QColor &window) : QPalette() {
    setColor(All, Button, button);
    setColor(All, Window, window);
    setColor(All, Light, button.lighter(150));
    setColor(All, Midlight, button.lighter(115));
    setColor(All, Dark, button.darker(150));
    setColor(All, Mid, button.darker(125));
}

/*!
    \fn QPalette::ColorGroup QPalette::currentColorGroup() const

    Returns the group that the brush() and color() functions without a group use.

    \sa setCurrentColorGroup()
*/

/*!
    \fn void QPalette::setCurrentColorGroup(ColorGroup group)

    Sets the current group to \a group.

    \sa currentColorGroup()
*/

/*!
    Returns the brush of \a role in \a group. \a group may be Current, which is the
    currentColorGroup().

    \sa setBrush()
*/
const QBrush &QPalette::brush(ColorGroup group, ColorRole role) const {
    const int g = (group == Current || group == All || group >= NColorGroups) ? static_cast<int>(m_group)
                                                                              : static_cast<int>(group);
    const int r = (role < 0 || role >= NColorRoles) ? static_cast<int>(NoRole) : static_cast<int>(role);
    return m_brushes[g][r];
}

/*!
    \fn const QBrush &QPalette::brush(ColorRole role) const
    \overload

    Returns the brush of \a role in the current group.
*/

/*!
    Sets the brush of \a role in \a group to \a brush. \a group may be All, which sets
    the role in every group, or Current.

    \sa brush()
*/
void QPalette::setBrush(ColorGroup group, ColorRole role, const QBrush &brush) {
    if (role < 0 || role >= NColorRoles)
        return;
    QBrush b = brush;
    if (b.style() == Qt::NoBrush)
        b.setStyle(Qt::SolidPattern);
    if (group == All) {
        for (int g = 0; g < NColorGroups; ++g)
            m_brushes[g][role] = b;
    } else if (group == Current) {
        m_brushes[m_group][role] = b;
    } else if (group < NColorGroups) {
        m_brushes[group][role] = b;
    }
}

/*!
    \fn void QPalette::setBrush(ColorRole role, const QBrush &brush)
    \overload

    Sets the brush of \a role in every group to \a brush.
*/

/*!
    \fn const QColor &QPalette::color(ColorGroup group, ColorRole role) const

    Returns the color of \a role in \a group.

    \sa brush()
*/

/*!
    \fn const QColor &QPalette::color(ColorRole role) const
    \overload

    Returns the color of \a role in the current group.
*/

/*!
    \fn void QPalette::setColor(ColorGroup group, ColorRole role, const QColor &color)

    Sets the color of \a role in \a group to \a color.

    \sa setBrush()
*/

/*!
    \fn void QPalette::setColor(ColorRole role, const QColor &color)
    \overload

    Sets the color of \a role in every group to \a color.
*/

/*!
    \fn const QBrush &QPalette::windowText() const

    Returns the WindowText brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::button() const

    Returns the Button brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::light() const

    Returns the Light brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::midlight() const

    Returns the Midlight brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::dark() const

    Returns the Dark brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::mid() const

    Returns the Mid brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::text() const

    Returns the Text brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::brightText() const

    Returns the BrightText brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::buttonText() const

    Returns the ButtonText brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::base() const

    Returns the Base brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::window() const

    Returns the Window brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::shadow() const

    Returns the Shadow brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::highlight() const

    Returns the Highlight brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::highlightedText() const

    Returns the HighlightedText brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::link() const

    Returns the Link brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::linkVisited() const

    Returns the LinkVisited brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::alternateBase() const

    Returns the AlternateBase brush of the current group.

    \sa brush()
*/

/*!
    \fn const QBrush &QPalette::placeholderText() const

    Returns the PlaceholderText brush of the current group.

    \sa brush()
*/
