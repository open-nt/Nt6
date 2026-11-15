// MIT License - see LICENSE
#pragma once

#include <QtXml/qdom.h>

#include <QtCore/qlist.h>

#include <atomic>
#include <memory>
#include <utility>

// The nodes of a tree are shared between handles (QDomNode and its subclasses) by a reference count. A parent holds
// a reference to each of its children and an element to each of its attributes; a handle holds one to its node. A node
// that nothing refers to is deleted. Nothing refers back: a node knows its parent and its owner element by plain
// pointers, which a node that is removed or that outlives its parent sets to null, and its document through a link
// that the document empties when it dies.

// Every document of a tree holds the one link that its nodes copy.
struct QDomDocumentLink {
    QDomDocumentPrivate *document = nullptr;
};
using QDomDocumentLinkPtr = std::shared_ptr<QDomDocumentLink>;

// A counter for changes to the structure of any tree: node lists compare it to know when to look again.
quint64 qDomStamp();
void qDomBumpStamp();

class QDomNodePrivate {
public:
    QDomNodePrivate(QDomNode::NodeType nodeType, QDomDocumentLinkPtr documentLink)
        : type(nodeType), link(std::move(documentLink)) {}
    QDomNodePrivate(const QDomNodePrivate &) = delete;
    QDomNodePrivate &operator=(const QDomNodePrivate &) = delete;
    virtual ~QDomNodePrivate();

    void ref() noexcept { m_ref.fetch_add(1, std::memory_order_relaxed); }
    void deref() {
        if (m_ref.fetch_sub(1, std::memory_order_acq_rel) == 1)
            delete this;
    }

    bool isElement() const { return type == QDomNode::ElementNode; }
    bool isAttr() const { return type == QDomNode::AttributeNode; }
    bool isDocument() const { return type == QDomNode::DocumentNode; }
    bool isText() const { return type == QDomNode::TextNode || type == QDomNode::CDATASectionNode; }
    bool isCharacterData() const { return isText() || type == QDomNode::CommentNode; }
    // Whether a node of this kind can have children at all.
    bool canHaveChildren() const;

    QDomNodePrivate *insertBefore(QDomNodePrivate *newChild, QDomNodePrivate *refChild, QDomNodePrivate *replaced);
    QDomNodePrivate *insertAfter(QDomNodePrivate *newChild, QDomNodePrivate *refChild);
    QDomNodePrivate *replaceChild(QDomNodePrivate *newChild, QDomNodePrivate *oldChild);
    QDomNodePrivate *removeChild(QDomNodePrivate *oldChild);
    // Adds a child that is known to be fit, as the readers and the copies do.
    void appendUnchecked(QDomNodePrivate *child);
    void removeAllChildren();
    void normalize();

    QString nodeName() const;

    QDomNode::NodeType type;
    QString name;  // the qualified name, or the target of an instruction
    QString value; // the data of text, comments and instructions, the value of an attribute or entity
    QDomNodePrivate *parent = nullptr;
    QDomNodePrivate *prev = nullptr;
    QDomNodePrivate *next = nullptr;
    QDomNodePrivate *first = nullptr;
    QDomNodePrivate *last = nullptr;
    QDomDocumentLinkPtr link;
    int line = -1;
    int column = -1;

private:
    void unlink(QDomNodePrivate *child);
    void linkBefore(QDomNodePrivate *child, QDomNodePrivate *refChild);
    bool acceptsChild(const QDomNodePrivate *child, const QDomNodePrivate *replaced) const;
    std::atomic<int> m_ref{0};
};

// Elements and attributes are the nodes that namespaces apply to. A node that was made without a namespace in mind has
// a null localName.
class QDomNamespacedPrivate : public QDomNodePrivate {
public:
    using QDomNodePrivate::QDomNodePrivate;
    QString namespaceUri;
    QString prefix;
    QString localName;
};

class QDomAttrPrivate : public QDomNamespacedPrivate {
public:
    explicit QDomAttrPrivate(QDomDocumentLinkPtr documentLink)
        : QDomNamespacedPrivate(QDomNode::AttributeNode, std::move(documentLink)) {}
    QDomNodePrivate *owner = nullptr; // the element that has the attribute
    bool specified = true;
};

class QDomElementPrivate : public QDomNamespacedPrivate {
public:
    explicit QDomElementPrivate(QDomDocumentLinkPtr documentLink)
        : QDomNamespacedPrivate(QDomNode::ElementNode, std::move(documentLink)) {}
    ~QDomElementPrivate() override;

    QDomNodePrivate *findAttribute(const QString &qualifiedName) const;
    QDomNodePrivate *findAttributeNS(const QString &uri, const QString &local) const;
    // Adds the attribute, which takes the place of the one at index, if that is not -1.
    void putAttribute(QDomNodePrivate *attribute, qsizetype index);
    void dropAttribute(QDomNodePrivate *attribute);

    QList<QDomNodePrivate *> attributes;
};

// Entities and notations.
class QDomDeclarationPrivate : public QDomNodePrivate {
public:
    using QDomNodePrivate::QDomNodePrivate;
    QString publicId;
    QString systemId;
    QString notationName;
};

class QDomDocumentTypePrivate : public QDomNodePrivate {
public:
    explicit QDomDocumentTypePrivate(QDomDocumentLinkPtr documentLink)
        : QDomNodePrivate(QDomNode::DocumentTypeNode, std::move(documentLink)) {}
    ~QDomDocumentTypePrivate() override;

    QString publicId;
    QString systemId;
    QString internalSubset;
    QList<QDomNodePrivate *> entities;
    QList<QDomNodePrivate *> notations;
};

class QDomDocumentPrivate : public QDomNodePrivate {
public:
    QDomDocumentPrivate();
    ~QDomDocumentPrivate() override;

    QDomNodePrivate *documentElement() const;
    QDomNodePrivate *doctype() const;
};

// What a node list lists.
class QDomNodeListPrivate {
public:
    enum Kind { Children, TagName, TagNameNS };

    const QList<QDomNodePrivate *> &items();

    QDomNode root;
    Kind kind = Children;
    QString tag;          // a tag name, or the local name for TagNameNS
    QString namespaceUri; // for TagNameNS
    bool built = false;
    quint64 stamp = 0;

private:
    QList<QDomNodePrivate *> m_items;
};

struct QDomPrivateAccess {
    template <typename T> static T make(QDomNodePrivate *node) { return T(node); }
    static QDomNodePrivate *impl(const QDomNode &node) { return node.impl; }
    static QDomNodePrivate *&implRef(QDomNode &node) { return node.impl; }
    static QDomNodeList makeList(std::shared_ptr<QDomNodeListPrivate> list) { return QDomNodeList(std::move(list)); }
    static QDomNodeListPrivate *list(const QDomNodeList &list) { return list.d.get(); }
    static QDomNamedNodeMap makeAttributeMap(const QDomNode &owner) {
        return QDomNamedNodeMap(owner, QDomNamedNodeMap::AttributeMap);
    }
    static QDomNamedNodeMap makeEntityMap(const QDomNode &owner) {
        return QDomNamedNodeMap(owner, QDomNamedNodeMap::EntityMap);
    }
    static QDomNamedNodeMap makeNotationMap(const QDomNode &owner) {
        return QDomNamedNodeMap(owner, QDomNamedNodeMap::NotationMap);
    }
    static QDomImplementation makeImplementation(QDomDocumentPrivate *document) { return QDomImplementation(document); }
    static QDomDocumentPrivate *document(const QDomImplementation &implementation) { return implementation.impl; }
};

// Creates a handle to the node, or a null handle if node is null.
template <typename T> T qDomWrap(QDomNodePrivate *node) { return node ? QDomPrivateAccess::make<T>(node) : T(); }

// Making and copying nodes.
QDomDocumentPrivate *qDomNewDocument();
QDomNodePrivate *qDomNewNode(QDomNode::NodeType type, const QDomDocumentLinkPtr &link);
QDomNodePrivate *qDomClone(const QDomNodePrivate *node, bool deep, const QDomDocumentLinkPtr &link);
// Makes the nodes of the tree belong to the document of link.
void qDomAdopt(QDomNodePrivate *node, const QDomDocumentLinkPtr &link);
void qDomSplitName(const QString &qualifiedName, QString *prefix, QString *localName);

// Writing.
void qDomSaveNode(const QDomNodePrivate *node, QTextStream &stream, int depth, int indent);
void qDomSaveDocument(const QDomNodePrivate *document, QTextStream &stream, int indent,
                      QDomNode::EncodingPolicy policy);

// Reading: replaces what the document holds with the content that the reader reads.
QDomDocument::ParseResult qDomParse(QDomDocumentPrivate *document, QXmlStreamReader *reader,
                                    QDomDocument::ParseOptions options);
