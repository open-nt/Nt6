// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <type_traits>

// The flags of Q_DECLARE_TYPEINFO().
enum {
    Q_COMPLEX_TYPE = 0,
    Q_PRIMITIVE_TYPE = 0x1,
    Q_RELOCATABLE_TYPE = 0x2,
    Q_MOVABLE_TYPE = 0x2,
    Q_DUMMY_TYPE = 0,
};

/*!
    \class QTypeInfo
    \brief The QTypeInfo class describes how a type may be stored by a container.

    The enumerators say whether values of the type \c T need their constructors and destructors
    (\c isComplex), may be moved in memory with \c memcpy() (\c isRelocatable), and what kind of type
    it is (\c isPointer, \c isIntegral). A program states these for a type of its own with
    \c Q_DECLARE_TYPEINFO():

    \code
    struct Point3D { int x, y, z; };
    Q_DECLARE_TYPEINFO(Point3D, Q_PRIMITIVE_TYPE);
    \endcode

    \differences
    The containers of Nt are built on the standard library and do not read QTypeInfo; it exists so that
    code that declares or reads it compiles. There is no \c isValueInitializationBitwiseZero.
    \enddifferences
*/
template <typename T> class QTypeInfo {
public:
    enum {
        isPointer = std::is_pointer_v<T>,
        isIntegral = std::is_integral_v<T>,
        isComplex = !std::is_trivial_v<T>,
        isRelocatable = std::is_trivially_copyable_v<T> && std::is_trivially_destructible_v<T>,
    };
};

#define Q_DECLARE_TYPEINFO_BODY(TYPE, FLAGS)                                                                           \
    class QTypeInfo<TYPE> {                                                                                            \
    public:                                                                                                            \
        enum {                                                                                                         \
            isPointer = std::is_pointer_v<TYPE>,                                                                       \
            isIntegral = std::is_integral_v<TYPE>,                                                                     \
            isComplex = (((FLAGS) & Q_PRIMITIVE_TYPE) == 0) && !std::is_trivial_v<TYPE>,                               \
            isRelocatable = !isComplex || (((FLAGS) & Q_RELOCATABLE_TYPE) != 0) ||                                     \
                            (std::is_trivially_copyable_v<TYPE> && std::is_trivially_destructible_v<TYPE>),            \
        };                                                                                                             \
    }
#define Q_DECLARE_TYPEINFO(TYPE, FLAGS) template <> Q_DECLARE_TYPEINFO_BODY(TYPE, FLAGS)
