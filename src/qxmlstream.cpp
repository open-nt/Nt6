// MIT License - see LICENSE
#include <QtCore/qxmlstream.h>

#include <QtCore/qlogging.h>
#include <QtCore/qstringconverter.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

/*!
    \class QXmlStreamReader
    \brief The QXmlStreamReader class provides a fast parser for reading well-formed XML.

    The reader reads a document as a stream of tokens. The program asks for the next token with
    readNext(), looks at what it is, and asks for the next: it pulls the tokens, which lets the
    code that reads a document be shaped like the document.

    \code
    QXmlStreamReader xml(&file);
    if (xml.readNextStartElement() && xml.name() == u"library") {
        while (xml.readNextStartElement()) {
            if (xml.name() == u"book")
                addBook(xml.attributes().value("isbn").toString(), xml.readElementText());
            else
                xml.skipCurrentElement();
        }
    }
    if (xml.hasError())
        qWarning() << xml.errorString() << "at line" << xml.lineNumber();
    \endcode

    The reader takes a device, or data that the program adds with addData(), also piece by piece
    as it arrives from a network: see readNext(). It checks that the document is well-formed and
    stops with an error at the first place where it is not.

    Namespaces are processed unless setNamespaceProcessing() turns that off. The references to
    characters and to the entities that the document declares are replaced in text() and in the
    values of attributes.

    \differences
    The functions that take text take a \c{const QString &}; there are no QAnyStringView overloads.
    Bytes are decoded as UTF-8, UTF-16 or Latin-1; a declaration that names another encoding is an
    error. The document type definition is read for its entities and notations only: attribute
    defaults are not applied, so QXmlStreamAttribute::isDefault() is always \c false, and
    parameter entities are not expanded. External entities are never fetched, and
    QXmlStreamEntityResolver::resolveEntity() is not called. Where the text of an entity is put
    into the document, lineNumber() and columnNumber() count its characters too. After the end of
    the document, readNext() returns \c Invalid without an error. readRawInnerData() writes the content
    out again from its tokens, so entity references come back expanded and attributes in double quotes. The names of elements are
    checked against a simplified set of the characters that XML allows in a name.
    \enddifferences

    \sa QXmlStreamWriter, QXmlStreamAttributes
*/

/*!
    \enum QXmlStreamReader::TokenType

    The kinds of token that readNext() reports.

    \value NoToken The reader has not read anything yet.
    \value Invalid An error has occurred; see error() and errorString().
    \value StartDocument The start of the document, with documentVersion() and documentEncoding() if it has an XML declaration.
    \value EndDocument The end of the document.
    \value StartElement The start of an element, with its name(), namespaceUri(), attributes() and namespaceDeclarations(). An empty-element tag is reported as a start and an end.
    \value EndElement The end of an element, with its name() and namespaceUri().
    \value Characters Text, in text(). isWhitespace() and isCDATA() say more about it.
    \value Comment A comment, in text().
    \value DTD The document type definition, as written in text(), with dtdName(), notationDeclarations() and entityDeclarations().
    \value EntityReference A reference to an entity that could not be replaced, with the name of the entity in name().
    \value ProcessingInstruction A processing instruction, with processingInstructionTarget() and processingInstructionData().
*/

/*!
    \enum QXmlStreamReader::ReadElementTextBehaviour

    What readElementText() does when the element has child elements.

    \value ErrorOnUnexpectedElement Raise \c UnexpectedElementError and return what was read so far.
    \value IncludeChildElements Add the text of the child elements to the result.
    \value SkipChildElements Leave the child elements and their text out.
*/

/*!
    \enum QXmlStreamReader::Error

    The kinds of error that the reader reports.

    \value NoError No error has occurred.
    \value UnexpectedElementError The reader met an element where it expected something else.
    \value CustomError The program raised the error with raiseError().
    \value NotWellFormedError The document is not well-formed XML.
    \value PrematureEndOfDocumentError The input ended before the document did. Add more data and go on.
*/

/*!
    \class QXmlStreamWriter
    \brief The QXmlStreamWriter class provides an XML writer with a simple streaming API.

    The writer is the counterpart of QXmlStreamReader: the program writes a document token by
    token, and the writer takes care of the syntax. It escapes text and values, keeps track of
    the elements that are open, and declares namespaces.

    \code
    QXmlStreamWriter xml(&file);
    xml.setAutoFormatting(true);
    xml.writeStartDocument();
    xml.writeStartElement("library");
    xml.writeStartElement("book");
    xml.writeAttribute("isbn", "0-201-63361-2");
    xml.writeCharacters("Design Patterns");
    xml.writeEndElement();
    xml.writeEndDocument();
    \endcode

    The writer does not check that what it is asked to write is a well-formed document: names
    are written as given, and it is for the program to write one root element.

    \differences
    The functions take a \c{const QString &}; there are no QAnyStringView overloads. Output to a
    device or a byte array is always UTF-8. A namespace that is declared with writeNamespace()
    before the element it is meant for is declared in that element when it starts. Every piece of output is
    checked for characters that XML does not allow, names included.
    \enddifferences

    \sa QXmlStreamReader
*/

/*!
    \class QXmlStreamAttribute
    \brief The QXmlStreamAttribute class represents a single XML attribute.

    An attribute has a qualified name, which with namespace processing splits into a prefix and
    a local name and stands in a namespace, and a value. QXmlStreamReader::attributes() returns
    those of a start element, and QXmlStreamWriter::writeAttribute() writes one.

    \differences
    The attribute holds its strings and does not refer to those of the reader. isDefault() is
    always \c false.
    \enddifferences

    \sa QXmlStreamAttributes
*/

/*!
    Constructs an empty attribute.
*/
QXmlStreamAttribute::QXmlStreamAttribute() = default;

/*!
    Constructs an attribute with the qualified name \a qualifiedName and the value \a value, in
    no namespace. Its local name is what follows the colon of the qualified name, if it has one.
*/
QXmlStreamAttribute::QXmlStreamAttribute(const QString &qualifiedName, const QString &value)
    : m_name(qualifiedName.mid(qualifiedName.indexOf(QChar(u':')) + 1)), m_qualifiedName(qualifiedName),
      m_value(value.isNull() ? QString("") : value) {}

/*!
    Constructs an attribute with the local name \a name in the namespace \a namespaceUri and the
    value \a value.
*/
QXmlStreamAttribute::QXmlStreamAttribute(const QString &namespaceUri, const QString &name, const QString &value)
    : m_namespaceUri(namespaceUri), m_name(name), m_qualifiedName(name), m_value(value.isNull() ? QString("") : value) {
}

/*!
    \class QXmlStreamAttributes
    \brief The QXmlStreamAttributes class represents a vector of QXmlStreamAttribute.

    The class is a QList of attributes with functions that find the value of one by its name.

    \code
    const QXmlStreamAttributes attributes = xml.attributes();
    if (attributes.hasAttribute("width"))
        width = attributes.value("width").toInt();
    \endcode

    \differences
    The names are \c{const QString &}; there are no QAnyStringView overloads.
    \enddifferences

    \sa QXmlStreamReader::attributes()
*/

/*!
    Returns the value of the attribute with the local name \a name in the namespace
    \a namespaceUri, or a null view if there is none.
*/
QStringView QXmlStreamAttributes::value(const QString &namespaceUri, const QString &name) const {
    for (const QXmlStreamAttribute &attribute : *this) {
        if (attribute.name() == QStringView(name) && attribute.namespaceUri() == QStringView(namespaceUri))
            return attribute.value();
    }
    return QStringView();
}

/*!
    \overload
    Returns the value of the attribute with the qualified name \a qualifiedName, or a null view
    if there is none.
*/
QStringView QXmlStreamAttributes::value(const QString &qualifiedName) const {
    for (const QXmlStreamAttribute &attribute : *this) {
        if (attribute.qualifiedName() == QStringView(qualifiedName))
            return attribute.value();
    }
    return QStringView();
}

/*!
    Appends an attribute with the local name \a name in the namespace \a namespaceUri and the
    value \a value.
*/
void QXmlStreamAttributes::append(const QString &namespaceUri, const QString &name, const QString &value) {
    append(QXmlStreamAttribute(namespaceUri, name, value));
}

/*!
    \overload
    Appends an attribute with the qualified name \a qualifiedName and the value \a value.
*/
void QXmlStreamAttributes::append(const QString &qualifiedName, const QString &value) {
    append(QXmlStreamAttribute(qualifiedName, value));
}

/*!
    \class QXmlStreamNamespaceDeclaration
    \brief The QXmlStreamNamespaceDeclaration class represents a namespace declaration.

    A declaration binds a prefix to a namespace URI for an element and what is inside it.
    QXmlStreamReader::namespaceDeclarations() returns those of a start element.

    \differences
    The declaration holds its strings and does not refer to those of the reader.
    \enddifferences

    \sa QXmlStreamReader::addExtraNamespaceDeclaration()
*/

/*!
    Constructs an empty declaration.
*/
QXmlStreamNamespaceDeclaration::QXmlStreamNamespaceDeclaration() = default;

/*!
    Constructs a declaration that binds \a prefix to \a namespaceUri. An empty prefix declares
    the default namespace.
*/
QXmlStreamNamespaceDeclaration::QXmlStreamNamespaceDeclaration(const QString &prefix, const QString &namespaceUri)
    : m_prefix(prefix), m_namespaceUri(namespaceUri) {}

/*!
    \class QXmlStreamNotationDeclaration
    \brief The QXmlStreamNotationDeclaration class represents a DTD notation declaration.

    A notation names a format of data that is not XML. QXmlStreamReader::notationDeclarations()
    returns those that the document type definition declares.

    \differences
    The declaration holds its strings and does not refer to those of the reader.
    \enddifferences
*/

/*!
    Constructs an empty declaration.
*/
QXmlStreamNotationDeclaration::QXmlStreamNotationDeclaration() = default;

/*!
    \class QXmlStreamEntityDeclaration
    \brief The QXmlStreamEntityDeclaration class represents a DTD entity declaration.

    QXmlStreamReader::entityDeclarations() returns the unparsed entities that the document type
    definition declares: data outside the document, of the kind that notationName() names.

    \differences
    The declaration holds its strings and does not refer to those of the reader.
    \enddifferences
*/

/*!
    Constructs an empty declaration.
*/
QXmlStreamEntityDeclaration::QXmlStreamEntityDeclaration() = default;

/*!
    \class QXmlStreamEntityResolver
    \brief The QXmlStreamEntityResolver class provides an entity resolver for a QXmlStreamReader.

    A program that knows the entities of a document type that the document itself does not
    declare, as those of HTML, subclasses the resolver, returns their text from
    resolveUndeclaredEntity(), and gives the resolver to the reader with
    QXmlStreamReader::setEntityResolver().

    \differences
    resolveEntity() is never called, as the reader does not read external entities.
    \enddifferences
*/

/*!
    Destroys the resolver.
*/
QXmlStreamEntityResolver::~QXmlStreamEntityResolver() = default;

/*!
    Returns the text of the external entity with the identifiers \a publicId and \a systemId.
    The default implementation returns an empty string.
*/
QString QXmlStreamEntityResolver::resolveEntity(const QString &publicId, const QString &systemId) {
    Q_UNUSED(publicId)
    Q_UNUSED(systemId)
    return QString();
}

/*!
    Returns the text that stands for the entity \a name, which the document does not declare, or
    an empty string if the resolver does not know it. The text is read as part of the document,
    so it may hold elements. The default implementation returns an empty string.
*/
QString QXmlStreamEntityResolver::resolveUndeclaredEntity(const QString &name) {
    Q_UNUSED(name)
    return QString();
}

namespace {

using Str = std::u16string;
enum class Step { Done, More, Fail };

const char16_t kXmlNamespace[] = u"http://www.w3.org/XML/1998/namespace";

bool isXmlSpace(char16_t c) { return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r'; }

// The characters that may start a name, and those that may follow. Outside ASCII the ranges of XML are wide, and
// what is excluded here are the signs, spaces and punctuation among them.
bool isNameStart(char16_t c) {
    if (c < 0x80)
        return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c == u'_' || c == u':';
    if (c < 0xC0 || c == 0xD7 || c == 0xF7 || c >= 0xFFFE)
        return false;
    if (c >= 0x2000 && c <= 0x206F)
        return c == 0x200C || c == 0x200D;
    return !(c >= 0x2190 && c <= 0x2BFF) && !(c >= 0x3000 && c <= 0x3000);
}
bool isNameChar(char16_t c) {
    return isNameStart(c) || (c >= u'0' && c <= u'9') || c == u'-' || c == u'.' || c == 0xB7 || c == 0x203F ||
           c == 0x2040;
}
bool isName(const Str &s) {
    if (s.empty() || !isNameStart(s[0]))
        return false;
    return std::all_of(s.begin() + 1, s.end(), isNameChar);
}

// A character that a document may contain.
bool isXmlChar(char32_t c) {
    return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) ||
           (c >= 0x10000 && c <= 0x10FFFF);
}

QString toQ(const Str &s) {
    return s.empty() ? QString("")
                     : QString(reinterpret_cast<const QChar *>(s.data()), static_cast<qsizetype>(s.size()));
}

void appendCodePoint(Str &out, char32_t c) {
    if (c >= 0x10000) {
        out += static_cast<char16_t>(0xD800 + ((c - 0x10000) >> 10));
        out += static_cast<char16_t>(0xDC00 + ((c - 0x10000) & 0x3FF));
    } else {
        out += static_cast<char16_t>(c);
    }
}

// The number of a character reference, without "&#" and ";": decimal, or hexadecimal after an x.
bool parseCharacterReference(const Str &body, char32_t *out) {
    const bool hex = body.size() > 1 && body[1] == u'x';
    const std::size_t first = hex ? 2 : 1;
    if (body.size() <= first)
        return false;
    char32_t value = 0;
    for (std::size_t i = first; i < body.size(); ++i) {
        const char16_t c = body[i];
        char32_t digit = 0;
        if (c >= u'0' && c <= u'9')
            digit = static_cast<char32_t>(c - u'0');
        else if (hex && c >= u'a' && c <= u'f')
            digit = static_cast<char32_t>(c - u'a' + 10);
        else if (hex && c >= u'A' && c <= u'F')
            digit = static_cast<char32_t>(c - u'A' + 10);
        else
            return false;
        value = value * (hex ? 16 : 10) + digit;
        if (value > 0x10FFFF)
            return false;
    }
    *out = value;
    return isXmlChar(value);
}

// The five entities that every document has.
bool predefinedEntity(const Str &name, char16_t *out) {
    static const struct {
        const char16_t *name;
        char16_t value;
    } entities[] = {{u"lt", u'<'}, {u"gt", u'>'}, {u"amp", u'&'}, {u"apos", u'\''}, {u"quot", u'"'}};
    for (const auto &entity : entities) {
        if (name == entity.name) {
            *out = entity.value;
            return true;
        }
    }
    return false;
}

} // namespace

struct QXmlStreamReader::Private {
    struct Entity {
        Str value;
        bool external = false;
    };
    struct Element {
        Str qualifiedName;
        QString name;
        QString prefix;
        QString namespaceUri;
        std::size_t bindings = 0;
    };
    struct Binding {
        Str prefix;
        Str uri;
    };
    enum Stage { Prolog, Content, Epilog, Finished };

    // Input
    QIODevice *device = nullptr;
    QByteArray raw;
    std::unique_ptr<QStringDecoder> decoder;
    bool pendingCr = false;
    bool sawFirstCharacter = false;
    bool starved = false; // the input ended inside the document, and nothing has been added since
    Str buf;
    std::size_t pos = 0;
    qint64 line = 1;
    qint64 column = 0;
    qint64 offset = 0;

    // The document so far
    Stage stage = Prolog;
    bool started = false;
    bool seenDtd = false;
    bool pendingEnd = false;
    std::vector<Element> elements;
    std::vector<Binding> bindings;
    std::map<Str, Entity> entities;
    bool unknownEntitiesAllowed = false; // the DTD has an external subset or parameter entities
    std::size_t expansionEnd = 0;        // where in buf the text of the entity being expanded ends
    int expanded = 0;
    int expansionLimit = 4096;
    bool namespaceProcessing = true;
    QXmlStreamEntityResolver *resolver = nullptr;

    // The token
    TokenType type = NoToken;
    Error error = NoError;
    QString errorString;
    QString name, prefix, qualifiedName, namespaceUri, text, piTarget, piData;
    QString dtdName, dtdPublicId, dtdSystemId, documentVersion, documentEncoding;
    bool whitespace = false;
    bool cdata = false;
    bool standalone = false;
    bool hasStandalone = false;
    QXmlStreamAttributes attributes;
    QXmlStreamNamespaceDeclarations namespaceDeclarations;
    QXmlStreamNotationDeclarations notationDeclarations;
    QXmlStreamEntityDeclarations entityDeclarations;

    Private() { bindings.push_back({u"xml", kXmlNamespace}); }

    Step fail(const QString &message) {
        error = NotWellFormedError;
        errorString = message;
        return Step::Fail;
    }

    // Appends decoded characters, with every kind of line end made a line feed.
    void appendCharacters(const char16_t *characters, std::size_t count) {
        for (std::size_t i = 0; i < count; ++i) {
            const char16_t c = characters[i];
            if (!sawFirstCharacter) {
                sawFirstCharacter = true;
                if (c == 0xFEFF)
                    continue; // the byte order mark
            }
            if (pendingCr) {
                pendingCr = false;
                if (c == u'\n')
                    continue;
            }
            if (c == u'\r') {
                pendingCr = true;
                buf += u'\n';
            } else {
                buf += c;
            }
        }
    }

    // Chooses how the bytes are decoded: from a byte order mark, the pattern of the first characters, or the
    // encoding that the XML declaration names. Returns More if that takes more bytes than there are.
    Step chooseDecoder() {
        const auto *b = reinterpret_cast<const uchar *>(raw.constData());
        const qsizetype n = raw.size();
        QStringConverter::Encoding encoding = QStringConverter::Utf8;
        static const char declaration[] = "<?xml";
        // UTF-16 shows in its byte order mark, or in "<?" with a zero byte next to each character.
        const bool littleEndian = (n >= 2 && b[0] == 0xFF && b[1] == 0xFE) ||
                                  (n >= 4 && b[0] == '<' && b[1] == 0 && b[2] == '?' && b[3] == 0);
        const bool bigEndian = (n >= 2 && b[0] == 0xFE && b[1] == 0xFF) ||
                               (n >= 4 && b[0] == 0 && b[1] == '<' && b[2] == 0 && b[3] == '?');
        if (littleEndian) {
            encoding = QStringConverter::Utf16LE;
        } else if (bigEndian) {
            encoding = QStringConverter::Utf16BE;
        } else if (n < 5 && n > 0 && std::memcmp(b, declaration, static_cast<std::size_t>(n)) == 0) {
            return Step::More;
        } else if (raw.startsWith(declaration)) {
            const qsizetype end = raw.indexOf("?>");
            if (end < 0 && n < 1024)
                return Step::More;
            const QByteArray head = raw.left(end < 0 ? n : end);
            qsizetype at = head.indexOf("encoding");
            if (at >= 0) {
                at += 8;
                while (at < head.size() && (head.at(at) == ' ' || head.at(at) == '=' || head.at(at) == '\t'))
                    ++at;
                const char quote = at < head.size() ? head.at(at) : '\0';
                const qsizetype close = quote == '"' || quote == '\'' ? head.indexOf(quote, at + 1) : -1;
                if (close > at) {
                    const QByteArray label = head.mid(at + 1, close - at - 1);
                    const auto named = QStringConverter::encodingForName(QString::fromLatin1(label));
                    if (!named)
                        return fail(QString("Encoding ") + QString::fromLatin1(label) + QString(" is unsupported"));
                    // A document that is read byte by byte as ASCII cannot be in a 16 or 32 bit encoding.
                    if (*named == QStringConverter::Latin1 || *named == QStringConverter::System)
                        encoding = *named;
                }
            }
        }
        decoder = std::make_unique<QStringDecoder>(encoding);
        return Step::Done;
    }

    // Brings more characters into buf. Returns false if there are none to be had now.
    bool fetchMore() {
        if (device) {
            const QByteArray chunk = device->read(64 * 1024);
            raw.append(chunk);
        }
        if (raw.isEmpty())
            return false;
        if (!decoder && chooseDecoder() != Step::Done)
            return false;
        const QString decoded = decoder->decode(QByteArrayView(raw));
        raw.clear();
        const std::size_t before = buf.size();
        appendCharacters(decoded.utf16(), static_cast<std::size_t>(decoded.size()));
        return buf.size() > before || (device && !device->atEnd());
    }

    // Moves past what a token used, keeping count of the lines.
    void commit(std::size_t newPos) {
        for (std::size_t i = pos; i < newPos; ++i) {
            if (buf[i] == u'\n') {
                ++line;
                column = 0;
            } else {
                ++column;
            }
        }
        offset += static_cast<qint64>(newPos - pos);
        pos = newPos;
        if (pos > 64 * 1024) {
            buf.erase(0, pos);
            expansionEnd = expansionEnd > pos ? expansionEnd - pos : 0;
            pos = 0;
        }
    }

    void resetToken() {
        name.clear();
        prefix.clear();
        qualifiedName.clear();
        namespaceUri.clear();
        text.clear();
        piTarget.clear();
        piData.clear();
        whitespace = false;
        cdata = false;
        attributes.clear();
        namespaceDeclarations.clear();
    }

    // Compares buf at `at` with a literal: Done if it is there, More if buf ends inside it, Fail otherwise.
    Step expect(std::size_t at, const char16_t *literal) const {
        for (std::size_t i = 0; literal[i]; ++i) {
            if (at + i >= buf.size())
                return Step::More;
            if (buf[at + i] != literal[i])
                return Step::Fail;
        }
        return Step::Done;
    }

    // Reads a name at *i. More if buf ends before the name does.
    Step readName(std::size_t *i, Str *out) {
        std::size_t j = *i;
        if (j >= buf.size())
            return Step::More;
        if (!isNameStart(buf[j]))
            return fail(QString("Invalid XML name."));
        while (j < buf.size() && isNameChar(buf[j]))
            ++j;
        if (j >= buf.size())
            return Step::More;
        *out = buf.substr(*i, j - *i);
        *i = j;
        return Step::Done;
    }

    // Skips white space at *i. More if buf ends in it.
    Step skipSpace(std::size_t *i) const {
        while (*i < buf.size() && isXmlSpace(buf[*i]))
            ++*i;
        return *i >= buf.size() ? Step::More : Step::Done;
    }

    // Reads a quoted literal at *i, without its quotes.
    Step readLiteral(std::size_t *i, Str *out) {
        if (*i >= buf.size())
            return Step::More;
        const char16_t quote = buf[*i];
        if (quote != u'"' && quote != u'\'')
            return fail(QString("Expected a quoted string."));
        const std::size_t close = buf.find(quote, *i + 1);
        if (close == Str::npos)
            return Step::More;
        *out = buf.substr(*i + 1, close - *i - 1);
        *i = close + 1;
        return Step::Done;
    }

    const Entity *findEntity(const Str &entityName) const {
        const auto it = entities.find(entityName);
        return it == entities.end() ? nullptr : &it->second;
    }

    // Replaces the references in the value of an attribute, and makes its white space spaces.
    bool expandAttributeValue(const Str &in, Str *out, int depth) {
        for (std::size_t i = 0; i < in.size(); ++i) {
            const char16_t c = in[i];
            if (c == u'<') {
                fail(QString("'<' is not allowed in the value of an attribute."));
                return false;
            }
            if (c != u'&') {
                *out += isXmlSpace(c) ? u' ' : c;
                continue;
            }
            const std::size_t semi = in.find(u';', i + 1);
            if (semi == Str::npos || semi == i + 1) {
                fail(QString("Entity reference without a name or a semicolon."));
                return false;
            }
            const Str reference = in.substr(i + 1, semi - i - 1);
            i = semi;
            char16_t simple = 0;
            if (reference[0] == u'#') {
                char32_t code = 0;
                if (!parseCharacterReference(reference, &code)) {
                    fail(QString("Invalid character reference."));
                    return false;
                }
                appendCodePoint(*out, code);
            } else if (predefinedEntity(reference, &simple)) {
                *out += simple;
            } else {
                const Entity *entity = findEntity(reference);
                if (!entity || entity->external) {
                    fail(QString("Entity '") + toQ(reference) + QString("' not declared."));
                    return false;
                }
                if (depth >= 16 || static_cast<int>(out->size() + entity->value.size()) > expansionLimit * 16) {
                    fail(QString("Recursive entity detected."));
                    return false;
                }
                if (!expandAttributeValue(entity->value, out, depth + 1))
                    return false;
            }
        }
        return true;
    }

    const Binding *findBinding(const Str &bindingPrefix) const {
        for (auto it = bindings.rbegin(); it != bindings.rend(); ++it) {
            if (it->prefix == bindingPrefix)
                return &*it;
        }
        return nullptr;
    }

    // Splits a qualified name at its colon. With namespaces a name has at most one, and not at an end.
    bool splitName(const Str &qualified, Str *namePrefix, Str *local) {
        const std::size_t colon = namespaceProcessing ? qualified.find(u':') : Str::npos;
        if (colon == Str::npos) {
            namePrefix->clear();
            *local = qualified;
            return true;
        }
        if (colon == 0 || colon + 1 == qualified.size() || qualified.find(u':', colon + 1) != Str::npos) {
            fail(QString("Invalid qualified name '") + toQ(qualified) + QString("'."));
            return false;
        }
        *namePrefix = qualified.substr(0, colon);
        *local = qualified.substr(colon + 1);
        return true;
    }

    Step parseDeclaration();
    Step parseDocumentType();
    Step parseInternalSubset(std::size_t *i, std::map<Str, Entity> *declared, bool *parameterEntities,
                             QXmlStreamNotationDeclarations *notations, QXmlStreamEntityDeclarations *unparsed);
    Step parseExternalId(std::size_t *i, bool publicIdOnly, Str *publicId, Str *systemId, bool *found);
    Step parseComment();
    Step parseProcessingInstruction();
    Step parseCData();
    Step parseStartTag();
    Step parseEndTag();
    Step parseText();
    Step parseMarkup();
    Step parseToken();
    void endElement();
};

// <?xml version="1.0" encoding="..." standalone="yes"?> at the very start, or nothing: either way the document
// starts.
Step QXmlStreamReader::Private::parseDeclaration() {
    const Step head = expect(pos, u"<?xml");
    if (head == Step::More)
        return Step::More;
    std::size_t i = pos + 5;
    if (head == Step::Done && i >= buf.size())
        return Step::More;
    if (head != Step::Done || !isXmlSpace(buf[i])) {
        // No declaration; a processing instruction whose name starts with xml is left for later.
        started = true;
        type = StartDocument;
        return Step::Done;
    }
    QString version, encoding;
    bool alone = false, hasAlone = false;
    int seen = 0;
    for (;;) {
        const std::size_t before = i;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        const Step end = expect(i, u"?>");
        if (end == Step::More)
            return Step::More;
        if (end == Step::Done) {
            i += 2;
            break;
        }
        if (i == before)
            return fail(QString("Invalid XML declaration."));
        Str attribute, value;
        Step step = readName(&i, &attribute);
        if (step != Step::Done)
            return step == Step::Fail ? fail(QString("Invalid XML declaration.")) : step;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        if (buf[i] != u'=')
            return fail(QString("Invalid XML declaration."));
        ++i;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        step = readLiteral(&i, &value);
        if (step != Step::Done)
            return step == Step::Fail ? fail(QString("Invalid XML declaration.")) : step;
        // The pseudo-attributes come in this order, and the version is not optional.
        if (attribute == u"version" && seen == 0) {
            if (value != u"1.0")
                return fail(QString("Unsupported XML version."));
            version = toQ(value);
            seen = 1;
        } else if (attribute == u"encoding" && seen == 1) {
            const bool valid = !value.empty() && std::all_of(value.begin(), value.end(), [](char16_t c) {
                return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || (c >= u'0' && c <= u'9') || c == u'.' ||
                       c == u'_' || c == u'-';
            });
            if (!valid)
                return fail(toQ(value) + QString(" is an invalid encoding name."));
            encoding = toQ(value);
            seen = 2;
        } else if (attribute == u"standalone" && (seen == 1 || seen == 2)) {
            if (value != u"yes" && value != u"no")
                return fail(QString("Standalone accepts only yes or no."));
            alone = value == u"yes";
            hasAlone = true;
            seen = 3;
        } else {
            return fail(QString("Invalid attribute in XML declaration: ") + toQ(attribute));
        }
    }
    if (seen == 0)
        return fail(QString("Invalid XML declaration."));
    documentVersion = version;
    documentEncoding = encoding;
    standalone = alone;
    hasStandalone = hasAlone;
    started = true;
    type = StartDocument;
    commit(i);
    return Step::Done;
}

// SYSTEM "system" or PUBLIC "public" "system"; in a notation the system literal after PUBLIC is optional.
Step QXmlStreamReader::Private::parseExternalId(std::size_t *i, bool publicIdOnly, Str *publicId, Str *systemId,
                                                bool *found) {
    *found = false;
    const Step system = expect(*i, u"SYSTEM"), pub = expect(*i, u"PUBLIC");
    if (system == Step::More || pub == Step::More)
        return Step::More;
    if (system != Step::Done && pub != Step::Done)
        return Step::Done;
    std::size_t j = *i + 6;
    if (j >= buf.size())
        return Step::More;
    if (!isXmlSpace(buf[j]))
        return fail(QString("Invalid document type declaration."));
    if (skipSpace(&j) == Step::More)
        return Step::More;
    Step step = Step::Done;
    if (pub == Step::Done) {
        step = readLiteral(&j, publicId);
        if (step != Step::Done)
            return step;
        const std::size_t afterPublic = j;
        if (skipSpace(&j) == Step::More)
            return Step::More;
        if (buf[j] != u'"' && buf[j] != u'\'') {
            if (!publicIdOnly)
                return fail(QString("Invalid document type declaration."));
            *i = afterPublic;
            *found = true;
            return Step::Done;
        }
        if (j == afterPublic)
            return fail(QString("Invalid document type declaration."));
    }
    step = readLiteral(&j, systemId);
    if (step != Step::Done)
        return step;
    *i = j;
    *found = true;
    return Step::Done;
}

// The declarations between [ and ] of a document type declaration. *i is left at the ].
Step QXmlStreamReader::Private::parseInternalSubset(std::size_t *i, std::map<Str, Entity> *declared,
                                                    bool *parameterEntities, QXmlStreamNotationDeclarations *notations,
                                                    QXmlStreamEntityDeclarations *unparsed) {
    // Skips to the > that ends a declaration, which a quoted string may contain.
    const auto skipDeclaration = [this](std::size_t *at) {
        char16_t quote = 0;
        for (; *at < buf.size(); ++*at) {
            const char16_t c = buf[*at];
            if (quote) {
                if (c == quote)
                    quote = 0;
            } else if (c == u'"' || c == u'\'') {
                quote = c;
            } else if (c == u'>') {
                ++*at;
                return Step::Done;
            }
        }
        return Step::More;
    };
    for (;;) {
        if (skipSpace(i) == Step::More)
            return Step::More;
        const char16_t c = buf[*i];
        if (c == u']')
            return Step::Done;
        if (c == u'%') {
            // A parameter entity reference: what it stands for is not read, so anything may be declared there.
            const std::size_t semi = buf.find(u';', *i);
            if (semi == Str::npos)
                return Step::More;
            if (!isName(buf.substr(*i + 1, semi - *i - 1)))
                return fail(QString("Invalid parameter entity reference."));
            *parameterEntities = true;
            *i = semi + 1;
            continue;
        }
        if (c != u'<')
            return fail(QString("Invalid document type declaration."));
        Step step = expect(*i, u"<!--");
        if (step == Step::More)
            return Step::More;
        if (step == Step::Done) {
            const std::size_t end = buf.find(u"-->", *i + 4);
            if (end == Str::npos)
                return Step::More;
            *i = end + 3;
            continue;
        }
        step = expect(*i, u"<?");
        if (step == Step::More)
            return Step::More;
        if (step == Step::Done) {
            const std::size_t end = buf.find(u"?>", *i + 2);
            if (end == Str::npos)
                return Step::More;
            *i = end + 2;
            continue;
        }
        const Step entity = expect(*i, u"<!ENTITY"), notation = expect(*i, u"<!NOTATION");
        const Step element = expect(*i, u"<!ELEMENT"), attlist = expect(*i, u"<!ATTLIST");
        if (entity == Step::More || notation == Step::More || element == Step::More || attlist == Step::More)
            return Step::More;
        if (element == Step::Done || attlist == Step::Done) {
            if (skipDeclaration(i) == Step::More)
                return Step::More;
            continue;
        }
        if (entity != Step::Done && notation != Step::Done)
            return fail(QString("Invalid document type declaration."));

        std::size_t j = *i + (entity == Step::Done ? 8 : 10);
        if (j >= buf.size())
            return Step::More;
        if (!isXmlSpace(buf[j]))
            return fail(QString("Invalid document type declaration."));
        if (skipSpace(&j) == Step::More)
            return Step::More;
        bool parameter = false;
        if (entity == Step::Done && buf[j] == u'%') {
            parameter = true;
            ++j;
            if (skipSpace(&j) == Step::More)
                return Step::More;
        }
        Str declaredName;
        step = readName(&j, &declaredName);
        if (step != Step::Done)
            return step;
        if (!isXmlSpace(buf[j]))
            return fail(QString("Invalid document type declaration."));
        if (skipSpace(&j) == Step::More)
            return Step::More;

        Str publicId, systemId, value, notationName;
        bool external = false;
        step = parseExternalId(&j, notation == Step::Done, &publicId, &systemId, &external);
        if (step != Step::Done)
            return step;
        if (notation == Step::Done && !external)
            return fail(QString("Invalid document type declaration."));
        if (entity == Step::Done && !external) {
            Str literal;
            step = readLiteral(&j, &literal);
            if (step != Step::Done)
                return step;
            // Character references are replaced now; references to other entities when this one is used.
            for (std::size_t k = 0; k < literal.size(); ++k) {
                const std::size_t semi = literal[k] == u'&' ? literal.find(u';', k) : Str::npos;
                char32_t code = 0;
                if (semi != Str::npos && semi > k + 1 && literal[k + 1] == u'#') {
                    if (!parseCharacterReference(literal.substr(k + 1, semi - k - 1), &code))
                        return fail(QString("Invalid character reference."));
                    appendCodePoint(value, code);
                    k = semi;
                } else if (literal[k] == u'%') {
                    return fail(QString("Parameter entity references are not supported in an entity value."));
                } else {
                    value += literal[k];
                }
            }
        }
        if (skipSpace(&j) == Step::More)
            return Step::More;
        if (entity == Step::Done && external && !parameter) {
            const Step ndata = expect(j, u"NDATA");
            if (ndata == Step::More)
                return Step::More;
            if (ndata == Step::Done) {
                j += 5;
                if (skipSpace(&j) == Step::More)
                    return Step::More;
                step = readName(&j, &notationName);
                if (step != Step::Done)
                    return step;
                if (skipSpace(&j) == Step::More)
                    return Step::More;
            }
        }
        if (buf[j] != u'>')
            return fail(QString("Invalid document type declaration."));
        *i = j + 1;

        if (notation == Step::Done) {
            QXmlStreamNotationDeclaration declaration;
            declaration.m_name = toQ(declaredName);
            declaration.m_publicId = toQ(publicId);
            declaration.m_systemId = toQ(systemId);
            notations->append(declaration);
        } else if (parameter) {
            *parameterEntities = true;
        } else if (declared->find(declaredName) == declared->end()) { // the first declaration of a name counts
            Entity made;
            made.value = value;
            made.external = external;
            (*declared)[declaredName] = made;
            if (!notationName.empty()) {
                QXmlStreamEntityDeclaration declaration;
                declaration.m_name = toQ(declaredName);
                declaration.m_notationName = toQ(notationName);
                declaration.m_publicId = toQ(publicId);
                declaration.m_systemId = toQ(systemId);
                unparsed->append(declaration);
            }
        }
    }
}

// <!DOCTYPE name SYSTEM "..." [ declarations ]>
Step QXmlStreamReader::Private::parseDocumentType() {
    std::size_t i = pos + 9;
    if (i >= buf.size())
        return Step::More;
    if (!isXmlSpace(buf[i]))
        return fail(QString("Invalid document type declaration."));
    if (skipSpace(&i) == Step::More)
        return Step::More;
    Str rootName, publicId, systemId;
    Step step = readName(&i, &rootName);
    if (step != Step::Done)
        return step;
    if (skipSpace(&i) == Step::More)
        return Step::More;
    bool external = false;
    step = parseExternalId(&i, false, &publicId, &systemId, &external);
    if (step != Step::Done)
        return step;
    if (skipSpace(&i) == Step::More)
        return Step::More;
    std::map<Str, Entity> declared;
    bool parameterEntities = false;
    QXmlStreamNotationDeclarations notations;
    QXmlStreamEntityDeclarations unparsed;
    if (buf[i] == u'[') {
        ++i;
        step = parseInternalSubset(&i, &declared, &parameterEntities, &notations, &unparsed);
        if (step != Step::Done)
            return step;
        ++i;
        if (skipSpace(&i) == Step::More)
            return Step::More;
    }
    if (buf[i] != u'>')
        return fail(QString("Invalid document type declaration."));
    ++i;
    seenDtd = true;
    entities = declared;
    unknownEntitiesAllowed = external || parameterEntities;
    notationDeclarations = notations;
    entityDeclarations = unparsed;
    dtdName = toQ(rootName);
    dtdPublicId = toQ(publicId);
    dtdSystemId = toQ(systemId);
    text = toQ(buf.substr(pos, i - pos));
    type = DTD;
    commit(i);
    return Step::Done;
}

Step QXmlStreamReader::Private::parseComment() {
    const std::size_t start = pos + 4;
    const std::size_t dashes = buf.find(u"--", start);
    if (dashes == Str::npos || dashes + 2 >= buf.size())
        return Step::More;
    if (buf[dashes + 2] != u'>')
        return fail(QString("'--' is not allowed in a comment."));
    text = toQ(buf.substr(start, dashes - start));
    type = Comment;
    commit(dashes + 3);
    return Step::Done;
}

Step QXmlStreamReader::Private::parseProcessingInstruction() {
    std::size_t i = pos + 2;
    Str target;
    const Step step = readName(&i, &target);
    if (step != Step::Done)
        return step == Step::Fail ? fail(QString("Invalid processing instruction name.")) : step;
    Str lower = target;
    for (char16_t &c : lower)
        c = c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
    if (lower == u"xml")
        return fail(QString("XML declaration not at start of document."));
    const bool space = isXmlSpace(buf[i]);
    if (skipSpace(&i) == Step::More)
        return Step::More;
    const std::size_t end = buf.find(u"?>", i);
    if (end == Str::npos)
        return Step::More;
    if (!space && end != i)
        return fail(QString("Invalid processing instruction name."));
    piTarget = toQ(target);
    piData = toQ(buf.substr(i, end - i));
    type = ProcessingInstruction;
    commit(end + 2);
    return Step::Done;
}

Step QXmlStreamReader::Private::parseCData() {
    const std::size_t start = pos + 9;
    const std::size_t end = buf.find(u"]]>", start);
    if (end == Str::npos)
        return Step::More;
    text = toQ(buf.substr(start, end - start));
    cdata = true;
    type = Characters;
    commit(end + 3);
    return Step::Done;
}

Step QXmlStreamReader::Private::parseStartTag() {
    std::size_t i = pos + 1;
    Str qualified;
    Step step = readName(&i, &qualified);
    if (step != Step::Done)
        return step == Step::Fail ? fail(QString("Start tag expected.")) : step;
    struct Raw {
        Str qualified;
        Str value;
    };
    std::vector<Raw> rawAttributes;
    bool empty = false;
    for (;;) {
        const std::size_t before = i;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        if (buf[i] == u'>') {
            ++i;
            break;
        }
        if (buf[i] == u'/') {
            if (i + 1 >= buf.size())
                return Step::More;
            if (buf[i + 1] != u'>')
                return fail(QString("Expected '>' after '/' in a tag."));
            i += 2;
            empty = true;
            break;
        }
        if (i == before)
            return fail(QString("Expected white space between attributes."));
        Raw attribute;
        step = readName(&i, &attribute.qualified);
        if (step != Step::Done)
            return step;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        if (buf[i] != u'=')
            return fail(QString("Expected '=' after the name of an attribute."));
        ++i;
        if (skipSpace(&i) == Step::More)
            return Step::More;
        Str literal;
        step = readLiteral(&i, &literal);
        if (step != Step::Done)
            return step == Step::Fail ? fail(QString("Expected a quoted value for an attribute.")) : step;
        if (!expandAttributeValue(literal, &attribute.value, 0))
            return Step::Fail;
        for (const Raw &other : rawAttributes) {
            if (other.qualified == attribute.qualified)
                return fail(QString("Attribute '") + toQ(attribute.qualified) + QString("' redefined."));
        }
        rawAttributes.push_back(attribute);
    }

    // The tag is complete: from here on nothing asks for more input.
    Element element;
    element.qualifiedName = qualified;
    element.bindings = bindings.size();
    QXmlStreamNamespaceDeclarations declarations;
    if (namespaceProcessing) {
        for (const Raw &attribute : rawAttributes) {
            const bool isDefault = attribute.qualified == u"xmlns";
            if (!isDefault && attribute.qualified.compare(0, 6, u"xmlns:") != 0)
                continue;
            const Str bound = isDefault ? Str() : attribute.qualified.substr(6);
            if (!isDefault && (bound.empty() || bound == u"xmlns" || attribute.value.empty() ||
                               (bound == u"xml") != (attribute.value == kXmlNamespace))) {
                bindings.resize(element.bindings);
                return fail(QString("Illegal namespace declaration."));
            }
            bindings.push_back({bound, attribute.value});
            declarations.append(QXmlStreamNamespaceDeclaration(toQ(bound), toQ(attribute.value)));
        }
    }
    const auto undo = [&] { bindings.resize(element.bindings); };
    Str elementPrefix, local;
    if (!splitName(qualified, &elementPrefix, &local)) {
        undo();
        return Step::Fail;
    }
    const Binding *binding = namespaceProcessing ? findBinding(elementPrefix) : nullptr;
    if (namespaceProcessing && !elementPrefix.empty() && !binding) {
        undo();
        return fail(QString("Namespace prefix '") + toQ(elementPrefix) + QString("' not declared"));
    }
    element.name = toQ(local);
    element.prefix = toQ(elementPrefix);
    element.namespaceUri = binding ? toQ(binding->uri) : QString("");

    QXmlStreamAttributes parsed;
    for (const Raw &attribute : rawAttributes) {
        if (namespaceProcessing &&
            (attribute.qualified == u"xmlns" || attribute.qualified.compare(0, 6, u"xmlns:") == 0))
            continue;
        Str attributePrefix, attributeLocal;
        if (!splitName(attribute.qualified, &attributePrefix, &attributeLocal)) {
            undo();
            return Step::Fail;
        }
        QXmlStreamAttribute made;
        made.m_qualifiedName = toQ(attribute.qualified);
        made.m_name = toQ(attributeLocal);
        made.m_value = toQ(attribute.value);
        made.m_namespaceUri = QString("");
        // The default namespace is not that of attributes without a prefix.
        if (!attributePrefix.empty()) {
            const Binding *attributeBinding = findBinding(attributePrefix);
            if (!attributeBinding) {
                undo();
                return fail(QString("Namespace prefix '") + toQ(attributePrefix) + QString("' not declared"));
            }
            made.m_namespaceUri = toQ(attributeBinding->uri);
            for (const QXmlStreamAttribute &other : parsed) {
                if (other.m_name == made.m_name && other.m_namespaceUri == made.m_namespaceUri) {
                    undo();
                    return fail(QString("Attribute '") + made.m_qualifiedName + QString("' redefined."));
                }
            }
        }
        parsed.append(made);
    }

    name = element.name;
    prefix = element.prefix;
    namespaceUri = element.namespaceUri;
    qualifiedName = toQ(qualified);
    attributes = parsed;
    namespaceDeclarations = declarations;
    elements.push_back(element);
    stage = Content;
    pendingEnd = empty;
    type = StartElement;
    commit(i);
    return Step::Done;
}

// Reports the end of the innermost element and leaves it.
void QXmlStreamReader::Private::endElement() {
    const Element &element = elements.back();
    name = element.name;
    prefix = element.prefix;
    namespaceUri = element.namespaceUri;
    qualifiedName = toQ(element.qualifiedName);
    bindings.resize(element.bindings);
    elements.pop_back();
    if (elements.empty())
        stage = Epilog;
    type = EndElement;
}

Step QXmlStreamReader::Private::parseEndTag() {
    std::size_t i = pos + 2;
    Str qualified;
    const Step step = readName(&i, &qualified);
    if (step != Step::Done)
        return step;
    if (skipSpace(&i) == Step::More)
        return Step::More;
    if (buf[i] != u'>')
        return fail(QString("Expected '>' at the end of a tag."));
    if (elements.empty() || elements.back().qualifiedName != qualified)
        return fail(QString("Opening and ending tag mismatch."));
    endElement();
    commit(i + 1);
    return Step::Done;
}

// The text up to the next tag, with its references replaced. The text of an entity of the document is put into
// the input in place of the reference, as it may hold elements.
Step QXmlStreamReader::Private::parseText() {
    Str out;
    std::size_t i = pos;
    bool onlySpace = true;
    for (;;) {
        if (i >= buf.size())
            return Step::More;
        const char16_t c = buf[i];
        if (c == u'<')
            break;
        if (c == u'&') {
            const std::size_t semi = buf.find(u';', i + 1);
            if (semi == Str::npos)
                return buf.size() - i > 1024 ? fail(QString("Entity reference without a semicolon.")) : Step::More;
            const Str reference = buf.substr(i + 1, semi - i - 1);
            char16_t simple = 0;
            if (!reference.empty() && reference[0] == u'#') {
                char32_t code = 0;
                if (!parseCharacterReference(reference, &code))
                    return fail(QString("Invalid character reference."));
                appendCodePoint(out, code);
                onlySpace = onlySpace && code < 0x80 && isXmlSpace(static_cast<char16_t>(code));
                i = semi + 1;
                continue;
            }
            if (!isName(reference))
                return fail(QString("Invalid entity reference."));
            if (predefinedEntity(reference, &simple)) {
                out += simple;
                onlySpace = false;
                i = semi + 1;
                continue;
            }
            const Entity *entity = findEntity(reference);
            Str replacement;
            bool replace = false;
            if (entity && !entity->external) {
                replacement = entity->value;
                replace = true;
            } else if (resolver && !entity) {
                replacement = resolver->resolveUndeclaredEntity(toQ(reference)).u16string();
                replace = !replacement.empty();
            }
            if (replace) {
                // An entity that names itself, also by way of others, never stops growing: the limit ends that.
                if (i >= expansionEnd) {
                    expanded = 0;
                    expansionEnd = semi + 1;
                }
                expanded += static_cast<int>(replacement.size());
                if (expanded > expansionLimit)
                    return fail(QString("The entity '") + toQ(reference) +
                                QString("' expands to more characters than the entity expansion limit allows."));
                expansionEnd = expansionEnd - (semi + 1 - i) + replacement.size();
                buf.replace(i, semi + 1 - i, replacement);
                continue;
            }
            if (!out.empty())
                break; // the text first; the reference is the next token
            if (!entity && !unknownEntitiesAllowed)
                return fail(QString("Entity '") + toQ(reference) + QString("' not declared."));
            name = toQ(reference);
            qualifiedName = name;
            type = EntityReference;
            commit(semi + 1);
            return Step::Done;
        }
        if (c == u']') {
            // "]]>" may only end a CDATA section.
            if (i + 2 >= buf.size() && (i + 1 >= buf.size() || buf[i + 1] == u']'))
                return Step::More;
            if (buf[i + 1] == u']' && buf[i + 2] == u'>')
                return fail(QString("Sequence ']]>' not allowed in content."));
        }
        if ((c < 0x20 && !isXmlSpace(c)) || c >= 0xFFFE)
            return fail(QString("Illegal character in the document."));
        onlySpace = onlySpace && isXmlSpace(c);
        out += c;
        ++i;
    }
    text = toQ(out);
    whitespace = onlySpace;
    type = Characters;
    commit(i);
    return Step::Done;
}

Step QXmlStreamReader::Private::parseMarkup() {
    if (pos + 1 >= buf.size())
        return Step::More;
    const char16_t next = buf[pos + 1];
    if (next == u'?')
        return parseProcessingInstruction();
    if (next == u'!') {
        const Step comment = expect(pos, u"<!--"), section = expect(pos, u"<![CDATA["),
                   doctype = expect(pos, u"<!DOCTYPE");
        if (comment == Step::Done)
            return parseComment();
        if (section == Step::Done) {
            if (stage != Content)
                return fail(stage == Prolog ? QString("Start tag expected.")
                                            : QString("Extra content at end of document."));
            return parseCData();
        }
        if (doctype == Step::Done) {
            if (stage != Prolog || seenDtd)
                return fail(QString("Unexpected document type declaration."));
            return parseDocumentType();
        }
        if (comment == Step::More || section == Step::More || doctype == Step::More)
            return Step::More;
        return fail(QString("Invalid markup after '<!'."));
    }
    if (next == u'/') {
        if (stage != Content)
            return fail(stage == Prolog ? QString("Start tag expected.")
                                        : QString("Extra content at end of document."));
        return parseEndTag();
    }
    if (stage == Epilog)
        return fail(QString("Extra content at end of document."));
    return parseStartTag();
}

Step QXmlStreamReader::Private::parseToken() {
    if (!started)
        return parseDeclaration();
    if (stage != Content) {
        // Between the declarations and around the root element white space means nothing.
        std::size_t i = pos;
        while (i < buf.size() && isXmlSpace(buf[i]))
            ++i;
        commit(i);
        if (pos >= buf.size())
            return Step::More;
        if (buf[pos] != u'<')
            return fail(stage == Prolog ? QString("Start tag expected.")
                                        : QString("Extra content at end of document."));
        return parseMarkup();
    }
    if (pos >= buf.size())
        return Step::More;
    return buf[pos] == u'<' ? parseMarkup() : parseText();
}

/*!
    Constructs a reader without input. Give it a device with setDevice() or data with addData().
*/
QXmlStreamReader::QXmlStreamReader() : d(std::make_unique<Private>()) {}

/*!
    Constructs a reader that reads from \a device, which must be open for reading.

    \sa setDevice()
*/
QXmlStreamReader::QXmlStreamReader(QIODevice *device) : QXmlStreamReader() { d->device = device; }

/*!
    Constructs a reader that reads \a data. The encoding is found from the byte order mark or the
    XML declaration, and is UTF-8 without them.

    \sa addData()
*/
QXmlStreamReader::QXmlStreamReader(const QByteArray &data) : QXmlStreamReader() { addData(data); }

/*!
    Constructs a reader that reads the text \a data. An encoding that its XML declaration names
    is reported and otherwise ignored.
*/
QXmlStreamReader::QXmlStreamReader(const QString &data) : QXmlStreamReader() { addData(data); }

/*!
    Constructs a reader that reads the null-terminated \a data.
*/
QXmlStreamReader::QXmlStreamReader(const char *data) : QXmlStreamReader() { addData(data); }

/*!
    Destroys the reader. The device is not closed or deleted.
*/
QXmlStreamReader::~QXmlStreamReader() = default;

/*!
    Makes the reader read from \a device, which must be open for reading, and forget the document
    it was reading.

    \sa device(), clear()
*/
void QXmlStreamReader::setDevice(QIODevice *device) {
    clear();
    d->device = device;
}

/*!
    Returns the device that the reader reads from, or \c nullptr if it reads data that was added.

    \sa setDevice()
*/
QIODevice *QXmlStreamReader::device() const { return d->device; }

/*!
    Adds \a data to the end of the document. Use this to parse a document that arrives in pieces:
    when readNext() reports \c PrematureEndOfDocumentError, add the next piece and call readNext()
    again. A reader that has a device ignores the data.

    \sa readNext(), clear()
*/
void QXmlStreamReader::addData(const QByteArray &data) {
    if (d->device) {
        qWarning("QXmlStreamReader: addData() with device()");
        return;
    }
    d->raw.append(data);
    d->starved = false;
}

/*!
    \overload
    Adds the text \a data to the end of the document.
*/
void QXmlStreamReader::addData(const QString &data) {
    if (d->device) {
        qWarning("QXmlStreamReader: addData() with device()");
        return;
    }
    // Bytes that were added before come first; from here on bytes are UTF-8.
    d->fetchMore();
    if (!d->decoder)
        d->decoder = std::make_unique<QStringDecoder>(QStringConverter::Utf8);
    d->appendCharacters(data.utf16(), static_cast<std::size_t>(data.size()));
    d->starved = false;
}

/*!
    \overload
    Adds the null-terminated \a data to the end of the document.
*/
void QXmlStreamReader::addData(const char *data) { addData(QByteArray(data)); }

/*!
    Forgets the document and the device, and puts the reader back into the state of a new one.
    The namespace processing, the entity resolver and the entity expansion limit are kept.

    \sa addData(), setDevice()
*/
void QXmlStreamReader::clear() {
    auto fresh = std::make_unique<Private>();
    fresh->namespaceProcessing = d->namespaceProcessing;
    fresh->resolver = d->resolver;
    fresh->expansionLimit = d->expansionLimit;
    d = std::move(fresh);
}

/*!
    Returns \c true if the reader has read up to the end of the document, or if an error has
    stopped it. After a \c PrematureEndOfDocumentError the reader is at the end only until more
    data arrives.

    \sa tokenType(), error()
*/
bool QXmlStreamReader::atEnd() const {
    if (d->type == EndDocument)
        return true;
    if (d->type != Invalid)
        return false;
    if (d->error != PrematureEndOfDocumentError)
        return true;
    // The input ran out: the reader is at the end until there is more of it.
    return d->starved && (!d->device || d->device->atEnd());
}

/*!
    Reads the next token and returns its type. The functions that describe a token, such as
    name() and text(), then describe this one, and the views they returned for the token before
    are no longer valid.

    When the input ends in the middle of the document, the type is \c Invalid and error() is
    \c PrematureEndOfDocumentError; after more data has been added, or the device has more, the
    next call goes on where the document stopped. Any other error ends the reading: every later
    call returns \c Invalid.

    \sa tokenType(), tokenString()
*/
QXmlStreamReader::TokenType QXmlStreamReader::readNext() {
    if (d->type == Invalid && d->error != PrematureEndOfDocumentError)
        return Invalid;
    if (d->stage == Private::Finished) {
        d->resetToken();
        d->type = Invalid;
        return Invalid;
    }
    if (d->error == PrematureEndOfDocumentError) {
        d->error = NoError;
        d->errorString.clear();
    }
    d->resetToken();
    if (d->pendingEnd) {
        // An empty-element tag is a start and an end.
        d->pendingEnd = false;
        d->endElement();
        return d->type;
    }
    Step step = d->parseToken();
    while (step == Step::More && d->fetchMore())
        step = d->parseToken();
    if (step == Step::Done)
        return d->type;
    d->resetToken();
    if (step == Step::More && d->error == NoError) {
        // After the root element only white space can be left: then the document is complete.
        bool rest = d->raw.isEmpty();
        for (std::size_t i = d->pos; rest && i < d->buf.size(); ++i)
            rest = isXmlSpace(d->buf[i]);
        if (d->stage == Private::Epilog && rest) {
            d->commit(d->buf.size());
            d->stage = Private::Finished;
            d->type = EndDocument;
            return EndDocument;
        }
        d->error = PrematureEndOfDocumentError;
        d->errorString = QString("Premature end of document.");
        d->starved = true;
    }
    d->type = Invalid;
    return Invalid;
}

/*!
    Reads until the next start element inside the current element. Returns \c true when it has
    found one, and \c false when it reached the end of the current element first, or the end of
    the document or an error. Use it to walk the children of an element:

    \code
    while (xml.readNextStartElement()) {
        if (xml.name() == u"title")
            title = xml.readElementText();
        else
            xml.skipCurrentElement();
    }
    \endcode

    \sa skipCurrentElement(), readNext()
*/
bool QXmlStreamReader::readNextStartElement() {
    while (readNext() != Invalid) {
        if (isEndElement() || isEndDocument())
            return false;
        if (isStartElement())
            return true;
    }
    return false;
}

/*!
    Reads up to the end of the current element, skipping everything inside it. Call it when the
    current token is a start element that the program has no use for.

    \sa readNextStartElement()
*/
void QXmlStreamReader::skipCurrentElement() {
    int depth = 1;
    while (depth > 0 && readNext() != Invalid) {
        if (isStartElement())
            ++depth;
        else if (isEndElement())
            --depth;
    }
}

/*!
    Returns the type of the token that readNext() read last.

    \sa tokenString()
*/
QXmlStreamReader::TokenType QXmlStreamReader::tokenType() const { return d->type; }

/*!
    Returns the name of the type of the current token, such as \c{"StartElement"}.

    \sa tokenType()
*/
QString QXmlStreamReader::tokenString() const {
    static const char *const names[] = {"NoToken",      "Invalid",         "StartDocument",        "EndDocument",
                                        "StartElement", "EndElement",      "Characters",           "Comment",
                                        "DTD",          "EntityReference", "ProcessingInstruction"};
    return QString::fromLatin1(names[d->type]);
}

/*!
    Turns namespace processing on or off, according to \a enable. It is on by default. With it,
    the \c xmlns attributes of an element are its namespaceDeclarations() and not among its
    attributes(), and names are split into prefix() and name() and given a namespaceUri().
    Without it, a name is all local name, colons included.

    \sa namespaceProcessing()
*/
void QXmlStreamReader::setNamespaceProcessing(bool enable) { d->namespaceProcessing = enable; }

/*!
    Returns \c true if namespaces are processed.

    \sa setNamespaceProcessing()
*/
bool QXmlStreamReader::namespaceProcessing() const { return d->namespaceProcessing; }

/*!
    Returns \c true if the current token is characters that are all white space.

    \sa isCharacters(), text()
*/
bool QXmlStreamReader::isWhitespace() const { return d->type == Characters && d->whitespace; }

/*!
    Returns \c true if the current token is characters that came from a CDATA section.

    \sa isCharacters(), text()
*/
bool QXmlStreamReader::isCDATA() const { return d->type == Characters && d->cdata; }

/*!
    Returns \c true if the XML declaration says that the document is standalone. A document
    without that declaration is not.

    \sa hasStandaloneDeclaration()
*/
bool QXmlStreamReader::isStandaloneDocument() const { return d->standalone; }

/*!
    \since 6.6

    Returns \c true if the XML declaration says whether the document is standalone, with
    \c yes or \c no.

    \sa isStandaloneDocument()
*/
bool QXmlStreamReader::hasStandaloneDeclaration() const { return d->hasStandalone; }

/*!
    Returns the version that the XML declaration gives, which is \c{1.0}, or an empty view if the
    document has no declaration.
*/
QStringView QXmlStreamReader::documentVersion() const { return d->documentVersion; }

/*!
    Returns the encoding that the XML declaration names, or an empty view if it names none.
*/
QStringView QXmlStreamReader::documentEncoding() const { return d->documentEncoding; }

/*!
    Returns the number of the line that the reader has read up to, starting with 1.

    \sa columnNumber(), characterOffset()
*/
qint64 QXmlStreamReader::lineNumber() const { return d->line; }

/*!
    Returns the number of characters that the reader has read on the current line.

    \sa lineNumber(), characterOffset()
*/
qint64 QXmlStreamReader::columnNumber() const { return d->column; }

/*!
    Returns the number of characters that the reader has read in all, counted in UTF-16 code
    units after line ends have been made single line feeds.

    \sa lineNumber(), columnNumber()
*/
qint64 QXmlStreamReader::characterOffset() const { return d->offset; }

/*!
    \since 6.10
    Reads the content of the current element, which must be a start element, and returns it as XML: the text
    and the child elements between its start and end tags. Afterwards the current token is the end of the element.
    Returns an empty string, and raises an error, if the current token is not a start element.

    \code
    // <note>Call <b>now</b>!</note>
    if (xml.readNextStartElement() && xml.name() == u"note")
        html = xml.readRawInnerData(); // "Call <b>now</b>!"
    \endcode

    \sa readElementText()
*/
QString QXmlStreamReader::readRawInnerData() {
    if (d->type != StartElement) {
        raiseError(QStringLiteral("Expected a start element."));
        return QString();
    }
    QString out;
    QXmlStreamWriter writer(&out);
    int depth = 1;
    while (!atEnd()) {
        const TokenType t = readNext();
        if (t == StartElement) {
            ++depth;
        } else if (t == EndElement && --depth == 0) {
            break;
        } else if (t == Invalid) {
            return QString();
        }
        writer.writeCurrentToken(*this);
    }
    return out;
}

/*!
    Returns the attributes of the current start element, without its namespace declarations.

    \sa namespaceDeclarations()
*/
QXmlStreamAttributes QXmlStreamReader::attributes() const { return d->attributes; }

/*!
    Reads the text of the current start element up to its end element and returns it. Comments
    and processing instructions inside are left out. What happens at a child element depends on
    \a behaviour: by default it is the error \c UnexpectedElementError.

    Afterwards the current token is the end element. If the current token is not a start element,
    the function raises an error and returns an empty string.

    \sa readNextStartElement()
*/
QString QXmlStreamReader::readElementText(ReadElementTextBehaviour behaviour) {
    QString result;
    if (!isStartElement()) {
        raiseError(QString("readElementText() needs a start element."));
        return result;
    }
    int depth = 1;
    while (depth > 0) {
        switch (readNext()) {
        case Characters:
            result.append(d->text);
            break;
        case EntityReference:
            // The reference could not be replaced by text.
            break;
        case EndElement:
            --depth;
            break;
        case ProcessingInstruction:
        case Comment:
            break;
        case StartElement:
            if (behaviour == SkipChildElements) {
                skipCurrentElement();
            } else if (behaviour == IncludeChildElements) {
                ++depth;
            } else {
                d->error = UnexpectedElementError;
                d->errorString = QString("Expected character data.");
                d->resetToken();
                d->type = Invalid;
                return result;
            }
            break;
        default:
            if (d->type != Invalid)
                raiseError(QString("Expected character data."));
            return result;
        }
    }
    return result;
}

/*!
    Returns the local name of the current start or end element, or the name of the entity of an
    entity reference.

    \sa qualifiedName(), namespaceUri()
*/
QStringView QXmlStreamReader::name() const { return d->name; }

/*!
    Returns the namespace URI of the current start or end element, or an empty view if it is in
    no namespace.

    \sa name(), prefix()
*/
QStringView QXmlStreamReader::namespaceUri() const { return d->namespaceUri; }

/*!
    Returns the name of the current start or end element as it is written, with its prefix.

    \sa name(), prefix()
*/
QStringView QXmlStreamReader::qualifiedName() const { return d->qualifiedName; }

/*!
    Returns the prefix of the current start or end element, or an empty view if it has none.

    \sa name(), qualifiedName()
*/
QStringView QXmlStreamReader::prefix() const { return d->prefix; }

/*!
    Returns the target of the current processing instruction.

    \sa processingInstructionData()
*/
QStringView QXmlStreamReader::processingInstructionTarget() const { return d->piTarget; }

/*!
    Returns the data of the current processing instruction, which is what follows the target.

    \sa processingInstructionTarget()
*/
QStringView QXmlStreamReader::processingInstructionData() const { return d->piData; }

/*!
    Returns the text of the current characters, comment or document type definition. For
    characters, the references are replaced and line ends are line feeds; for the definition it
    is the whole declaration as written.

    \sa isWhitespace(), isCDATA()
*/
QStringView QXmlStreamReader::text() const { return d->text; }

/*!
    Returns the namespaces that the current start element declares.

    \sa addExtraNamespaceDeclaration()
*/
QXmlStreamNamespaceDeclarations QXmlStreamReader::namespaceDeclarations() const { return d->namespaceDeclarations; }

/*!
    Adds \a extraNamespaceDeclaraction to the namespaces that the reader knows, as if the document
    had declared it on an element around the root. Use this to read a fragment whose prefixes are
    declared elsewhere.

    \sa addExtraNamespaceDeclarations(), namespaceDeclarations()
*/
void QXmlStreamReader::addExtraNamespaceDeclaration(const QXmlStreamNamespaceDeclaration &extraNamespaceDeclaraction) {
    const Private::Binding binding{extraNamespaceDeclaraction.prefix().toString().u16string(),
                                   extraNamespaceDeclaraction.namespaceUri().toString().u16string()};
    // Below the declarations of the elements that are open, which the binding counts of the elements rely on.
    std::size_t at = 1;
    while (at < d->bindings.size() && (d->elements.empty() || at < d->elements.front().bindings))
        ++at;
    d->bindings.insert(d->bindings.begin() + static_cast<std::ptrdiff_t>(at), binding);
    for (Private::Element &element : d->elements)
        ++element.bindings;
}

/*!
    Adds each of \a extraNamespaceDeclaractions as addExtraNamespaceDeclaration() does.
*/
void QXmlStreamReader::addExtraNamespaceDeclarations(
    const QXmlStreamNamespaceDeclarations &extraNamespaceDeclaractions) {
    for (const QXmlStreamNamespaceDeclaration &declaration : extraNamespaceDeclaractions)
        addExtraNamespaceDeclaration(declaration);
}

/*!
    Returns the notations that the document type definition declares. They are known once the
    token of the definition has been read.

    \sa entityDeclarations()
*/
QXmlStreamNotationDeclarations QXmlStreamReader::notationDeclarations() const { return d->notationDeclarations; }

/*!
    Returns the unparsed entities that the document type definition declares: those that are
    external and have a notation. They are known once the token of the definition has been read.

    \sa notationDeclarations()
*/
QXmlStreamEntityDeclarations QXmlStreamReader::entityDeclarations() const { return d->entityDeclarations; }

/*!
    Returns the name that the document type definition gives the root element.

    \sa dtdPublicId(), dtdSystemId()
*/
QStringView QXmlStreamReader::dtdName() const { return d->dtdName; }

/*!
    Returns the public identifier of the document type definition, or an empty view.

    \sa dtdName(), dtdSystemId()
*/
QStringView QXmlStreamReader::dtdPublicId() const { return d->dtdPublicId; }

/*!
    Returns the system identifier of the document type definition, or an empty view.

    \sa dtdName(), dtdPublicId()
*/
QStringView QXmlStreamReader::dtdSystemId() const { return d->dtdSystemId; }

/*!
    \since 5.15

    Returns the largest number of characters that one entity reference may expand to. The default
    is 4096.

    \sa setEntityExpansionLimit()
*/
int QXmlStreamReader::entityExpansionLimit() const { return d->expansionLimit; }

/*!
    \since 5.15

    Sets the largest number of characters that one entity reference may expand to, counting the
    entities that it refers to in turn, to \a limit. A document that goes over it is not
    well-formed. The limit keeps a small document from growing without bound in memory.

    \sa entityExpansionLimit()
*/
void QXmlStreamReader::setEntityExpansionLimit(int limit) { d->expansionLimit = limit; }

/*!
    Raises the error \c CustomError with the text \a message, which ends the reading: atEnd()
    becomes \c true and readNext() returns \c Invalid. Use it to stop on a document that is
    well-formed but not what the program expects, so that one place handles all errors.

    \sa error(), errorString()
*/
void QXmlStreamReader::raiseError(const QString &message) {
    d->error = CustomError;
    d->errorString = message.isEmpty() ? QString("Invalid document.") : message;
    d->resetToken();
    d->type = Invalid;
}

/*!
    Returns a text for the error that has occurred, or an empty string. Add lineNumber() and
    columnNumber() to tell the user where.

    \sa error()
*/
QString QXmlStreamReader::errorString() const { return d->errorString; }

/*!
    Returns the kind of error that has occurred, or \c NoError.

    \sa errorString(), raiseError()
*/
QXmlStreamReader::Error QXmlStreamReader::error() const { return d->error; }

/*!
    Makes the reader ask \a resolver about entities that the document does not declare. The reader
    does not own the resolver.

    \sa entityResolver()
*/
void QXmlStreamReader::setEntityResolver(QXmlStreamEntityResolver *resolver) { d->resolver = resolver; }

/*!
    Returns the entity resolver, or \c nullptr if there is none.

    \sa setEntityResolver()
*/
QXmlStreamEntityResolver *QXmlStreamReader::entityResolver() const { return d->resolver; }

struct QXmlStreamWriter::Private {
    struct Element {
        QString qualifiedName;
        std::size_t bindings = 0;
        bool hasChildren = false;
        bool hasText = false;
    };
    struct Binding {
        QString prefix;
        QString uri;
    };

    QIODevice *device = nullptr;
    QByteArray *array = nullptr;
    QString *string = nullptr;
    bool autoFormatting = false;
    int indent = 4;
    Error error = Error::None;
    QString errorString;
    bool stopOnError = false;
    bool wroteSomething = false;
    bool inStartTag = false;
    bool inEmptyTag = false;
    std::vector<Element> elements;
    std::vector<Binding> bindings;
    std::size_t scopeMark = 0;
    std::vector<Binding> unwritten; // declared before the element they belong to has started
    int generated = 0;

    // Records the first error; later ones keep it.
    void raise(Error e, const QString &message) {
        if (error != Error::None)
            return;
        error = e;
        errorString = message;
    }

    // Checks that `s` holds only characters that XML 1.0 allows and that UTF-8 can encode.
    void check(const QString &s) {
        const char16_t *p = s.utf16();
        const qsizetype n = s.size();
        for (qsizetype i = 0; i < n; ++i) {
            const char16_t c = p[i];
            if (c >= 0xd800 && c <= 0xdbff && i + 1 < n && p[i + 1] >= 0xdc00 && p[i + 1] <= 0xdfff) {
                ++i;
                continue;
            }
            if (c >= 0xd800 && c <= 0xdfff) {
                raise(Error::Encoding, QStringLiteral("Encountered an invalid UTF-16 sequence."));
                return;
            }
            if ((c < 0x20 && c != 0x9 && c != 0xa && c != 0xd) || c == 0xfffe || c == 0xffff) {
                raise(Error::InvalidCharacter, QStringLiteral("Encountered an invalid XML 1.0 character: U+%1.")
                                                   .arg(QString::number(c, 16).toUpper().rightJustified(4, u'0')));
                return;
            }
        }
    }

    void write(const QString &s) {
        if (s.isEmpty() || (stopOnError && error != Error::None))
            return;
        check(s);
        if (stopOnError && error != Error::None)
            return;
        wroteSomething = true;
        if (string) {
            string->append(s);
        } else if (array) {
            array->append(s.toUtf8());
        } else if (device) {
            const QByteArray bytes = s.toUtf8();
            if (device->write(bytes) != bytes.size())
                raise(Error::IO, QStringLiteral("Writing to the device failed."));
        } else {
            raise(Error::IO, QStringLiteral("There is no device to write to."));
        }
    }
    void write(const char *s) { write(QString::fromLatin1(s)); }

    // The characters that would be read as markup become references; in the value of an attribute so does white
    // space that a reader would turn into spaces.
    static QString escaped(const QString &s, bool attribute) {
        QString out;
        for (const QChar c : s) {
            switch (c.unicode()) {
            case u'<':
                out.append(QString("&lt;"));
                break;
            case u'>':
                out.append(QString("&gt;"));
                break;
            case u'&':
                out.append(QString("&amp;"));
                break;
            case u'"':
                out.append(QString("&quot;"));
                break;
            case u'\r':
                out.append(QString("&#13;"));
                break;
            case u'\n':
                if (attribute)
                    out.append(QString("&#10;"));
                else
                    out.append(c);
                break;
            case u'\t':
                if (attribute)
                    out.append(QString("&#9;"));
                else
                    out.append(c);
                break;
            default:
                out.append(c);
            }
        }
        return out;
    }

    // Ends the start tag that is still open for attributes.
    void closeStartTag() {
        if (!inStartTag)
            return;
        inStartTag = false;
        if (inEmptyTag) {
            write("/>");
            inEmptyTag = false;
            bindings.resize(elements.back().bindings);
            scopeMark = bindings.size();
            elements.pop_back();
        } else {
            write(">");
        }
    }

    void newLine(std::size_t depth) {
        if (wroteSomething)
            write("\n");
        if (indent > 0)
            write(QString(static_cast<qsizetype>(depth) * indent, QChar(u' ')));
        else if (indent < 0)
            write(QString(static_cast<qsizetype>(depth) * -indent, QChar(u'\t')));
    }

    // Before something that stands on a line of its own inside the current element.
    void startChild() {
        closeStartTag();
        if (!elements.empty())
            elements.back().hasChildren = true;
        if (autoFormatting && (elements.empty() || !elements.back().hasText))
            newLine(elements.size());
    }

    QString prefixFor(const QString &uri, bool forAttribute) const {
        for (auto it = bindings.rbegin(); it != bindings.rend(); ++it) {
            if (it->uri != uri || (forAttribute && it->prefix.isEmpty()))
                continue;
            // A prefix that was bound to another namespace since does not stand for this one any more.
            const QString &candidate = it->prefix;
            const auto later =
                std::find_if(bindings.rbegin(), it, [&](const Binding &b) { return b.prefix == candidate; });
            if (later == it)
                return candidate;
        }
        return QString();
    }
    bool isBound(const QString &uri, bool forAttribute) const {
        for (const Binding &b : bindings) {
            if (b.uri == uri && !(forAttribute && b.prefix.isEmpty()) && prefixFor(uri, forAttribute) == b.prefix)
                return true;
        }
        return false;
    }
    QString newPrefix() {
        for (;;) {
            const QString candidate = QString("n") + QString::number(++generated);
            const bool taken =
                std::any_of(bindings.begin(), bindings.end(), [&](const Binding &b) { return b.prefix == candidate; });
            if (!taken)
                return candidate;
        }
    }
    void writeDeclaration(const Binding &binding) {
        write(binding.prefix.isEmpty() ? QString(" xmlns=\"") : QString(" xmlns:") + binding.prefix + QString("=\""));
        write(escaped(binding.uri, true));
        write("\"");
    }
    // Makes the namespace known and, inside a start tag, writes its declaration; outside one the declaration
    // waits for the next start tag.
    void declare(const QString &prefix, const QString &uri) {
        const Binding binding{prefix, uri};
        bindings.push_back(binding);
        if (inStartTag) {
            // The declaration is the element's: it lasts until the element ends, not until its first child does.
            writeDeclaration(binding);
            scopeMark = bindings.size();
        } else {
            unwritten.push_back(binding);
        }
    }

    void startElement(const QString &namespaceUri, const QString &name, bool empty) {
        startChild();
        Element element;
        element.bindings = scopeMark;
        QString qualified = name;
        bool declareNew = false;
        QString prefix;
        if (!namespaceUri.isEmpty()) {
            if (isBound(namespaceUri, false)) {
                prefix = prefixFor(namespaceUri, false);
            } else {
                prefix = newPrefix();
                declareNew = true;
            }
            if (!prefix.isEmpty())
                qualified = prefix + QChar(u':') + name;
        }
        element.qualifiedName = qualified;
        elements.push_back(element);
        write("<");
        write(qualified);
        inStartTag = true;
        inEmptyTag = empty;
        for (const Binding &binding : unwritten)
            writeDeclaration(binding);
        unwritten.clear();
        if (declareNew)
            declare(prefix, namespaceUri);
        scopeMark = bindings.size();
    }
};

/*!
    Constructs a writer without a place to write to. Give it one with setDevice().
*/
QXmlStreamWriter::QXmlStreamWriter() : d(std::make_unique<Private>()) {}

/*!
    Constructs a writer that writes UTF-8 to \a device, which must be open for writing.
*/
QXmlStreamWriter::QXmlStreamWriter(QIODevice *device) : QXmlStreamWriter() { d->device = device; }

/*!
    Constructs a writer that appends UTF-8 to \a array.
*/
QXmlStreamWriter::QXmlStreamWriter(QByteArray *array) : QXmlStreamWriter() { d->array = array; }

/*!
    Constructs a writer that appends to \a string. The XML declaration it writes names no
    encoding, as a string has none.
*/
QXmlStreamWriter::QXmlStreamWriter(QString *string) : QXmlStreamWriter() { d->string = string; }

/*!
    Destroys the writer. Elements that are still open are not closed: call writeEndDocument().
*/
QXmlStreamWriter::~QXmlStreamWriter() = default;

/*!
    Makes the writer write to \a device from now on, which must be open for writing.

    \sa device()
*/
void QXmlStreamWriter::setDevice(QIODevice *device) {
    d->device = device;
    d->array = nullptr;
    d->string = nullptr;
}

/*!
    Returns the device that the writer writes to, or \c nullptr if it writes to a byte array, a
    string or nothing.

    \sa setDevice()
*/
QIODevice *QXmlStreamWriter::device() const { return d->device; }

/*!
    Turns auto formatting on or off, according to \a enable. With it the writer starts a line for
    each element and indents it by its depth, which makes the document readable. An element that
    has text is kept on one line, so that the text is not changed. It is off by default.

    \sa autoFormatting(), setAutoFormattingIndent()
*/
void QXmlStreamWriter::setAutoFormatting(bool enable) { d->autoFormatting = enable; }

/*!
    Returns \c true if auto formatting is on.

    \sa setAutoFormatting()
*/
bool QXmlStreamWriter::autoFormatting() const { return d->autoFormatting; }

/*!
    Sets what auto formatting indents each level with: \a spacesOrTabs spaces if it is positive,
    and that many tabs if it is negative. The default is 4 spaces.

    \sa autoFormattingIndent(), setAutoFormatting()
*/
void QXmlStreamWriter::setAutoFormattingIndent(int spacesOrTabs) { d->indent = spacesOrTabs; }

/*!
    Returns the number of spaces, or if negative of tabs, that auto formatting indents with.

    \sa setAutoFormattingIndent()
*/
int QXmlStreamWriter::autoFormattingIndent() const { return d->indent; }

/*!
    Writes an attribute with the name \a qualifiedName and the value \a value to the start
    element that was just written. Attributes can be written only before the element gets
    content.
*/
void QXmlStreamWriter::writeAttribute(const QString &qualifiedName, const QString &value) {
    if (!d->inStartTag) {
        qWarning("QXmlStreamWriter: writeAttribute() without a start element");
        return;
    }
    d->write(" ");
    d->write(qualifiedName);
    d->write("=\"");
    d->write(Private::escaped(value, true));
    d->write("\"");
}

/*!
    \overload
    Writes an attribute with the local name \a name in the namespace \a namespaceUri and the
    value \a value. If the namespace has no prefix yet, the writer makes one up and declares it.
*/
void QXmlStreamWriter::writeAttribute(const QString &namespaceUri, const QString &name, const QString &value) {
    if (!d->inStartTag) {
        qWarning("QXmlStreamWriter: writeAttribute() without a start element");
        return;
    }
    if (namespaceUri.isEmpty()) {
        writeAttribute(name, value);
        return;
    }
    QString prefix;
    if (d->isBound(namespaceUri, true)) {
        prefix = d->prefixFor(namespaceUri, true);
    } else {
        prefix = d->newPrefix();
        d->declare(prefix, namespaceUri);
    }
    writeAttribute(prefix + QChar(u':') + name, value);
}

/*!
    \overload
    Writes \a attribute, in its namespace if it has one.
*/
void QXmlStreamWriter::writeAttribute(const QXmlStreamAttribute &attribute) {
    if (attribute.namespaceUri().isEmpty())
        writeAttribute(attribute.qualifiedName().toString(), attribute.value().toString());
    else
        writeAttribute(attribute.namespaceUri().toString(), attribute.name().toString(), attribute.value().toString());
}

/*!
    Writes each of \a attributes.

    \sa writeAttribute()
*/
void QXmlStreamWriter::writeAttributes(const QXmlStreamAttributes &attributes) {
    for (const QXmlStreamAttribute &attribute : attributes)
        writeAttribute(attribute);
}

/*!
    Writes \a text as a CDATA section, in which nothing needs to be escaped. If the text has the
    characters that end a section, it is written as two sections.

    \sa writeCharacters()
*/
void QXmlStreamWriter::writeCDATA(const QString &text) {
    d->closeStartTag();
    if (!d->elements.empty())
        d->elements.back().hasText = true;
    QString copy(text);
    copy.replace(QString("]]>"), QString("]]]]><![CDATA[>"));
    d->write("<![CDATA[");
    d->write(copy);
    d->write("]]>");
}

/*!
    Writes \a text, with the characters \c{<}, \c{>}, \c{&} and \c{"} written as references.

    \sa writeCDATA(), writeTextElement()
*/
void QXmlStreamWriter::writeCharacters(const QString &text) {
    d->closeStartTag();
    if (!d->elements.empty())
        d->elements.back().hasText = true;
    d->write(Private::escaped(text, false));
}

/*!
    Writes \a text as a comment. The text must not contain two hyphens in a row or end with one.
*/
void QXmlStreamWriter::writeComment(const QString &text) {
    d->startChild();
    d->write("<!--");
    d->write(text);
    d->write("-->");
}

/*!
    Writes \a dtd, which is a whole document type declaration, as it is.
*/
void QXmlStreamWriter::writeDTD(const QString &dtd) {
    d->closeStartTag();
    if (d->autoFormatting)
        d->newLine(0);
    d->write(dtd);
}

/*!
    Writes an element with the name \a qualifiedName that has no content. Attributes can follow.

    \sa writeStartElement()
*/
void QXmlStreamWriter::writeEmptyElement(const QString &qualifiedName) {
    d->startElement(QString(), qualifiedName, true);
}

/*!
    \overload
    Writes an element with the local name \a name in the namespace \a namespaceUri that has no
    content.
*/
void QXmlStreamWriter::writeEmptyElement(const QString &namespaceUri, const QString &name) {
    d->startElement(namespaceUri, name, true);
}

/*!
    Writes an element with the name \a qualifiedName that holds \a text.

    \sa writeStartElement(), writeCharacters()
*/
void QXmlStreamWriter::writeTextElement(const QString &qualifiedName, const QString &text) {
    writeStartElement(qualifiedName);
    writeCharacters(text);
    writeEndElement();
}

/*!
    \overload
    Writes an element with the local name \a name in the namespace \a namespaceUri that holds
    \a text.
*/
void QXmlStreamWriter::writeTextElement(const QString &namespaceUri, const QString &name, const QString &text) {
    writeStartElement(namespaceUri, name);
    writeCharacters(text);
    writeEndElement();
}

/*!
    Closes every element that is open, and with auto formatting ends the last line.

    \sa writeStartDocument()
*/
void QXmlStreamWriter::writeEndDocument() {
    d->closeStartTag();
    while (!d->elements.empty())
        writeEndElement();
    if (d->autoFormatting)
        d->write("\n");
}

/*!
    Closes the element that was started last. An element that got no content is written as an
    empty-element tag.

    \sa writeStartElement()
*/
void QXmlStreamWriter::writeEndElement() {
    if (d->inStartTag && d->inEmptyTag)
        d->closeStartTag();
    if (d->elements.empty())
        return;
    const Private::Element element = d->elements.back();
    if (d->inStartTag) {
        d->inStartTag = false;
        d->write("/>");
    } else {
        if (d->autoFormatting && element.hasChildren && !element.hasText)
            d->newLine(d->elements.size() - 1);
        d->write("</");
        d->write(element.qualifiedName);
        d->write(">");
    }
    d->bindings.resize(element.bindings);
    d->scopeMark = d->bindings.size();
    d->elements.pop_back();
}

/*!
    Writes a reference to the entity \a name, as \c{&name;}.
*/
void QXmlStreamWriter::writeEntityReference(const QString &name) {
    d->closeStartTag();
    if (!d->elements.empty())
        d->elements.back().hasText = true;
    d->write("&");
    d->write(name);
    d->write(";");
}

/*!
    Declares the namespace \a namespaceUri with the prefix \a prefix, or with a prefix that the
    writer makes up if \a prefix is empty. Right after writeStartElement() the declaration goes
    into that element; anywhere else it goes into the element that is started next. Elements and
    attributes that are written with this namespace then use the prefix.

    \sa writeDefaultNamespace(), writeStartElement()
*/
void QXmlStreamWriter::writeNamespace(const QString &namespaceUri, const QString &prefix) {
    d->declare(prefix.isEmpty() ? d->newPrefix() : prefix, namespaceUri);
}

/*!
    Declares \a namespaceUri as the default namespace, which elements without a prefix are in.
    The declaration goes where that of writeNamespace() does.

    \sa writeNamespace()
*/
void QXmlStreamWriter::writeDefaultNamespace(const QString &namespaceUri) { d->declare(QString(), namespaceUri); }

/*!
    Writes a processing instruction with the given \a target and \a data.
*/
void QXmlStreamWriter::writeProcessingInstruction(const QString &target, const QString &data) {
    d->startChild();
    d->write("<?");
    d->write(target);
    if (!data.isEmpty()) {
        d->write(" ");
        d->write(data);
    }
    d->write("?>");
}

/*!
    Writes the XML declaration for version 1.0. A writer that writes bytes names UTF-8 as the
    encoding.

    \sa writeEndDocument()
*/
void QXmlStreamWriter::writeStartDocument() { writeStartDocument(QString("1.0")); }

/*!
    \overload
    Writes the XML declaration with the version \a version.
*/
void QXmlStreamWriter::writeStartDocument(const QString &version) {
    d->write("<?xml version=\"");
    d->write(version);
    d->write(d->string ? "\"" : "\" encoding=\"UTF-8\"");
    d->write("?>");
}

/*!
    \overload
    Writes the XML declaration with the version \a version and the statement that the document
    is standalone, or is not, according to \a standalone.
*/
void QXmlStreamWriter::writeStartDocument(const QString &version, bool standalone) {
    d->write("<?xml version=\"");
    d->write(version);
    d->write(d->string ? "\"" : "\" encoding=\"UTF-8\"");
    d->write(standalone ? " standalone=\"yes\"?>" : " standalone=\"no\"?>");
}

/*!
    Starts an element with the name \a qualifiedName. Attributes and namespace declarations can
    follow, then the content, and writeEndElement() closes it.

    \sa writeEndElement(), writeEmptyElement()
*/
void QXmlStreamWriter::writeStartElement(const QString &qualifiedName) {
    d->startElement(QString(), qualifiedName, false);
}

/*!
    \overload
    Starts an element with the local name \a name in the namespace \a namespaceUri. If the
    namespace has no prefix yet, the writer makes one up and declares it in the element.

    \sa writeNamespace()
*/
void QXmlStreamWriter::writeStartElement(const QString &namespaceUri, const QString &name) {
    d->startElement(namespaceUri, name, false);
}

/*!
    Writes the token that \a reader is at. A loop that calls this for every token copies a
    document, which is a way to change parts of one and keep the rest.

    \sa QXmlStreamReader::tokenType()
*/
void QXmlStreamWriter::writeCurrentToken(const QXmlStreamReader &reader) {
    switch (reader.tokenType()) {
    case QXmlStreamReader::StartDocument:
        if (reader.documentVersion().isEmpty())
            break;
        if (reader.hasStandaloneDeclaration())
            writeStartDocument(reader.documentVersion().toString(), reader.isStandaloneDocument());
        else
            writeStartDocument(reader.documentVersion().toString());
        break;
    case QXmlStreamReader::EndDocument:
        writeEndDocument();
        break;
    case QXmlStreamReader::StartElement: {
        d->closeStartTag();
        // The declarations of the element first, so that it is written with the prefixes it had.
        const QXmlStreamNamespaceDeclarations declarations = reader.namespaceDeclarations();
        for (const QXmlStreamNamespaceDeclaration &declaration : declarations) {
            if (declaration.prefix().isEmpty())
                writeDefaultNamespace(declaration.namespaceUri().toString());
            else
                writeNamespace(declaration.namespaceUri().toString(), declaration.prefix().toString());
        }
        if (reader.namespaceUri().isEmpty())
            writeStartElement(reader.qualifiedName().toString());
        else
            writeStartElement(reader.namespaceUri().toString(), reader.name().toString());
        writeAttributes(reader.attributes());
        break;
    }
    case QXmlStreamReader::EndElement:
        writeEndElement();
        break;
    case QXmlStreamReader::Characters:
        if (reader.isCDATA())
            writeCDATA(reader.text().toString());
        else
            writeCharacters(reader.text().toString());
        break;
    case QXmlStreamReader::Comment:
        writeComment(reader.text().toString());
        break;
    case QXmlStreamReader::DTD:
        writeDTD(reader.text().toString());
        break;
    case QXmlStreamReader::EntityReference:
        writeEntityReference(reader.name().toString());
        break;
    case QXmlStreamReader::ProcessingInstruction:
        writeProcessingInstruction(reader.processingInstructionTarget().toString(),
                                   reader.processingInstructionData().toString());
        break;
    default:
        break;
    }
}

/*!
    Returns \c true if writing to the device has failed, or the writer has nothing to write to.
    What is written after an error is lost.
*/
bool QXmlStreamWriter::hasError() const { return d->error != Error::None; }

/*!
    \since 6.10
    Returns what went wrong first, or \c Error::None.

    \sa errorString(), hasError()
*/
QXmlStreamWriter::Error QXmlStreamWriter::error() const { return d->error; }

/*!
    \since 6.10
    Returns a description of the first error, or an empty string if there was none.

    \sa error()
*/
QString QXmlStreamWriter::errorString() const { return d->errorString; }

/*!
    \since 6.10
    Records a custom error with the description \a message, unless an error was recorded already. The program can
    use it to stop writing a document it finds wrong.

    \sa error(), setStopWritingOnError()
*/
void QXmlStreamWriter::raiseError(const QString &message) { d->raise(Error::Custom, message); }

/*!
    \since 6.10
    Makes the writer stop writing at the first error if \a stop is \c true, so that what it wrote ends where the
    document stopped being valid. By default it goes on.

    \sa stopWritingOnError(), error()
*/
void QXmlStreamWriter::setStopWritingOnError(bool stop) { d->stopOnError = stop; }

/*!
    \since 6.10
    Returns \c true if the writer stops writing at the first error; otherwise returns \c false.

    \sa setStopWritingOnError()
*/
bool QXmlStreamWriter::stopWritingOnError() const { return d->stopOnError; }
