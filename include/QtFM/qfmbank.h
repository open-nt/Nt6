// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtFM/qfmpatch.h>

namespace QtFM {

class PatchBank {
public:
    PatchBank();

    static PatchBank fromJson(const QByteArray &json, QString *error = nullptr);
    static PatchBank fromCbor(const QByteArray &cbor, QString *error = nullptr);
    static PatchBank fromChipBank(const QByteArray &data, InstrumentFormat format, QString *error = nullptr);
    static PatchBank generalMidi(ChipType chip = ChipType::None);

    QByteArray toJson() const;
    QByteArray toCbor() const;
    QByteArray toChipBank(InstrumentFormat format, QString *error = nullptr) const;
    QByteArray toCHeader(const QString &symbol) const;

    QString name() const;
    void setName(const QString &name);

    int size() const;
    bool isEmpty() const;
    QStringList names() const;
    bool contains(const QString &name) const;
    Patch patch(const QString &name) const;
    Patch at(int index) const;
    void insert(const QString &name, const Patch &patch);
    bool remove(const QString &name);

    bool operator==(const PatchBank &other) const;
    bool operator!=(const PatchBank &other) const { return !(*this == other); }

private:
    QString m_name;
    QStringList m_names;
    QList<Patch> m_patches;
};

} // namespace QtFM
