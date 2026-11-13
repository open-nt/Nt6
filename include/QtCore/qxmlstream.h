// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringview.h>

#include <memory>

class QXmlStreamAttribute {
public:
    QXmlStreamAttribute();
    QXmlStreamAttribute(const QString &qualifiedName, const QString &value);
    QXmlStreamAttribute(const QString &namespaceUri, const QString &name, const QString &value);

    /*!
        Returns the namespace URI of the attribute, or an empty view if it has none.
    */
    QStringView namespaceUri() const { return m_namespaceUri; }
    /*!
        Returns the local name of the attribute: the name without its prefix.
    */
    QStringView name() const { return m_name; }
    /*!
        Returns the name of the attribute as it is written, with its prefix.
    */
    QStringView qualifiedName() const { return m_qualifiedName; }
    /*!
        Returns the prefix of the attribute, or an empty view if it has none.
    */
    QStringView prefix() const {
        return QStringView(m_qualifiedName).left(qMax<qsizetype>(0, m_qualifiedName.size() - m_name.size() - 1));
    }
    /*!
        Returns the value of the attribute, with references replaced.
    */
    QStringView value() const { return m_value; }
    /*!
        Returns \c true if the value came from a default in the document type definition and was
        not written in the element. The reader does not apply defaults, so this is \c false.
    */
    bool isDefault() const { return false; }

    /*!
        Returns \c true if \a a and \a b have the same namespace URI, local name and value. The
        prefix does not matter.
    */
    friend bool operator==(const QXmlStreamAttribute &a, const QXmlStreamAttribute &b) {
        return a.m_namespaceUri == b.m_namespaceUri && a.m_name == b.m_name && a.m_value == b.m_value &&
               (!a.m_namespaceUri.isNull() || a.m_qualifiedName == b.m_qualifiedName);
    }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QXmlStreamAttribute &a, const QXmlStreamAttribute &b) { return !(a == b); }

private:
    friend class QXmlStreamReader;
    QString m_namespaceUri;
    QString m_name;
    QString m_qualifiedName;
    QString m_value;
};

class QXmlStreamAttributes : public QList<QXmlStreamAttribute> {
public:
    /*!
        Constructs an empty list of attributes.
    */
    QXmlStreamAttributes() = default;

    QStringView value(const QString &namespaceUri, const QString &name) const;
    QStringView value(const QString &qualifiedName) const;
    void append(const QString &namespaceUri, const QString &name, const QString &value);
    void append(const QString &qualifiedName, const QString &value);
    using QList<QXmlStreamAttribute>::append;

    /*!
        Returns \c true if the list has an attribute with the qualified name \a qualifiedName.
    */
    bool hasAttribute(const QString &qualifiedName) const { return !value(qualifiedName).isNull(); }
    /*!
        \overload
        Returns \c true if the list has an attribute with the local name \a name in the
        namespace \a namespaceUri.
    */
    bool hasAttribute(const QString &namespaceUri, const QString &name) const {
        return !value(namespaceUri, name).isNull();
    }
};

class QXmlStreamNamespaceDeclaration {
public:
    QXmlStreamNamespaceDeclaration();
    QXmlStreamNamespaceDeclaration(const QString &prefix, const QString &namespaceUri);

    /*!
        Returns the prefix that the declaration binds, which is empty for the default namespace.
    */
    QStringView prefix() const { return m_prefix; }
    /*!
        Returns the namespace URI that the prefix stands for.
    */
    QStringView namespaceUri() const { return m_namespaceUri; }

    /*!
        Returns \c true if \a a and \a b bind the same prefix to the same namespace URI.
    */
    friend bool operator==(const QXmlStreamNamespaceDeclaration &a, const QXmlStreamNamespaceDeclaration &b) {
        return a.m_prefix == b.m_prefix && a.m_namespaceUri == b.m_namespaceUri;
    }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QXmlStreamNamespaceDeclaration &a, const QXmlStreamNamespaceDeclaration &b) {
        return !(a == b);
    }

private:
    QString m_prefix;
    QString m_namespaceUri;
};
using QXmlStreamNamespaceDeclarations = QList<QXmlStreamNamespaceDeclaration>;

class QXmlStreamNotationDeclaration {
public:
    QXmlStreamNotationDeclaration();

    /*!
        Returns the name of the notation.
    */
    QStringView name() const { return m_name; }
    /*!
        Returns the system identifier of the notation, or an empty view.
    */
    QStringView systemId() const { return m_systemId; }
    /*!
        Returns the public identifier of the notation, or an empty view.
    */
    QStringView publicId() const { return m_publicId; }

    /*!
        Returns \c true if \a a and \a b have the same name and identifiers.
    */
    friend bool operator==(const QXmlStreamNotationDeclaration &a, const QXmlStreamNotationDeclaration &b) {
        return a.m_name == b.m_name && a.m_systemId == b.m_systemId && a.m_publicId == b.m_publicId;
    }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QXmlStreamNotationDeclaration &a, const QXmlStreamNotationDeclaration &b) {
        return !(a == b);
    }

private:
    friend class QXmlStreamReader;
    QString m_name;
    QString m_systemId;
    QString m_publicId;
};
using QXmlStreamNotationDeclarations = QList<QXmlStreamNotationDeclaration>;

class QXmlStreamEntityDeclaration {
public:
    QXmlStreamEntityDeclaration();

    /*!
        Returns the name of the entity.
    */
    QStringView name() const { return m_name; }
    /*!
        Returns the name of the notation that says what kind of data the entity is.
    */
    QStringView notationName() const { return m_notationName; }
    /*!
        Returns the system identifier of the entity, or an empty view.
    */
    QStringView systemId() const { return m_systemId; }
    /*!
        Returns the public identifier of the entity, or an empty view.
    */
    QStringView publicId() const { return m_publicId; }
    /*!
        Returns the replacement text of the entity, which an external entity does not have.
    */
    QStringView value() const { return m_value; }

    /*!
        Returns \c true if \a a and \a b have the same name, notation, identifiers and value.
    */
    friend bool operator==(const QXmlStreamEntityDeclaration &a, const QXmlStreamEntityDeclaration &b) {
        return a.m_name == b.m_name && a.m_notationName == b.m_notationName && a.m_systemId == b.m_systemId &&
               a.m_publicId == b.m_publicId && a.m_value == b.m_value;
    }
    /*!
        Returns \c true if \a a and \a b differ.
    */
    friend bool operator!=(const QXmlStreamEntityDeclaration &a, const QXmlStreamEntityDeclaration &b) {
        return !(a == b);
    }

private:
    friend class QXmlStreamReader;
    QString m_name;
    QString m_notationName;
    QString m_systemId;
    QString m_publicId;
    QString m_value;
};
using QXmlStreamEntityDeclarations = QList<QXmlStreamEntityDeclaration>;

class QXmlStreamEntityResolver {
public:
    virtual ~QXmlStreamEntityResolver();
    virtual QString resolveEntity(const QString &publicId, const QString &systemId);
    virtual QString resolveUndeclaredEntity(const QString &name);
};

class QXmlStreamReader {
public:
    enum TokenType {
        NoToken = 0,
        Invalid,
        StartDocument,
        EndDocument,
        StartElement,
        EndElement,
        Characters,
        Comment,
        DTD,
        EntityReference,
        ProcessingInstruction,
    };
    enum ReadElementTextBehaviour { ErrorOnUnexpectedElement, IncludeChildElements, SkipChildElements };
    enum Error { NoError, UnexpectedElementError, CustomError, NotWellFormedError, PrematureEndOfDocumentError };

    QXmlStreamReader();
    explicit QXmlStreamReader(QIODevice *device);
    explicit QXmlStreamReader(const QByteArray &data);
    explicit QXmlStreamReader(const QString &data);
    explicit QXmlStreamReader(const char *data);
    ~QXmlStreamReader();
    QXmlStreamReader(const QXmlStreamReader &) = delete;
    QXmlStreamReader &operator=(const QXmlStreamReader &) = delete;

    void setDevice(QIODevice *device);
    QIODevice *device() const;
    void addData(const QByteArray &data);
    void addData(const QString &data);
    void addData(const char *data);
    void clear();

    bool atEnd() const;
    TokenType readNext();
    bool readNextStartElement();
    void skipCurrentElement();

    TokenType tokenType() const;
    QString tokenString() const;

    void setNamespaceProcessing(bool enable);
    bool namespaceProcessing() const;

    /*!
        Returns \c true if tokenType() is \c StartDocument.
    */
    bool isStartDocument() const { return tokenType() == StartDocument; }
    /*!
        Returns \c true if tokenType() is \c EndDocument.
    */
    bool isEndDocument() const { return tokenType() == EndDocument; }
    /*!
        Returns \c true if tokenType() is \c StartElement.
    */
    bool isStartElement() const { return tokenType() == StartElement; }
    /*!
        Returns \c true if tokenType() is \c EndElement.
    */
    bool isEndElement() const { return tokenType() == EndElement; }
    /*!
        Returns \c true if tokenType() is \c Characters.

        \sa isWhitespace(), isCDATA()
    */
    bool isCharacters() const { return tokenType() == Characters; }
    bool isWhitespace() const;
    bool isCDATA() const;
    /*!
        Returns \c true if tokenType() is \c Comment.
    */
    bool isComment() const { return tokenType() == Comment; }
    /*!
        Returns \c true if tokenType() is \c DTD.
    */
    bool isDTD() const { return tokenType() == DTD; }
    /*!
        Returns \c true if tokenType() is \c EntityReference.
    */
    bool isEntityReference() const { return tokenType() == EntityReference; }
    /*!
        Returns \c true if tokenType() is \c ProcessingInstruction.
    */
    bool isProcessingInstruction() const { return tokenType() == ProcessingInstruction; }

    bool isStandaloneDocument() const;
    bool hasStandaloneDeclaration() const;
    QStringView documentVersion() const;
    QStringView documentEncoding() const;

    qint64 lineNumber() const;
    qint64 columnNumber() const;
    qint64 characterOffset() const;

    QXmlStreamAttributes attributes() const;
    QString readElementText(ReadElementTextBehaviour behaviour = ErrorOnUnexpectedElement);
    QString readRawInnerData();

    QStringView name() const;
    QStringView namespaceUri() const;
    QStringView qualifiedName() const;
    QStringView prefix() const;
    QStringView processingInstructionTarget() const;
    QStringView processingInstructionData() const;
    QStringView text() const;

    QXmlStreamNamespaceDeclarations namespaceDeclarations() const;
    void addExtraNamespaceDeclaration(const QXmlStreamNamespaceDeclaration &extraNamespaceDeclaraction);
    void addExtraNamespaceDeclarations(const QXmlStreamNamespaceDeclarations &extraNamespaceDeclaractions);
    QXmlStreamNotationDeclarations notationDeclarations() const;
    QXmlStreamEntityDeclarations entityDeclarations() const;
    QStringView dtdName() const;
    QStringView dtdPublicId() const;
    QStringView dtdSystemId() const;

    int entityExpansionLimit() const;
    void setEntityExpansionLimit(int limit);

    void raiseError(const QString &message = QString());
    QString errorString() const;
    Error error() const;
    /*!
        Returns \c true if an error has occurred.

        \sa error(), errorString()
    */
    bool hasError() const { return error() != NoError; }

    void setEntityResolver(QXmlStreamEntityResolver *resolver);
    QXmlStreamEntityResolver *entityResolver() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

class QXmlStreamWriter {
public:
    /*!
        \enum QXmlStreamWriter::Error
        \since 6.10
        This enum describes what went wrong while writing.

        \value None Nothing went wrong.
        \value IO The device could not be written to, or the writer has nothing to write to.
        \value Encoding The text has a lone UTF-16 surrogate, which UTF-8 cannot encode.
        \value InvalidCharacter The text has a character that XML does not allow, such as a control character.
        \value Custom raiseError() was called.
    */
    enum class Error { None, IO, Encoding, InvalidCharacter, Custom };

    QXmlStreamWriter();
    explicit QXmlStreamWriter(QIODevice *device);
    explicit QXmlStreamWriter(QByteArray *array);
    explicit QXmlStreamWriter(QString *string);
    ~QXmlStreamWriter();
    QXmlStreamWriter(const QXmlStreamWriter &) = delete;
    QXmlStreamWriter &operator=(const QXmlStreamWriter &) = delete;

    void setDevice(QIODevice *device);
    QIODevice *device() const;

    void setAutoFormatting(bool enable);
    bool autoFormatting() const;
    void setAutoFormattingIndent(int spacesOrTabs);
    int autoFormattingIndent() const;

    void writeAttribute(const QString &qualifiedName, const QString &value);
    void writeAttribute(const QString &namespaceUri, const QString &name, const QString &value);
    void writeAttribute(const QXmlStreamAttribute &attribute);
    void writeAttributes(const QXmlStreamAttributes &attributes);

    void writeCDATA(const QString &text);
    void writeCharacters(const QString &text);
    void writeComment(const QString &text);
    void writeDTD(const QString &dtd);

    void writeEmptyElement(const QString &qualifiedName);
    void writeEmptyElement(const QString &namespaceUri, const QString &name);
    void writeTextElement(const QString &qualifiedName, const QString &text);
    void writeTextElement(const QString &namespaceUri, const QString &name, const QString &text);

    void writeEndDocument();
    void writeEndElement();

    void writeEntityReference(const QString &name);
    void writeNamespace(const QString &namespaceUri, const QString &prefix = QString());
    void writeDefaultNamespace(const QString &namespaceUri);
    void writeProcessingInstruction(const QString &target, const QString &data = QString());

    void writeStartDocument();
    void writeStartDocument(const QString &version);
    void writeStartDocument(const QString &version, bool standalone);
    void writeStartElement(const QString &qualifiedName);
    void writeStartElement(const QString &namespaceUri, const QString &name);

    void writeCurrentToken(const QXmlStreamReader &reader);

    bool hasError() const;
    Error error() const;
    QString errorString() const;
    void raiseError(const QString &message);
    void setStopWritingOnError(bool stop);
    bool stopWritingOnError() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
