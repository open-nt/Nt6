// MIT License - see LICENSE
//
// Overview page about Nt FM for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-fm.html
    \title Using Nt FM
    \brief How to synthesise game sound effects with four-operator FM instead of shipping WAV files.

    Nt FM synthesises sound effects in real time: lasers, coins, jumps, hits, explosions, user
    interface clicks, alarms and engine loops. A game ships patches of a few hundred bytes instead
    of sample files, and every play can vary in pitch, length and texture at no cost in assets.
    The module needs only Nt Core.

    \code
    find_package(Nt6 REQUIRED)
    add_executable(game main.cpp)
    target_link_libraries(game PRIVATE Nt::FM)
    \endcode

    \section1 Playing a Sound

    A QtFM::Engine owns the voices. start() opens the default sound output (ALSA on Linux), and
    play() starts a patch:

    \snippet fm/fmplay.cpp engine

    play() returns at once, from any thread. The patch is compiled on the calling thread and handed
    to the audio thread through a lock-free queue; the audio thread never locks or allocates.
    PlayOptions transpose, pan, delay, lengthen or prioritise a play. A delay starts the voice on
    an exact sample, which keeps rhythmic effects and jingles in time:

    \snippet fm/fmplay.cpp generate

    Patch::generate() above makes a random but plausible patch of a category from a seed, in the
    spirit of sfxr. Patch::mutated() makes variations of an existing one.

    \section1 Loops and Live Control

    A patch with \c{"loop": true} sustains until it is stopped. The QtFM::VoiceHandle that play()
    returns changes its pitch, gain, pan, filter or LFO while it plays, smoothed over about 5 ms so
    nothing clicks:

    \snippet fm/fmplay.cpp loop

    \section1 Rendering Offline

    Engine::renderToWav() renders a patch to a WAV file, for tools, tests and pre-baking. With a
    fixed seed the same patch always gives the same file:

    \snippet fm/fmplay.cpp wav

    Without start(), Engine::render() pulls samples, into a mixer of the game's own for example.
    To feed another sound API, subclass QtFM::AudioOutput:

    \snippet fm/fmplay.cpp output

    \section1 The Synthesiser

    Each voice has four operators. An operator is a phase accumulator reading a waveform: sine,
    the three other OPL2 waveforms (half, absolute and quarter sine), square, saw or triangle. It
    runs at the note's frequency times \c ratio, plus \c offset Hz, or at \c fixed Hz for inharmonic
    hits, and its output phase-modulates the operators it feeds:

    \badcode
    y(t) = A(t) sin(2 pi f t + I(t) m(t) + beta y(t - 1))
    \endcode

    where \e A is the operator's envelope and level, \e m the sum of its modulators, \e I the
    modulation index and \e beta the feedback of operator 1. The \c algorithm field picks one of
    the eight routings of the Yamaha OPN and OPM chips; a patch with two operators uses the OPL
    connections instead.

    \table
    \header \li Algorithm \li Routing (operators 1 to 4) \li Carriers
    \row \li 0 \li 1 > 2 > 3 > 4 \li 4
    \row \li 1 \li (1 + 2) > 3 > 4 \li 4
    \row \li 2 \li (1 + (2 > 3)) > 4 \li 4
    \row \li 3 \li ((1 > 2) + 3) > 4 \li 4
    \row \li 4 \li (1 > 2) + (3 > 4) \li 2, 4
    \row \li 5 \li 1 > 2, 1 > 3, 1 > 4 \li 2, 3, 4
    \row \li 6 \li (1 > 2) + 3 + 4 \li 2, 3, 4
    \row \li 7 \li 1 + 2 + 3 + 4 \li all
    \row \li 0 (two operators) \li 1 > 2 \li 2
    \row \li 1 (two operators) \li 1 + 2 \li 1, 2
    \endtable

    Every operator has a DAHDSR envelope: delay, attack, hold, decay, sustain level and release,
    each up to 10 seconds, with a \c curve that makes the segments exponential. A patch that is
    neither a loop nor given a duration is a one-shot: its envelopes run through release without
    a note-off.

    After the operators come a noise source (white noise, or the 7- and 15-bit shift registers of
    the NES and SN76489), mixed in or used as a modulator; a state-variable low-, high- or
    band-pass filter with a cutoff sweep; a bit crusher; gain and pan. One LFO per voice moves
    pitch, amplitude, modulation index and cutoff. The mix goes through a soft-clip limiter.

    \section1 The Patch Format

    A patch is a JSON object. Unknown keys are ignored, missing keys take their defaults and
    out-of-range values are clamped. Patch::toJson() writes every field; a patch written by hand
    names only what it changes:

    \snippet fm/fmplay.cpp patch

    \table
    \header \li Key \li Meaning
    \row \li \c format, \c version \li \c "qtfm-patch" and 1. Both are optional; another value makes the document invalid.
    \row \li \c name, \c category \li Free text.
    \row \li \c chip \li \c null, or \c "opl2", \c "opl3", \c "opll", \c "opn2" or \c "opm": the chip the patch was made for.
    \row \li \c algorithm \li 0 to 7, or 0 and 1 with two operators.
    \row \li \c feedback \li Feedback of operator 1, 0 to 7, as on the OPL and OPN chips.
    \row \li \c operators \li Two or four objects with \c ratio (0.5 to 32), \c offset (Hz), \c fixed (Hz),
         \c level (0 to 1), \c wave (\c sine, \c half-sine, \c abs-sine, \c quarter-sine, \c square,
         \c saw, \c triangle), \c keyScaleRate (0 to 1), \c keyScaleLevel (dB per octave above A4) and
         \c env.
    \row \li \c env \li \c delay, \c a, \c h, \c d (seconds), \c s (level, 0 to 1), \c r (seconds) and \c curve
         (0 is linear, negative is exponential; -3 by default).
    \row \li \c pitch \li \c base (a MIDI note or a name such as \c "A5"; A4 is 69), \c env (\c start, \c end
         in semitones, \c time, \c curve), \c slide (semitones a second), \c deltaSlide (semitones a
         second, a second) and \c arp (\c steps of up to four semitone offsets, \c rate in steps a
         second, \c repeat).
    \row \li \c lfo \li \c wave (\c sine, \c triangle, \c square, \c saw, \c sample-hold), \c rate (Hz), and the
         depths \c pitch (semitones), \c amp (0 to 1), \c index (-1 to 1) and \c cutoff (octaves).
    \row \li \c index \li An envelope of the modulation index: \c start, \c end, \c time, \c curve; 1 throughout by
         default.
    \row \li \c noise \li \c mode (\c white, \c lfsr7, \c lfsr15), \c level, \c rate (Hz of its clock; 0 is every
         sample), \c modulate (0 to mix it in, or the number of the operator it modulates) and \c env.
    \row \li \c filter \li \c type (\c none, \c lowpass, \c highpass, \c bandpass), \c cutoff (Hz), \c resonance
         (0 to 1) and \c sweep (\c end in Hz, \c time, \c curve).
    \row \li \c crush \li \c bits (1 to 16) and \c rate (Hz of the sample-rate reduction; 0 is off).
    \row \li \c gain, \c pan \li Decibels, and -1 to 1.
    \row \li \c loop, \c duration \li Whether the patch sustains until stopped, and its length in seconds before
         release (-1: none).
    \row \li \c random \li Variance per play, keyed by the path of a field: \c pitch.base, \c pitch.slide,
         \c pitch.env.start, \c operators.1.ratio (operators count from 0), \c operators.0.env.d,
         \c filter.cutoff, \c noise.level, \c gain and so on. Each play adds a uniform random amount of
         up to the variance, drawn from a seeded xorshift32 generator.
    \endtable

    Patch::toCbor() and Patch::fromCbor() store the same fields as CBOR, which is smaller.

    \section1 AdLib and Yamaha Chips

    Patch::fromChipInstrument() imports an AdLib instrument in the SBI format into a native
    two-operator patch, keeping its multipliers, waveforms, levels, feedback, connection and
    envelopes. Patch::generate() with a chip target keeps to that chip's limits, and
    Patch::fitsChip() lists the fields that a patch would have to change to fit one.

    Nt FM does not emulate the chips yet: chip patches play on the native engine, close to the
    original but without the chips' quantisation. Emulation cores, register-level access and
    playback of VGM, IMF, DRO and RAD music are planned.

    \section1 Limits

    \list
    \li The native engine only: no chip emulation, music player, sequences or QML types yet.
    \li Floating point only; the \c qint16 output is converted from it. There is no fixed-point
        path for microcontrollers, and Nt FM is not built for Zephyr.
    \li No oversampling: square and saw waves, and high feedback, alias at high pitches.
    \li The default output is ALSA on Linux. Elsewhere, pass an AudioOutput to Engine::start().
    \endlist
*/
