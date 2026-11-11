// MIT License - see LICENSE
#include "qimagecodecs_p.h"

#ifdef NT_HAVE_JPEG

#include <algorithm>
#include <csetjmp>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include <jpeglib.h>

namespace NtImageCodecs {

namespace {

// libjpeg reports errors by calling error_exit, which must not return. It jumps back with longjmp, so the
// code between setjmp and the jump holds no object with a destructor.
struct ErrorManager {
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void onError(j_common_ptr cinfo) {
    std::longjmp(reinterpret_cast<ErrorManager *>(cinfo->err)->jump, 1);
}

void onMessage(j_common_ptr, int) {}

bool decode(const uchar *data, std::size_t size, QImage *result) {
    jpeg_decompress_struct cinfo;
    ErrorManager error;
    cinfo.err = jpeg_std_error(&error.base);
    error.base.error_exit = onError;
    error.base.emit_message = onMessage;
    if (setjmp(error.jump)) { // NOLINT(cert-err52-cpp): libjpeg reports errors this way
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, data, static_cast<unsigned long>(size));
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    const bool cmyk = cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK;
    cinfo.out_color_space = cmyk ? JCS_CMYK : JCS_RGB;
    if (cinfo.image_width == 0 || cinfo.image_height == 0 || cinfo.image_width > 0x7fffffffu ||
        cinfo.image_height > 0x7fffffffu || cinfo.image_width > kMaxPixels / cinfo.image_height) {
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    jpeg_start_decompress(&cinfo);
    const int components = cinfo.output_components;
    *result = QImage(static_cast<int>(cinfo.output_width), static_cast<int>(cinfo.output_height), QImage::Format_RGB32);
    if (result->isNull() || (components != 3 && components != 4)) {
        *result = QImage();
        jpeg_abort_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    // Adobe files store CMYK inverted.
    const bool inverted = cinfo.saw_Adobe_marker != 0;
    // The row buffer belongs to libjpeg's memory manager, which frees it with the decompressor.
    JSAMPARRAY buffer = (*cinfo.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE,
                                                   cinfo.output_width * static_cast<JDIMENSION>(components), 1);
    JSAMPROW rowPointer[1] = {buffer[0]};
    while (cinfo.output_scanline < cinfo.output_height) {
        const int y = static_cast<int>(cinfo.output_scanline);
        QRgb *line = reinterpret_cast<QRgb *>(result->scanLine(y));
        jpeg_read_scanlines(&cinfo, rowPointer, 1);
        const uchar *src = buffer[0];
        for (JDIMENSION x = 0; x < cinfo.output_width; ++x) {
            if (components == 3) {
                line[x] = qRgb(src[x * 3], src[x * 3 + 1], src[x * 3 + 2]);
            } else {
                int c = src[x * 4];
                int m = src[x * 4 + 1];
                int ye = src[x * 4 + 2];
                int k = src[x * 4 + 3];
                if (!inverted) {
                    c = 255 - c;
                    m = 255 - m;
                    ye = 255 - ye;
                    k = 255 - k;
                }
                line[x] = qRgb(c * k / 255, m * k / 255, ye * k / 255);
            }
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return true;
}

bool encode(const QImage &image, int quality, unsigned char **buffer, unsigned long *length) {
    jpeg_compress_struct cinfo;
    ErrorManager error;
    cinfo.err = jpeg_std_error(&error.base);
    error.base.error_exit = onError;
    error.base.emit_message = onMessage;
    if (setjmp(error.jump)) { // NOLINT(cert-err52-cpp): libjpeg reports errors this way
        jpeg_destroy_compress(&cinfo);
        return false;
    }
    jpeg_create_compress(&cinfo);
    jpeg_mem_dest(&cinfo, buffer, length);
    cinfo.image_width = static_cast<JDIMENSION>(image.width());
    cinfo.image_height = static_cast<JDIMENSION>(image.height());
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    JSAMPARRAY row = (*cinfo.mem->alloc_sarray)(reinterpret_cast<j_common_ptr>(&cinfo), JPOOL_IMAGE,
                                                cinfo.image_width * 3, 1);
    while (cinfo.next_scanline < cinfo.image_height) {
        const QRgb *line = reinterpret_cast<const QRgb *>(image.constScanLine(static_cast<int>(cinfo.next_scanline)));
        for (JDIMENSION x = 0; x < cinfo.image_width; ++x) {
            QRgb px = line[x];
            if (image.format() == QImage::Format_ARGB32_Premultiplied)
                px = qUnpremultiply(px);
            row[0][x * 3] = static_cast<JSAMPLE>(qRed(px));
            row[0][x * 3 + 1] = static_cast<JSAMPLE>(qGreen(px));
            row[0][x * 3 + 2] = static_cast<JSAMPLE>(qBlue(px));
        }
        jpeg_write_scanlines(&cinfo, row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    return true;
}

} // namespace

QImage readJpeg(const uchar *data, std::size_t size) {
    QImage image;
    if (!decode(data, size, &image))
        return QImage();
    return image;
}

bool writeJpeg(const QImage &image, QByteArray *out, int quality) {
    if (image.isNull())
        return false;
    unsigned char *buffer = nullptr;
    unsigned long length = 0;
    const bool ok = encode(image, quality < 0 ? 75 : std::min(quality, 100), &buffer, &length);
    if (ok)
        *out = QByteArray(reinterpret_cast<const char *>(buffer), static_cast<qsizetype>(length));
    std::free(buffer);
    return ok;
}

} // namespace NtImageCodecs

#else // NT_HAVE_JPEG

namespace NtImageCodecs {

QImage readJpeg(const uchar *, std::size_t) { return QImage(); }
bool writeJpeg(const QImage &, QByteArray *, int) { return false; }

} // namespace NtImageCodecs

#endif
