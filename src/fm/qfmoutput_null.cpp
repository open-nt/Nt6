// MIT License - see LICENSE
//
// Platforms without a default sound output yet: Engine::start() needs an AudioOutput of the
// application's own.

#include "qfmoutput_p.h"

namespace QtFM::Private {

std::unique_ptr<AudioOutput> createDefaultAudioOutput(int blockSize) {
    Q_UNUSED(blockSize);
    return nullptr;
}

} // namespace QtFM::Private
