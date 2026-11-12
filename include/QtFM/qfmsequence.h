// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtFM/qfmpatch.h>

namespace QtFM {

class Sequence {
public:
    struct Note {
        float beat = 0;
        Patch patch;
        float pitch = 0;
        float length = -1;
        float gain = 0;
    };

    Sequence();

    void setTempo(float beatsPerMinute);
    float tempo() const;

    void addNote(float beat, const Patch &patch, float pitch = 0, float length = -1, float gain = 0);
    void clear();
    QList<Note> notes() const;
    int noteCount() const;
    float duration() const;
    double secondsAt(float beat) const;

private:
    QList<Note> m_notes;
    float m_tempo = 120;
};

} // namespace QtFM
