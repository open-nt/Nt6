// MIT License - see LICENSE
#include <QtCore/qchar.h>

// Case mapping and classification cover ASCII, Latin-1, Latin Extended-A, Greek and
// Cyrillic only; other scripts are returned unchanged.

bool QChar::isLetter() const noexcept {
    const char16_t c = ucs;
    if ((c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z'))
        return true;
    if (c < 0xaa)
        return false;
    if (c == 0xaa || c == 0xb5 || c == 0xba)
        return true;
    if (c >= 0xc0 && c <= 0x24f)
        return c != 0xd7 && c != 0xf7;
    return (c >= 0x370 && c <= 0x3ff && c != 0x37e && c != 0x387) || (c >= 0x400 && c <= 0x481) ||
           (c >= 0x48a && c <= 0x52f) || (c >= 0x3040 && c <= 0x30ff) || (c >= 0x4e00 && c <= 0x9fff) ||
           (c >= 0xac00 && c <= 0xd7a3);
}

bool QChar::isSpace() const noexcept {
    const char16_t c = ucs;
    return c == u' ' || (c >= 0x09 && c <= 0x0d) || c == 0x85 || c == 0xa0 || c == 0x1680 ||
           (c >= 0x2000 && c <= 0x200a) || c == 0x2028 || c == 0x2029 || c == 0x202f || c == 0x205f ||
           c == 0x3000;
}

bool QChar::isUpper() const noexcept { return isLetter() && toLower().ucs != ucs; }

bool QChar::isLower() const noexcept { return isLetter() && toUpper().ucs != ucs; }

bool QChar::isPunct() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= 0x21 && c <= 0x2f && c != u'$' && c != u'+') ||
               (c >= 0x3a && c <= 0x40 && c != u'<' && c != u'=' && c != u'>') ||
               (c >= 0x5b && c <= 0x60 && c != u'^' && c != u'`') || c == u'{' || c == u'}';
    return c == 0xa1 || c == 0xa7 || c == 0xab || c == 0xb6 || c == 0xb7 || c == 0xbb || c == 0xbf ||
           (c >= 0x2010 && c <= 0x2027) || (c >= 0x2030 && c <= 0x205e);
}

QChar QChar::toLower() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= u'A' && c <= u'Z') ? QChar(char16_t(c + 32)) : *this;
    if ((c >= 0xc0 && c <= 0xde && c != 0xd7))
        return QChar(char16_t(c + 32));
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14a && c <= 0x177))
        return (c % 2 == 0) ? QChar(char16_t(c + 1)) : *this;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e))
        return (c % 2 == 1) ? QChar(char16_t(c + 1)) : *this;
    if (c >= 0x391 && c <= 0x3a9 && c != 0x3a2)
        return QChar(char16_t(c + 32));
    if (c >= 0x410 && c <= 0x42f)
        return QChar(char16_t(c + 32));
    if (c >= 0x400 && c <= 0x40f)
        return QChar(char16_t(c + 80));
    return *this;
}

QChar QChar::toUpper() const noexcept {
    const char16_t c = ucs;
    if (c < 0x80)
        return (c >= u'a' && c <= u'z') ? QChar(char16_t(c - 32)) : *this;
    if (c >= 0xe0 && c <= 0xfe && c != 0xf7)
        return QChar(char16_t(c - 32));
    if ((c >= 0x100 && c <= 0x137) || (c >= 0x14a && c <= 0x177))
        return (c % 2 == 1) ? QChar(char16_t(c - 1)) : *this;
    if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17e))
        return (c % 2 == 0) ? QChar(char16_t(c - 1)) : *this;
    if (c >= 0x3b1 && c <= 0x3c9 && c != 0x3c2)
        return QChar(char16_t(c - 32));
    if (c >= 0x430 && c <= 0x44f)
        return QChar(char16_t(c - 32));
    if (c >= 0x450 && c <= 0x45f)
        return QChar(char16_t(c - 80));
    return *this;
}
