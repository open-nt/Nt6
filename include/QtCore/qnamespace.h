// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

/*!
    \namespace Qt
    \inmodule NtCore
    \brief The Qt namespace contains miscellaneous identifiers used throughout Nt.

    It holds the enums shared by several classes, such as Qt::CaseSensitivity
    and Qt::TimeSpec.

    \differences
    Only the enums listed here exist. Qt's other \c Qt:: enums, such as the
    window flags, cursor shapes and item-view roles, are not provided. The
    enums that Nt GUI uses (keys, mouse buttons, modifiers, colors, pen and
    brush styles and alignment) hold a subset of Qt's values, with the same numbers.
    \enddifferences
*/
namespace Qt {

/*!
    \enum Qt::AspectRatioMode

    This enum describes how to scale a rectangle or size to fit another.

    \value IgnoreAspectRatio The size is scaled freely; the aspect ratio is not preserved.
    \value KeepAspectRatio The size is scaled to the largest rectangle that fits inside the given size, preserving the aspect ratio.
    \value KeepAspectRatioByExpanding The size is scaled to the smallest rectangle that contains the given size, preserving the aspect ratio.

    \sa QSize::scaled()
*/
enum AspectRatioMode {
    IgnoreAspectRatio,
    KeepAspectRatio,
    KeepAspectRatioByExpanding
};

/*!
    \enum Qt::DateFormat

    This enum selects the textual format of dates and times.

    \value TextDate A locale-independent text form such as \c{Sat Nov 7 2026}; the default.
    \value ISODate ISO 8601 extended format, such as \c{2026-11-07} and \c{20:10:14}.
    \value ISODateWithMs ISO 8601 extended format with milliseconds, such as \c{20:10:14.844}.
    \value RFC2822Date The RFC 2822 format, such as \c{07 Nov 2026}.

    The \c toString() and \c fromString() functions of \c QDate, \c QTime
    and \c QDateTime accept this enum.
*/
enum DateFormat {
    TextDate,
    ISODate = 1,
    ISODateWithMs = 9,
    RFC2822Date = 8
};

/*!
    \enum Qt::TimeSpec

    This enum describes how a date and time is to be interpreted.

    \value LocalTime The local time of the system.
    \value UTC Coordinated Universal Time.
    \value OffsetFromUTC A fixed offset in seconds from UTC.
    \value TimeZone Declared for compatibility; Nt has no time zone database and QDateTime does not support it.

    The \c QDateTime class uses this enum.
*/
enum TimeSpec {
    LocalTime,
    UTC,
    OffsetFromUTC,
    TimeZone
};

/*!
    \enum Qt::ConnectionType

    This enum describes how a signal is delivered to a slot.

    \value AutoConnection The default. The slot is called directly if the receiver lives in the thread that emits the signal, and queued otherwise. The decision is made at emission time.
    \value DirectConnection The slot is called immediately when the signal is emitted, in the emitting thread.
    \value QueuedConnection The arguments are copied and the slot is called later from the event loop of the receiver's thread.
    \value BlockingQueuedConnection Like \c QueuedConnection, but the emitting thread blocks until the slot has returned. If the receiver lives in the emitting thread, the slot is called directly instead of deadlocking.

    \sa QObject::connect(), {Signals and Slots}
*/
enum ConnectionType { AutoConnection = 0, DirectConnection = 1, QueuedConnection = 2, BlockingQueuedConnection = 3 };

/*!
    \enum Qt::CaseSensitivity

    This enum selects whether a string comparison or search distinguishes
    upper and lower case.

    \value CaseInsensitive Case is ignored.
    \value CaseSensitive Case is significant.
*/
enum CaseSensitivity {
    CaseInsensitive = 0,
    CaseSensitive = 1
};

/*!
    \enum Qt::SplitBehaviorFlags

    This enum controls whether empty parts are kept when splitting a string.
    The SplitBehavior type is a typedef for \c{QFlags<SplitBehaviorFlags>}.

    \value KeepEmptyParts Empty parts are kept in the result.
    \value SkipEmptyParts Empty parts are left out of the result.
*/
enum SplitBehaviorFlags {
    KeepEmptyParts = 0,
    SkipEmptyParts = 1
};
Q_DECLARE_FLAGS(SplitBehavior, SplitBehaviorFlags)

/*!
    \enum Qt::TimerType

    This enum describes the accuracy a timer should have.

    \value PreciseTimer Accepted for compatibility; it has no effect.
    \value CoarseTimer Accepted for compatibility; it has no effect.
    \value VeryCoarseTimer Accepted for compatibility; it has no effect.

    \sa QBasicTimer, QTimer
*/
enum TimerType {
    PreciseTimer,
    CoarseTimer,
    VeryCoarseTimer
};

/*!
    \enum Qt::FindChildOption

    This enum controls how QObject::findChild() and QObject::findChildren()
    search. The FindChildOptions type is a typedef for
    \c{QFlags<FindChildOption>}.

    \value FindDirectChildrenOnly Only direct children are searched.
    \value FindChildrenRecursively All descendants are searched.
*/
enum FindChildOption {
    FindDirectChildrenOnly = 0,
    FindChildrenRecursively = 1
};
Q_DECLARE_FLAGS(FindChildOptions, FindChildOption)

/*!
    \enum Qt::GlobalColor

    This enum names the predefined colors that QColor and QBrush accept.

    \value color0 Zero pixel value, transparent in a bitmap.
    \value color1 Pixel value one, opaque in a bitmap.
    \value black Black, 0x000000.
    \value white White, 0xffffff.
    \value darkGray Dark gray, 0x808080.
    \value gray Gray, 0xa0a0a4.
    \value lightGray Light gray, 0xc0c0c0.
    \value red Red, 0xff0000.
    \value green Green, 0x00ff00.
    \value blue Blue, 0x0000ff.
    \value cyan Cyan, 0x00ffff.
    \value magenta Magenta, 0xff00ff.
    \value yellow Yellow, 0xffff00.
    \value darkRed Dark red, 0x800000.
    \value darkGreen Dark green, 0x008000.
    \value darkBlue Dark blue, 0x000080.
    \value darkCyan Dark cyan, 0x008080.
    \value darkMagenta Dark magenta, 0x800080.
    \value darkYellow Dark yellow, 0x808000.
    \value transparent Fully transparent.
*/
enum GlobalColor {
    color0,
    color1,
    black,
    white,
    darkGray,
    gray,
    lightGray,
    red,
    green,
    blue,
    cyan,
    magenta,
    yellow,
    darkRed,
    darkGreen,
    darkBlue,
    darkCyan,
    darkMagenta,
    darkYellow,
    transparent
};

/*!
    \enum Qt::MouseButton

    This enum describes the mouse buttons. The MouseButtons type is a typedef
    for \c{QFlags<MouseButton>}.

    \value NoButton No button, as in a mouse move event.
    \value LeftButton The left button.
    \value RightButton The right button.
    \value MiddleButton The middle button, often the wheel.
    \value BackButton The back navigation button.
    \value ForwardButton The forward navigation button.
    \value AllButtons Every button.
*/
enum MouseButton {
    NoButton = 0x00000000,
    LeftButton = 0x00000001,
    RightButton = 0x00000002,
    MiddleButton = 0x00000004,
    BackButton = 0x00000008,
    ForwardButton = 0x00000010,
    AllButtons = 0x07ffffff
};
Q_DECLARE_FLAGS(MouseButtons, MouseButton)
Q_DECLARE_OPERATORS_FOR_FLAGS(MouseButtons)

/*!
    \enum Qt::KeyboardModifier

    This enum describes the modifier keys. The KeyboardModifiers type is a
    typedef for \c{QFlags<KeyboardModifier>}.

    \value NoModifier No modifier key is pressed.
    \value ShiftModifier A Shift key is pressed.
    \value ControlModifier A Ctrl key is pressed.
    \value AltModifier An Alt key is pressed.
    \value MetaModifier A Meta (Super, Windows) key is pressed.
    \value KeypadModifier The key is on the keypad.
*/
enum KeyboardModifier {
    NoModifier = 0x00000000,
    ShiftModifier = 0x02000000,
    ControlModifier = 0x04000000,
    AltModifier = 0x08000000,
    MetaModifier = 0x10000000,
    KeypadModifier = 0x20000000
};
Q_DECLARE_FLAGS(KeyboardModifiers, KeyboardModifier)
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardModifiers)

/*!
    \enum Qt::Modifier

    This enum holds the modifiers as they are added to a key code in a QKeySequence, as in \c{Qt::CTRL | Qt::Key_S}.

    \value SHIFT The Shift key.
    \value CTRL The Ctrl key.
    \value ALT The Alt key.
    \value META The Meta (Super, Windows) key.
    \value MODIFIER_MASK The bits that a key code does not use, where the modifiers are.
*/
enum Modifier {
    META = MetaModifier,
    SHIFT = ShiftModifier,
    CTRL = ControlModifier,
    ALT = AltModifier,
    MODIFIER_MASK = -0x2000000
};

/*!
    \enum Qt::Key

    This enum holds the keys that QKeyEvent reports. Printable keys use their
    Latin-1 upper case code, so \c Key_A is 0x41.

    \value Key_Escape The Escape key.
    \value Key_Tab The Tab key.
    \value Key_Backtab Shift+Tab.
    \value Key_Backspace The Backspace key.
    \value Key_Return The main Return key.
    \value Key_Enter The Enter key on the keypad.
    \value Key_Insert The Insert key.
    \value Key_Delete The Delete key.
    \value Key_Pause The Pause key.
    \value Key_Print The Print Screen key.
    \value Key_Home The Home key.
    \value Key_End The End key.
    \value Key_Left The left arrow.
    \value Key_Up The up arrow.
    \value Key_Right The right arrow.
    \value Key_Down The down arrow.
    \value Key_PageUp The Page Up key.
    \value Key_PageDown The Page Down key.
    \value Key_Shift A Shift key.
    \value Key_Control A Ctrl key.
    \value Key_Meta A Meta key.
    \value Key_Alt An Alt key.
    \value Key_CapsLock The Caps Lock key.
    \value Key_NumLock The Num Lock key.
    \value Key_ScrollLock The Scroll Lock key.
    \value Key_F1 The F1 key.
    \value Key_F2 The F2 key.
    \value Key_F3 The F3 key.
    \value Key_F4 The F4 key.
    \value Key_F5 The F5 key.
    \value Key_F6 The F6 key.
    \value Key_F7 The F7 key.
    \value Key_F8 The F8 key.
    \value Key_F9 The F9 key.
    \value Key_F10 The F10 key.
    \value Key_F11 The F11 key.
    \value Key_F12 The F12 key.
    \value Key_Space The space bar.
    \value Key_0 The 0 key.
    \value Key_1 The 1 key.
    \value Key_2 The 2 key.
    \value Key_3 The 3 key.
    \value Key_4 The 4 key.
    \value Key_5 The 5 key.
    \value Key_6 The 6 key.
    \value Key_7 The 7 key.
    \value Key_8 The 8 key.
    \value Key_9 The 9 key.
    \value Key_A The A key.
    \value Key_B The B key.
    \value Key_C The C key.
    \value Key_D The D key.
    \value Key_E The E key.
    \value Key_F The F key.
    \value Key_G The G key.
    \value Key_H The H key.
    \value Key_I The I key.
    \value Key_J The J key.
    \value Key_K The K key.
    \value Key_L The L key.
    \value Key_M The M key.
    \value Key_N The N key.
    \value Key_O The O key.
    \value Key_P The P key.
    \value Key_Q The Q key.
    \value Key_R The R key.
    \value Key_S The S key.
    \value Key_T The T key.
    \value Key_U The U key.
    \value Key_V The V key.
    \value Key_W The W key.
    \value Key_X The X key.
    \value Key_Y The Y key.
    \value Key_Z The Z key.
    \value Key_Menu The Menu key, which opens a context menu.
    \value Key_Plus The + key.
    \value Key_Comma The , key.
    \value Key_Minus The - key.
    \value Key_Period The . key.
    \value Key_Slash The / key.
    \value Key_Equal The = key.
    \value Key_unknown A key without a Qt::Key value.
*/
enum Key {
    Key_Escape = 0x01000000,
    Key_Tab = 0x01000001,
    Key_Backtab = 0x01000002,
    Key_Backspace = 0x01000003,
    Key_Return = 0x01000004,
    Key_Enter = 0x01000005,
    Key_Insert = 0x01000006,
    Key_Delete = 0x01000007,
    Key_Pause = 0x01000008,
    Key_Print = 0x01000009,
    Key_Home = 0x01000010,
    Key_End = 0x01000011,
    Key_Left = 0x01000012,
    Key_Up = 0x01000013,
    Key_Right = 0x01000014,
    Key_Down = 0x01000015,
    Key_PageUp = 0x01000016,
    Key_PageDown = 0x01000017,
    Key_Shift = 0x01000020,
    Key_Control = 0x01000021,
    Key_Meta = 0x01000022,
    Key_Alt = 0x01000023,
    Key_CapsLock = 0x01000024,
    Key_NumLock = 0x01000025,
    Key_ScrollLock = 0x01000026,
    Key_F1 = 0x01000030,
    Key_F2 = 0x01000031,
    Key_F3 = 0x01000032,
    Key_F4 = 0x01000033,
    Key_F5 = 0x01000034,
    Key_F6 = 0x01000035,
    Key_F7 = 0x01000036,
    Key_F8 = 0x01000037,
    Key_F9 = 0x01000038,
    Key_F10 = 0x01000039,
    Key_F11 = 0x0100003a,
    Key_F12 = 0x0100003b,
    Key_Menu = 0x01000055,
    Key_Space = 0x20,
    Key_Plus = 0x2b,
    Key_Comma = 0x2c,
    Key_Minus = 0x2d,
    Key_Period = 0x2e,
    Key_Slash = 0x2f,
    Key_Equal = 0x3d,
    Key_0 = 0x30,
    Key_1 = 0x31,
    Key_2 = 0x32,
    Key_3 = 0x33,
    Key_4 = 0x34,
    Key_5 = 0x35,
    Key_6 = 0x36,
    Key_7 = 0x37,
    Key_8 = 0x38,
    Key_9 = 0x39,
    Key_A = 0x41,
    Key_B = 0x42,
    Key_C = 0x43,
    Key_D = 0x44,
    Key_E = 0x45,
    Key_F = 0x46,
    Key_G = 0x47,
    Key_H = 0x48,
    Key_I = 0x49, // NOLINT(misc-confusable-identifiers): the name is Qt's
    Key_J = 0x4a,
    Key_K = 0x4b,
    Key_L = 0x4c,
    Key_M = 0x4d,
    Key_N = 0x4e,
    Key_O = 0x4f, // NOLINT(misc-confusable-identifiers): the name is Qt's
    Key_P = 0x50,
    Key_Q = 0x51,
    Key_R = 0x52,
    Key_S = 0x53,
    Key_T = 0x54,
    Key_U = 0x55,
    Key_V = 0x56,
    Key_W = 0x57,
    Key_X = 0x58,
    Key_Y = 0x59,
    Key_Z = 0x5a,
    Key_unknown = 0x01ffffff
};

/*!
    \enum Qt::PenStyle

    This enum describes how QPainter draws lines.

    \value NoPen No line is drawn.
    \value SolidLine A plain line.
*/
enum PenStyle { NoPen, SolidLine };

/*!
    \enum Qt::BrushStyle

    This enum describes how QPainter fills shapes.

    \value NoBrush No fill.
    \value SolidPattern A uniform fill.
*/
enum BrushStyle { NoBrush, SolidPattern };

/*!
    \enum Qt::AlignmentFlag

    This enum describes how text and items are aligned. The Alignment type is
    a typedef for \c{QFlags<AlignmentFlag>}.

    \value AlignLeft Aligns with the left edge.
    \value AlignRight Aligns with the right edge.
    \value AlignHCenter Centers horizontally.
    \value AlignTop Aligns with the top.
    \value AlignBottom Aligns with the bottom.
    \value AlignVCenter Centers vertically.
    \value AlignCenter Centers both ways: \c{AlignHCenter | AlignVCenter}.
*/
enum AlignmentFlag {
    AlignLeft = 0x0001,
    AlignRight = 0x0002,
    AlignHCenter = 0x0004,
    AlignTop = 0x0020,
    AlignBottom = 0x0040,
    AlignVCenter = 0x0080,
    AlignCenter = AlignVCenter | AlignHCenter
};
Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(Alignment)

/*!
    \enum Qt::TextFlag

    This enum holds the flags that QPainter::drawText() and
    QFontMetrics::boundingRect() accept in addition to Qt::AlignmentFlag.

    \value TextSingleLine Treats all whitespace, new lines included, as spaces.
    \value TextDontClip Text that does not fit is drawn outside the rectangle instead of being clipped.
    \value TextWordWrap Breaks lines at word boundaries to fit the width of the rectangle.
    \value TextWrapAnywhere Breaks lines between any two characters if a word does not fit.
*/
enum TextFlag { TextSingleLine = 0x0100, TextDontClip = 0x0200, TextWordWrap = 0x1000, TextWrapAnywhere = 0x2000 };

/*!
    \enum Qt::TextElideMode

    This enum says where QFontMetrics::elidedText() replaces text with an ellipsis.

    \value ElideLeft The ellipsis is at the start of the text.
    \value ElideRight The ellipsis is at the end of the text.
    \value ElideMiddle The ellipsis is in the middle of the text.
    \value ElideNone The text is not shortened.
*/
enum TextElideMode { ElideLeft, ElideRight, ElideMiddle, ElideNone };

/*!
    \enum Qt::ScrollBarPolicy

    This enum says when a scroll area shows a scroll bar.

    \value ScrollBarAsNeeded The scroll bar is shown when the contents do not fit.
    \value ScrollBarAlwaysOff The scroll bar is never shown.
    \value ScrollBarAlwaysOn The scroll bar is always shown.
*/
enum ScrollBarPolicy { ScrollBarAsNeeded, ScrollBarAlwaysOff, ScrollBarAlwaysOn };

/*!
    \enum Qt::ArrowType

    This enum says which way the arrow of a tool button points.

    \value NoArrow No arrow.
    \value UpArrow An arrow that points up.
    \value DownArrow An arrow that points down.
    \value LeftArrow An arrow that points to the left.
    \value RightArrow An arrow that points to the right.
*/
enum ArrowType { NoArrow, UpArrow, DownArrow, LeftArrow, RightArrow };

/*!
    \enum Qt::SortOrder

    This enum says in which direction items are sorted.

    \value AscendingOrder From the smallest to the largest.
    \value DescendingOrder From the largest to the smallest.
*/
enum SortOrder { AscendingOrder, DescendingOrder };

/*!
    \enum Qt::ItemFlag

    This enum describes what the user can do with an item of a table.

    \value NoItemFlags The item cannot be used.
    \value ItemIsSelectable The user can select the item.
    \value ItemIsEnabled The user can interact with the item; a disabled item is drawn in the disabled colors and
           cannot be selected.
*/
enum ItemFlag { NoItemFlags = 0, ItemIsSelectable = 1, ItemIsEnabled = 32 };
Q_DECLARE_FLAGS(ItemFlags, ItemFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFlags)

/*!
    \enum Qt::ColorScheme

    This enum describes the color scheme of the desktop: whether the interface is meant to be light
    on dark or dark on light. QStyleHints::colorScheme() returns it.

    \value Unknown The color scheme is not known; applications should treat it as Light.
    \value Light Dark text on light backgrounds.
    \value Dark Light text on dark backgrounds.
*/
enum class ColorScheme { Unknown, Light, Dark };

/*!
    \enum Qt::FocusPolicy

    This enum says how a widget gets the keyboard focus.

    \value NoFocus The widget does not accept focus.
    \value TabFocus The widget accepts focus by tabbing.
    \value ClickFocus The widget accepts focus by clicking.
    \value StrongFocus The widget accepts focus by tabbing and clicking.
    \value WheelFocus Accepted for compatibility; the same as StrongFocus.
*/
enum FocusPolicy {
    NoFocus = 0,
    TabFocus = 0x1,
    ClickFocus = 0x2,
    StrongFocus = TabFocus | ClickFocus | 0x8,
    WheelFocus = StrongFocus | 0x4
};

/*!
    \enum Qt::Orientation

    This enum describes a direction.

    \value Horizontal Left to right.
    \value Vertical Top to bottom.

    The Orientations type is a typedef for \c{QFlags<Orientation>}.
*/
enum Orientation { Horizontal = 0x1, Vertical = 0x2 };
Q_DECLARE_FLAGS(Orientations, Orientation)
Q_DECLARE_OPERATORS_FOR_FLAGS(Orientations)

/*!
    \enum Qt::WindowType

    This enum gives the kind of window a widget is, and hints for how the window system decorates it. The
    WindowFlags type is a typedef for \c{QFlags<WindowType>}.

    \value Widget A widget that is a child of its parent; with no parent it is a window anyway.
    \value Window A window, even if the widget has a parent.
    \value Dialog A dialog: a window that the window system keeps above its parent.
    \value Sheet Accepted for compatibility; the same as Dialog.
    \value Drawer Accepted for compatibility; the same as Dialog.
    \value Popup A popup window, such as a menu.
    \value Tool A tool window, kept above its parent.
    \value ToolTip A tool tip.
    \value SplashScreen A splash screen.
    \value SubWindow A child window inside another widget.
    \value WindowType_Mask The bits of the flags that hold the kind of window.
    \value MSWindowsFixedSizeDialogHint Accepted for compatibility; it does nothing.
    \value FramelessWindowHint A window without a frame or a title bar.
    \value WindowTitleHint A window with a title bar.
    \value WindowSystemMenuHint A window with a window menu.
    \value WindowMinimizeButtonHint A window with a button that minimizes it.
    \value WindowMaximizeButtonHint A window with a button that maximizes it.
    \value WindowMinMaxButtonsHint Both of the above.
    \value WindowContextHelpButtonHint A window with a help button.
    \value WindowStaysOnTopHint A window that is kept above the others.
    \value WindowCloseButtonHint A window with a close button.
    \value CustomizeWindowHint The decoration hints replace the ones the window system would choose.
*/
enum WindowType {
    Widget = 0x00000000,
    Window = 0x00000001,
    Dialog = 0x00000002 | Window,
    Sheet = 0x00000004 | Window,
    Drawer = Sheet | Dialog,
    Popup = 0x00000008 | Window,
    Tool = Popup | Dialog,
    ToolTip = Popup | Sheet,
    SplashScreen = ToolTip | Dialog,
    SubWindow = 0x00000012,
    WindowType_Mask = 0x000000ff,
    MSWindowsFixedSizeDialogHint = 0x00000100,
    FramelessWindowHint = 0x00000800,
    WindowTitleHint = 0x00001000,
    WindowSystemMenuHint = 0x00002000,
    WindowMinimizeButtonHint = 0x00004000,
    WindowMaximizeButtonHint = 0x00008000,
    WindowMinMaxButtonsHint = WindowMinimizeButtonHint | WindowMaximizeButtonHint,
    WindowContextHelpButtonHint = 0x00010000,
    WindowStaysOnTopHint = 0x00040000,
    CustomizeWindowHint = 0x02000000,
    WindowCloseButtonHint = 0x08000000
};
Q_DECLARE_FLAGS(WindowFlags, WindowType)
Q_DECLARE_OPERATORS_FOR_FLAGS(WindowFlags)

/*!
    \enum Qt::WindowState

    The state of a window, such as a QMdiSubWindow. The WindowStates type is a typedef for
    \c{QFlags<WindowState>}.

    \value WindowNoState Normal.
    \value WindowMinimized Minimized to a title bar or an icon.
    \value WindowMaximized Taking all the room it has.
    \value WindowFullScreen Taking the whole screen.
    \value WindowActive The active window.
*/
enum WindowState {
    WindowNoState = 0x00000000,
    WindowMinimized = 0x00000001,
    WindowMaximized = 0x00000002,
    WindowFullScreen = 0x00000004,
    WindowActive = 0x00000008
};
Q_DECLARE_FLAGS(WindowStates, WindowState)
Q_DECLARE_OPERATORS_FOR_FLAGS(WindowStates)

/*!
    \enum Qt::WindowModality

    This enum says which windows a modal window blocks the input of while it is shown.

    \value NonModal None.
    \value WindowModal Its parent window, and the parent window of that, and so on.
    \value ApplicationModal All the other windows of the application.
*/
enum WindowModality { NonModal, WindowModal, ApplicationModal };

/*!
    \enum Qt::ToolButtonStyle

    This enum says what a tool button shows.

    \value ToolButtonIconOnly Only the icon.
    \value ToolButtonTextOnly Only the text.
    \value ToolButtonTextBesideIcon The text beside the icon.
    \value ToolButtonTextUnderIcon The text under the icon.
    \value ToolButtonFollowStyle What the style says.
*/
enum ToolButtonStyle {
    ToolButtonIconOnly,
    ToolButtonTextOnly,
    ToolButtonTextBesideIcon,
    ToolButtonTextUnderIcon,
    ToolButtonFollowStyle
};

/*!
    \enum Qt::ToolBarArea

    This enum gives the sides of a QMainWindow where tool bars go. The ToolBarAreas type is a typedef for
    \c{QFlags<ToolBarArea>}.

    \value LeftToolBarArea The left side.
    \value RightToolBarArea The right side.
    \value TopToolBarArea The top, below the menu bar.
    \value BottomToolBarArea The bottom, above the status bar.
    \value AllToolBarAreas All the sides.
    \value NoToolBarArea None.
*/
enum ToolBarArea {
    LeftToolBarArea = 0x1,
    RightToolBarArea = 0x2,
    TopToolBarArea = 0x4,
    BottomToolBarArea = 0x8,
    AllToolBarAreas = 0xf,
    NoToolBarArea = 0
};
Q_DECLARE_FLAGS(ToolBarAreas, ToolBarArea)
Q_DECLARE_OPERATORS_FOR_FLAGS(ToolBarAreas)

/*!
    \enum Qt::DockWidgetArea

    This enum gives the sides of a QMainWindow where dock widgets go. The DockWidgetAreas type is a typedef for
    \c{QFlags<DockWidgetArea>}.

    \value LeftDockWidgetArea The left side.
    \value RightDockWidgetArea The right side.
    \value TopDockWidgetArea The top.
    \value BottomDockWidgetArea The bottom.
    \value AllDockWidgetAreas All the sides.
    \value NoDockWidgetArea None.
*/
enum DockWidgetArea {
    LeftDockWidgetArea = 0x1,
    RightDockWidgetArea = 0x2,
    TopDockWidgetArea = 0x4,
    BottomDockWidgetArea = 0x8,
    AllDockWidgetAreas = 0xf,
    NoDockWidgetArea = 0
};
Q_DECLARE_FLAGS(DockWidgetAreas, DockWidgetArea)
Q_DECLARE_OPERATORS_FOR_FLAGS(DockWidgetAreas)

/*!
    \enum Qt::DayOfWeek

    The days of the week, numbered as QDate::dayOfWeek() numbers them.

    \value Monday Monday, 1.
    \value Tuesday Tuesday, 2.
    \value Wednesday Wednesday, 3.
    \value Thursday Thursday, 4.
    \value Friday Friday, 5.
    \value Saturday Saturday, 6.
    \value Sunday Sunday, 7.
*/
enum DayOfWeek { Monday = 1, Tuesday = 2, Wednesday = 3, Thursday = 4, Friday = 5, Saturday = 6, Sunday = 7 };

/*!
    \enum Qt::LayoutDirection

    The direction in which text is written and laid out.

    \value LeftToRight Left to right.
    \value RightToLeft Right to left.
    \value LayoutDirectionAuto The direction is determined automatically from the content.

    \sa QLocale::textDirection()
*/
enum LayoutDirection { LeftToRight, RightToLeft, LayoutDirectionAuto };

/*!
    \enum Qt::MatchFlag

    How a text is matched against items, as by QCompleter. The MatchFlags type is a typedef for
    \c{QFlags<MatchFlag>}.

    \value MatchExactly The whole text.
    \value MatchContains The text anywhere in the item.
    \value MatchStartsWith The start of the item.
    \value MatchEndsWith The end of the item.
    \value MatchRegularExpression A regular expression; not supported by Nt.
    \value MatchWildcard A wildcard pattern; not supported by Nt.
    \value MatchFixedString The text as a string, ignoring case unless MatchCaseSensitive is set.
    \value MatchTypeMask The bits that hold the kind of match.
    \value MatchCaseSensitive Case matters.
    \value MatchWrap Searching wraps round to the start.
    \value MatchRecursive Items under items are searched too.
*/
enum MatchFlag {
    MatchExactly = 0,
    MatchContains = 1,
    MatchStartsWith = 2,
    MatchEndsWith = 3,
    MatchRegularExpression = 4,
    MatchWildcard = 5,
    MatchFixedString = 8,
    MatchTypeMask = 0x0F,
    MatchCaseSensitive = 16,
    MatchWrap = 32,
    MatchRecursive = 64
};
Q_DECLARE_FLAGS(MatchFlags, MatchFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(MatchFlags)

/*!
    \typedef Qt::HANDLE

    An opaque thread handle: the value QThread::currentThreadId() returns, a \c{void *}.
*/
using HANDLE = void *;

} // namespace Qt
