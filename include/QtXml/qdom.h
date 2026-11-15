// MIT License - see LICENSE
#pragma once

#include <QtCore/qanystringview.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qglobal.h>
#include <QtCore/qiodevice.h>
#include <QtCore/qlist.h>
#include <QtCore/qstring.h>

#include <memory>

class QDebug;
class QTextStream;
class QXmlStreamReader;

class QDomAttr;
class QDomCDATASection;
class QDomCharacterData;
class QDomComment;
class QDomDocument;
class QDomDocumentFragment;
class QDomDocumentType;
class QDomElement;
class QDomEntity;
class QDomEntityReference;
class QDomNamedNodeMap;
class QDomNode;
class QDomNodeList;
class QDomNotation;
class QDomProcessingInstruction;
class QDomText;

class QDomDocumentPrivate;
class QDomNodeListPrivate;
class QDomNodePrivate;
struct QDomPrivateAccess;

class QDomImplementation {
public:
    enum InvalidDataPolicy { AcceptInvalidData, DropInvalidData, ReturnNullNode };

    QDomImplementation();
    QDomImplementation(const QDomImplementation &x);
    ~QDomImplementation();
    QDomImplementation &operator=(const QDomImplementation &x);
    bool operator==(const QDomImplementation &x) const;
    bool operator!=(const QDomImplementation &x) const;

    bool hasFeature(const QString &feature, const QString &version) const;
    QDomDocumentType createDocumentType(const QString &qName, const QString &publicId, const QString &systemId);
    QDomDocument createDocument(const QString &nsURI, const QString &qName, const QDomDocumentType &doctype);

    static InvalidDataPolicy invalidDataPolicy();
    static void setInvalidDataPolicy(InvalidDataPolicy policy);

    bool isNull() const;

private:
    friend struct QDomPrivateAccess;
    explicit QDomImplementation(QDomDocumentPrivate *document);
    QDomDocumentPrivate *impl = nullptr;
};

class QDomNode {
public:
    enum NodeType {
        ElementNode = 1,
        AttributeNode = 2,
        TextNode = 3,
        CDATASectionNode = 4,
        EntityReferenceNode = 5,
        EntityNode = 6,
        ProcessingInstructionNode = 7,
        CommentNode = 8,
        DocumentNode = 9,
        DocumentTypeNode = 10,
        DocumentFragmentNode = 11,
        NotationNode = 12,
        BaseNode = 21,
        CharacterDataNode = 22
    };
    enum EncodingPolicy { EncodingFromDocument = 1, EncodingFromTextStream = 2 };

    QDomNode();
    QDomNode(const QDomNode &node);
    QDomNode(QDomNode &&node) noexcept;
    QDomNode &operator=(const QDomNode &node);
    QDomNode &operator=(QDomNode &&node) noexcept;
    bool operator==(const QDomNode &node) const;
    bool operator!=(const QDomNode &node) const;
    ~QDomNode();

    QDomNode insertBefore(const QDomNode &newChild, const QDomNode &refChild);
    QDomNode insertAfter(const QDomNode &newChild, const QDomNode &refChild);
    QDomNode replaceChild(const QDomNode &newChild, const QDomNode &oldChild);
    QDomNode removeChild(const QDomNode &oldChild);
    QDomNode appendChild(const QDomNode &newChild);
    QDomNode prependChild(const QDomNode &newChild);

    bool hasChildNodes() const;
    QDomNode cloneNode(bool deep = true) const;
    void normalize();
    bool isSupported(const QString &feature, const QString &version) const;

    QString nodeName() const;
    NodeType nodeType() const;
    QDomNode parentNode() const;
    QDomNodeList childNodes() const;
    QDomNode firstChild() const;
    QDomNode lastChild() const;
    QDomNode previousSibling() const;
    QDomNode nextSibling() const;
    QDomNamedNodeMap attributes() const;
    QDomDocument ownerDocument() const;
    QString namespaceURI() const;
    QString localName() const;
    bool hasAttributes() const;
    QString nodeValue() const;
    void setNodeValue(const QString &value);
    QString prefix() const;
    void setPrefix(const QString &pre);

    bool isAttr() const;
    bool isCDATASection() const;
    bool isDocumentFragment() const;
    bool isDocument() const;
    bool isDocumentType() const;
    bool isElement() const;
    bool isEntityReference() const;
    bool isText() const;
    bool isEntity() const;
    bool isNotation() const;
    bool isProcessingInstruction() const;
    bool isCharacterData() const;
    bool isComment() const;

    QDomNode namedItem(const QString &name) const;

    bool isNull() const;
    void clear();

    QDomAttr toAttr() const;
    QDomCDATASection toCDATASection() const;
    QDomDocumentFragment toDocumentFragment() const;
    QDomDocument toDocument() const;
    QDomDocumentType toDocumentType() const;
    QDomElement toElement() const;
    QDomEntityReference toEntityReference() const;
    QDomText toText() const;
    QDomEntity toEntity() const;
    QDomNotation toNotation() const;
    QDomProcessingInstruction toProcessingInstruction() const;
    QDomCharacterData toCharacterData() const;
    QDomComment toComment() const;

    void save(QTextStream &stream, int indent, EncodingPolicy encodingPolicy = EncodingFromDocument) const;

    QDomElement firstChildElement(const QString &tagName = QString(), const QString &namespaceURI = QString()) const;
    QDomElement lastChildElement(const QString &tagName = QString(), const QString &namespaceURI = QString()) const;
    QDomElement previousSiblingElement(const QString &tagName = QString(),
                                       const QString &namespaceURI = QString()) const;
    QDomElement nextSiblingElement(const QString &tagName = QString(), const QString &namespaceURI = QString()) const;

    int lineNumber() const;
    int columnNumber() const;

protected:
    explicit QDomNode(QDomNodePrivate *node);
    /*! \internal */
    QDomNodePrivate *impl = nullptr;

private:
    friend struct QDomPrivateAccess;
};

class QDomNodeList {
public:
    QDomNodeList();
    QDomNodeList(const QDomNodeList &list);
    QDomNodeList &operator=(const QDomNodeList &list);
    ~QDomNodeList();
    bool operator==(const QDomNodeList &list) const;
    bool operator!=(const QDomNodeList &list) const;

    QDomNode item(int index) const;
    QDomNode at(int index) const;
    int length() const;
    int count() const;
    int size() const;
    bool isEmpty() const;

private:
    friend struct QDomPrivateAccess;
    explicit QDomNodeList(std::shared_ptr<QDomNodeListPrivate> list);
    std::shared_ptr<QDomNodeListPrivate> d;
};

class QDomNamedNodeMap {
public:
    QDomNamedNodeMap();
    QDomNamedNodeMap(const QDomNamedNodeMap &map);
    QDomNamedNodeMap &operator=(const QDomNamedNodeMap &map);
    ~QDomNamedNodeMap();
    bool operator==(const QDomNamedNodeMap &map) const;
    bool operator!=(const QDomNamedNodeMap &map) const;

    QDomNode namedItem(const QString &name) const;
    QDomNode setNamedItem(const QDomNode &newNode);
    QDomNode removeNamedItem(const QString &name);
    QDomNode item(int index) const;
    QDomNode namedItemNS(const QString &nsURI, const QString &localName) const;
    QDomNode setNamedItemNS(const QDomNode &newNode);
    QDomNode removeNamedItemNS(const QString &nsURI, const QString &localName);

    int length() const;
    int count() const;
    int size() const;
    bool isEmpty() const;
    bool contains(const QString &name) const;

private:
    friend struct QDomPrivateAccess;
    enum Kind { AttributeMap, EntityMap, NotationMap };
    QDomNamedNodeMap(const QDomNode &owner, Kind kind);
    QList<QDomNodePrivate *> *nodes() const;
    QDomNode m_owner;
    Kind m_kind = AttributeMap;
};

class QDomDocumentType : public QDomNode {
public:
    QDomDocumentType();

    QString name() const;
    QDomNamedNodeMap entities() const;
    QDomNamedNodeMap notations() const;
    QString publicId() const;
    QString systemId() const;
    QString internalSubset() const;

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomDocumentType(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomDocument : public QDomNode {
public:
    enum class ParseOption { Default = 0x00, UseNamespaceProcessing = 0x01, PreserveSpacingOnlyNodes = 0x02 };
    Q_DECLARE_FLAGS(ParseOptions, ParseOption)

    struct ParseResult {
        QString errorMessage;
        qsizetype errorLine = 0;
        qsizetype errorColumn = 0;
        explicit operator bool() const noexcept { return errorMessage.isEmpty(); }
    };

    QDomDocument();
    explicit QDomDocument(const QString &name);
    explicit QDomDocument(const QDomDocumentType &doctype);

    QDomElement createElement(const QString &tagName);
    QDomDocumentFragment createDocumentFragment();
    QDomText createTextNode(const QString &data);
    QDomComment createComment(const QString &data);
    QDomCDATASection createCDATASection(const QString &data);
    QDomProcessingInstruction createProcessingInstruction(const QString &target, const QString &data);
    QDomAttr createAttribute(const QString &name);
    QDomEntityReference createEntityReference(const QString &name);
    QDomNodeList elementsByTagName(const QString &tagname) const;
    QDomNode importNode(const QDomNode &importedNode, bool deep);
    QDomElement createElementNS(const QString &nsURI, const QString &qName);
    QDomAttr createAttributeNS(const QString &nsURI, const QString &qName);
    QDomNodeList elementsByTagNameNS(const QString &nsURI, const QString &localName) const;
    QDomElement elementById(const QString &elementId);

    QDomDocumentType doctype() const;
    QDomImplementation implementation() const;
    QDomElement documentElement() const;

    ParseResult setContent(const QByteArray &data, ParseOptions options = ParseOption::Default);
    ParseResult setContent(QAnyStringView data, ParseOptions options = ParseOption::Default);
    ParseResult setContent(const char *data, ParseOptions options = ParseOption::Default);
    ParseResult setContent(QIODevice *device, ParseOptions options = ParseOption::Default);
    ParseResult setContent(QXmlStreamReader *reader, ParseOptions options = ParseOption::Default);

    bool setContent(const QByteArray &text, bool namespaceProcessing, QString *errorMsg = nullptr,
                    int *errorLine = nullptr, int *errorColumn = nullptr);
    bool setContent(const QString &text, bool namespaceProcessing, QString *errorMsg = nullptr,
                    int *errorLine = nullptr, int *errorColumn = nullptr);
    bool setContent(QIODevice *dev, bool namespaceProcessing, QString *errorMsg = nullptr, int *errorLine = nullptr,
                    int *errorColumn = nullptr);
    bool setContent(QXmlStreamReader *reader, bool namespaceProcessing, QString *errorMsg = nullptr,
                    int *errorLine = nullptr, int *errorColumn = nullptr);
    bool setContent(const QByteArray &text, QString *errorMsg, int *errorLine = nullptr, int *errorColumn = nullptr);
    bool setContent(const QString &text, QString *errorMsg, int *errorLine = nullptr, int *errorColumn = nullptr);
    bool setContent(QIODevice *dev, QString *errorMsg, int *errorLine = nullptr, int *errorColumn = nullptr);

    QDomNode::NodeType nodeType() const;

    QString toString(int indent = 1) const;
    QByteArray toByteArray(int indent = 1) const;

private:
    friend struct QDomPrivateAccess;
    explicit QDomDocument(QDomNodePrivate *node);
};
Q_DECLARE_OPERATORS_FOR_FLAGS(QDomDocument::ParseOptions)

class QDomDocumentFragment : public QDomNode {
public:
    QDomDocumentFragment();

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomDocumentFragment(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomCharacterData : public QDomNode {
public:
    QDomCharacterData();

    QString substringData(unsigned long offset, unsigned long count) const;
    void appendData(const QString &arg);
    void insertData(unsigned long offset, const QString &arg);
    void deleteData(unsigned long offset, unsigned long count);
    void replaceData(unsigned long offset, unsigned long count, const QString &arg);

    int length() const;
    QString data() const;
    void setData(const QString &data);

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomCharacterData(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomAttr : public QDomNode {
public:
    QDomAttr();

    QString name() const;
    bool specified() const;
    QDomElement ownerElement() const;

    QString value() const;
    void setValue(const QString &value);

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomAttr(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomElement : public QDomNode {
public:
    QDomElement();

    void setTagName(const QString &name);
    QString tagName() const;

    QDomAttr attributeNode(const QString &name);
    QDomAttr setAttributeNode(const QDomAttr &newAttr);
    QDomAttr removeAttributeNode(const QDomAttr &oldAttr);
    QDomAttr attributeNodeNS(const QString &nsURI, const QString &localName);
    QDomAttr setAttributeNodeNS(const QDomAttr &newAttr);

    QString attribute(const QString &name, const QString &defValue = QString()) const;
    void setAttribute(const QString &name, const QString &value);
    void setAttribute(const QString &name, qlonglong value);
    void setAttribute(const QString &name, qulonglong value);
    void setAttribute(const QString &name, int value);
    void setAttribute(const QString &name, uint value);
    void setAttribute(const QString &name, float value);
    void setAttribute(const QString &name, double value);
    void removeAttribute(const QString &name);
    bool hasAttribute(const QString &name) const;

    QString attributeNS(const QString &nsURI, const QString &localName, const QString &defValue = QString()) const;
    void setAttributeNS(const QString &nsURI, const QString &qName, const QString &value);
    void setAttributeNS(const QString &nsURI, const QString &qName, int value);
    void setAttributeNS(const QString &nsURI, const QString &qName, uint value);
    void setAttributeNS(const QString &nsURI, const QString &qName, qlonglong value);
    void setAttributeNS(const QString &nsURI, const QString &qName, qulonglong value);
    void setAttributeNS(const QString &nsURI, const QString &qName, double value);
    void removeAttributeNS(const QString &nsURI, const QString &localName);
    bool hasAttributeNS(const QString &nsURI, const QString &localName) const;

    QDomNodeList elementsByTagName(const QString &tagname) const;
    QDomNodeList elementsByTagNameNS(const QString &nsURI, const QString &localName) const;

    QDomNamedNodeMap attributes() const;
    QDomNode::NodeType nodeType() const;

    QString text() const;

protected:
    explicit QDomElement(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomText : public QDomCharacterData {
public:
    QDomText();

    QDomText splitText(int offset);

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomText(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomComment : public QDomCharacterData {
public:
    QDomComment();

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomComment(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomCDATASection : public QDomText {
public:
    QDomCDATASection();

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomCDATASection(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomNotation : public QDomNode {
public:
    QDomNotation();

    QString publicId() const;
    QString systemId() const;

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomNotation(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomEntity : public QDomNode {
public:
    QDomEntity();

    QString publicId() const;
    QString systemId() const;
    QString notationName() const;

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomEntity(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomEntityReference : public QDomNode {
public:
    QDomEntityReference();

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomEntityReference(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

class QDomProcessingInstruction : public QDomNode {
public:
    QDomProcessingInstruction();

    QString target() const;
    QString data() const;
    void setData(const QString &data);

    QDomNode::NodeType nodeType() const;

protected:
    explicit QDomProcessingInstruction(QDomNodePrivate *node);

private:
    friend struct QDomPrivateAccess;
};

QTextStream &operator<<(QTextStream &stream, const QDomNode &node);
QDebug operator<<(QDebug debug, const QDomNode &node);
