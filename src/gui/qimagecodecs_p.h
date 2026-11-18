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

// The format of the image in `data`, from its first bytes: "png", "jpeg" or "bmp", or null.
const char *formatOf(const uchar *data, std::size_t size);
// The image in `data` in whatever supported format it is, or a null image.
QImage decodeAny(const uchar *data, std::size_t size);
// The writer's name for the format named `name` ("png", "bmp", "jpg" or "jpeg", in any case): "png", "bmp" or
// "jpeg", or null if there is no writer for it in this build.
const char *writableFormat(const QByteArray &name);
// Writes `image` in the format named `name` to `out`; any QImage format is converted first. Returns false if the image
// is null, the format has no writer or the writer fails.
bool encode(const QImage &image, const QByteArray &name, int quality, QByteArray *out);

QImage readPng(const uchar *data, std::size_t size);
bool writePng(const QImage &image, QByteArray *out, int quality);

QImage readBmp(const uchar *data, std::size_t size);
bool writeBmp(const QImage &image, QByteArray *out);

QImage readJpeg(const uchar *data, std::size_t size);
bool writeJpeg(const QImage &image, QByteArray *out, int quality);

} // namespace NtImageCodecs
