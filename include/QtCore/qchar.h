// MIT License - see LICENSE
#pragma once

#include <QtCore/qglobal.h>

#include <functional>
#include <type_traits>

class QString;

namespace QtPrivate {
// The UTF-16 form of one code point, returned by QChar::fromUcs4(): one or two code units.
struct Ucs4Chars {
    char16_t chars[2];
    bool pair;
    constexpr qsizetype size() const noexcept { return pair ? 2 : 1; }
    constexpr const char16_t *data() const noexcept { return chars; }
    constexpr const char16_t *begin() const noexcept { return chars; }
    constexpr const char16_t *end() const noexcept { return chars + size(); }
    // Converts to QStringView (or any view constructible from a pointer and a size) without
    // including it here.
    template <typename View, typename = std::enable_if_t<std::is_constructible_v<View, const char16_t *, qsizetype>>>
    constexpr operator View() const noexcept {
        return View(chars, size());
    }
};
} // namespace QtPrivate

/*!
    \class QLatin1Char
    \brief The QLatin1Char class provides an 8-bit ASCII/Latin-1 character.

    QLatin1Char wraps a single \c char so that it can be passed where a
    QChar is expected without being ambiguous with other integer types. It is
    converted to a QChar by interpreting the byte as a Latin-1 code point.

    \differences
    None.
    \enddifferences

    \sa QChar, QLatin1StringView
*/
class QLatin1Char {
public:
    /*!
        Constructs a Latin-1 character for \a c.
    */
    constexpr explicit QLatin1Char(char c) noexcept : ch(c) {}
    /*!
        Returns the Latin-1 character value as a \c char.

        \sa unicode()
    */
    constexpr char toLatin1() const noexcept { return ch; }
    /*!
        Returns the Unicode code unit for this character, in the range 0 to 255.

        \sa toLatin1()
    */
    constexpr char16_t unicode() const noexcept { return static_cast<unsigned char>(ch); }

    // Comparisons, by code point (so 'é' sorts after 'z'), with another QLatin1Char or a char.
    friend constexpr bool operator==(QLatin1Char lhs, QLatin1Char rhs) noexcept {
        return lhs.unicode() == rhs.unicode();
    }
    friend constexpr bool operator==(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) == rhs; }
    friend constexpr bool operator==(QLatin1Char lhs, char rhs) noexcept { return lhs == QLatin1Char(rhs); }
    friend constexpr bool operator!=(QLatin1Char lhs, QLatin1Char rhs) noexcept {
        return lhs.unicode() != rhs.unicode();
    }
    friend constexpr bool operator!=(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) != rhs; }
    friend constexpr bool operator!=(QLatin1Char lhs, char rhs) noexcept { return lhs != QLatin1Char(rhs); }
    friend constexpr bool operator<(QLatin1Char lhs, QLatin1Char rhs) noexcept { return lhs.unicode() < rhs.unicode(); }
    friend constexpr bool operator<(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) < rhs; }
    friend constexpr bool operator<(QLatin1Char lhs, char rhs) noexcept { return lhs < QLatin1Char(rhs); }
    friend constexpr bool operator>(QLatin1Char lhs, QLatin1Char rhs) noexcept { return lhs.unicode() > rhs.unicode(); }
    friend constexpr bool operator>(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) > rhs; }
    friend constexpr bool operator>(QLatin1Char lhs, char rhs) noexcept { return lhs > QLatin1Char(rhs); }
    friend constexpr bool operator<=(QLatin1Char lhs, QLatin1Char rhs) noexcept {
        return lhs.unicode() <= rhs.unicode();
    }
    friend constexpr bool operator<=(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) <= rhs; }
    friend constexpr bool operator<=(QLatin1Char lhs, char rhs) noexcept { return lhs <= QLatin1Char(rhs); }
    friend constexpr bool operator>=(QLatin1Char lhs, QLatin1Char rhs) noexcept {
        return lhs.unicode() >= rhs.unicode();
    }
    friend constexpr bool operator>=(char lhs, QLatin1Char rhs) noexcept { return QLatin1Char(lhs) >= rhs; }
    friend constexpr bool operator>=(QLatin1Char lhs, char rhs) noexcept { return lhs >= QLatin1Char(rhs); }

private:
    char ch;
};

class QChar {
public:
    /*!
        \enum QChar::SpecialCharacter

        Names for commonly used code points.

        \value Null U+0000, the null character.
        \value Tabulation U+0009, horizontal tabulation.
        \value LineFeed U+000A, line feed.
        \value CarriageReturn U+000D, carriage return.
        \value Space U+0020, the space character.
        \value Nbsp U+00A0, the no-break space.
        \value SoftHyphen U+00AD, the soft hyphen.
        \value ReplacementCharacter U+FFFD, used in place of invalid or unrepresentable input.
        \value ObjectReplacementCharacter U+FFFC, the object replacement character.
        \value ByteOrderMark U+FEFF, the byte order mark.
        \value ByteOrderSwapped U+FFFE, a byte order mark read with the wrong byte order.
        \value ParagraphSeparator U+2029, the paragraph separator.
        \value LineSeparator U+2028, the line separator.
        \value VisualTabCharacter U+2192, the arrow some editors draw for a tab.
        \value LastValidCodePoint U+10FFFF, the last valid Unicode code point.
    */
    enum SpecialCharacter {
        Null = 0x0000,
        Tabulation = 0x0009,
        LineFeed = 0x000a,
        CarriageReturn = 0x000d,
        Space = 0x0020,
        Nbsp = 0x00a0,
        SoftHyphen = 0x00ad,
        ReplacementCharacter = 0xfffd,
        ObjectReplacementCharacter = 0xfffc,
        ByteOrderMark = 0xfeff,
        ByteOrderSwapped = 0xfffe,
        ParagraphSeparator = 0x2029,
        LineSeparator = 0x2028,
        VisualTabCharacter = 0x2192,
        LastValidCodePoint = 0x10ffff
    };

    /*!
        Constructs a null character (U+0000).

        \sa isNull()
    */
    constexpr QChar() noexcept = default;
    /*!
        Constructs a character with the UTF-16 code unit \a c.
    */
    constexpr QChar(char16_t c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character from the Latin-1 byte \a c. The byte is
        zero-extended, not interpreted as a signed value.
    */
    constexpr QChar(char c) noexcept : ucs(static_cast<unsigned char>(c)) {}
    /*!
        \overload

        Constructs a character with the code unit \a c.
    */
    constexpr QChar(uchar c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character with the code unit \a c, reinterpreted as an
        unsigned 16-bit value.
    */
    constexpr QChar(short c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character with the code unit \a c.
    */
    constexpr QChar(ushort c) noexcept : ucs(c) {}
    /*!
        \overload

        Constructs a character from \a c, truncated to 16 bits.
    */
    constexpr QChar(int c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character from \a c, truncated to 16 bits.
    */
    constexpr QChar(uint c) noexcept : ucs(static_cast<char16_t>(c)) {}
    /*!
        \overload

        Constructs a character with the code point of the special character \a s.
    */
    constexpr QChar(SpecialCharacter s) noexcept : ucs(static_cast<char16_t>(s)) {}
    /*!
        \overload

        Constructs a character from the Latin-1 character \a c.
    */
    constexpr QChar(QLatin1Char c) noexcept : ucs(c.unicode()) {}

    /*!
        Returns the UTF-16 code unit of this character.

        \sa cell(), row()
    */
    constexpr char16_t unicode() const noexcept { return ucs; }
    /*!
        \overload

        Returns a reference to the UTF-16 code unit of this character, which
        can be used to modify it.
    */
    constexpr char16_t &unicode() noexcept { return ucs; }
    /*!
        Returns the low byte of the code unit (the "cell" within its row).

        \sa row(), unicode()
    */
    constexpr uchar cell() const noexcept { return static_cast<uchar>(ucs & 0xff); }
    /*!
        Returns the high byte of the code unit (the "row" of its plane).

        \sa cell(), unicode()
    */
    constexpr uchar row() const noexcept { return static_cast<uchar>(ucs >> 8); }
    /*!
        Sets the low byte of the code unit to \a acell.

        \sa cell(), setRow()
    */
    constexpr void setCell(uchar acell) noexcept { ucs = static_cast<char16_t>((ucs & 0xff00) | acell); }
    /*!
        Sets the high byte of the code unit to \a arow.

        \sa row(), setCell()
    */
    constexpr void setRow(uchar arow) noexcept { ucs = static_cast<char16_t>((ucs & 0x00ff) | (arow << 8)); }
    /*!
        Returns \c true if this is the null character (U+0000); otherwise
        returns \c false.
    */
    constexpr bool isNull() const noexcept { return ucs == 0; }
    /*!
        Returns the Latin-1 representation of this character, or \c '\\0' if
        it is outside the range 0 to 255.

        \sa fromLatin1(), unicode()
    */
    constexpr char toLatin1() const noexcept { return ucs > 0xff ? '\0' : static_cast<char>(ucs); }
    /*!
        Converts the Latin-1 byte \a c to a QChar.

        \sa toLatin1()
    */
    static constexpr QChar fromLatin1(char c) noexcept { return QChar(static_cast<unsigned char>(c)); }
    /*!
        \since 6.0
        Returns the character with the UTF-16 code unit \a c.

        \sa fromLatin1(), fromUcs4()
    */
    static constexpr QChar fromUcs2(char16_t c) noexcept { return QChar(c); }

    /*!
        \enum QChar::Category

        The general category of a character in the Unicode database.

        \value Mark_NonSpacing Mn, a non-spacing combining mark.
        \value Mark_SpacingCombining Mc, a spacing combining mark.
        \value Mark_Enclosing Me, an enclosing mark.
        \value Number_DecimalDigit Nd, a decimal digit.
        \value Number_Letter Nl, a letter-like number, such as a Roman numeral.
        \value Number_Other No, another number, such as a superscript digit.
        \value Separator_Space Zs, a space.
        \value Separator_Line Zl, the line separator.
        \value Separator_Paragraph Zp, the paragraph separator.
        \value Other_Control Cc, a control character.
        \value Other_Format Cf, a format character.
        \value Other_Surrogate Cs, a surrogate.
        \value Other_PrivateUse Co, a private-use character.
        \value Other_NotAssigned Cn, a code point that is not assigned.
        \value Letter_Uppercase Lu, an uppercase letter.
        \value Letter_Lowercase Ll, a lowercase letter.
        \value Letter_Titlecase Lt, a titlecase letter, such as U+01C5.
        \value Letter_Modifier Lm, a modifier letter.
        \value Letter_Other Lo, another letter, such as an ideograph.
        \value Punctuation_Connector Pc, connector punctuation, such as the underscore.
        \value Punctuation_Dash Pd, a dash.
        \value Punctuation_Open Ps, opening punctuation, such as a parenthesis.
        \value Punctuation_Close Pe, closing punctuation.
        \value Punctuation_InitialQuote Pi, an initial quotation mark.
        \value Punctuation_FinalQuote Pf, a final quotation mark.
        \value Punctuation_Other Po, other punctuation.
        \value Symbol_Math Sm, a mathematical symbol.
        \value Symbol_Currency Sc, a currency symbol.
        \value Symbol_Modifier Sk, a modifier symbol.
        \value Symbol_Other So, another symbol.
    */
    enum Category {
        Mark_NonSpacing,
        Mark_SpacingCombining,
        Mark_Enclosing,
        Number_DecimalDigit,
        Number_Letter,
        Number_Other,
        Separator_Space,
        Separator_Line,
        Separator_Paragraph,
        Other_Control,
        Other_Format,
        Other_Surrogate,
        Other_PrivateUse,
        Other_NotAssigned,
        Letter_Uppercase,
        Letter_Lowercase,
        Letter_Titlecase,
        Letter_Modifier,
        Letter_Other,
        Punctuation_Connector,
        Punctuation_Dash,
        Punctuation_Open,
        Punctuation_Close,
        Punctuation_InitialQuote,
        Punctuation_FinalQuote,
        Punctuation_Other,
        Symbol_Math,
        Symbol_Currency,
        Symbol_Modifier,
        Symbol_Other
    };

    /*!
        Returns the general category of this character.
    */
    Category category() const noexcept { return category(char32_t(ucs)); }

    /*!
        \enum QChar::Decomposition

        The kind of a character's decomposition mapping, from the tags of the
        Unicode Character Database.

        \value NoDecomposition The character has no decomposition.
        \value Canonical A canonical decomposition.
        \value Font A font variant.
        \value NoBreak A no-break version of a space or hyphen.
        \value Initial An initial presentation form (Arabic).
        \value Medial A medial presentation form (Arabic).
        \value Final A final presentation form (Arabic).
        \value Isolated An isolated presentation form (Arabic).
        \value Circle An encircled form.
        \value Super A superscript form.
        \value Sub A subscript form.
        \value Vertical A vertical layout presentation form.
        \value Wide A wide (zenkaku) compatibility character.
        \value Narrow A narrow (hankaku) compatibility character.
        \value Small A small variant form.
        \value Square A CJK squared font variant.
        \value Compat Another compatibility character.
        \value Fraction A vulgar fraction form.
    */
    enum Decomposition {
        NoDecomposition,
        Canonical,
        Font,
        NoBreak,
        Initial,
        Medial,
        Final,
        Isolated,
        Circle,
        Super,
        Sub,
        Vertical,
        Wide,
        Narrow,
        Small,
        Square,
        Compat,
        Fraction
    };
    /*!
        Returns the tag of this character's decomposition mapping, or
        \c NoDecomposition.

        \sa decomposition()
    */
    Decomposition decompositionTag() const noexcept { return decompositionTag(char32_t(ucs)); }
    QString decomposition() const;
    /*!
        Returns the canonical combining class of this character, which orders
        combining marks during normalization; 0 for most characters.

        \sa QString::normalized()
    */
    unsigned char combiningClass() const noexcept { return combiningClass(char32_t(ucs)); }
    static Decomposition decompositionTag(char32_t ucs4) noexcept;
    static QString decomposition(char32_t ucs4);
    static unsigned char combiningClass(char32_t ucs4) noexcept;

    /*!
        \enum QChar::Direction

        The bidirectional class of a character, from the Unicode Character
        Database, used by the bidirectional algorithm (UAX #9).

        \value DirL Left-to-right.
        \value DirR Right-to-left.
        \value DirEN European number.
        \value DirES European number separator.
        \value DirET European number terminator.
        \value DirAN Arabic number.
        \value DirCS Common number separator.
        \value DirB Paragraph separator.
        \value DirS Segment separator, such as a tab.
        \value DirWS Whitespace.
        \value DirON Other neutral.
        \value DirLRE Left-to-right embedding.
        \value DirLRO Left-to-right override.
        \value DirAL Right-to-left Arabic letter.
        \value DirRLE Right-to-left embedding.
        \value DirRLO Right-to-left override.
        \value DirPDF Pop directional format.
        \value DirNSM Nonspacing mark.
        \value DirBN Boundary neutral.
        \value DirLRI Left-to-right isolate.
        \value DirRLI Right-to-left isolate.
        \value DirFSI First strong isolate.
        \value DirPDI Pop directional isolate.
    */
    enum Direction {
        DirL,
        DirR,
        DirEN,
        DirES,
        DirET,
        DirAN,
        DirCS,
        DirB,
        DirS,
        DirWS,
        DirON,
        DirLRE,
        DirLRO,
        DirAL,
        DirRLE,
        DirRLO,
        DirPDF,
        DirNSM,
        DirBN,
        DirLRI,
        DirRLI,
        DirFSI,
        DirPDI
    };

    /*!
        \enum QChar::JoiningType

        How a character joins its neighbours in cursive scripts such as Arabic
        and Syriac (ArabicShaping.txt).

        \value Joining_None The character does not join.
        \value Joining_Causing The character causes joining, like U+200D ZERO WIDTH JOINER.
        \value Joining_Dual The character joins on both sides.
        \value Joining_Right The character joins on its right side only.
        \value Joining_Left The character joins on its left side only.
        \value Joining_Transparent The character does not affect joining, like most combining marks.
    */
    enum JoiningType { Joining_None, Joining_Causing, Joining_Dual, Joining_Right, Joining_Left, Joining_Transparent };

    /*!
        \enum QChar::Script

        The script of a character (Scripts.txt of the Unicode Character
        Database).

        \value Script_Unknown Unassigned, private-use and other characters that belong to no script.
        \value Script_Inherited Combining marks that take the script of the character they follow.
        \value Script_Common Characters used by several scripts, such as punctuation, digits and symbols.
        \value Script_Adlam Adlam.
        \value Script_Ahom Ahom.
        \value Script_AnatolianHieroglyphs Anatolian Hieroglyphs.
        \value Script_Arabic Arabic.
        \value Script_Armenian Armenian.
        \value Script_Avestan Avestan.
        \value Script_Balinese Balinese.
        \value Script_Bamum Bamum.
        \value Script_BassaVah Bassa Vah.
        \value Script_Batak Batak.
        \value Script_Bengali Bengali.
        \value Script_Bhaiksuki Bhaiksuki.
        \value Script_Bopomofo Bopomofo.
        \value Script_Brahmi Brahmi.
        \value Script_Braille Braille.
        \value Script_Buginese Buginese.
        \value Script_Buhid Buhid.
        \value Script_CanadianAboriginal Canadian Aboriginal.
        \value Script_Carian Carian.
        \value Script_CaucasianAlbanian Caucasian Albanian.
        \value Script_Chakma Chakma.
        \value Script_Cham Cham.
        \value Script_Cherokee Cherokee.
        \value Script_Chorasmian Chorasmian.
        \value Script_Coptic Coptic.
        \value Script_Cuneiform Cuneiform.
        \value Script_Cypriot Cypriot.
        \value Script_CyproMinoan Cypro Minoan.
        \value Script_Cyrillic Cyrillic.
        \value Script_Deseret Deseret.
        \value Script_Devanagari Devanagari.
        \value Script_DivesAkuru Dives Akuru.
        \value Script_Dogra Dogra.
        \value Script_Duployan Duployan.
        \value Script_EgyptianHieroglyphs Egyptian Hieroglyphs.
        \value Script_Elbasan Elbasan.
        \value Script_Elymaic Elymaic.
        \value Script_Ethiopic Ethiopic.
        \value Script_Garay Garay.
        \value Script_Georgian Georgian.
        \value Script_Glagolitic Glagolitic.
        \value Script_Gothic Gothic.
        \value Script_Grantha Grantha.
        \value Script_Greek Greek.
        \value Script_Gujarati Gujarati.
        \value Script_GunjalaGondi Gunjala Gondi.
        \value Script_Gurmukhi Gurmukhi.
        \value Script_GurungKhema Gurung Khema.
        \value Script_Han Han.
        \value Script_Hangul Hangul.
        \value Script_HanifiRohingya Hanifi Rohingya.
        \value Script_Hanunoo Hanunoo.
        \value Script_Hatran Hatran.
        \value Script_Hebrew Hebrew.
        \value Script_Hiragana Hiragana.
        \value Script_ImperialAramaic Imperial Aramaic.
        \value Script_InscriptionalPahlavi Inscriptional Pahlavi.
        \value Script_InscriptionalParthian Inscriptional Parthian.
        \value Script_Javanese Javanese.
        \value Script_Kaithi Kaithi.
        \value Script_Kannada Kannada.
        \value Script_Katakana Katakana.
        \value Script_Kawi Kawi.
        \value Script_KayahLi Kayah Li.
        \value Script_Kharoshthi Kharoshthi.
        \value Script_KhitanSmallScript Khitan Small Script.
        \value Script_Khmer Khmer.
        \value Script_Khojki Khojki.
        \value Script_Khudawadi Khudawadi.
        \value Script_KiratRai Kirat Rai.
        \value Script_Lao Lao.
        \value Script_Latin Latin.
        \value Script_Lepcha Lepcha.
        \value Script_Limbu Limbu.
        \value Script_LinearA Linear A.
        \value Script_LinearB Linear B.
        \value Script_Lisu Lisu.
        \value Script_Lycian Lycian.
        \value Script_Lydian Lydian.
        \value Script_Mahajani Mahajani.
        \value Script_Makasar Makasar.
        \value Script_Malayalam Malayalam.
        \value Script_Mandaic Mandaic.
        \value Script_Manichaean Manichaean.
        \value Script_Marchen Marchen.
        \value Script_MasaramGondi Masaram Gondi.
        \value Script_Medefaidrin Medefaidrin.
        \value Script_MeeteiMayek Meetei Mayek.
        \value Script_MendeKikakui Mende Kikakui.
        \value Script_MeroiticCursive Meroitic Cursive.
        \value Script_MeroiticHieroglyphs Meroitic Hieroglyphs.
        \value Script_Miao Miao.
        \value Script_Modi Modi.
        \value Script_Mongolian Mongolian.
        \value Script_Mro Mro.
        \value Script_Multani Multani.
        \value Script_Myanmar Myanmar.
        \value Script_Nabataean Nabataean.
        \value Script_NagMundari Nag Mundari.
        \value Script_Nandinagari Nandinagari.
        \value Script_NewTaiLue New Tai Lue.
        \value Script_Newa Newa.
        \value Script_Nko Nko.
        \value Script_Nushu Nushu.
        \value Script_NyiakengPuachueHmong Nyiakeng Puachue Hmong.
        \value Script_Ogham Ogham.
        \value Script_OlChiki Ol Chiki.
        \value Script_OlOnal Ol Onal.
        \value Script_OldHungarian Old Hungarian.
        \value Script_OldItalic Old Italic.
        \value Script_OldNorthArabian Old North Arabian.
        \value Script_OldPermic Old Permic.
        \value Script_OldPersian Old Persian.
        \value Script_OldSogdian Old Sogdian.
        \value Script_OldSouthArabian Old South Arabian.
        \value Script_OldTurkic Old Turkic.
        \value Script_OldUyghur Old Uyghur.
        \value Script_Oriya Oriya.
        \value Script_Osage Osage.
        \value Script_Osmanya Osmanya.
        \value Script_PahawhHmong Pahawh Hmong.
        \value Script_Palmyrene Palmyrene.
        \value Script_PauCinHau Pau Cin Hau.
        \value Script_PhagsPa Phags Pa.
        \value Script_Phoenician Phoenician.
        \value Script_PsalterPahlavi Psalter Pahlavi.
        \value Script_Rejang Rejang.
        \value Script_Runic Runic.
        \value Script_Samaritan Samaritan.
        \value Script_Saurashtra Saurashtra.
        \value Script_Sharada Sharada.
        \value Script_Shavian Shavian.
        \value Script_Siddham Siddham.
        \value Script_SignWriting SignWriting.
        \value Script_Sinhala Sinhala.
        \value Script_Sogdian Sogdian.
        \value Script_SoraSompeng Sora Sompeng.
        \value Script_Soyombo Soyombo.
        \value Script_Sundanese Sundanese.
        \value Script_Sunuwar Sunuwar.
        \value Script_SylotiNagri Syloti Nagri.
        \value Script_Syriac Syriac.
        \value Script_Tagalog Tagalog.
        \value Script_Tagbanwa Tagbanwa.
        \value Script_TaiLe Tai Le.
        \value Script_TaiTham Tai Tham.
        \value Script_TaiViet Tai Viet.
        \value Script_Takri Takri.
        \value Script_Tamil Tamil.
        \value Script_Tangsa Tangsa.
        \value Script_Tangut Tangut.
        \value Script_Telugu Telugu.
        \value Script_Thaana Thaana.
        \value Script_Thai Thai.
        \value Script_Tibetan Tibetan.
        \value Script_Tifinagh Tifinagh.
        \value Script_Tirhuta Tirhuta.
        \value Script_Todhri Todhri.
        \value Script_Toto Toto.
        \value Script_TuluTigalari Tulu Tigalari.
        \value Script_Ugaritic Ugaritic.
        \value Script_Vai Vai.
        \value Script_Vithkuqi Vithkuqi.
        \value Script_Wancho Wancho.
        \value Script_WarangCiti Warang Citi.
        \value Script_Yezidi Yezidi.
        \value Script_Yi Yi.
        \value Script_ZanabazarSquare Zanabazar Square.
        \value ScriptCount The number of scripts; not a script.
    */
    enum Script {
        Script_Unknown,
        Script_Inherited,
        Script_Common,
        Script_Adlam,
        Script_Ahom,
        Script_AnatolianHieroglyphs,
        Script_Arabic,
        Script_Armenian,
        Script_Avestan,
        Script_Balinese,
        Script_Bamum,
        Script_BassaVah,
        Script_Batak,
        Script_Bengali,
        Script_Bhaiksuki,
        Script_Bopomofo,
        Script_Brahmi,
        Script_Braille,
        Script_Buginese,
        Script_Buhid,
        Script_CanadianAboriginal,
        Script_Carian,
        Script_CaucasianAlbanian,
        Script_Chakma,
        Script_Cham,
        Script_Cherokee,
        Script_Chorasmian,
        Script_Coptic,
        Script_Cuneiform,
        Script_Cypriot,
        Script_CyproMinoan,
        Script_Cyrillic,
        Script_Deseret,
        Script_Devanagari,
        Script_DivesAkuru,
        Script_Dogra,
        Script_Duployan,
        Script_EgyptianHieroglyphs,
        Script_Elbasan,
        Script_Elymaic,
        Script_Ethiopic,
        Script_Garay,
        Script_Georgian,
        Script_Glagolitic,
        Script_Gothic,
        Script_Grantha,
        Script_Greek,
        Script_Gujarati,
        Script_GunjalaGondi,
        Script_Gurmukhi,
        Script_GurungKhema,
        Script_Han,
        Script_Hangul,
        Script_HanifiRohingya,
        Script_Hanunoo,
        Script_Hatran,
        Script_Hebrew,
        Script_Hiragana,
        Script_ImperialAramaic,
        Script_InscriptionalPahlavi,
        Script_InscriptionalParthian,
        Script_Javanese,
        Script_Kaithi,
        Script_Kannada,
        Script_Katakana,
        Script_Kawi,
        Script_KayahLi,
        Script_Kharoshthi,
        Script_KhitanSmallScript,
        Script_Khmer,
        Script_Khojki,
        Script_Khudawadi,
        Script_KiratRai,
        Script_Lao,
        Script_Latin,
        Script_Lepcha,
        Script_Limbu,
        Script_LinearA,
        Script_LinearB,
        Script_Lisu,
        Script_Lycian,
        Script_Lydian,
        Script_Mahajani,
        Script_Makasar,
        Script_Malayalam,
        Script_Mandaic,
        Script_Manichaean,
        Script_Marchen,
        Script_MasaramGondi,
        Script_Medefaidrin,
        Script_MeeteiMayek,
        Script_MendeKikakui,
        Script_MeroiticCursive,
        Script_MeroiticHieroglyphs,
        Script_Miao,
        Script_Modi,
        Script_Mongolian,
        Script_Mro,
        Script_Multani,
        Script_Myanmar,
        Script_Nabataean,
        Script_NagMundari,
        Script_Nandinagari,
        Script_NewTaiLue,
        Script_Newa,
        Script_Nko,
        Script_Nushu,
        Script_NyiakengPuachueHmong,
        Script_Ogham,
        Script_OlChiki,
        Script_OlOnal,
        Script_OldHungarian,
        Script_OldItalic,
        Script_OldNorthArabian,
        Script_OldPermic,
        Script_OldPersian,
        Script_OldSogdian,
        Script_OldSouthArabian,
        Script_OldTurkic,
        Script_OldUyghur,
        Script_Oriya,
        Script_Osage,
        Script_Osmanya,
        Script_PahawhHmong,
        Script_Palmyrene,
        Script_PauCinHau,
        Script_PhagsPa,
        Script_Phoenician,
        Script_PsalterPahlavi,
        Script_Rejang,
        Script_Runic,
        Script_Samaritan,
        Script_Saurashtra,
        Script_Sharada,
        Script_Shavian,
        Script_Siddham,
        Script_SignWriting,
        Script_Sinhala,
        Script_Sogdian,
        Script_SoraSompeng,
        Script_Soyombo,
        Script_Sundanese,
        Script_Sunuwar,
        Script_SylotiNagri,
        Script_Syriac,
        Script_Tagalog,
        Script_Tagbanwa,
        Script_TaiLe,
        Script_TaiTham,
        Script_TaiViet,
        Script_Takri,
        Script_Tamil,
        Script_Tangsa,
        Script_Tangut,
        Script_Telugu,
        Script_Thaana,
        Script_Thai,
        Script_Tibetan,
        Script_Tifinagh,
        Script_Tirhuta,
        Script_Todhri,
        Script_Toto,
        Script_TuluTigalari,
        Script_Ugaritic,
        Script_Vai,
        Script_Vithkuqi,
        Script_Wancho,
        Script_WarangCiti,
        Script_Yezidi,
        Script_Yi,
        Script_ZanabazarSquare,
        ScriptCount
    };

    /*!
        \enum QChar::UnicodeVersion

        The version of Unicode that assigned a character.

        \value Unicode_Unassigned The character is not assigned.
        \value Unicode_1_1 Version 1.1.
        \value Unicode_2_0 Version 2.0.
        \value Unicode_2_1_2 Version 2.1.2.
        \value Unicode_3_0 Version 3.0.
        \value Unicode_3_1 Version 3.1.
        \value Unicode_3_2 Version 3.2.
        \value Unicode_4_0 Version 4.0.
        \value Unicode_4_1 Version 4.1.
        \value Unicode_5_0 Version 5.0.
        \value Unicode_5_1 Version 5.1.
        \value Unicode_5_2 Version 5.2.
        \value Unicode_6_0 Version 6.0.
        \value Unicode_6_1 Version 6.1.
        \value Unicode_6_2 Version 6.2.
        \value Unicode_6_3 Version 6.3.
        \value Unicode_7_0 Version 7.0.
        \value Unicode_8_0 Version 8.0.
        \value Unicode_9_0 Version 9.0.
        \value Unicode_10_0 Version 10.0.
        \value Unicode_11_0 Version 11.0.
        \value Unicode_12_0 Version 12.0.
        \value Unicode_12_1 Version 12.1.
        \value Unicode_13_0 Version 13.0.
        \value Unicode_14_0 Version 14.0.
        \value Unicode_15_0 Version 15.0.
        \value Unicode_15_1 Version 15.1.
        \value Unicode_16_0 Version 16.0.
    */
    enum UnicodeVersion {
        Unicode_Unassigned,
        Unicode_1_1,
        Unicode_2_0,
        Unicode_2_1_2,
        Unicode_3_0,
        Unicode_3_1,
        Unicode_3_2,
        Unicode_4_0,
        Unicode_4_1,
        Unicode_5_0,
        Unicode_5_1,
        Unicode_5_2,
        Unicode_6_0,
        Unicode_6_1,
        Unicode_6_2,
        Unicode_6_3,
        Unicode_7_0,
        Unicode_8_0,
        Unicode_9_0,
        Unicode_10_0,
        Unicode_11_0,
        Unicode_12_0,
        Unicode_12_1,
        Unicode_13_0,
        Unicode_14_0,
        Unicode_15_0,
        Unicode_15_1,
        Unicode_16_0
    };

    /*!
        Returns the bidirectional class of this character.

        \sa hasMirrored(), QString::isRightToLeft()
    */
    Direction direction() const noexcept { return direction(char32_t(ucs)); }
    /*!
        Returns how this character joins its neighbours in cursive scripts.
    */
    JoiningType joiningType() const noexcept { return joiningType(char32_t(ucs)); }
    /*!
        Returns the script this character belongs to.
    */
    Script script() const noexcept { return script(char32_t(ucs)); }
    /*!
        Returns the Unicode version that assigned this character, or
        \c Unicode_Unassigned.

        \sa currentUnicodeVersion()
    */
    UnicodeVersion unicodeVersion() const noexcept { return unicodeVersion(char32_t(ucs)); }
    /*!
        Returns \c true if this character is mirrored when it appears in
        right-to-left text, such as a parenthesis; otherwise returns \c false.

        \sa mirroredChar()
    */
    bool hasMirrored() const noexcept { return hasMirrored(char32_t(ucs)); }
    /*!
        Returns the mirror image of this character, such as \c{)} for
        \c{(}, or the character itself if it has none.

        \sa hasMirrored()
    */
    QChar mirroredChar() const noexcept { return QChar(static_cast<char16_t>(mirroredChar(char32_t(ucs)))); }
    static Direction direction(char32_t ucs4) noexcept;
    static JoiningType joiningType(char32_t ucs4) noexcept;
    static Script script(char32_t ucs4) noexcept;
    static UnicodeVersion unicodeVersion(char32_t ucs4) noexcept;
    static UnicodeVersion currentUnicodeVersion() noexcept;
    static bool hasMirrored(char32_t ucs4) noexcept;
    static char32_t mirroredChar(char32_t ucs4) noexcept;
    /*!
        Returns \c true if this is a decimal digit of any script (category
        Number_DecimalDigit); otherwise returns \c false.

        \sa isNumber(), digitValue()
    */
    bool isDigit() const noexcept { return isDigit(char32_t(ucs)); }
    /*!
        Returns \c true if this is a number (any Number_* category); otherwise
        returns \c false.

        \sa isDigit(), isLetterOrNumber()
    */
    bool isNumber() const noexcept { return isNumber(char32_t(ucs)); }
    /*!
        Returns \c true if this is a letter (any Letter_* category); otherwise
        returns \c false.

        \sa isLetterOrNumber(), isUpper(), isLower()
    */
    bool isLetter() const noexcept { return isLetter(char32_t(ucs)); }
    /*!
        Returns \c true if this is a letter or a number; otherwise returns
        \c false.

        \sa isLetter(), isNumber()
    */
    bool isLetterOrNumber() const noexcept { return isLetterOrNumber(char32_t(ucs)); }
    /*!
        Returns \c true if this is a white space character: a Separator_*
        character, U+0009 to U+000D or U+0085; otherwise returns \c false.
    */
    bool isSpace() const noexcept { return isSpace(char32_t(ucs)); }
    /*!
        Returns \c true if this is an uppercase letter (Letter_Uppercase);
        otherwise returns \c false.

        \sa isLower(), isTitleCase(), toUpper()
    */
    bool isUpper() const noexcept { return isUpper(char32_t(ucs)); }
    /*!
        Returns \c true if this is a lowercase letter (Letter_Lowercase);
        otherwise returns \c false.

        \sa isUpper(), toLower()
    */
    bool isLower() const noexcept { return isLower(char32_t(ucs)); }
    /*!
        Returns \c true if this is a titlecase letter (Letter_Titlecase), such
        as U+01C5; otherwise returns \c false.

        \sa toTitleCase()
    */
    bool isTitleCase() const noexcept { return isTitleCase(char32_t(ucs)); }
    /*!
        Returns \c true if this is a punctuation character (any Punctuation_*
        category); otherwise returns \c false.
    */
    bool isPunct() const noexcept { return isPunct(char32_t(ucs)); }
    /*!
        Returns \c true if this is a combining mark (any Mark_* category);
        otherwise returns \c false.
    */
    bool isMark() const noexcept { return isMark(char32_t(ucs)); }
    /*!
        Returns \c true if this is a symbol (any Symbol_* category); otherwise
        returns \c false.
    */
    bool isSymbol() const noexcept { return isSymbol(char32_t(ucs)); }
    /*!
        Returns \c true if this is a printable character, that is, neither a
        control character nor an unassigned code point; otherwise returns
        \c false.
    */
    bool isPrint() const noexcept { return isPrint(char32_t(ucs)); }
    /*!
        Returns \c true if this is a UTF-16 high surrogate (U+D800 to U+DBFF);
        otherwise returns \c false.

        \sa isLowSurrogate(), isSurrogate()
    */
    bool isHighSurrogate() const noexcept { return (ucs & 0xfc00) == 0xd800; }
    /*!
        Returns \c true if this is a UTF-16 low surrogate (U+DC00 to U+DFFF);
        otherwise returns \c false.

        \sa isHighSurrogate(), isSurrogate()
    */
    bool isLowSurrogate() const noexcept { return (ucs & 0xfc00) == 0xdc00; }
    /*!
        Returns \c true if this is a UTF-16 surrogate (U+D800 to U+DFFF);
        otherwise returns \c false.

        \sa isHighSurrogate(), isLowSurrogate()
    */
    bool isSurrogate() const noexcept { return (ucs & 0xf800) == 0xd800; }
    /*!
        Returns \c true if this is a noncharacter, such as U+FFFE; otherwise
        returns \c false.
    */
    bool isNonCharacter() const noexcept { return isNonCharacter(char32_t(ucs)); }
    /*!
        Returns the value of this character if it is a decimal digit of any
        script; otherwise returns -1.

        \sa isDigit()
    */
    int digitValue() const noexcept { return digitValue(char32_t(ucs)); }
    /*!
        Returns the lowercase equivalent of this character, or the character
        itself if it has none. The mapping is the simple (one-to-one) mapping
        of Unicode.

        \sa toUpper(), toCaseFolded(), QString::toLower()
    */
    QChar toLower() const noexcept { return QChar(static_cast<char16_t>(toLower(char32_t(ucs)))); }
    /*!
        Returns the uppercase equivalent of this character, or the character
        itself if it has none. The mapping is the simple (one-to-one) mapping
        of Unicode, so U+00DF stays as it is; QString::toUpper() uses the full
        mapping.

        \sa toLower(), toTitleCase(), QString::toUpper()
    */
    QChar toUpper() const noexcept { return QChar(static_cast<char16_t>(toUpper(char32_t(ucs)))); }
    /*!
        Returns the titlecase equivalent of this character, or the character
        itself if it has none.

        \sa toUpper(), isTitleCase()
    */
    QChar toTitleCase() const noexcept { return QChar(static_cast<char16_t>(toTitleCase(char32_t(ucs)))); }
    /*!
        Returns the case folded equivalent of this character, which is used
        to compare text regardless of case.

        \sa toLower(), QString::toCaseFolded()
    */
    QChar toCaseFolded() const noexcept { return QChar(static_cast<char16_t>(toCaseFolded(char32_t(ucs)))); }

    static Category category(char32_t ucs4) noexcept;
    static bool isDigit(char32_t ucs4) noexcept;
    static bool isNumber(char32_t ucs4) noexcept;
    static bool isLetter(char32_t ucs4) noexcept;
    static bool isLetterOrNumber(char32_t ucs4) noexcept;
    static bool isSpace(char32_t ucs4) noexcept;
    static bool isUpper(char32_t ucs4) noexcept;
    static bool isLower(char32_t ucs4) noexcept;
    static bool isTitleCase(char32_t ucs4) noexcept;
    static bool isPunct(char32_t ucs4) noexcept;
    static bool isMark(char32_t ucs4) noexcept;
    static bool isSymbol(char32_t ucs4) noexcept;
    static bool isPrint(char32_t ucs4) noexcept;
    static int digitValue(char32_t ucs4) noexcept;
    static char32_t toLower(char32_t ucs4) noexcept;
    static char32_t toUpper(char32_t ucs4) noexcept;
    static char32_t toTitleCase(char32_t ucs4) noexcept;
    static char32_t toCaseFolded(char32_t ucs4) noexcept;

    /*!
        Returns \c true if \a ucs4 is a noncharacter (U+FDD0 to U+FDEF, or a
        code point ending in FFFE or FFFF); otherwise returns \c false.
    */
    static constexpr bool isNonCharacter(char32_t ucs4) noexcept {
        return ucs4 >= 0xfdd0 && (ucs4 <= 0xfdef || (ucs4 & 0xfffe) == 0xfffe);
    }
    /*!
        Returns \c true if \a ucs4 is a high surrogate; otherwise returns
        \c false.
    */
    static constexpr bool isHighSurrogate(char32_t ucs4) noexcept { return (ucs4 & 0xfffffc00) == 0xd800; }
    /*!
        Returns \c true if \a ucs4 is a low surrogate; otherwise returns
        \c false.
    */
    static constexpr bool isLowSurrogate(char32_t ucs4) noexcept { return (ucs4 & 0xfffffc00) == 0xdc00; }
    /*!
        Returns \c true if \a ucs4 is a surrogate; otherwise returns \c false.
    */
    static constexpr bool isSurrogate(char32_t ucs4) noexcept { return ucs4 - 0xd800u < 2048u; }
    /*!
        Returns \c true if \a ucs4 lies outside the Basic Multilingual Plane
        and so needs a surrogate pair in UTF-16; otherwise returns \c false.
    */
    static constexpr bool requiresSurrogates(char32_t ucs4) noexcept { return ucs4 >= 0x10000; }
    /*!
        Returns the code point of the surrogate pair \a high and \a low.
    */
    static constexpr char32_t surrogateToUcs4(char16_t high, char16_t low) noexcept {
        return (char32_t(high) << 10) + low - 0x35fdc00;
    }
    /*!
        \overload
        Returns the code point of the surrogate pair \a high and \a low.
    */
    static constexpr char32_t surrogateToUcs4(QChar high, QChar low) noexcept {
        return surrogateToUcs4(high.ucs, low.ucs);
    }
    /*!
        Returns the high surrogate of the code point \a ucs4, which must
        require surrogates.
    */
    static constexpr char16_t highSurrogate(char32_t ucs4) noexcept { return char16_t((ucs4 >> 10) + 0xd7c0); }
    /*!
        Returns the low surrogate of the code point \a ucs4, which must require
        surrogates.
    */
    static constexpr char16_t lowSurrogate(char32_t ucs4) noexcept { return char16_t(ucs4 % 0x400 + 0xdc00); }
    /*!
        \since 6.0

        Returns the UTF-16 code units of the code point \a c: one, or a
        surrogate pair. The result converts to QStringView, and has
        \c size(), \c data(), \c begin() and \c end():

        \code
        QString s = "smile ";
        s += QStringView(QChar::fromUcs4(U'\U0001F600'));   // two code units
        \endcode
    */
    static constexpr QtPrivate::Ucs4Chars fromUcs4(char32_t c) noexcept {
        if (requiresSurrogates(c))
            return {{highSurrogate(c), lowSurrogate(c)}, true};
        return {{char16_t(c), u'\0'}, false};
    }

    friend constexpr bool operator==(QChar a, QChar b) noexcept { return a.ucs == b.ucs; }
    friend constexpr bool operator!=(QChar a, QChar b) noexcept { return a.ucs != b.ucs; }
    friend constexpr bool operator<(QChar a, QChar b) noexcept { return a.ucs < b.ucs; }
    friend constexpr bool operator>(QChar a, QChar b) noexcept { return a.ucs > b.ucs; }
    friend constexpr bool operator<=(QChar a, QChar b) noexcept { return a.ucs <= b.ucs; }
    friend constexpr bool operator>=(QChar a, QChar b) noexcept { return a.ucs >= b.ucs; }

private:
    char16_t ucs = 0;
};

static_assert(sizeof(QChar) == sizeof(char16_t), "QChar must be 16 bits");

namespace QtPrivate {
// Returns true for the ASCII digits 0 to 9 only, as number parsers need.
constexpr bool isAsciiDigit(QChar c) noexcept { return c.unicode() >= u'0' && c.unicode() <= u'9'; }
} // namespace QtPrivate

template <>
struct std::hash<QChar> {
    std::size_t operator()(QChar c) const noexcept { return std::hash<char16_t>{}(c.unicode()); }
};
