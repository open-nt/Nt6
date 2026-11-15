// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>

/*!
    \class QIODeviceBase
    \brief The QIODeviceBase class provides the open mode flags shared by I/O devices.

    QIODeviceBase is the base class of QIODevice. It holds the OpenModeFlag
    enum and the OpenMode flags type, so that they can be named through any
    device class, for example as \c QFile::ReadOnly.

    \differences
    QIODeviceBase has no other members.
    \enddifferences

    \sa QIODevice
*/
class QIODeviceBase {
public:
    /*!
        \enum QIODeviceBase::OpenModeFlag

        Describes how a device is opened. The OpenMode type is a typedef for
        \c{QFlags<OpenModeFlag>}.

        \value NotOpen The device is not open.
        \value ReadOnly The device is open for reading.
        \value WriteOnly The device is open for writing. Note that this mode
               implies \c Truncate for files unless \c Append or \c ReadOnly
               is also given.
        \value ReadWrite The device is open for reading and writing.
        \value Append The device is opened in append mode, so that all data is
               written to the end of the file. Implies \c WriteOnly for files.
        \value Truncate If possible, the device is truncated before it is
               opened. All earlier contents are lost. Implies \c WriteOnly for
               files.
        \value Text Marks the device as a text device. QFile does not
               translate line endings.
        \value Unbuffered Accepted for compatibility; devices in Nt do not
               change their behavior.
        \value NewOnly Fail if the file to be opened already exists. Create
               and open the file only if it does not exist.
        \value ExistingOnly Fail if the file to be opened does not exist.
    */
    enum OpenModeFlag {
        NotOpen = 0x0000,
        ReadOnly = 0x0001,
        WriteOnly = 0x0002,
        ReadWrite = ReadOnly | WriteOnly,
        Append = 0x0004,
        Truncate = 0x0008,
        Text = 0x0010,
        Unbuffered = 0x0020,
        NewOnly = 0x0040,
        ExistingOnly = 0x0080
    };
    Q_DECLARE_FLAGS(OpenMode, OpenModeFlag)
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QIODeviceBase::OpenMode)
