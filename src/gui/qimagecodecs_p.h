// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtGui/qimage.h>

#include <cstddef>

// The image file formats behind QImage::load() and QImage::save(). Each reader returns a null image
// if the data is not a valid file of its format; each writer returns false if it cannot write.
namespace NtImageCodecs {

// The largest image that is read: its pixels take 256 MiB.
constexpr std::size_t kMaxPixels = (256u << 20) / 4;

QImage readPng(const uchar *data, std::size_t size);
bool writePng(const QImage &image, QByteArray *out, int quality);

QImage readBmp(const uchar *data, std::size_t size);
bool writeBmp(const QImage &image, QByteArray *out);

QImage readJpeg(const uchar *data, std::size_t size);
bool writeJpeg(const QImage &image, QByteArray *out, int quality);

} // namespace NtImageCodecs
