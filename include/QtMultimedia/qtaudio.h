// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

namespace QtAudio {

enum Error { NoError, OpenError, IOError, UnderrunError, FatalError };
enum State { ActiveState, SuspendedState, StoppedState, IdleState };
enum VolumeScale { LinearVolumeScale, CubicVolumeScale, LogarithmicVolumeScale, DecibelVolumeScale };

float convertVolume(float volume, VolumeScale from, VolumeScale to);

} // namespace QtAudio

namespace QAudio = QtAudio;
