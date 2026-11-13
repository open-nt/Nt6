// MIT License - see LICENSE
#include <QtMultimedia/qmediaformat.h>
#include <QtMultimedia/qmediametadata.h>

#include <QtCore/qdatetime.h>

/*!
    \class QMediaMetaData
    \inmodule NtMultimedia
    \brief The QMediaMetaData class holds the metadata of a media file: its title, artist, date and so on.

    QMediaPlayer::metaData() reads it from the file being played, and QMediaRecorder::setMetaData()
    writes it into the file being recorded:

    \code
    QMediaMetaData tags;
    tags.insert(QMediaMetaData::Title, QStringLiteral("Morning briefing"));
    tags.insert(QMediaMetaData::Date, QDate::currentDate());
    recorder.setMetaData(tags);
    \endcode

    \differences
    QMediaMetaData is not a Q_GADGET. WAV files keep only text tags in their \c LIST/INFO chunk, so
    images, the orientation and the resolution are never read or written; Language is a string, not
    a QLocale::Language.
    \enddifferences

    \sa QMediaPlayer, QMediaRecorder
*/

/*!
    \enum QMediaMetaData::Key

    The kinds of metadata. The types of the values are those of Qt.

    \value Title The title, a QString. WAV tag \c INAM.
    \value Author The authors, a QString.
    \value Comment A comment, a QString. WAV tag \c ICMT.
    \value Description A description, a QString. WAV tag \c ISBJ.
    \value Genre The genre, a QString. WAV tag \c IGNR.
    \value Date When the media was made, a QDate (or a QString if it is not a date). WAV tag \c ICRD.
    \value Language The language, a QString. WAV tag \c ILNG.
    \value Publisher The publisher, a QString.
    \value Copyright The copyright, a QString. WAV tag \c ICOP.
    \value Url A URL, a QUrl.
    \value Duration The length in milliseconds, a qint64.
    \value MediaType The kind of media, a QString.
    \value FileFormat The file format, a QMediaFormat::FileFormat.
    \value AudioBitRate The audio bit rate in bits a second, an int.
    \value AudioCodec The audio codec, a QMediaFormat::AudioCodec.
    \value VideoBitRate The video bit rate in bits a second, an int.
    \value VideoCodec The video codec, a QMediaFormat::VideoCodec.
    \value VideoFrameRate The video frame rate, a qreal.
    \value AlbumTitle The album, a QString. WAV tag \c IPRD.
    \value AlbumArtist The artist of the album, a QString.
    \value ContributingArtist The artist, a QString. WAV tag \c IART.
    \value TrackNumber The track number, an int. WAV tag \c ITRK.
    \value Composer The composer, a QString. WAV tag \c IMUS.
    \value LeadPerformer The lead performer, a QString.
    \value ThumbnailImage A small picture, a QImage.
    \value CoverArtImage The cover art, a QImage.
    \value Orientation The rotation of video, in degrees, an int.
    \value Resolution The size of video, a QSize.
    \value HasHdrContent Whether video has HDR content, a bool.
*/

/*!
    \variable QMediaMetaData::NumMetaData

    The number of values of Key.
*/

/*!
    Returns the value of \a k, or an invalid QVariant if there is none.
*/
QVariant QMediaMetaData::value(Key k) const {
    const auto it = m_data.find(k);
    return it == m_data.end() ? QVariant() : it->second;
}

/*!
    Sets the value of \a k to \a value.
*/
void QMediaMetaData::insert(Key k, const QVariant &value) { m_data[k] = value; }

/*!
    \fn void QMediaMetaData::remove(QMediaMetaData::Key k)

    Removes the value of \a k.
*/

/*!
    Returns the keys that have values, in the order of Key.
*/
QList<QMediaMetaData::Key> QMediaMetaData::keys() const {
    QList<Key> list;
    for (const auto &entry : m_data)
        list.append(entry.first);
    return list;
}

/*!
    \fn QVariant &QMediaMetaData::operator[](QMediaMetaData::Key k)

    Returns a reference to the value of \a k, adding an invalid one if there is none.
*/

/*!
    \fn void QMediaMetaData::clear()

    Removes every value.
*/

/*!
    \fn bool QMediaMetaData::isEmpty() const

    Returns \c true if there are no values; otherwise returns \c false.
*/

/*!
    Returns the value of \a k as text for the user: the names of formats and codecs, dates in ISO
    form, and the text of other values.
*/
QString QMediaMetaData::stringValue(Key k) const {
    const QVariant v = value(k);
    if (!v.isValid())
        return QString();
    switch (k) {
    case FileFormat:
        return QMediaFormat::fileFormatName(v.value<QMediaFormat::FileFormat>());
    case AudioCodec:
        return QMediaFormat::audioCodecName(v.value<QMediaFormat::AudioCodec>());
    case VideoCodec:
        return QMediaFormat::videoCodecName(v.value<QMediaFormat::VideoCodec>());
    case Date:
        if (v.typeId() == QMetaType::QDate)
            return v.toDate().toString(Qt::ISODate);
        return v.toString();
    default:
        return v.toString();
    }
}

/*!
    Returns the name of \a k for the user, such as "Title" or "Contributing artist".
*/
QString QMediaMetaData::metaDataKeyToString(Key k) {
    static const char *const names[] = {"Title",
                                        "Author",
                                        "Comment",
                                        "Description",
                                        "Genre",
                                        "Date",
                                        "Language",
                                        "Publisher",
                                        "Copyright",
                                        "Url",
                                        "Duration",
                                        "Media type",
                                        "File format",
                                        "Audio bit rate",
                                        "Audio codec",
                                        "Video bit rate",
                                        "Video codec",
                                        "Video frame rate",
                                        "Album title",
                                        "Album artist",
                                        "Contributing artist",
                                        "Track number",
                                        "Composer",
                                        "Lead performer",
                                        "Thumbnail image",
                                        "Cover art image",
                                        "Orientation",
                                        "Resolution",
                                        "Has HDR content"};
    if (k < 0 || k >= NumMetaData)
        return QString();
    return QString::fromLatin1(names[k]);
}
