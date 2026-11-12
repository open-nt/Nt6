// MIT License - see LICENSE
#pragma once

#include <QtFM/qfmaudiooutput.h>

#include <memory>

namespace QtFM::Private {

// Returns the platform's default sound output, or nullptr if the platform has none. Rendering
// happens on a thread of the output's own, blockSize frames at a time.
std::unique_ptr<AudioOutput> createDefaultAudioOutput(int blockSize);

} // namespace QtFM::Private
