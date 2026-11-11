// MIT License - see LICENSE
#include <QtGui/qtextformat.h>

/*!
    \class QTextFormat
    \brief The QTextFormat class provides formatting information for a QTextDocument.

    A format is a set of properties, each of them a number from Property
    with a value. A property that is not set is left to whatever the format
    is applied on top of: merge() adds the properties of another format to
    this one. The subclasses have functions for the properties that go with
    characters (QTextCharFormat), paragraphs (QTextBlockFormat) and lists
    (QTextListFormat).

    \differences
    Only the properties of Property are known, and a format is one of the
    three kinds named here: there are no frame, table, image or table cell
    formats, no object index, no pen properties and no \c colorProperty()
    or \c lengthProperty(). Formats are plain values that are not shared.
    \enddifferences

    \sa QTextCharFormat, QTextBlockFormat, QTextDocument
*/

/*!
    \class QTextCharFormat
    \brief The QTextCharFormat class provides formatting information for characters in a QTextDocument.

    A character format says how a piece of text looks: its font family,
    size, weight and slant, whether it is underlined or struck out, its
    colors, and whether it is a link.

    \code
    QTextCharFormat warning;
    warning.setFontWeight(QFont::Bold);
    warning.setForeground(QBrush(Qt::red));
    \endcode

    \differences
    There are no outlines, tool tips, vertical alignments (subscript and
    superscript), letter and word spacing, capitalization, underline styles
    and colors, and no anchor names. The font is made of the properties
    that Nt's QFont has.
    \enddifferences

    \sa QTextFormat, QTextLayout, QTextDocument
*/

/*!
    \class QTextBlockFormat
    \brief The QTextBlockFormat class provides formatting information for blocks of text in a QTextDocument.

    A block is a paragraph. Its format says how its lines are aligned, how
    much room there is around it, how far it is indented, and whether it is
    a heading.

    \differences
    There are no line heights, tab positions, page breaks or markers, and
    no setting to keep a block on one line.
    \enddifferences

    \sa QTextFormat, QTextDocument
*/

/*!
    \class QTextListFormat
    \brief The QTextListFormat class provides formatting information for lists in a QTextDocument.

    The format of a list says what its items are marked with and how far
    the list is indented.

    \differences
    There are no Roman numerals and no number prefix, suffix or start
    value.
    \enddifferences

    \sa QTextFormat, QTextDocument
*/

/*!
    Constructs an invalid format, of the type \c InvalidFormat.
*/
QTextFormat::QTextFormat() : m_type(InvalidFormat) {}

/*!
    Constructs a format of the type \a type, one of FormatType, without
    properties.
*/
QTextFormat::QTextFormat(int type) : m_type(type) {}

/*!
    Adds the properties of \a other to this format. A property that both
    have gets the value that it has in \a other.
*/
void QTextFormat::merge(const QTextFormat &other) {
    for (auto it = other.m_properties.constBegin(); it != other.m_properties.constEnd(); ++it)
        m_properties.insert(it.key(), it.value());
}

/*!
    \fn bool QTextFormat::isValid() const

    Returns \c true if the format has a type; a default-constructed one
    has none.
*/

/*!
    \fn bool QTextFormat::isEmpty() const

    Returns \c true if the format has no properties.
*/

/*!
    \fn int QTextFormat::type() const

    Returns the type of the format, one of FormatType.
*/

/*!
    Returns the value of the property \a propertyId as a \c bool, or
    \c false if it is not set.
*/
bool QTextFormat::boolProperty(int propertyId) const { return m_properties.value(propertyId).toBool(); }

/*!
    Returns the value of the property \a propertyId as an \c int, or 0 if
    it is not set.
*/
int QTextFormat::intProperty(int propertyId) const { return m_properties.value(propertyId).toInt(); }

/*!
    Returns the value of the property \a propertyId as a number, or 0 if
    it is not set.
*/
qreal QTextFormat::doubleProperty(int propertyId) const { return m_properties.value(propertyId).toDouble(); }

/*!
    Returns the value of the property \a propertyId as a string, or an
    empty string if it is not set.
*/
QString QTextFormat::stringProperty(int propertyId) const { return m_properties.value(propertyId).toString(); }

/*!
    Returns the value of the property \a propertyId as a brush, or a brush
    of the style Qt::NoBrush if it is not set.
*/
QBrush QTextFormat::brushProperty(int propertyId) const {
    const QVariant value = m_properties.value(propertyId);
    return value.isValid() ? value.value<QBrush>() : QBrush(Qt::NoBrush);
}

/*!
    Returns the value of the property \a propertyId, or an invalid
    QVariant if it is not set.

    \sa setProperty(), hasProperty()
*/
QVariant QTextFormat::property(int propertyId) const { return m_properties.value(propertyId); }

/*!
    Sets the property \a propertyId to \a value. An invalid \a value
    clears the property.

    \sa property(), clearProperty()
*/
void QTextFormat::setProperty(int propertyId, const QVariant &value) {
    if (value.isValid())
        m_properties.insert(propertyId, value);
    else
        m_properties.remove(propertyId);
}

/*!
    Removes the property \a propertyId from the format.
*/
void QTextFormat::clearProperty(int propertyId) { m_properties.remove(propertyId); }

/*!
    Returns \c true if the property \a propertyId is set; otherwise
    returns \c false.
*/
bool QTextFormat::hasProperty(int propertyId) const { return m_properties.contains(propertyId); }

/*!
    \fn int QTextFormat::propertyCount() const

    Returns the number of properties that are set.
*/

/*!
    \fn QMap<int, QVariant> QTextFormat::properties() const

    Returns the properties that are set, with their values.
*/

/*!
    \fn bool QTextFormat::isCharFormat() const

    Returns \c true if this is the format of characters.
*/

/*!
    \fn bool QTextFormat::isBlockFormat() const

    Returns \c true if this is the format of a block.
*/

/*!
    \fn bool QTextFormat::isListFormat() const

    Returns \c true if this is the format of a list.
*/

/*!
    Returns the format as a QTextBlockFormat, with its properties, or an
    empty block format if it is not the format of a block.
*/
QTextBlockFormat QTextFormat::toBlockFormat() const {
    QTextBlockFormat format;
    if (isBlockFormat())
        format.merge(*this);
    return format;
}

/*!
    Returns the format as a QTextCharFormat, with its properties, or an
    empty character format if it is not the format of characters.
*/
QTextCharFormat QTextFormat::toCharFormat() const {
    QTextCharFormat format;
    if (isCharFormat())
        format.merge(*this);
    return format;
}

/*!
    Returns the format as a QTextListFormat, with its properties, or an
    empty list format if it is not the format of a list.
*/
QTextListFormat QTextFormat::toListFormat() const {
    QTextListFormat format;
    if (isListFormat())
        format.merge(*this);
    return format;
}

/*!
    \fn void QTextFormat::setBackground(const QBrush &brush)

    Sets the brush behind the text to \a brush.

    \sa background(), clearBackground()
*/

/*!
    \fn QBrush QTextFormat::background() const

    Returns the brush behind the text; its style is Qt::NoBrush if none is
    set.
*/

/*!
    \fn void QTextFormat::clearBackground()

    Takes the background brush away.
*/

/*!
    \fn void QTextFormat::setForeground(const QBrush &brush)

    Sets the brush that the text is drawn with to \a brush.

    \sa foreground(), clearForeground()
*/

/*!
    \fn QBrush QTextFormat::foreground() const

    Returns the brush that the text is drawn with; its style is
    Qt::NoBrush if none is set, and the text then has the color of its
    surroundings.
*/

/*!
    \fn void QTextFormat::clearForeground()

    Takes the foreground brush away.
*/

/*!
    Returns \c true if this format has the same type and the same
    properties with the same values as \a other; otherwise returns
    \c false.
*/
bool QTextFormat::operator==(const QTextFormat &other) const {
    if (m_type != other.m_type || m_properties.size() != other.m_properties.size())
        return false;
    auto a = m_properties.constBegin();
    auto b = other.m_properties.constBegin();
    for (; a != m_properties.constEnd(); ++a, ++b) {
        if (a.key() != b.key() || a.value().typeId() != b.value().typeId())
            return false;
        // Brushes have no registered comparison: compare what they are made of.
        if (a.value().typeId() == QMetaType::fromType<QBrush>().id()) {
            const QBrush x = a.value().value<QBrush>();
            const QBrush y = b.value().value<QBrush>();
            if (x.style() != y.style() || x.color() != y.color())
                return false;
        } else if (a.value() != b.value()) {
            return false;
        }
    }
    return true;
}

/*!
    Constructs a character format without properties.
*/
QTextCharFormat::QTextCharFormat() : QTextFormat(CharFormat) {}

/*!
    Sets the font properties of the format to those of \a font: its
    families, point size, weight and slant.

    \sa font()
*/
void QTextCharFormat::setFont(const QFont &font) {
    if (!font.family().isEmpty())
        setFontFamilies(QStringList{font.family()});
    if (font.pointSizeF() > 0)
        setFontPointSize(font.pointSizeF());
    setFontWeight(font.weight());
    setFontItalic(font.italic());
}

/*!
    Returns a font made of the font properties of the format. A property
    that is not set leaves the default of QFont.

    \sa setFont()
*/
QFont QTextCharFormat::font() const {
    QFont f;
    const QStringList families = fontFamilies();
    if (!families.isEmpty())
        f.setFamily(families.first());
    if (hasProperty(FontPointSize) && fontPointSize() > 0)
        f.setPointSizeF(fontPointSize());
    if (hasProperty(FontWeight))
        f.setWeight(static_cast<QFont::Weight>(fontWeight()));
    if (hasProperty(FontItalic))
        f.setItalic(fontItalic());
    return f;
}

/*!
    \fn void QTextCharFormat::setFontFamilies(const QStringList &families)

    Sets the families of the font to \a families, in order of preference.
    The first one is used.
*/

/*!
    \fn QStringList QTextCharFormat::fontFamilies() const

    Returns the families of the font, or an empty list if none are set.
*/

/*!
    \fn void QTextCharFormat::setFontPointSize(qreal size)

    Sets the size of the font to \a size points.
*/

/*!
    \fn qreal QTextCharFormat::fontPointSize() const

    Returns the size of the font in points, or 0 if none is set.
*/

/*!
    \fn void QTextCharFormat::setFontWeight(int weight)

    Sets the weight of the font to \a weight, a QFont::Weight value.
*/

/*!
    \fn int QTextCharFormat::fontWeight() const

    Returns the weight of the font; QFont::Normal if none is set.
*/

/*!
    \fn void QTextCharFormat::setFontItalic(bool italic)

    Sets whether the font is italic to \a italic.
*/

/*!
    \fn bool QTextCharFormat::fontItalic() const

    Returns whether the font is italic.
*/

/*!
    \fn void QTextCharFormat::setFontUnderline(bool underline)

    Sets whether the text is underlined to \a underline.
*/

/*!
    \fn bool QTextCharFormat::fontUnderline() const

    Returns whether the text is underlined.
*/

/*!
    \fn void QTextCharFormat::setFontStrikeOut(bool strikeOut)

    Sets whether the text is struck out to \a strikeOut.
*/

/*!
    \fn bool QTextCharFormat::fontStrikeOut() const

    Returns whether the text is struck out.
*/

/*!
    \fn void QTextCharFormat::setFontFixedPitch(bool fixedPitch)

    Sets whether the text is in a font of fixed pitch to \a fixedPitch.
    Such text is drawn in the \c monospace family unless families are set.
*/

/*!
    \fn bool QTextCharFormat::fontFixedPitch() const

    Returns whether the text is in a font of fixed pitch.
*/

/*!
    \fn void QTextCharFormat::setAnchor(bool anchor)

    Sets whether the text is a link to \a anchor.

    \sa setAnchorHref()
*/

/*!
    \fn bool QTextCharFormat::isAnchor() const

    Returns whether the text is a link.
*/

/*!
    \fn void QTextCharFormat::setAnchorHref(const QString &value)

    Sets the target of the link to \a value, a URL as text.
*/

/*!
    \fn QString QTextCharFormat::anchorHref() const

    Returns the target of the link, or an empty string.
*/

/*!
    Constructs a block format without properties.
*/
QTextBlockFormat::QTextBlockFormat() : QTextFormat(BlockFormat) {}

/*!
    \fn void QTextBlockFormat::setAlignment(Qt::Alignment alignment)

    Sets how the lines of the block are aligned to \a alignment.
*/

/*!
    Returns how the lines of the block are aligned; to the left if nothing
    is set.
*/
Qt::Alignment QTextBlockFormat::alignment() const {
    const int value = intProperty(BlockAlignment);
    return value == 0 ? Qt::Alignment(Qt::AlignLeft) : Qt::Alignment(static_cast<Qt::AlignmentFlag>(value));
}

/*!
    \fn void QTextBlockFormat::setTopMargin(qreal margin)

    Sets the space above the block to \a margin pixels.
*/

/*!
    \fn qreal QTextBlockFormat::topMargin() const

    Returns the space above the block.
*/

/*!
    \fn void QTextBlockFormat::setBottomMargin(qreal margin)

    Sets the space below the block to \a margin pixels.
*/

/*!
    \fn qreal QTextBlockFormat::bottomMargin() const

    Returns the space below the block.
*/

/*!
    \fn void QTextBlockFormat::setLeftMargin(qreal margin)

    Sets the space left of the block to \a margin pixels.
*/

/*!
    \fn qreal QTextBlockFormat::leftMargin() const

    Returns the space left of the block.
*/

/*!
    \fn void QTextBlockFormat::setRightMargin(qreal margin)

    Sets the space right of the block to \a margin pixels.
*/

/*!
    \fn qreal QTextBlockFormat::rightMargin() const

    Returns the space right of the block.
*/

/*!
    \fn void QTextBlockFormat::setTextIndent(qreal indent)

    Sets the indentation of the first line of the block to \a indent
    pixels.
*/

/*!
    \fn qreal QTextBlockFormat::textIndent() const

    Returns the indentation of the first line of the block.
*/

/*!
    \fn void QTextBlockFormat::setIndent(int indent)

    Sets the number of steps that the block is indented by to \a indent;
    a step is 40 pixels.
*/

/*!
    \fn int QTextBlockFormat::indent() const

    Returns the number of steps that the block is indented by.
*/

/*!
    \fn void QTextBlockFormat::setHeadingLevel(int level)

    Marks the block as a heading of the level \a level, 1 to 6, or as an
    ordinary paragraph with 0.
*/

/*!
    \fn int QTextBlockFormat::headingLevel() const

    Returns the level of the heading that the block is, or 0.
*/

/*!
    Constructs a list format without properties.
*/
QTextListFormat::QTextListFormat() : QTextFormat(ListFormat) {}

/*!
    \fn void QTextListFormat::setStyle(Style style)

    Sets what the items of the list are marked with to \a style.
*/

/*!
    \fn QTextListFormat::Style QTextListFormat::style() const

    Returns what the items of the list are marked with.
*/

/*!
    \fn void QTextListFormat::setIndent(int indent)

    Sets the number of steps that the list is indented by to \a indent.
*/

/*!
    \fn int QTextListFormat::indent() const

    Returns the number of steps that the list is indented by.
*/
