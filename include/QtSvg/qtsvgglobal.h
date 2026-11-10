// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

namespace QtSvg {

enum Option {
    NoOption = 0x00,
    Tiny12FeaturesOnly = 0x01,
    AssumeTrustedSource = 0x02,
    DisableSMILAnimations = 0x10,
    DisableCSSAnimations = 0x20,
    DisableAnimations = 0xf0
};
Q_DECLARE_FLAGS(Options, Option)
Q_DECLARE_OPERATORS_FOR_FLAGS(Options)

} // namespace QtSvg
