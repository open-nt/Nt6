// MIT License - see LICENSE
//
// The interfaces of the core protocol and of xdg-shell that the compositor implements, as the protocol XML
// defines them: the requests a client may send (with the interface a new_id creates) and the events the
// compositor sends, in opcode order. The signatures use the characters of qwaylandwire_p.h.
#pragma once

#include "qwaylandwire_p.h"

namespace QtWaylandPrivate {

// wl_compositor
constexpr std::uint32_t kCompositorCreateSurface = 0;
constexpr std::uint32_t kCompositorCreateRegion = 1;
// wl_surface requests
constexpr std::uint32_t kSurfaceDestroy = 0;
constexpr std::uint32_t kSurfaceAttach = 1;
constexpr std::uint32_t kSurfaceDamage = 2;
constexpr std::uint32_t kSurfaceFrame = 3;
constexpr std::uint32_t kSurfaceSetOpaqueRegion = 4;
constexpr std::uint32_t kSurfaceSetInputRegion = 5;
constexpr std::uint32_t kSurfaceCommit = 6;
constexpr std::uint32_t kSurfaceSetBufferTransform = 7;
constexpr std::uint32_t kSurfaceSetBufferScale = 8;
constexpr std::uint32_t kSurfaceDamageBuffer = 9;
constexpr std::uint32_t kSurfaceOffset = 10;
// wl_surface events
constexpr std::uint32_t kSurfaceEnter = 0;
constexpr std::uint32_t kSurfaceLeave = 1;
constexpr std::uint32_t kSurfacePreferredBufferScale = 2;
// wl_region requests
constexpr std::uint32_t kRegionDestroy = 0;
constexpr std::uint32_t kRegionAdd = 1;
constexpr std::uint32_t kRegionSubtract = 2;
// wl_shm
constexpr std::uint32_t kShmCreatePool = 0;
constexpr std::uint32_t kShmRelease = 1;
constexpr std::uint32_t kShmFormatEvent = 0;
constexpr std::uint32_t kShmFormatArgb8888 = 0;
constexpr std::uint32_t kShmFormatXrgb8888 = 1;
constexpr std::uint32_t kShmErrorInvalidFormat = 0;
constexpr std::uint32_t kShmErrorInvalidStride = 1;
constexpr std::uint32_t kShmErrorInvalidFd = 2;
// wl_shm_pool
constexpr std::uint32_t kShmPoolCreateBuffer = 0;
constexpr std::uint32_t kShmPoolDestroy = 1;
constexpr std::uint32_t kShmPoolResize = 2;
// wl_buffer
constexpr std::uint32_t kBufferDestroy = 0;
constexpr std::uint32_t kBufferRelease = 0;
// wl_output
constexpr std::uint32_t kOutputRelease = 0;
constexpr std::uint32_t kOutputGeometry = 0;
constexpr std::uint32_t kOutputMode = 1;
constexpr std::uint32_t kOutputDone = 2;
constexpr std::uint32_t kOutputScale = 3;
constexpr std::uint32_t kOutputName = 4;
constexpr std::uint32_t kOutputDescription = 5;
constexpr std::uint32_t kOutputModeCurrent = 1;
constexpr std::uint32_t kOutputModePreferred = 2;
// wl_seat
constexpr std::uint32_t kSeatGetPointer = 0;
constexpr std::uint32_t kSeatGetKeyboard = 1;
constexpr std::uint32_t kSeatGetTouch = 2;
constexpr std::uint32_t kSeatRelease = 3;
constexpr std::uint32_t kSeatCapabilities = 0;
constexpr std::uint32_t kSeatName = 1;
// wl_pointer
constexpr std::uint32_t kPointerSetCursor = 0;
constexpr std::uint32_t kPointerRelease = 1;
constexpr std::uint32_t kPointerEnter = 0;
constexpr std::uint32_t kPointerLeave = 1;
constexpr std::uint32_t kPointerMotion = 2;
constexpr std::uint32_t kPointerButton = 3;
constexpr std::uint32_t kPointerAxis = 4;
constexpr std::uint32_t kPointerFrame = 5;
constexpr std::uint32_t kPointerAxisSource = 6;
constexpr std::uint32_t kPointerAxisStop = 7;
constexpr std::uint32_t kPointerAxisDiscrete = 8;
constexpr std::uint32_t kPointerAxisValue120 = 9;
// wl_keyboard
constexpr std::uint32_t kKeyboardRelease = 0;
constexpr std::uint32_t kKeyboardKeymap = 0;
constexpr std::uint32_t kKeyboardEnter = 1;
constexpr std::uint32_t kKeyboardLeave = 2;
constexpr std::uint32_t kKeyboardKey = 3;
constexpr std::uint32_t kKeyboardModifiers = 4;
constexpr std::uint32_t kKeyboardRepeatInfo = 5;
constexpr std::uint32_t kKeymapFormatNone = 0;
constexpr std::uint32_t kKeymapFormatXkbV1 = 1;
// wl_touch
constexpr std::uint32_t kTouchRelease = 0;
constexpr std::uint32_t kTouchDown = 0;
constexpr std::uint32_t kTouchUp = 1;
constexpr std::uint32_t kTouchMotion = 2;
constexpr std::uint32_t kTouchFrame = 3;
constexpr std::uint32_t kTouchCancel = 4;
// xdg_wm_base
constexpr std::uint32_t kWmBaseDestroy = 0;
constexpr std::uint32_t kWmBaseCreatePositioner = 1;
constexpr std::uint32_t kWmBaseGetXdgSurface = 2;
constexpr std::uint32_t kWmBasePong = 3;
constexpr std::uint32_t kWmBasePing = 0;
constexpr std::uint32_t kWmBaseErrorRole = 0;
constexpr std::uint32_t kWmBaseErrorDefunctSurfaces = 1;
constexpr std::uint32_t kWmBaseErrorInvalidPositioner = 5;
// xdg_positioner
constexpr std::uint32_t kPositionerDestroy = 0;
constexpr std::uint32_t kPositionerSetSize = 1;
constexpr std::uint32_t kPositionerSetAnchorRect = 2;
constexpr std::uint32_t kPositionerSetAnchor = 3;
constexpr std::uint32_t kPositionerSetGravity = 4;
constexpr std::uint32_t kPositionerSetConstraintAdjustment = 5;
constexpr std::uint32_t kPositionerSetOffset = 6;
constexpr std::uint32_t kPositionerSetReactive = 7;
constexpr std::uint32_t kPositionerSetParentSize = 8;
constexpr std::uint32_t kPositionerSetParentConfigure = 9;
// xdg_surface
constexpr std::uint32_t kXdgSurfaceDestroy = 0;
constexpr std::uint32_t kXdgSurfaceGetToplevel = 1;
constexpr std::uint32_t kXdgSurfaceGetPopup = 2;
constexpr std::uint32_t kXdgSurfaceSetWindowGeometry = 3;
constexpr std::uint32_t kXdgSurfaceAckConfigure = 4;
constexpr std::uint32_t kXdgSurfaceConfigure = 0;
// xdg_toplevel
constexpr std::uint32_t kToplevelDestroy = 0;
constexpr std::uint32_t kToplevelSetParent = 1;
constexpr std::uint32_t kToplevelSetTitle = 2;
constexpr std::uint32_t kToplevelSetAppId = 3;
constexpr std::uint32_t kToplevelShowWindowMenu = 4;
constexpr std::uint32_t kToplevelMove = 5;
constexpr std::uint32_t kToplevelResize = 6;
constexpr std::uint32_t kToplevelSetMaxSize = 7;
constexpr std::uint32_t kToplevelSetMinSize = 8;
constexpr std::uint32_t kToplevelSetMaximized = 9;
constexpr std::uint32_t kToplevelUnsetMaximized = 10;
constexpr std::uint32_t kToplevelSetFullscreen = 11;
constexpr std::uint32_t kToplevelUnsetFullscreen = 12;
constexpr std::uint32_t kToplevelSetMinimized = 13;
constexpr std::uint32_t kToplevelConfigure = 0;
constexpr std::uint32_t kToplevelClose = 1;
constexpr std::uint32_t kToplevelConfigureBounds = 2;
constexpr std::uint32_t kToplevelWmCapabilities = 3;
// xdg_popup
constexpr std::uint32_t kPopupDestroy = 0;
constexpr std::uint32_t kPopupGrab = 1;
constexpr std::uint32_t kPopupReposition = 2;
constexpr std::uint32_t kPopupConfigure = 0;
constexpr std::uint32_t kPopupDone = 1;
constexpr std::uint32_t kPopupRepositioned = 2;

extern const Interface kCompositorInterface;
extern const Interface kSurfaceInterface;
extern const Interface kRegionInterface;
extern const Interface kShmInterface;
extern const Interface kShmPoolInterface;
extern const Interface kBufferInterface;
extern const Interface kOutputInterface;
extern const Interface kSeatInterface;
extern const Interface kPointerInterface;
extern const Interface kKeyboardInterface;
extern const Interface kTouchInterface;
extern const Interface kWmBaseInterface;
extern const Interface kPositionerInterface;
extern const Interface kXdgSurfaceInterface;
extern const Interface kToplevelInterface;
extern const Interface kPopupInterface;

} // namespace QtWaylandPrivate
