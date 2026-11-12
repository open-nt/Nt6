// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

namespace QtFM {

enum class SampleFormat { Float32, Q15 };

enum class ChipType { None, OPL2, OPL3, OPLL, OPN2, OPM };

enum class Category { Pickup, Laser, Explosion, PowerUp, Hit, Jump, Blip, Alarm, Engine };

enum class InstrumentFormat { Sbi };

enum class Waveform { Sine, HalfSine, AbsSine, QuarterSine, Square, Saw, Triangle };

enum class ParamId { Pitch, Gain, Pan, FilterCutoff, FilterResonance, LfoRate, ModulationIndex };

} // namespace QtFM
