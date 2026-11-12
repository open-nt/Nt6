// MIT License - see LICENSE
// The MIME types that QMimeDatabase knows where there is no shared-mime-info database: the common types of
// files and of the web, with the patterns of their names and the bytes that they start with.
#include <QtCore/qglobal.h>

#include "qmimedatabase_p.h"

#include <functional>

namespace QtPrivate {

namespace {

struct BuiltInType {
    const char *name;
    const char *comment;
    const char *globs;   // separated by spaces
    const char *parents; // separated by spaces
    const char *aliases; // separated by spaces
    const char *genericIcon;
};

struct BuiltInMagic {
    const char *type;
    int priority;
    qsizetype offset;
    const char *bytes;
    qsizetype length;
};

const BuiltInType kTypes[] = {
    {"application/octet-stream", "unknown", "*.bin", "", "", "application-x-executable"},
    {"application/x-zerosize", "empty document", "", "", "", ""},
    {"inode/directory", "folder", "", "", "", "folder"},
    {"text/plain", "plain text document", "*.txt *.asc *,v", "", "", "text-x-generic"},
    {"text/html", "HTML document", "*.html *.htm", "text/plain", "", "text-html"},
    {"text/css", "CSS stylesheet", "*.css", "text/plain", "", "text-x-generic"},
    {"text/csv", "CSV document", "*.csv", "text/plain", "text/x-comma-separated-values text/x-csv",
     "x-office-spreadsheet"},
    {"text/markdown", "Markdown document", "*.md *.mkd *.markdown", "text/plain", "text/x-markdown", "text-x-generic"},
    {"text/javascript", "JavaScript program", "*.js *.mjs *.jsm", "application/ecmascript",
     "application/javascript application/x-javascript", "text-x-script"},
    {"text/xml", "XML document", "", "text/plain", "", "text-x-generic"},
    {"text/x-c++src", "C++ source code", "*.cpp *.cxx *.cc *.C *.c++", "text/x-csrc", "", "text-x-script"},
    {"text/x-c++hdr", "C++ header", "*.hh *.hpp *.hxx *.h++", "text/x-chdr", "", "text-x-script"},
    {"text/x-csrc", "C source code", "*.c", "text/plain", "text/x-c", "text-x-script"},
    {"text/x-chdr", "C header", "*.h", "text/x-csrc", "", "text-x-script"},
    {"text/x-python3", "Python 3 script", "*.py *.py3", "text/x-python", "", "text-x-script"},
    {"text/x-python", "Python script", "*.pyx *.wsgi", "application/x-executable text/plain", "", "text-x-script"},
    {"text/x-qml", "Qt Markup Language file", "*.qml *.qmltypes *.qmlproject", "text/plain", "", "text-x-generic"},
    {"text/x-cmake", "CMake source code", "*.cmake CMakeLists.txt", "text/plain", "", "text-x-script"},
    {"application/x-shellscript", "shell script", "*.sh", "application/x-executable text/plain", "text/x-sh",
     "text-x-script"},
    {"application/ecmascript", "ECMAScript program", "*.es", "text/plain", "", "text-x-script"},
    {"application/json", "JSON document", "*.json", "application/javascript", "application/x-json", "text-x-script"},
    {"application/xml", "XML document", "*.xml *.xbl *.xsd *.rng", "text/plain", "text/xml", "text-x-generic"},
    {"application/xhtml+xml", "XHTML page", "*.xhtml *.xht", "application/xml", "", "text-html"},
    {"application/yaml", "YAML document", "*.yaml *.yml", "text/plain", "application/x-yaml text/yaml text/x-yaml",
     "text-x-generic"},
    {"application/toml", "TOML file", "*.toml", "text/plain", "", "text-x-generic"},
    {"application/x-desktop", "desktop configuration file", "*.desktop *.kdelnk", "text/plain", "", "text-x-generic"},
    {"application/pdf", "PDF document", "*.pdf", "", "application/x-pdf image/pdf application/acrobat",
     "x-office-document"},
    {"application/postscript", "PS document", "*.ps", "text/plain", "", "x-office-document"},
    {"application/rtf", "RTF document", "*.rtf", "text/plain", "text/rtf", "x-office-document"},
    {"application/zip", "Zip archive", "*.zip", "", "application/x-zip application/x-zip-compressed",
     "package-x-generic"},
    {"application/gzip", "Gzip archive", "*.gz", "", "application/x-gzip", "package-x-generic"},
    {"application/x-bzip2", "Bzip2 archive", "*.bz2", "", "application/x-bzip", "package-x-generic"},
    {"application/x-xz", "XZ archive", "*.xz", "", "", "package-x-generic"},
    {"application/zstd", "Zstandard archive", "*.zst", "", "application/x-zstd", "package-x-generic"},
    {"application/x-tar", "Tar archive", "*.tar *.gtar *.gem", "", "application/x-gtar", "package-x-generic"},
    {"application/x-compressed-tar", "Tar archive (gzip-compressed)", "*.tar.gz *.tgz", "application/gzip", "",
     "package-x-generic"},
    {"application/x-bzip2-compressed-tar", "Tar archive (bzip-compressed)", "*.tar.bz2 *.tbz2 *.tb2",
     "application/x-bzip2", "", "package-x-generic"},
    {"application/x-xz-compressed-tar", "Tar archive (XZ-compressed)", "*.tar.xz *.txz", "application/x-xz", "",
     "package-x-generic"},
    {"application/x-7z-compressed", "7-zip archive", "*.7z", "", "", "package-x-generic"},
    {"application/vnd.rar", "RAR archive", "*.rar", "", "application/x-rar application/x-rar-compressed",
     "package-x-generic"},
    {"application/x-sqlite3", "SQLite3 database", "*.sqlite *.sqlite3 *.db", "", "application/vnd.sqlite3",
     "x-office-spreadsheet"},
    {"application/x-executable", "executable", "", "", "", "application-x-executable"},
    {"application/x-sharedlib", "shared library", "*.so", "", "", "application-x-executable"},
    {"application/vnd.android.package-archive", "Android package", "*.apk", "application/x-java-archive", "",
     "package-x-generic"},
    {"application/x-java-archive", "Java archive", "*.jar", "application/zip", "application/java-archive",
     "package-x-generic"},
    {"application/wasm", "WebAssembly module", "*.wasm", "", "", "application-x-executable"},
    {"application/cbor", "CBOR document", "*.cbor", "", "", "application-x-executable"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document", "Word 2007 document", "*.docx",
     "application/zip", "", "x-office-document"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "Excel 2007 spreadsheet", "*.xlsx",
     "application/zip", "", "x-office-spreadsheet"},
    {"application/vnd.openxmlformats-officedocument.presentationml.presentation", "PowerPoint 2007 presentation",
     "*.pptx", "application/zip", "", "x-office-presentation"},
    {"application/vnd.oasis.opendocument.text", "ODT document", "*.odt", "application/zip", "", "x-office-document"},
    {"application/vnd.oasis.opendocument.spreadsheet", "ODS spreadsheet", "*.ods", "application/zip", "",
     "x-office-spreadsheet"},
    {"image/png", "PNG image", "*.png", "", "", "image-x-generic"},
    {"image/jpeg", "JPEG image", "*.jpg *.jpeg *.jpe *.jfif", "", "image/pjpeg", "image-x-generic"},
    {"image/gif", "GIF image", "*.gif", "", "", "image-x-generic"},
    {"image/webp", "WebP image", "*.webp", "", "", "image-x-generic"},
    {"image/bmp", "Windows BMP image", "*.bmp *.dib", "", "image/x-bmp image/x-ms-bmp", "image-x-generic"},
    {"image/tiff", "TIFF image", "*.tif *.tiff", "", "", "image-x-generic"},
    {"image/svg+xml", "SVG image", "*.svg", "application/xml", "image/svg", "image-x-generic"},
    {"image/svg+xml-compressed", "compressed SVG image", "*.svgz", "application/gzip", "", "image-x-generic"},
    {"image/vnd.microsoft.icon", "Windows icon", "*.ico", "", "image/x-icon image/ico", "image-x-generic"},
    {"image/avif", "AVIF image", "*.avif *.avifs", "", "image/avif-sequence", "image-x-generic"},
    {"image/x-portable-pixmap", "PPM image", "*.ppm", "image/x-portable-anymap", "", "image-x-generic"},
    {"image/x-portable-anymap", "PNM image", "*.pnm", "", "", "image-x-generic"},
    {"audio/mpeg", "MP3 audio", "*.mp3 *.mpga", "", "audio/x-mp3 audio/mp3", "audio-x-generic"},
    {"audio/x-wav", "WAV audio", "*.wav", "", "audio/wav audio/vnd.wave", "audio-x-generic"},
    {"audio/flac", "FLAC audio", "*.flac", "", "audio/x-flac", "audio-x-generic"},
    {"audio/x-vorbis+ogg", "Ogg Vorbis audio", "*.ogg *.oga", "audio/ogg", "", "audio-x-generic"},
    {"audio/ogg", "Ogg audio", "", "", "", "audio-x-generic"},
    {"audio/mp4", "MPEG-4 audio", "*.m4a *.f4a", "", "audio/x-m4a", "audio-x-generic"},
    {"video/mp4", "MPEG-4 video", "*.mp4 *.m4v *.f4v", "", "video/x-m4v", "video-x-generic"},
    {"video/webm", "WebM video", "*.webm", "", "", "video-x-generic"},
    {"video/x-matroska", "Matroska video", "*.mkv", "", "", "video-x-generic"},
    {"video/quicktime", "QuickTime video", "*.mov *.qt", "", "", "video-x-generic"},
    {"video/x-msvideo", "AVI video", "*.avi", "", "video/avi", "video-x-generic"},
    {"font/ttf", "TrueType font", "*.ttf", "", "application/x-font-ttf", "font-x-generic"},
    {"font/otf", "OpenType font", "*.otf", "font/ttf", "application/x-font-otf", "font-x-generic"},
    {"font/woff", "WOFF font", "*.woff", "", "application/font-woff", "font-x-generic"},
    {"font/woff2", "WOFF2 font", "*.woff2", "", "", "font-x-generic"},
};

const BuiltInMagic kMagic[] = {
    {"image/png", 50, 0, "\x89PNG\r\n\x1a\n", 8},
    {"image/jpeg", 50, 0, "\xff\xd8\xff", 3},
    {"image/gif", 50, 0, "GIF87a", 6},
    {"image/gif", 50, 0, "GIF89a", 6},
    {"image/webp", 50, 8, "WEBP", 4},
    {"image/bmp", 40, 0, "BM", 2},
    {"image/tiff", 50, 0, "MM\x00\x2a", 4},
    {"image/tiff", 50, 0, "II\x2a\x00", 4},
    {"image/vnd.microsoft.icon", 40, 0, "\x00\x00\x01\x00", 4},
    {"image/avif", 50, 4, "ftypavif", 8},
    {"image/x-portable-pixmap", 50, 0, "P6", 2},
    {"image/svg+xml", 80, 0, "<svg", 4},
    {"application/pdf", 50, 0, "%PDF-", 5},
    {"application/postscript", 50, 0, "%!", 2},
    {"application/rtf", 50, 0, "{\\rtf", 5},
    {"application/zip", 40, 0, "PK\x03\x04", 4},
    {"application/gzip", 50, 0, "\x1f\x8b", 2},
    {"application/x-bzip2", 50, 0, "BZh", 3},
    {"application/x-xz", 50, 0,
     "\xfd"
     "7zXZ\x00",
     6},
    {"application/zstd", 50, 0, "\x28\xb5\x2f\xfd", 4},
    {"application/x-7z-compressed", 50, 0, "7z\xbc\xaf\x27\x1c", 6},
    {"application/vnd.rar", 50, 0, "Rar!\x1a", 5},
    {"application/x-tar", 60, 257, "ustar", 5},
    {"application/x-sqlite3", 50, 0, "SQLite format 3", 15},
    {"application/x-executable", 40, 0,
     "\x7f"
     "ELF",
     4},
    {"application/wasm", 50, 0,
     "\x00"
     "asm",
     4},
    {"application/xml", 40, 0, "<?xml", 5},
    {"text/html", 50, 0, "<!DOCTYPE html", 14},
    {"text/html", 50, 0, "<!doctype html", 14},
    {"text/html", 40, 0, "<html", 5},
    {"text/html", 40, 0, "<HTML", 5},
    {"application/x-shellscript", 50, 0, "#!/bin/sh", 9},
    {"application/x-shellscript", 50, 0, "#!/bin/bash", 11},
    {"application/x-shellscript", 50, 0, "#!/usr/bin/env sh", 17},
    {"application/x-shellscript", 50, 0, "#!/usr/bin/env bash", 19},
    {"text/x-python3", 50, 0, "#!/usr/bin/env python3", 22},
    {"text/x-python3", 50, 0, "#!/usr/bin/python3", 18},
    {"application/cbor", 50, 0, "\xd9\xd9\xf7", 3},
    {"audio/mpeg", 50, 0, "ID3", 3},
    {"audio/x-wav", 50, 8, "WAVE", 4},
    {"audio/flac", 50, 0, "fLaC", 4},
    {"audio/ogg", 40, 0, "OggS", 4},
    {"audio/mp4", 50, 4, "ftypM4A", 7},
    {"video/mp4", 50, 4, "ftypisom", 8},
    {"video/mp4", 50, 4, "ftypmp42", 8},
    {"video/mp4", 50, 4, "ftypmp41", 8},
    {"video/quicktime", 50, 4, "ftypqt", 6},
    {"video/quicktime", 40, 4, "moov", 4},
    {"video/webm", 50, 0, "\x1a\x45\xdf\xa3", 4},
    {"video/x-msvideo", 50, 8, "AVI ", 4},
    {"font/ttf", 50, 0, "\x00\x01\x00\x00", 4},
    {"font/otf", 50, 0, "OTTO", 4},
    {"font/woff", 50, 0, "wOFF", 4},
    {"font/woff2", 50, 0, "wOF2", 4},
};

} // namespace

void addBuiltInMimeTypes(
    const std::function<void(const char *, const char *, const char *, const char *, const char *, const char *)> &type,
    const std::function<void(const char *, int, qsizetype, const char *, qsizetype)> &magic) {
    for (const BuiltInType &t : kTypes)
        type(t.name, t.comment, t.globs, t.parents, t.aliases, t.genericIcon);
    for (const BuiltInMagic &m : kMagic)
        magic(m.type, m.priority, m.offset, m.bytes, m.length);
}

} // namespace QtPrivate
