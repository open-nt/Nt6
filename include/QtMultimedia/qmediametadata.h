// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qvariant.h>

#include <map>

class QMediaMetaData {
public:
    enum Key {
        Title,
        Author,
        Comment,
        Description,
        Genre,
        Date,
        Language,
        Publisher,
        Copyright,
        Url,
        Duration,
        MediaType,
        FileFormat,
        AudioBitRate,
        AudioCodec,
        VideoBitRate,
        VideoCodec,
        VideoFrameRate,
        AlbumTitle,
        AlbumArtist,
        ContributingArtist,
        TrackNumber,
        Composer,
        LeadPerformer,
        ThumbnailImage,
        CoverArtImage,
        Orientation,
        Resolution,
        HasHdrContent
    };
    static constexpr int NumMetaData = HasHdrContent + 1;

    QVariant value(Key k) const;
    void insert(Key k, const QVariant &value);
    void remove(Key k) { m_data.erase(k); }
    QList<Key> keys() const;

    QVariant &operator[](Key k) { return m_data[k]; }
    void clear() { m_data.clear(); }

    bool isEmpty() const { return m_data.empty(); }
    QString stringValue(Key k) const;

    static QString metaDataKeyToString(Key k);

    friend bool operator==(const QMediaMetaData &a, const QMediaMetaData &b) { return a.m_data == b.m_data; }
    friend bool operator!=(const QMediaMetaData &a, const QMediaMetaData &b) { return !(a == b); }

private:
    std::map<Key, QVariant> m_data;
};
