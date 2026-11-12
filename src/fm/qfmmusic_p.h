// MIT License - see LICENSE
#pragma once

// Chip music: a register log parsed into timed writes on the control thread, played by the audio
// thread into a chip core that belongs to the music.

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtFM/qfmchip.h>

#include <memory>
#include <vector>

namespace QtFM::Private {

struct MusicEvent {
    double time;
    quint16 reg;
    quint8 value;
};

struct MusicData {
    std::unique_ptr<ChipCore> core;
    std::vector<MusicEvent> events;
    double duration = 0;
    double loopTime = -1;
    std::size_t loopIndex = 0;
    ChipType chip = ChipType::None;
};

// Parses data in format; imfRate is the tick rate of IMF files. Returns nullptr, with the reason in
// error, if the data cannot be played.
std::unique_ptr<MusicData> parseMusic(const QByteArray &data, MusicFormat format, int imfRate, QString *error);

// Inflates a gzip stream; returns an empty array on error or without zlib.
QByteArray gunzip(const QByteArray &data);

} // namespace QtFM::Private
