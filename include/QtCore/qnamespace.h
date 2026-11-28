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
    \value TimeZone A time zone given as a QTimeZone.

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
    \value UniqueConnection A flag combined with one of the types above: connect() fails if the same signal is already connected to the same slot of the same receiver. It applies to member function slots and to slots named by string, not to functors.
    \value SingleShotConnection A flag combined with one of the types above: the slot is called for the first emission only, and the connection is then removed.

    \sa QObject::connect(), {Signals and Slots}
*/
enum ConnectionType {
    AutoConnection = 0,
    DirectConnection = 1,
    QueuedConnection = 2,
    BlockingQueuedConnection = 3,
    UniqueConnection = 0x80,
    SingleShotConnection = 0x100
};

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
    \enum Qt::Initialization

    This enum is the tag of the constructors that leave the contents of a
    container unset, such as \c{QByteArray(size, Qt::Uninitialized)}, for
    callers that fill in every element afterwards.

    \value Uninitialized The elements need no particular value. Nt
           value-initializes them: bytes and characters are zero, and class
           types are default-constructed.
*/
enum Initialization { Uninitialized };

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
    \enum Qt::TimerId
    \since 6.8

    The type of a timer identifier, as returned by QTimer::id() and
    QBasicTimer::id(). The value is the \c int identifier that
    QObject::startTimer() returns.

    \value Invalid The identifier of a timer that is not running.

    \sa QTimer::id(), QBasicTimer::id(), QTimerEvent::id()
*/
enum class TimerId { Invalid = 0 };

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
    \enum Qt::MouseEventSource

    Where a mouse event came from.

    \value MouseEventNotSynthesized The event comes from a mouse.
    \value MouseEventSynthesizedBySystem The system made the event from a touch or a pen.
    \value MouseEventSynthesizedByQt The library made the event from a touch.
    \value MouseEventSynthesizedByApplication The application made the event.
*/
enum MouseEventSource {
    MouseEventNotSynthesized,
    MouseEventSynthesizedBySystem,
    MouseEventSynthesizedByQt,
    MouseEventSynthesizedByApplication
};

/*!
    \enum Qt::MouseEventFlag

    Extra information about a mouse event. The MouseEventFlags type is a typedef for \c{QFlags<MouseEventFlag>}.

    \value NoMouseEventFlag No flag.
    \value MouseEventCreatedDoubleClick The press is the second of a double click.
    \value MouseEventFlagMask All the flags.
*/
enum MouseEventFlag { NoMouseEventFlag = 0x00, MouseEventCreatedDoubleClick = 0x01, MouseEventFlagMask = 0xFF };
Q_DECLARE_FLAGS(MouseEventFlags, MouseEventFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(MouseEventFlags)

/*!
    \enum Qt::ScrollPhase

    The phase of a scroll gesture.

    \value NoScrollPhase The event is not part of a scroll gesture.
    \value ScrollBegin The scroll gesture starts.
    \value ScrollUpdate The scroll gesture goes on.
    \value ScrollEnd The scroll gesture ends.
    \value ScrollMomentum The scroll goes on by momentum.
*/
enum ScrollPhase { NoScrollPhase = 0, ScrollBegin, ScrollUpdate, ScrollEnd, ScrollMomentum };

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
    \value Key_F13 The F13 key.
    \value Key_F14 The F14 key.
    \value Key_F15 The F15 key.
    \value Key_F16 The F16 key.
    \value Key_F17 The F17 key.
    \value Key_F18 The F18 key.
    \value Key_F19 The F19 key.
    \value Key_F20 The F20 key.
    \value Key_F21 The F21 key.
    \value Key_F22 The F22 key.
    \value Key_F23 The F23 key.
    \value Key_F24 The F24 key.
    \value Key_F25 The F25 key.
    \value Key_F26 The F26 key.
    \value Key_F27 The F27 key.
    \value Key_F28 The F28 key.
    \value Key_F29 The F29 key.
    \value Key_F30 The F30 key.
    \value Key_F31 The F31 key.
    \value Key_F32 The F32 key.
    \value Key_F33 The F33 key.
    \value Key_F34 The F34 key.
    \value Key_F35 The F35 key.
    \value Key_Exclam The ! key.
    \value Key_QuoteDbl The " key.
    \value Key_NumberSign The # key.
    \value Key_Dollar The $ key.
    \value Key_Percent The % key.
    \value Key_Ampersand The & key.
    \value Key_Apostrophe The ' key.
    \value Key_ParenLeft The ( key.
    \value Key_ParenRight The ) key.
    \value Key_Asterisk The * key.
    \value Key_Colon The : key.
    \value Key_Semicolon The ; key.
    \value Key_Less The < key.
    \value Key_Greater The > key.
    \value Key_Question The ? key.
    \value Key_At The @ key.
    \value Key_BracketLeft The [ key.
    \value Key_Backslash The backslash key.
    \value Key_BracketRight The ] key.
    \value Key_AsciiCircum The ^ key.
    \value Key_Underscore The _ key.
    \value Key_QuoteLeft The ` key.
    \value Key_BraceLeft The { key.
    \value Key_Bar The | key.
    \value Key_BraceRight The } key.
    \value Key_AsciiTilde The ~ key.
    \value Key_Super_L The left Super (Windows) key.
    \value Key_Super_R The right Super (Windows) key.
    \value Key_Hyper_L The left Hyper key.
    \value Key_Hyper_R The right Hyper key.
    \value Key_Help The Help key.
    \value Key_Direction_L The left direction key.
    \value Key_Direction_R The right direction key.
    \value Key_SysReq The SysRq key.
    \value Key_Clear The Clear key.
    \value Key_AltGr The AltGr key.
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
    \value Key_Back The Back key of a browser, remote control or gamepad.
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
    Key_F13 = 0x0100003c,
    Key_F14 = 0x0100003d,
    Key_F15 = 0x0100003e,
    Key_F16 = 0x0100003f,
    Key_F17 = 0x01000040,
    Key_F18 = 0x01000041,
    Key_F19 = 0x01000042,
    Key_F20 = 0x01000043,
    Key_F21 = 0x01000044,
    Key_F22 = 0x01000045,
    Key_F23 = 0x01000046,
    Key_F24 = 0x01000047,
    Key_F25 = 0x01000048,
    Key_F26 = 0x01000049,
    Key_F27 = 0x0100004a,
    Key_F28 = 0x0100004b,
    Key_F29 = 0x0100004c,
    Key_F30 = 0x0100004d,
    Key_F31 = 0x0100004e,
    Key_F32 = 0x0100004f,
    Key_F33 = 0x01000050,
    Key_F34 = 0x01000051,
    Key_F35 = 0x01000052,
    Key_Menu = 0x01000055,
    Key_Back = 0x01000061,
    Key_Space = 0x20,
    Key_Exclam = 0x21,
    Key_QuoteDbl = 0x22,
    Key_NumberSign = 0x23,
    Key_Dollar = 0x24,
    Key_Percent = 0x25,
    Key_Ampersand = 0x26,
    Key_Apostrophe = 0x27,
    Key_ParenLeft = 0x28,
    Key_ParenRight = 0x29,
    Key_Asterisk = 0x2a,
    Key_Colon = 0x3a,
    Key_Semicolon = 0x3b,
    Key_Less = 0x3c,
    Key_Greater = 0x3e,
    Key_Question = 0x3f,
    Key_At = 0x40,
    Key_BracketLeft = 0x5b,
    Key_Backslash = 0x5c,
    Key_BracketRight = 0x5d,
    Key_AsciiCircum = 0x5e,
    Key_Underscore = 0x5f,
    Key_QuoteLeft = 0x60,
    Key_BraceLeft = 0x7b,
    Key_Bar = 0x7c,
    Key_BraceRight = 0x7d,
    Key_AsciiTilde = 0x7e,
    Key_Super_L = 0x1000053,
    Key_Super_R = 0x1000054,
    Key_Hyper_L = 0x1000056,
    Key_Hyper_R = 0x1000057,
    Key_Help = 0x1000058,
    Key_Direction_L = 0x1000059,
    Key_Direction_R = 0x1000060,
    Key_SysReq = 0x1000028,
    Key_Clear = 0x100000b,
    Key_AltGr = 0x1001103,
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
    \value DashLine Dashes separated by gaps.
    \value DotLine Dots separated by gaps.
    \value DashDotLine Dashes and dots, alternating.
    \value DashDotDotLine A dash and two dots, repeating.
    \value CustomDashLine The dashes of QPen::dashPattern().
    \omitvalue MPenStyle
*/
enum PenStyle { NoPen, SolidLine, DashLine, DotLine, DashDotLine, DashDotDotLine, CustomDashLine, MPenStyle = 0x0f };

/*!
    \enum Qt::PenCapStyle

    This enum describes how the ends of lines are drawn.

    \value FlatCap The line stops at its end point.
    \value SquareCap The line goes on for half its width past its end point, squarely.
    \value RoundCap The line ends in a half circle around its end point.
    \omitvalue MPenCapStyle
*/
enum PenCapStyle { FlatCap = 0x00, SquareCap = 0x10, RoundCap = 0x20, MPenCapStyle = 0x30 };

/*!
    \enum Qt::PenJoinStyle

    This enum describes how the segments of a line meet.

    \value MiterJoin The outer edges are extended to meet, up to the miter limit of the pen.
    \value BevelJoin The corner is cut off.
    \value RoundJoin The corner is rounded.
    \value SvgMiterJoin As MiterJoin; a corner beyond the miter limit is beveled.
    \omitvalue MPenJoinStyle
*/
enum PenJoinStyle { MiterJoin = 0x00, BevelJoin = 0x40, RoundJoin = 0x80, SvgMiterJoin = 0x100, MPenJoinStyle = 0x1c0 };

/*!
    \enum Qt::BrushStyle

    This enum describes how QPainter fills shapes.

    \value NoBrush No fill.
    \value SolidPattern A uniform fill.
    \value LinearGradientPattern A QLinearGradient.
    \value RadialGradientPattern A QRadialGradient.
    \value ConicalGradientPattern A QConicalGradient.
*/
enum BrushStyle { NoBrush, SolidPattern, LinearGradientPattern = 15, RadialGradientPattern = 16, ConicalGradientPattern = 17 };

/*!
    \enum Qt::FillRule

    This enum describes which points are inside a shape whose outline crosses itself.

    \value OddEvenFill A point is inside if a ray from it crosses the outline an odd number of times.
    \value WindingFill A point is inside if the outline winds around it: the windings in each direction do not
           cancel out.
*/
enum FillRule { OddEvenFill, WindingFill };

/*!
    \enum Qt::Axis

    This enum names the axes of a rotation.

    \value XAxis The x axis.
    \value YAxis The y axis.
    \value ZAxis The z axis, out of the screen: rotations in the plane.
*/
enum Axis { XAxis, YAxis, ZAxis };

/*!
    \enum Qt::SizeMode

    This enum describes how the radii of rounded rectangles are given.

    \value AbsoluteSize In the units of the painter.
    \value RelativeSize As percentages of half the width and half the height of the rectangle.
*/
enum SizeMode { AbsoluteSize, RelativeSize };

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
    \value AlignJustify Justifies the text; Nt aligns it left.
    \value AlignAbsolute Left and right are not swapped in right-to-left text; Nt never swaps them.
*/
enum AlignmentFlag {
    AlignLeft = 0x0001,
    AlignRight = 0x0002,
    AlignHCenter = 0x0004,
    AlignTop = 0x0020,
    AlignBottom = 0x0040,
    AlignVCenter = 0x0080,
    AlignJustify = 0x0008,
    AlignAbsolute = 0x0010,
    AlignCenter = AlignVCenter | AlignHCenter
};
Q_DECLARE_FLAGS(Alignment, AlignmentFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(Alignment)
/*!
    \variable Qt::AlignHorizontal_Mask
    \since 6.0
    \brief The bits of the horizontal alignment. Qt has it in AlignmentFlag; in Nt it is a constant.
*/
constexpr Alignment AlignHorizontal_Mask = AlignLeft | AlignRight | AlignHCenter | AlignJustify | AlignAbsolute;
/*!
    \variable Qt::AlignVertical_Mask
    \since 6.0
    \brief The bits of the vertical alignment, with the bit of Qt's \c AlignBaseline, which Nt leaves out of
    AlignmentFlag as it shares its bit with TextSingleLine.
*/
constexpr Alignment AlignVertical_Mask = AlignTop | AlignBottom | AlignVCenter | Alignment::fromInt(0x0100);

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
    \enum Qt::TextFormat

    This enum says how a text is to be read, by a QLabel for example.

    \value PlainText The text is shown as it is.
    \value RichText The text is HTML, as QTextDocument::setHtml() reads it.
    \value AutoText The text is rich text if Qt::mightBeRichText() says so, and plain text otherwise.
    \value MarkdownText The text is Markdown. Nt shows it as plain text.
*/
enum TextFormat { PlainText, RichText, AutoText, MarkdownText };

/*!
    \enum Qt::WhiteSpaceMode

    This enum says what Qt::convertFromPlainText() does with white space.

    \value WhiteSpaceNormal Spaces are left to the rules of HTML, which show a run of them as one.
    \value WhiteSpacePre Spaces are kept as they are.
    \value WhiteSpaceNoWrap Spaces are left as with WhiteSpaceNormal.
    \omitvalue WhiteSpaceModeUndefined
*/
enum WhiteSpaceMode { WhiteSpaceNormal, WhiteSpacePre, WhiteSpaceNoWrap, WhiteSpaceModeUndefined = -1 };

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
    \enum Qt::ItemSelectionMode

    How items are matched against a point, rectangle or path.

    \value ContainsItemShape The area contains the whole shape of the item.
    \value IntersectsItemShape The area touches the shape of the item.
    \value ContainsItemBoundingRect The area contains the whole bounding rectangle of the item.
    \value IntersectsItemBoundingRect The area touches the bounding rectangle of the item.
*/
enum ItemSelectionMode {
    ContainsItemShape = 0x0,
    IntersectsItemShape = 0x1,
    ContainsItemBoundingRect = 0x2,
    IntersectsItemBoundingRect = 0x3
};

/*!
    \enum Qt::FocusReason

    Why the keyboard focus moved.

    \value MouseFocusReason A mouse click.
    \value TabFocusReason The Tab key.
    \value BacktabFocusReason Shift and Tab.
    \value ActiveWindowFocusReason Another window became active.
    \value PopupFocusReason A popup opened or closed.
    \value ShortcutFocusReason A shortcut.
    \value MenuBarFocusReason The menu bar.
    \value OtherFocusReason Anything else.
    \value NoFocusReason No reason.
*/
enum FocusReason {
    MouseFocusReason,
    TabFocusReason,
    BacktabFocusReason,
    ActiveWindowFocusReason,
    PopupFocusReason,
    ShortcutFocusReason,
    MenuBarFocusReason,
    OtherFocusReason,
    NoFocusReason
};

/*!
    \enum Qt::ContextMenuPolicy
    \since 6.0

    This enum tells what a widget does when the user asks for a context menu, with the right mouse button or the
    Menu key.

    \value NoContextMenu The widget has none; the request goes to its parent.
    \value DefaultContextMenu QWidget::contextMenuEvent() is called. This is the default.
    \value ActionsContextMenu A menu of the actions of the widget opens.
    \value CustomContextMenu The widget emits QWidget::customContextMenuRequested().
    \value PreventContextMenu The widget has none, and the request goes no further.
*/
enum ContextMenuPolicy { NoContextMenu, DefaultContextMenu, ActionsContextMenu, CustomContextMenu, PreventContextMenu };

/*!
    \enum Qt::TileRule
    \since 6.0

    How qDrawBorderPixmap() fills a part of a frame with a part of a pixmap.

    \value StretchTile It is stretched to fill the part.
    \value RepeatTile It is repeated, and the last one is cut off.
    \value RoundTile It is repeated a whole number of times, scaled to fit.
*/
enum TileRule { StretchTile, RepeatTile, RoundTile };

/*!
    \enum Qt::HighDpiScaleFactorRoundingPolicy
    \since 6.0

    How a scale factor between whole numbers, such as 1.5, is rounded.

    \value Unset Not set.
    \value Round Rounded to the nearest whole number.
    \value Ceil Rounded up.
    \value Floor Rounded down.
    \value RoundPreferFloor Rounded down from .5, up above it.
    \value PassThrough Not rounded.
*/
enum class HighDpiScaleFactorRoundingPolicy { Unset, Round, Ceil, Floor, RoundPreferFloor, PassThrough };

/*!
    \enum Qt::ItemSelectionOperation
    \since 6.0

    How a selection changes the one there is.

    \value ReplaceSelection It takes the place of the selection.
    \value AddToSelection It is added to the selection.
*/
enum ItemSelectionOperation { ReplaceSelection, AddToSelection };

/*!
    \enum Qt::InputMethodHint
    \since 6.0

    Hints to an input method, such as an on-screen keyboard, about the text a widget takes. Nt has no input methods
    and keeps the hints for the application.

    \value ImhNone No hints.
    \value ImhHiddenText The characters are hidden, as in a password field.
    \value ImhSensitiveData The text is not to be stored or learnt.
    \value ImhNoAutoUppercase No upper case at the start of a sentence.
    \value ImhPreferNumbers Numbers are preferred.
    \value ImhPreferUppercase Upper case is preferred.
    \value ImhPreferLowercase Lower case is preferred.
    \value ImhNoPredictiveText No predicted words.
    \value ImhDate A date.
    \value ImhTime A time.
    \value ImhPreferLatin Latin characters are preferred.
    \value ImhMultiLine Several lines.
    \value ImhNoEditMenu No edit menu.
    \value ImhNoTextHandles No text handles.
    \value ImhDigitsOnly Only digits.
    \value ImhFormattedNumbersOnly Only numbers, with a sign and a decimal point.
    \value ImhUppercaseOnly Only upper case.
    \value ImhLowercaseOnly Only lower case.
    \value ImhDialableCharactersOnly Only characters of a telephone number.
    \value ImhEmailCharactersOnly Only characters of an e-mail address.
    \value ImhUrlCharactersOnly Only characters of a URL.
    \value ImhLatinOnly Only Latin characters.
    \value ImhExclusiveInputMask The mask of the hints that allow only some characters.
*/
enum InputMethodHint {
    ImhNone = 0x0,
    ImhHiddenText = 0x1,
    ImhSensitiveData = 0x2,
    ImhNoAutoUppercase = 0x4,
    ImhPreferNumbers = 0x8,
    ImhPreferUppercase = 0x10,
    ImhPreferLowercase = 0x20,
    ImhNoPredictiveText = 0x40,
    ImhDate = 0x80,
    ImhTime = 0x100,
    ImhPreferLatin = 0x200,
    ImhMultiLine = 0x400,
    ImhNoEditMenu = 0x800,
    ImhNoTextHandles = 0x1000,
    ImhDigitsOnly = 0x10000,
    ImhFormattedNumbersOnly = 0x20000,
    ImhUppercaseOnly = 0x40000,
    ImhLowercaseOnly = 0x80000,
    ImhDialableCharactersOnly = 0x100000,
    ImhEmailCharactersOnly = 0x200000,
    ImhUrlCharactersOnly = 0x400000,
    ImhLatinOnly = 0x800000,
    ImhExclusiveInputMask = static_cast<int>(0xffff0000)
};
Q_DECLARE_FLAGS(InputMethodHints, InputMethodHint)
Q_DECLARE_OPERATORS_FOR_FLAGS(InputMethodHints)

/*!
    \enum Qt::InputMethodQuery
    \since 6.0

    What an input method asks a widget about. Nt has no input methods.

    \value ImEnabled Whether the widget takes input from an input method.
    \value ImCursorRectangle The rectangle of the cursor.
    \value ImFont The font of the text.
    \value ImCursorPosition The position of the cursor in the surrounding text.
    \value ImSurroundingText The text around the cursor.
    \value ImCurrentSelection The selected text.
    \value ImMaximumTextLength The longest text the widget takes.
    \value ImAnchorPosition The position of the anchor of the selection.
    \value ImHints The Qt::InputMethodHints of the widget.
    \value ImPreferredLanguage The language the widget prefers.
    \value ImAbsolutePosition The position of the cursor in the whole text.
    \value ImTextBeforeCursor The text before the cursor.
    \value ImTextAfterCursor The text after the cursor.
    \value ImEnterKeyType The kind of the Enter key.
    \value ImAnchorRectangle The rectangle of the anchor.
    \value ImInputItemClipRectangle The visible part of the widget.
    \value ImReadOnly Whether the text is read-only.
    \value ImQueryInput The queries of typing.
    \value ImQueryAll All queries.
    \omitvalue ImPlatformData
*/
enum InputMethodQuery : unsigned {
    ImEnabled = 0x1,
    ImCursorRectangle = 0x2,
    ImFont = 0x4,
    ImCursorPosition = 0x8,
    ImSurroundingText = 0x10,
    ImCurrentSelection = 0x20,
    ImMaximumTextLength = 0x40,
    ImAnchorPosition = 0x80,
    ImHints = 0x100,
    ImPreferredLanguage = 0x200,
    ImAbsolutePosition = 0x400,
    ImTextBeforeCursor = 0x800,
    ImTextAfterCursor = 0x1000,
    ImEnterKeyType = 0x2000,
    ImAnchorRectangle = 0x4000,
    ImInputItemClipRectangle = 0x8000,
    ImReadOnly = 0x10000,
    ImPlatformData = 0x80000000,
    ImQueryInput = ImCursorRectangle | ImCursorPosition | ImSurroundingText | ImCurrentSelection | ImAnchorRectangle |
                   ImAnchorPosition,
    ImQueryAll = 0xffffffff
};
Q_DECLARE_FLAGS(InputMethodQueries, InputMethodQuery)
Q_DECLARE_OPERATORS_FOR_FLAGS(InputMethodQueries)

/*!
    \enum Qt::SizeHint
    \since 6.0

    This enum names the size hints of a QGraphicsLayoutItem.

    \value MinimumSize The smallest size.
    \value PreferredSize The best size.
    \value MaximumSize The largest size.
    \value MinimumDescent The smallest descent of text below its baseline; Nt does not use it.
    \omitvalue NSizeHints
*/
enum SizeHint { MinimumSize, PreferredSize, MaximumSize, MinimumDescent, NSizeHints };

/*!
    \enum Qt::GestureType
    \since 6.0

    This enum names gestures. Nt has no gesture framework; QScroller::grabGesture() returns a value of it.

    \value TapGesture A tap.
    \value TapAndHoldGesture A tap and hold, a long press.
    \value PanGesture A pan, a drag with one or more fingers.
    \value PinchGesture A pinch.
    \value SwipeGesture A swipe.
    \value CustomGesture The first value of a gesture of the application, also the one of a QScroller.
    \omitvalue LastGestureType
*/
/*!
    \enum Qt::GestureState

    The state of a gesture.

    \value NoGesture No gesture.
    \value GestureStarted A continuous gesture has started.
    \value GestureUpdated A gesture continues.
    \value GestureFinished A gesture ended successfully.
    \value GestureCanceled A gesture was canceled.
*/
enum GestureState { NoGesture, GestureStarted = 1, GestureUpdated = 2, GestureFinished = 3, GestureCanceled = 4 };

/*!
    \enum Qt::GestureFlag

    How a widget or item wants gestures. GestureFlags is a typedef for \c{QFlags<GestureFlag>}.

    \value DontStartGestureOnChildren A gesture does not start on a child of the widget.
    \value ReceivePartialGestures The widget gets gestures that may still be recognized.
    \value IgnoredGesturesPropagateToParent An ignored gesture is offered to the parent.
*/
enum GestureFlag {
    DontStartGestureOnChildren = 0x01,
    ReceivePartialGestures = 0x02,
    IgnoredGesturesPropagateToParent = 0x04
};
Q_DECLARE_FLAGS(GestureFlags, GestureFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(GestureFlags)

enum GestureType : unsigned {
    TapGesture = 1,
    TapAndHoldGesture = 2,
    PanGesture = 3,
    PinchGesture = 4,
    SwipeGesture = 5,
    CustomGesture = 0x0100,
    LastGestureType = ~0u
};

/*!
    \enum Qt::TextInteractionFlag

    How the user may interact with displayed text.

    \value NoTextInteraction Not at all.
    \value TextSelectableByMouse Text can be selected with the mouse.
    \value TextSelectableByKeyboard Text can be selected with the keyboard.
    \value LinksAccessibleByMouse Links can be followed with the mouse.
    \value LinksAccessibleByKeyboard Links can be followed with the keyboard.
    \value TextEditable Text can be edited.
    \value TextEditorInteraction The flags of a text editor.
    \value TextBrowserInteraction The flags of a text browser.

    The TextInteractionFlags type is a typedef for \c{QFlags<TextInteractionFlag>}.
*/
enum TextInteractionFlag {
    NoTextInteraction = 0,
    TextSelectableByMouse = 1,
    TextSelectableByKeyboard = 2,
    LinksAccessibleByMouse = 4,
    LinksAccessibleByKeyboard = 8,
    TextEditable = 16,
    TextEditorInteraction = TextSelectableByMouse | TextSelectableByKeyboard | TextEditable,
    TextBrowserInteraction = TextSelectableByMouse | LinksAccessibleByMouse | LinksAccessibleByKeyboard
};
Q_DECLARE_FLAGS(TextInteractionFlags, TextInteractionFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(TextInteractionFlags)

/*!
    \enum Qt::ClipOperation

    How a new clip combines with the current one.

    \value NoClip Clipping is turned off.
    \value ReplaceClip The new clip replaces the current one.
    \value IntersectClip The clip becomes the intersection of the two.
*/
enum ClipOperation { NoClip, ReplaceClip, IntersectClip };

/*!
    \enum Qt::ItemFlag

    This enum describes what the user can do with an item of a table.

    \value NoItemFlags The item cannot be used.
    \value ItemIsSelectable The user can select the item.
    \value ItemIsEditable The user can edit the item.
    \value ItemIsDragEnabled The item can be dragged.
    \value ItemIsDropEnabled The item can be used as a drop target.
    \value ItemIsUserCheckable The user can check or uncheck the item.
    \value ItemIsEnabled The user can interact with the item; a disabled item is drawn in the disabled colors and
           cannot be selected.
    \value ItemIsAutoTristate The check state of the item depends on the check states of its children.
    \value ItemNeverHasChildren The item never has children; views can use this for optimization.
    \value ItemIsUserTristate The user can cycle through three check states.
*/
enum ItemFlag {
    NoItemFlags = 0,
    ItemIsSelectable = 1,
    ItemIsEditable = 2,
    ItemIsDragEnabled = 4,
    ItemIsDropEnabled = 8,
    ItemIsUserCheckable = 16,
    ItemIsEnabled = 32,
    ItemIsAutoTristate = 64,
    ItemNeverHasChildren = 128,
    ItemIsUserTristate = 256
};
Q_DECLARE_FLAGS(ItemFlags, ItemFlag)
Q_DECLARE_OPERATORS_FOR_FLAGS(ItemFlags)

/*!
    \enum Qt::ItemDataRole

    The roles under which an item model stores the data of an item. Roles from UserRole on are for
    application-specific data.

    \value DisplayRole The text shown for the item (QString).
    \value DecorationRole The icon or color shown with the item.
    \value EditRole The data in a form suitable for editing (QString).
    \value ToolTipRole The tool tip of the item (QString).
    \value StatusTipRole The status bar text of the item (QString).
    \value WhatsThisRole The "What's This?" text of the item (QString).
    \value FontRole The font of the item.
    \value TextAlignmentRole The alignment of the text (Qt::Alignment).
    \value BackgroundRole The background of the item.
    \value ForegroundRole The foreground (text) color of the item.
    \value CheckStateRole The check state of the item (Qt::CheckState).
    \value AccessibleTextRole The text for accessibility tools.
    \value AccessibleDescriptionRole The description for accessibility tools.
    \value SizeHintRole The size hint of the item.
    \value InitialSortOrderRole The initial sort order of a header section (Qt::SortOrder).
    \value DisplayPropertyRole Reserved.
    \value DecorationPropertyRole Reserved.
    \value ToolTipPropertyRole Reserved.
    \value StatusTipPropertyRole Reserved.
    \value WhatsThisPropertyRole Reserved.
    \value UserRole The first role for application-specific data.
*/
enum ItemDataRole {
    DisplayRole = 0,
    DecorationRole = 1,
    EditRole = 2,
    ToolTipRole = 3,
    StatusTipRole = 4,
    WhatsThisRole = 5,
    FontRole = 6,
    TextAlignmentRole = 7,
    BackgroundRole = 8,
    ForegroundRole = 9,
    CheckStateRole = 10,
    AccessibleTextRole = 11,
    AccessibleDescriptionRole = 12,
    SizeHintRole = 13,
    InitialSortOrderRole = 14,
    DisplayPropertyRole = 27,
    DecorationPropertyRole = 28,
    ToolTipPropertyRole = 29,
    StatusTipPropertyRole = 30,
    WhatsThisPropertyRole = 31,
    UserRole = 0x0100
};

/*!
    \enum Qt::CheckState

    \value Unchecked The item is not checked.
    \value PartiallyChecked Some of the children of the item are checked.
    \value Checked The item is checked.
*/
enum CheckState { Unchecked, PartiallyChecked, Checked };

/*!
    \enum Qt::DropAction

    The actions a drag and drop operation can perform.

    \value CopyAction Copy the data to the target.
    \value MoveAction Move the data from the source to the target.
    \value LinkAction Create a link from the source to the target.
    \value ActionMask The bits of the actions.
    \value TargetMoveAction The target moves the data and the source should not delete it.
    \value IgnoreAction Ignore the action.
*/
enum DropAction { CopyAction = 0x1, MoveAction = 0x2, LinkAction = 0x4, ActionMask = 0xff, TargetMoveAction = 0x8002, IgnoreAction = 0x0 };
Q_DECLARE_FLAGS(DropActions, DropAction)
Q_DECLARE_OPERATORS_FOR_FLAGS(DropActions)

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
    \enum Qt::PermissionStatus
    \since 6.5
    This enum describes whether the application may use a feature that needs the user's permission.

    \value Undetermined The user has not been asked yet; QCoreApplication::requestPermission() asks.
    \value Granted The application may use the feature.
    \value Denied The application may not use the feature.

    \sa QPermission, QCoreApplication::checkPermission()
*/
enum class PermissionStatus { Undetermined, Granted, Denied };

/*!
    \enum Qt::ApplicationAttribute
    \since 4.4
    This enum describes attributes that change how the whole application behaves. Set them with
    QCoreApplication::setAttribute(), before the application object is made for most of them.

    \value AA_QtQuickUseDefaultSizePolicy Qt Quick layouts use the size policies of their items.
    \value AA_DontShowIconsInMenus Menus do not show the icons of their actions.
    \value AA_NativeWindows Widgets get native windows.
    \value AA_DontCreateNativeWidgetSiblings The siblings of native widgets stay alien.
    \value AA_PluginApplication The application is a plugin of another program and does not own the process.
    \value AA_DontUseNativeMenuBar Menu bars are not given to the desktop.
    \value AA_MacDontSwapCtrlAndMeta The Control and Meta keys are not swapped (macOS only).
    \value AA_Use96Dpi Fonts are laid out for 96 dots per inch, whatever the screen.
    \value AA_DisableNativeVirtualKeyboard No virtual keyboard of the platform is shown.
    \value AA_SynthesizeTouchForUnhandledMouseEvents Mouse events that nobody accepts come again as touch events.
    \value AA_SynthesizeMouseForUnhandledTouchEvents Touch events that nobody accepts come again as mouse events.
    \value AA_ForceRasterWidgets Widgets are painted in software.
    \value AA_UseDesktopOpenGL OpenGL contexts use desktop OpenGL.
    \value AA_UseOpenGLES OpenGL contexts use OpenGL ES.
    \value AA_UseSoftwareOpenGL OpenGL contexts use a software renderer.
    \value AA_ShareOpenGLContexts OpenGL contexts share their resources with each other.
    \value AA_SetPalette Set by the application when it set a palette.
    \value AA_UseStyleSheetPropagationInWidgetStyles Style sheets pass fonts and palettes on to child widgets.
    \value AA_DontUseNativeDialogs File and color dialogs of the desktop are not used.
    \value AA_SynthesizeMouseForUnhandledTabletEvents Tablet events that nobody accepts come again as mouse events.
    \value AA_CompressHighFrequencyEvents Mouse moves and similar events are merged while the application is busy.
    \value AA_DontCheckOpenGLContextThreadAffinity OpenGL contexts may be used from any thread.
    \value AA_DisableShaderDiskCache Compiled shaders are not cached on disk.
    \value AA_DontShowShortcutsInContextMenus Context menus do not show the shortcuts of their actions.
    \value AA_CompressTabletEvents Tablet events are merged while the application is busy.
    \value AA_DisableSessionManager The application does not talk to the session manager of the desktop.
    \value AA_AttributeCount The number of attributes.

    \sa QCoreApplication::setAttribute(), QCoreApplication::testAttribute()
*/
enum ApplicationAttribute {
    AA_QtQuickUseDefaultSizePolicy = 1,
    AA_DontShowIconsInMenus = 2,
    AA_NativeWindows = 3,
    AA_DontCreateNativeWidgetSiblings = 4,
    AA_PluginApplication = 5,
    AA_DontUseNativeMenuBar = 6,
    AA_MacDontSwapCtrlAndMeta = 7,
    AA_Use96Dpi = 8,
    AA_DisableNativeVirtualKeyboard = 9,
    AA_SynthesizeTouchForUnhandledMouseEvents = 11,
    AA_SynthesizeMouseForUnhandledTouchEvents = 12,
    AA_ForceRasterWidgets = 14,
    AA_UseDesktopOpenGL = 15,
    AA_UseOpenGLES = 16,
    AA_UseSoftwareOpenGL = 17,
    AA_ShareOpenGLContexts = 18,
    AA_SetPalette = 19,
    AA_UseStyleSheetPropagationInWidgetStyles = 22,
    AA_DontUseNativeDialogs = 23,
    AA_SynthesizeMouseForUnhandledTabletEvents = 24,
    AA_CompressHighFrequencyEvents = 25,
    AA_DontCheckOpenGLContextThreadAffinity = 26,
    AA_DisableShaderDiskCache = 27,
    AA_DontShowShortcutsInContextMenus = 28,
    AA_CompressTabletEvents = 29,
    AA_DisableSessionManager = 31,
    AA_AttributeCount
};

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
    \enum Qt::TransformationMode

    This enum says how an image is resampled when it is scaled.

    \value FastTransformation The nearest pixel, quickly.
    \value SmoothTransformation A bilinear blend of the four nearest pixels.
*/
enum TransformationMode { FastTransformation, SmoothTransformation };

/*!
    \enum Qt::ShortcutContext

    This enum says where a QShortcut is active.

    \value WidgetShortcut While its parent widget has the focus.
    \value WindowShortcut While its parent is in the active window.
    \value ApplicationShortcut While any window of the application is active.
    \value WidgetWithChildrenShortcut While its parent widget or one of its children has the focus.
*/
/*!
    \enum Qt::WindowFrameSection

    The parts of a window frame.

    \value NoSection Not on the frame.
    \value LeftSection The left edge.
    \value TopLeftSection The top left corner.
    \value TopSection The top edge.
    \value TopRightSection The top right corner.
    \value RightSection The right edge.
    \value BottomRightSection The bottom right corner.
    \value BottomSection The bottom edge.
    \value BottomLeftSection The bottom left corner.
    \value TitleBarArea The title bar.
*/
enum WindowFrameSection {
    NoSection,
    LeftSection,
    TopLeftSection,
    TopSection,
    TopRightSection,
    RightSection,
    BottomRightSection,
    BottomSection,
    BottomLeftSection,
    TitleBarArea
};

enum ShortcutContext { WidgetShortcut, WindowShortcut, ApplicationShortcut, WidgetWithChildrenShortcut };

/*!
    \enum Qt::Edge

    This enum names the edges of a rectangle.

    \value TopEdge The top edge.
    \value LeftEdge The left edge.
    \value RightEdge The right edge.
    \value BottomEdge The bottom edge.

    The Edges type is a typedef for \c{QFlags<Edge>}.
*/
enum Edge { TopEdge = 0x00001, LeftEdge = 0x00002, RightEdge = 0x00004, BottomEdge = 0x00008 };
Q_DECLARE_FLAGS(Edges, Edge)
Q_DECLARE_OPERATORS_FOR_FLAGS(Edges)

/*!
    \enum Qt::ScreenOrientation

    This enum describes the orientation of a screen or of content.

    \value PrimaryOrientation The orientation the screen has by nature.
    \value PortraitOrientation Taller than wide, with the top up.
    \value LandscapeOrientation Wider than tall, rotated 90 degrees from portrait.
    \value InvertedPortraitOrientation Portrait turned upside down.
    \value InvertedLandscapeOrientation Landscape turned upside down.

    The ScreenOrientations type is a typedef for \c{QFlags<ScreenOrientation>}.
*/
enum ScreenOrientation {
    PrimaryOrientation = 0x0,
    PortraitOrientation = 0x1,
    LandscapeOrientation = 0x2,
    InvertedPortraitOrientation = 0x4,
    InvertedLandscapeOrientation = 0x8
};
Q_DECLARE_FLAGS(ScreenOrientations, ScreenOrientation)
Q_DECLARE_OPERATORS_FOR_FLAGS(ScreenOrientations)

/*!
    \enum Qt::TouchPointState

    This enum describes what a touch point did.

    \value TouchPointUnknownState Nothing is known about the point.
    \value TouchPointPressed The point went down.
    \value TouchPointMoved The point moved.
    \value TouchPointStationary The point stayed where it was.
    \value TouchPointReleased The point came up.

    The TouchPointStates type is a typedef for \c{QFlags<TouchPointState>}.
*/
enum TouchPointState {
    TouchPointUnknownState = 0x00,
    TouchPointPressed = 0x01,
    TouchPointMoved = 0x02,
    TouchPointStationary = 0x04,
    TouchPointReleased = 0x08
};
Q_DECLARE_FLAGS(TouchPointStates, TouchPointState)
Q_DECLARE_OPERATORS_FOR_FLAGS(TouchPointStates)

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
    \value Desktop The desktop; accepted for compatibility.
    \value WindowType_Mask The bits of the flags that hold the kind of window.
    \value MSWindowsFixedSizeDialogHint Accepted for compatibility; it does nothing.
    \value X11BypassWindowManagerHint A window the window manager does not manage: no frame, no focus handling,
           stacked as the application says. On X11 it takes effect when the window is next shown.
    \value BypassWindowManagerHint The same as X11BypassWindowManagerHint.
    \value FramelessWindowHint A window without a frame or a title bar.
    \value WindowTitleHint A window with a title bar.
    \value WindowSystemMenuHint A window with a window menu.
    \value WindowMinimizeButtonHint A window with a button that minimizes it.
    \value WindowMaximizeButtonHint A window with a button that maximizes it.
    \value WindowMinMaxButtonsHint Both of the above.
    \value WindowContextHelpButtonHint A window with a help button.
    \value WindowStaysOnTopHint A window that is kept above the others.
    \value WindowTransparentForInput A window that input passes through; accepted for compatibility, Nt does not
           apply it yet.
    \value WindowDoesNotAcceptFocus A window that does not take the keyboard focus.
    \value WindowStaysOnBottomHint A window that is kept below the others.
    \value NoDropShadowWindowHint A window without a shadow; accepted for compatibility.
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
    Desktop = 0x00000010 | Window,
    WindowType_Mask = 0x000000ff,
    MSWindowsFixedSizeDialogHint = 0x00000100,
    X11BypassWindowManagerHint = 0x00000400,
    BypassWindowManagerHint = X11BypassWindowManagerHint,
    FramelessWindowHint = 0x00000800,
    WindowTitleHint = 0x00001000,
    WindowSystemMenuHint = 0x00002000,
    WindowMinimizeButtonHint = 0x00004000,
    WindowMaximizeButtonHint = 0x00008000,
    WindowMinMaxButtonsHint = WindowMinimizeButtonHint | WindowMaximizeButtonHint,
    WindowContextHelpButtonHint = 0x00010000,
    WindowStaysOnTopHint = 0x00040000,
    WindowTransparentForInput = 0x00080000,
    WindowDoesNotAcceptFocus = 0x00200000,
    CustomizeWindowHint = 0x02000000,
    WindowStaysOnBottomHint = 0x04000000,
    WindowCloseButtonHint = 0x08000000,
    NoDropShadowWindowHint = 0x40000000
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
    \enum Qt::CursorShape

    The shapes of the mouse cursor, for QCursor.

    \value ArrowCursor The standard arrow.
    \value UpArrowCursor An arrow that points up.
    \value CrossCursor A crosshair, for picking a point.
    \value WaitCursor An hourglass or a watch: the application is busy and takes no input.
    \value IBeamCursor A caret, over text that can be selected and edited.
    \value SizeVerCursor A vertical double arrow, for resizing something vertically.
    \value SizeHorCursor A horizontal double arrow.
    \value SizeBDiagCursor A double arrow from the bottom left to the top right.
    \value SizeFDiagCursor A double arrow from the top left to the bottom right.
    \value SizeAllCursor Arrows in the four directions, for moving something.
    \value BlankCursor No cursor at all.
    \value SplitVCursor For dragging a horizontal splitter up and down.
    \value SplitHCursor For dragging a vertical splitter left and right.
    \value PointingHandCursor A hand with a pointing finger, over a link.
    \value ForbiddenCursor A crossed circle: what is dragged cannot be dropped here.
    \value WhatsThisCursor An arrow with a question mark.
    \value BusyCursor An arrow with an hourglass: the application works, but still takes input.
    \value OpenHandCursor An open hand, over something that can be dragged.
    \value ClosedHandCursor A closed hand, while it is dragged.
    \value DragCopyCursor What is dragged will be copied.
    \value DragMoveCursor What is dragged will be moved.
    \value DragLinkCursor A link to what is dragged will be made.
    \omitvalue LastCursor
    \value BitmapCursor A cursor made of a bitmap. Nt has none.
    \value CustomCursor A cursor of the platform that has no shape of its own here.

    \sa QCursor
*/
enum CursorShape {
    ArrowCursor,
    UpArrowCursor,
    CrossCursor,
    WaitCursor,
    IBeamCursor,
    SizeVerCursor,
    SizeHorCursor,
    SizeBDiagCursor,
    SizeFDiagCursor,
    SizeAllCursor,
    BlankCursor,
    SplitVCursor,
    SplitHCursor,
    PointingHandCursor,
    ForbiddenCursor,
    WhatsThisCursor,
    BusyCursor,
    OpenHandCursor,
    ClosedHandCursor,
    DragCopyCursor,
    DragMoveCursor,
    DragLinkCursor,
    LastCursor = DragLinkCursor,
    BitmapCursor = 24,
    CustomCursor = 25
};

/*!
    \enum Qt::WidgetAttribute

    The attributes of a widget, which QWidget::setAttribute() sets and
    QWidget::testAttribute() reads. The numbers are those of Qt. Nt has the
    attributes listed here; of these, the ones described as kept for the
    application are stored and returned, and change nothing by themselves.

    \value WA_Disabled The widget is disabled; the same as QWidget::setEnabled() with \c false.
    \value WA_UnderMouse The mouse is over the widget. Set by Nt; see QWidget::underMouse().
    \value WA_MouseTracking The widget gets mouse move events while no button is down; see QWidget::setMouseTracking().
    \value WA_OpaquePaintEvent The widget paints all of its pixels itself, so its background is not filled first.
    \value WA_StaticContents Kept for the application.
    \value WA_NoSystemBackground The background of the widget is not filled before it is painted.
    \value WA_UpdatesDisabled The widget is not repainted; see QWidget::setUpdatesEnabled().
    \value WA_InputMethodEnabled Kept for the application; Nt has no input methods.
    \value WA_SetPalette The widget has a palette of its own. Set by QWidget::setPalette().
    \value WA_SetFont The widget has a font of its own. Set by QWidget::setFont().
    \value WA_SetCursor The widget has a cursor of its own. Set by QWidget::setCursor().
    \value WA_WindowModified The window has changes that are not saved; see QWidget::setWindowModified().
    \value WA_Resized Kept for the application.
    \value WA_Moved Kept for the application.
    \value WA_TransparentForMouseEvents The mouse does not find the widget or its children: events go to what is behind them.
    \value WA_DeleteOnClose The widget is deleted when it is closed and accepts the close event.
    \value WA_ShowModal Kept for the application; use QWidget::setWindowModality().
    \value WA_NoMousePropagation A mouse event that the widget does not accept is not passed on to its parent.
    \value WA_Hover The widget is repainted when the mouse enters or leaves it. Nt repaints every widget then, so this changes nothing.
    \value WA_QuitOnClose The window counts as one of those that keep the application running: closing the last of them quits it. On by default for windows.
    \value WA_KeyboardFocusChange Kept for the application.
    \value WA_AcceptDrops Kept for the application; Nt has no drag and drop.
    \value WA_AlwaysShowToolTips Kept for the application: Nt shows tool tips in windows that are not active too.
    \value WA_SetStyle The widget has a style of its own. Set by QWidget::setStyle().
    \value WA_StyledBackground Kept for the application; Nt has no style sheets.
    \value WA_ShowWithoutActivating Kept for the application: whether a window that is shown becomes active is up to the window system.
    \value WA_DontShowOnScreen A window that is shown is visible to the application, with its show events and
           layout, but gets no window on the screen: for rendering it with QWidget::render() or QWidget::grab().
    \value WA_TranslucentBackground Kept for the application; windows of Nt are opaque.
    \value WA_AcceptTouchEvents The widget takes touch events (QTouchEvent) in event(). Touch events that no widget accepts are delivered as mouse events for the first finger.
    \omitvalue WA_AttributeCount

    \sa QWidget::setAttribute(), QWidget::testAttribute()
*/
enum WidgetAttribute {
    WA_Disabled = 0,
    WA_UnderMouse = 1,
    WA_MouseTracking = 2,
    WA_OpaquePaintEvent = 4,
    WA_StaticContents = 5,
    WA_NoSystemBackground = 9,
    WA_UpdatesDisabled = 10,
    WA_InputMethodEnabled = 14,
    WA_SetPalette = 36,
    WA_SetFont = 37,
    WA_SetCursor = 38,
    WA_WindowModified = 41,
    WA_Resized = 42,
    WA_Moved = 43,
    WA_TransparentForMouseEvents = 51,
    WA_DeleteOnClose = 55,
    WA_ShowModal = 70,
    WA_NoMousePropagation = 73,
    WA_Hover = 74,
    WA_QuitOnClose = 76,
    WA_KeyboardFocusChange = 77,
    WA_AcceptDrops = 78,
    WA_AlwaysShowToolTips = 84,
    WA_SetStyle = 86,
    WA_StyledBackground = 93,
    WA_ShowWithoutActivating = 98,
    WA_DontShowOnScreen = 103,
    WA_TranslucentBackground = 120,
    WA_AcceptTouchEvents = 121,
    WA_AttributeCount = 132
};

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
    \enum Qt::ApplicationState

    The state of the application, as QGuiApplication::applicationState() reports it.

    \value ApplicationSuspended The application is about to be suspended; it cannot run code.
    \value ApplicationHidden The application is hidden and runs in the background.
    \value ApplicationInactive The application is visible but not in front; on Android, the activity is paused,
        for example when the user has gone to the home screen or another app is in front.
    \value ApplicationActive The application is visible and in front. On desktop platforms the application is
        always in this state.

    \sa QGuiApplication::applicationState(), QGuiApplication::applicationStateChanged()
*/
enum ApplicationState {
    ApplicationSuspended = 0x00000000,
    ApplicationHidden = 0x00000001,
    ApplicationInactive = 0x00000002,
    ApplicationActive = 0x00000004
};
Q_DECLARE_FLAGS(ApplicationStates, ApplicationState)

/*!
    \enum Qt::CursorMoveStyle
    \since 6.0

    How the arrow keys move the cursor in text that mixes directions.

    \value LogicalMoveStyle The cursor moves to the next character in the text.
    \value VisualMoveStyle The cursor moves to the next character on the screen.
*/
enum CursorMoveStyle { LogicalMoveStyle, VisualMoveStyle };

/*!
    \enum Qt::UIEffect
    \since 6.0

    The animations and fades of the user interface that QApplication::setEffectEnabled() turns on or off.

    \value UI_General All of the effects.
    \value UI_AnimateMenu Menus slide open.
    \value UI_FadeMenu Menus fade in.
    \value UI_AnimateCombo The lists of combo boxes slide open.
    \value UI_AnimateTooltip Tool tips slide open.
    \value UI_FadeTooltip Tool tips fade in.
    \value UI_AnimateToolBox The pages of tool boxes slide.
*/
enum UIEffect {
    UI_General,
    UI_AnimateMenu,
    UI_FadeMenu,
    UI_AnimateCombo,
    UI_AnimateTooltip,
    UI_FadeTooltip,
    UI_AnimateToolBox
};

/*!
    \enum Qt::NavigationMode
    \since 6.0

    How the keys move the focus on a device without a mouse.

    \value NavigationModeNone Only the mouse and the touch screen.
    \value NavigationModeKeypadTabOrder The keypad moves through the tab order.
    \value NavigationModeKeypadDirectional The keypad moves in the direction of its keys.
    \value NavigationModeCursorAuto A mouse cursor is moved with the keys when there is no mouse.
    \value NavigationModeCursorForceVisible A mouse cursor is always moved with the keys.
*/
enum NavigationMode {
    NavigationModeNone,
    NavigationModeKeypadTabOrder,
    NavigationModeKeypadDirectional,
    NavigationModeCursorAuto,
    NavigationModeCursorForceVisible
};

/*!
    \enum Qt::Corner
    \since 6.0

    A corner of a rectangle.

    \value TopLeftCorner The top left corner.
    \value TopRightCorner The top right corner.
    \value BottomLeftCorner The bottom left corner.
    \value BottomRightCorner The bottom right corner.
*/
/*!
    \enum Qt::AnchorPoint

    The edges of an item that a QGraphicsAnchorLayout anchors.

    \value AnchorLeft The left edge.
    \value AnchorHorizontalCenter The vertical line through the middle.
    \value AnchorRight The right edge.
    \value AnchorTop The top edge.
    \value AnchorVerticalCenter The horizontal line through the middle.
    \value AnchorBottom The bottom edge.
*/
enum AnchorPoint { AnchorLeft = 0, AnchorHorizontalCenter, AnchorRight, AnchorTop, AnchorVerticalCenter, AnchorBottom };

enum Corner { TopLeftCorner = 0x00000, TopRightCorner = 0x00001, BottomLeftCorner = 0x00002, BottomRightCorner = 0x00003 };

/*!
    \enum Qt::MatchFlag

    How a text is matched against items, as by QCompleter. The MatchFlags type is a typedef for
    \c{QFlags<MatchFlag>}.

    \value MatchExactly The whole text.
    \value MatchContains The text anywhere in the item.
    \value MatchStartsWith The start of the item.
    \value MatchEndsWith The end of the item.
    \value MatchRegularExpression A regular expression (QRegularExpression).
    \value MatchWildcard A shell wildcard pattern.
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
