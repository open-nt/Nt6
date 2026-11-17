// MIT License - see LICENSE
#pragma once

#include <QtCore/qfileinfo.h>
#include <QtCore/qflags.h>
#include <QtCore/qstring.h>
#include <QtGui/qicon.h>

class QAbstractFileIconProvider {
public:
    enum IconType { Computer, Desktop, Trashcan, Network, Drive, Folder, File };
    enum Option { DontUseCustomDirectoryIcons = 0x00000001 };
    Q_DECLARE_FLAGS(Options, Option)

    QAbstractFileIconProvider();
    virtual ~QAbstractFileIconProvider();
    QAbstractFileIconProvider(const QAbstractFileIconProvider &) = delete;
    QAbstractFileIconProvider &operator=(const QAbstractFileIconProvider &) = delete;

    virtual QIcon icon(IconType type) const;
    virtual QIcon icon(const QFileInfo &info) const;
    virtual QString type(const QFileInfo &info) const;

    virtual void setOptions(Options options) { m_options = options; }
    virtual Options options() const { return m_options; }

private:
    Options m_options;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QAbstractFileIconProvider::Options)
