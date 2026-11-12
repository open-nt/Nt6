// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtFM/qfmglobal.h>

#include <memory>

namespace QtFM {

namespace Private {
struct EngineCore;
} // namespace Private

class ChipCore {
public:
    virtual ~ChipCore() = default;
    virtual ChipType type() const = 0;
    virtual int nativeRate() const = 0;
    virtual void reset() = 0;
    virtual void write(quint16 reg, quint8 value) = 0;
    virtual void generate(qint32 *stereo, int frames) = 0;

    static std::unique_ptr<ChipCore> create(ChipType type);
};

class Chip {
public:
    Chip(const Chip &) = delete;
    Chip &operator=(const Chip &) = delete;

    ChipType type() const;
    void writeRegister(quint16 reg, quint8 value, float delay = 0);
    void reset();
    void setGain(float dB);

private:
    friend class Engine;
    Chip(Private::EngineCore *core, int index, ChipType type);
    Private::EngineCore *m_core;
    int m_index;
    ChipType m_type;
};

class MusicPlayer {
public:
    MusicPlayer(const MusicPlayer &) = delete;
    MusicPlayer &operator=(const MusicPlayer &) = delete;

    bool load(const QByteArray &data, MusicFormat format = MusicFormat::Auto, QString *error = nullptr);
    void play();
    void pause();
    void stop();
    void setLoops(bool loops);
    bool loops() const;
    void setGain(float dB);
    bool isLoaded() const;
    bool isPlaying() const;
    double position() const;
    double duration() const;
    ChipType chip() const;

    void setImfRate(int ticksPerSecond);
    int imfRate() const;

private:
    friend class Engine;
    explicit MusicPlayer(Private::EngineCore *core);
    Private::EngineCore *m_core;
    double m_duration = 0;
    float m_gain = 1;
    int m_imfRate = 560;
    bool m_loops = true;
    ChipType m_chip = ChipType::None;
};

} // namespace QtFM
