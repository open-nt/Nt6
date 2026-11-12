// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtFM/qfmglobal.h>

namespace QtFM {

class PatchData;

class Patch {
public:
    Patch();
    Patch(const Patch &other);
    Patch(Patch &&other) noexcept;
    Patch &operator=(const Patch &other);
    Patch &operator=(Patch &&other) noexcept;
    ~Patch();

    static Patch fromJson(const QByteArray &json, QString *error = nullptr);
    static Patch fromCbor(const QByteArray &cbor, QString *error = nullptr);
    static Patch fromChipInstrument(const QByteArray &data, InstrumentFormat format, QString *error = nullptr);
    static Patch generate(Category category, quint32 seed, ChipType target = ChipType::None);

    QByteArray toJson() const;
    QByteArray toCbor() const;
    QByteArray toChipInstrument(InstrumentFormat format, QString *error = nullptr) const;

    Patch mutated(float amount, quint32 seed) const;
    bool fitsChip(ChipType chip, QStringList *violations = nullptr) const;
    bool isValid() const;

    QString name() const;
    QString category() const;
    ChipType chip() const;
    int operatorCount() const;
    bool isLoop() const;
    float duration() const;

    bool operator==(const Patch &other) const;
    bool operator!=(const Patch &other) const { return !(*this == other); }

    /*! \internal */
    const PatchData *data() const { return d.constData(); }

private:
    friend class PatchBank;
    explicit Patch(PatchData *data);
    QSharedDataPointer<PatchData> d;
};

} // namespace QtFM
