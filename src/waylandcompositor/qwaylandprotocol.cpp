// MIT License - see LICENSE
#include "qwaylandprotocol_p.h"

namespace QtWaylandPrivate {

const Interface kCompositorInterface = {
    "wl_compositor", 6, {{"create_surface", "n", "wl_surface"}, {"create_region", "n", "wl_region"}}, {}};

const Interface kSurfaceInterface = {"wl_surface",
                                     6,
                                     {{"destroy", ""},
                                      {"attach", "?oii"},
                                      {"damage", "iiii"},
                                      {"frame", "n", "wl_callback"},
                                      {"set_opaque_region", "?o"},
                                      {"set_input_region", "?o"},
                                      {"commit", ""},
                                      {"set_buffer_transform", "i", nullptr, 2},
                                      {"set_buffer_scale", "i", nullptr, 3},
                                      {"damage_buffer", "iiii", nullptr, 4},
                                      {"offset", "ii", nullptr, 5}},
                                     {{"enter", "o"},
                                      {"leave", "o"},
                                      {"preferred_buffer_scale", "i", nullptr, 6},
                                      {"preferred_buffer_transform", "u", nullptr, 6}}};

const Interface kRegionInterface = {"wl_region", 1, {{"destroy", ""}, {"add", "iiii"}, {"subtract", "iiii"}}, {}};

const Interface kShmInterface = {
    "wl_shm", 2, {{"create_pool", "nhi", "wl_shm_pool"}, {"release", "", nullptr, 2}}, {{"format", "u"}}};

const Interface kShmPoolInterface = {
    "wl_shm_pool", 2, {{"create_buffer", "niiiiu", "wl_buffer"}, {"destroy", ""}, {"resize", "i"}}, {}};

const Interface kBufferInterface = {"wl_buffer", 1, {{"destroy", ""}}, {{"release", ""}}};

const Interface kOutputInterface = {"wl_output",
                                    4,
                                    {{"release", "", nullptr, 3}},
                                    {{"geometry", "iiiiissi"},
                                     {"mode", "uiii"},
                                     {"done", "", nullptr, 2},
                                     {"scale", "i", nullptr, 2},
                                     {"name", "s", nullptr, 4},
                                     {"description", "s", nullptr, 4}}};

const Interface kSeatInterface = {"wl_seat",
                                  9,
                                  {{"get_pointer", "n", "wl_pointer"},
                                   {"get_keyboard", "n", "wl_keyboard"},
                                   {"get_touch", "n", "wl_touch"},
                                   {"release", "", nullptr, 5}},
                                  {{"capabilities", "u"}, {"name", "s", nullptr, 2}}};

const Interface kPointerInterface = {"wl_pointer",
                                     9,
                                     {{"set_cursor", "u?oii"}, {"release", "", nullptr, 3}},
                                     {{"enter", "uoff"},
                                      {"leave", "uo"},
                                      {"motion", "uff"},
                                      {"button", "uuuu"},
                                      {"axis", "uuf"},
                                      {"frame", "", nullptr, 5},
                                      {"axis_source", "u", nullptr, 5},
                                      {"axis_stop", "uu", nullptr, 5},
                                      {"axis_discrete", "ui", nullptr, 5},
                                      {"axis_value120", "ui", nullptr, 8},
                                      {"axis_relative_direction", "uu", nullptr, 9}}};

const Interface kKeyboardInterface = {"wl_keyboard",
                                      9,
                                      {{"release", "", nullptr, 3}},
                                      {{"keymap", "uhu"},
                                       {"enter", "uoa"},
                                       {"leave", "uo"},
                                       {"key", "uuuu"},
                                       {"modifiers", "uuuuu"},
                                       {"repeat_info", "ii", nullptr, 4}}};

const Interface kTouchInterface = {"wl_touch",
                                   9,
                                   {{"release", "", nullptr, 3}},
                                   {{"down", "uuoiff"},
                                    {"up", "uui"},
                                    {"motion", "uiff"},
                                    {"frame", ""},
                                    {"cancel", ""},
                                    {"shape", "iff", nullptr, 6},
                                    {"orientation", "if", nullptr, 6}}};

const Interface kWmBaseInterface = {"xdg_wm_base",
                                    6,
                                    {{"destroy", ""},
                                     {"create_positioner", "n", "xdg_positioner"},
                                     {"get_xdg_surface", "no", "xdg_surface"},
                                     {"pong", "u"}},
                                    {{"ping", "u"}}};

const Interface kPositionerInterface = {"xdg_positioner",
                                        6,
                                        {{"destroy", ""},
                                         {"set_size", "ii"},
                                         {"set_anchor_rect", "iiii"},
                                         {"set_anchor", "u"},
                                         {"set_gravity", "u"},
                                         {"set_constraint_adjustment", "u"},
                                         {"set_offset", "ii"},
                                         {"set_reactive", "", nullptr, 3},
                                         {"set_parent_size", "ii", nullptr, 3},
                                         {"set_parent_configure", "u", nullptr, 3}},
                                        {}};

const Interface kXdgSurfaceInterface = {"xdg_surface",
                                        6,
                                        {{"destroy", ""},
                                         {"get_toplevel", "n", "xdg_toplevel"},
                                         {"get_popup", "n?oo", "xdg_popup"},
                                         {"set_window_geometry", "iiii"},
                                         {"ack_configure", "u"}},
                                        {{"configure", "u"}}};

const Interface kToplevelInterface = {"xdg_toplevel",
                                      6,
                                      {{"destroy", ""},
                                       {"set_parent", "?o"},
                                       {"set_title", "s"},
                                       {"set_app_id", "s"},
                                       {"show_window_menu", "ouii"},
                                       {"move", "ou"},
                                       {"resize", "ouu"},
                                       {"set_max_size", "ii"},
                                       {"set_min_size", "ii"},
                                       {"set_maximized", ""},
                                       {"unset_maximized", ""},
                                       {"set_fullscreen", "?o"},
                                       {"unset_fullscreen", ""},
                                       {"set_minimized", ""}},
                                      {{"configure", "iia"},
                                       {"close", ""},
                                       {"configure_bounds", "ii", nullptr, 4},
                                       {"wm_capabilities", "a", nullptr, 5}}};

const Interface kPopupInterface = {"xdg_popup",
                                   6,
                                   {{"destroy", ""}, {"grab", "ou"}, {"reposition", "ou", nullptr, 3}},
                                   {{"configure", "iiii"}, {"popup_done", ""}, {"repositioned", "u", nullptr, 3}}};

} // namespace QtWaylandPrivate
