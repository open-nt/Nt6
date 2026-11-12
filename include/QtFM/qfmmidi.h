// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtFM/qfmbank.h>
#include <QtFM/qfmengine.h>

namespace QtFM {

class MidiFile {
public:
    struct Note {
        double time = 0;
        double length = 0;
        int channel = 0;
        int program = 0;
        int key = 60;
        int velocity = 100;
        float gain = 0;
        float pan = 0;
        float bend = 0;
    };

    struct Control {
        enum Kind { Level, Pan, Bend };
        double time = 0;
        int channel = 0;
        Kind kind = Level;
        float value = 0;
    };

    MidiFile();

    static MidiFile fromData(const QByteArray &data, QString *error = nullptr);

    bool isValid() const;
    QList<Note> notes() const;
    QList<Control> controls() const;
    double duration() const;
    int format() const;
    int trackCount() const;

private:
    QList<Note> m_notes;
    QList<Control> m_controls;
    double m_duration = 0;
    int m_format = -1;
    int m_tracks = 0;
};

class MidiPlayer : public QObject {
    Q_OBJECT
public:
    explicit MidiPlayer(Engine *engine, QObject *parent = nullptr);
    ~MidiPlayer() override;

    void setFile(const MidiFile &file);
    MidiFile file() const;
    void setInstruments(const PatchBank &bank);
    PatchBank instruments() const;
    void setLoops(bool loops);
    bool loops() const;
    void setGain(float dB);
    float gain() const;
    void setTranspose(int semitones);
    int transpose() const;

    Patch instrument(int channel, int program, int key) const;

    void play();
    void stop();
    bool isPlaying() const;
    double position() const;

    void schedule(quint64 untilFrame);

signals:
    void finished();

private:
    void tick();
    QPointer<Engine> m_engine;
    MidiFile m_file;
    PatchBank m_bank;
    QList<Patch> m_programs;
    QList<Patch> m_drums;
    struct Tracked {
        VoiceHandle voice;
        int channel;
        quint64 start;
        quint64 end;
        float level;
        float bend;
    };
    void startNote(const MidiFile::Note &note, quint64 frame);
    void applyControl(const MidiFile::Control &control, quint64 frame);
    QList<Tracked> m_voices;
    class QTimer *m_timer;
    quint64 m_start = 0;
    quint64 m_origin = 0;
    qsizetype m_next = 0;
    qsizetype m_nextControl = 0;
    float m_gain = 0;
    int m_transpose = 0;
    bool m_loops = false;
    bool m_playing = false;
};

} // namespace QtFM
