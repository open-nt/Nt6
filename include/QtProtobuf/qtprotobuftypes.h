// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>

#include <cstdint>

namespace QtProtobuf {

using int32 = std::int32_t;
using int64 = std::int64_t;
using uint32 = std::uint32_t;
using uint64 = std::uint64_t;
using sint32 = std::int32_t;
using sint64 = std::int64_t;
using fixed32 = std::uint32_t;
using fixed64 = std::uint64_t;
using sfixed32 = std::int32_t;
using sfixed64 = std::int64_t;
using boolean = bool;

using int32List = QList<int32>;
using int64List = QList<int64>;
using uint32List = QList<uint32>;
using uint64List = QList<uint64>;
using sint32List = QList<sint32>;
using sint64List = QList<sint64>;
using fixed32List = QList<fixed32>;
using fixed64List = QList<fixed64>;
using sfixed32List = QList<sfixed32>;
using sfixed64List = QList<sfixed64>;
using boolList = QList<bool>;
using floatList = QList<float>;
using doubleList = QList<double>;

enum class WireTypes {
    Unknown = -1,
    Varint = 0,
    Fixed64 = 1,
    LengthDelimited = 2,
    StartGroup = 3,
    EndGroup = 4,
    Fixed32 = 5
};

constexpr int InvalidFieldNumber = 0;

void qRegisterProtobufTypes();

} // namespace QtProtobuf
