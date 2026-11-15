// MIT License - see LICENSE
#include "qdom_p.h"

#include <QtCore/qdebug.h>
#include <QtCore/qlocale.h>
#include <QtCore/qtextstream.h>
#include <QtCore/qxmlstream.h>

#include <atomic>
#include <utility>

/*!
    \module NtXml
    \title Nt XML C++ Classes
    \brief The document object model for XML.
    \cmaketarget Nt::Xml

    Nt XML reads an XML document into a tree of nodes that a program can walk, search and change, and
    writes the tree out again. QDomDocument stands for the document, QDomElement, QDomText and the
    other QDomNode classes for what is in it. It is the whole of the tree in memory: for a large
    document that is read once from beginning to end, QXmlStreamReader in Nt Core is faster and uses
    less memory.

    Link with \c Nt::Xml and include \c{<QtXml>}. See \l{Using Nt XML}.
*/

namespace {

std::atomic<int> g_invalidDataPolicy{QDomImplementation::AcceptInvalidData};

QDomNodePrivate *P(const QDomNode &node) { return QDomPrivateAccess::impl(node); }

// What XML 1.0 allows in text, names and the like. A lone surrogate is none of them.
bool isXmlChar(char32_t c) {
    return c == 0x9 || c == 0xA || c == 0xD || (c >= 0x20 && c <= 0xD7FF) || (c >= 0xE000 && c <= 0xFFFD) ||
           (c >= 0x10000 && c <= 0x10FFFF);
}

bool isNameStartChar(char32_t c) {
    return c == ':' || c == '_' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= 0xC0 && c <= 0xD6) ||
           (c >= 0xD8 && c <= 0xF6) || (c >= 0xF8 && c <= 0x2FF) || (c >= 0x370 && c <= 0x37D) ||
           (c >= 0x37F && c <= 0x1FFF) || (c >= 0x200C && c <= 0x200D) || (c >= 0x2070 && c <= 0x218F) ||
           (c >= 0x2C00 && c <= 0x2FEF) || (c >= 0x3001 && c <= 0xD7FF) || (c >= 0xF900 && c <= 0xFDCF) ||
           (c >= 0xFDF0 && c <= 0xFFFD) || (c >= 0x10000 && c <= 0xEFFFF);
}

bool isNameChar(char32_t c) {
    return isNameStartChar(c) || c == '-' || c == '.' || (c >= '0' && c <= '9') || c == 0xB7 ||
           (c >= 0x300 && c <= 0x36F) || (c >= 0x203F && c <= 0x2040);
}

// Calls keep(c) for each code point of text and returns the text of those it said yes to, or null if it said yes to
// all of them. A lone surrogate is passed on as it is.
template <typename Keep> bool filterText(const QString &text, QString *kept, Keep keep) {
    bool allKept = true;
    QString out;
    for (qsizetype i = 0; i < text.size(); ++i) {
        const char16_t u = text.at(i).unicode();
        char32_t c = u;
        qsizetype units = 1;
        if ((u & 0xFC00) == 0xD800 && i + 1 < text.size() && (text.at(i + 1).unicode() & 0xFC00) == 0xDC00) {
            c = 0x10000 + ((static_cast<char32_t>(u) - 0xD800) << 10) + (text.at(i + 1).unicode() - 0xDC00);
            units = 2;
        }
        if (keep(c, out.size())) {
            out += text.mid(i, units);
        } else {
            allKept = false;
        }
        i += units - 1;
    }
    if (kept)
        *kept = out;
    return allKept;
}

// The invalid data policy, applied to the data of a node that the program makes or changes. Each function returns
// false if the node must not be made; the text of a node that is made may have been put right.
bool applyPolicy(bool valid, QString *text, const QString &fixed, bool fixable) {
    if (valid)
        return true;
    switch (static_cast<QDomImplementation::InvalidDataPolicy>(g_invalidDataPolicy.load())) {
    case QDomImplementation::AcceptInvalidData:
        return true;
    case QDomImplementation::DropInvalidData:
        if (!fixable)
            return false;
        *text = fixed;
        return true;
    case QDomImplementation::ReturnNullNode:
        break;
    }
    return false;
}

// A qualified name has at most one colon, and a prefix and a local name on both sides of it.
bool hasQualifiedShape(const QString &name) {
    const qsizetype colon = name.indexOf(QLatin1Char(':'));
    return colon < 0 || (colon > 0 && colon < name.size() - 1 && name.indexOf(QLatin1Char(':'), colon + 1) < 0);
}

bool checkName(QString *name, bool qualified) {
    QString fixed;
    bool valid = !name->isEmpty() && filterText(*name, &fixed, [](char32_t c, qsizetype at) {
        return at == 0 ? isNameStartChar(c) : isNameChar(c);
    });
    if (qualified)
        valid = valid && hasQualifiedShape(*name);
    const bool fixable = !fixed.isEmpty() && (!qualified || hasQualifiedShape(fixed));
    return applyPolicy(valid, name, fixed, fixable);
}

bool checkData(QString *text) {
    QString fixed;
    const bool valid = filterText(*text, &fixed, [](char32_t c, qsizetype) { return isXmlChar(c); });
    return applyPolicy(valid, text, fixed, true);
}

bool checkComment(QString *text) {
    QString fixed;
    bool valid = filterText(*text, &fixed, [](char32_t c, qsizetype) { return isXmlChar(c); });
    // "--" must not appear, nor a dash at the end.
    QString clean = fixed;
    clean.replace(QString("--"), QString("- -"));
    if (clean.endsWith(QLatin1Char('-')))
        clean.chop(1);
    valid = valid && clean == *text;
    return applyPolicy(valid, text, clean, true);
}

bool checkCDATA(QString *text) {
    QString fixed;
    bool valid = filterText(*text, &fixed, [](char32_t c, qsizetype) { return isXmlChar(c); });
    const QString clean = fixed.replace(QString("]]>"), QString());
    valid = valid && clean == *text;
    return applyPolicy(valid, text, clean, true);
}

bool checkInstructionData(QString *text) {
    QString fixed;
    bool valid = filterText(*text, &fixed, [](char32_t c, qsizetype) { return isXmlChar(c); });
    const QString clean = fixed.replace(QString("?>"), QString());
    valid = valid && clean == *text;
    return applyPolicy(valid, text, clean, true);
}

// Gives the node the data, after the policy has had its say.
bool assignValue(QDomNodePrivate *node, QString data) {
    bool ok = true;
    switch (node->type) {
    case QDomNode::CommentNode:
        ok = checkComment(&data);
        break;
    case QDomNode::CDATASectionNode:
        ok = checkCDATA(&data);
        break;
    case QDomNode::ProcessingInstructionNode:
        ok = checkInstructionData(&data);
        break;
    case QDomNode::TextNode:
    case QDomNode::AttributeNode:
    case QDomNode::EntityNode:
        ok = checkData(&data);
        break;
    default:
        return false;
    }
    if (ok)
        node->value = std::move(data);
    return ok;
}

QDomNodeList tagList(const QDomNode &root, QDomNodeListPrivate::Kind kind, const QString &tag, const QString &ns) {
    auto list = std::make_shared<QDomNodeListPrivate>();
    list->root = root;
    list->kind = kind;
    list->tag = tag;
    list->namespaceUri = ns;
    return QDomPrivateAccess::makeList(std::move(list));
}

// The value of a number the way a document writes it: the shortest text that reads back as the same number.
QString numberText(double value) { return QString::number(value, 'g', QLocale::FloatingPointShortest); }

QString numberText(float value) {
    for (int precision = 1; precision < 9; ++precision) {
        const QString text = QString::number(static_cast<double>(value), 'g', precision);
        if (text.toFloat() == value)
            return text;
    }
    return QString::number(static_cast<double>(value), 'g', 9);
}

// Puts the attribute in the element, in the place of the one with the same name. Returns the attribute that went, or a
// null node if there was none or the attribute belongs to another element.
QDomNode putAttrNode(QDomElementPrivate *element, QDomNodePrivate *a, bool namespaced) {
    if (!a->isAttr())
        return QDomNode();
    auto *attr = static_cast<QDomAttrPrivate *>(a);
    if (attr->owner)
        return QDomNode();
    QDomNodePrivate *existing =
        namespaced ? element->findAttributeNS(attr->namespaceUri, attr->localName) : element->findAttribute(attr->name);
    QDomNode old = qDomWrap<QDomNode>(existing);
    const qsizetype index = existing ? element->attributes.indexOf(existing) : -1;
    if (element->link && a->link != element->link)
        a->link = element->link;
    element->putAttribute(a, index);
    return old;
}

QDomDocumentPrivate *ensureDocument(QDomDocument &document) {
    QDomNodePrivate *&p = QDomPrivateAccess::implRef(document);
    if (!p) {
        p = qDomNewDocument();
        p->ref();
    }
    return static_cast<QDomDocumentPrivate *>(p);
}

QDomDocumentPrivate *documentOf(const QDomDocument &document) {
    return static_cast<QDomDocumentPrivate *>(P(document));
}

bool reportResult(const QDomDocument::ParseResult &result, QString *errorMsg, int *errorLine, int *errorColumn) {
    if (errorMsg)
        *errorMsg = result.errorMessage;
    if (errorLine)
        *errorLine = static_cast<int>(result.errorLine);
    if (errorColumn)
        *errorColumn = static_cast<int>(result.errorColumn);
    return static_cast<bool>(result);
}

} // namespace

/*!
    \class QDomImplementation
    \brief The QDomImplementation class provides information about the features of the DOM implementation.

    The class makes document types and documents without a document to start from, and sets what
    happens when the data given to a QDomDocument is not valid XML.

    \code
    QDomImplementation implementation;
    QDomDocumentType type = implementation.createDocumentType("svg", "-//W3C//DTD SVG 1.1//EN", "svg11.dtd");
    QDomDocument document = implementation.createDocument("http://www.w3.org/2000/svg", "svg", type);
    \endcode

    \differences
    A default-constructed implementation is null; QDomDocument::implementation() returns one that is
    not. Two are equal if they come from the same document.
    \enddifferences

    \sa QDomDocument
*/

/*!
    \enum QDomImplementation::InvalidDataPolicy

    What the functions that make nodes do with a name or text that XML does not allow, such as a name that
    starts with a digit or text with a control character. The policy is the same for the whole program.

    \value AcceptInvalidData The data is used as it is. The document that is written may not be well-formed XML.
    \value DropInvalidData The characters that are not allowed are left out, and where that is not enough
           (a name with nothing left), no node is made.
    \value ReturnNullNode No node is made: the function returns a null node.
*/

/*!
    Constructs a null implementation.
*/
QDomImplementation::QDomImplementation() = default;

QDomImplementation::QDomImplementation(QDomDocumentPrivate *document) : impl(document) {
    if (impl)
        impl->ref();
}

/*!
    Constructs a copy of \a x.
*/
QDomImplementation::QDomImplementation(const QDomImplementation &x) : impl(x.impl) {
    if (impl)
        impl->ref();
}

/*!
    Destroys the implementation.
*/
QDomImplementation::~QDomImplementation() {
    if (impl)
        impl->deref();
}

/*!
    Assigns \a x to this implementation.
*/
QDomImplementation &QDomImplementation::operator=(const QDomImplementation &x) {
    if (this == &x)
        return *this;
    if (x.impl)
        x.impl->ref();
    if (impl)
        impl->deref();
    impl = x.impl;
    return *this;
}

/*!
    Returns \c true if \a x and this implementation come from the same document.
*/
bool QDomImplementation::operator==(const QDomImplementation &x) const { return impl == x.impl; }

/*!
    Returns \c true if \a x and this implementation do not come from the same document.
*/
bool QDomImplementation::operator!=(const QDomImplementation &x) const { return impl != x.impl; }

/*!
    Returns \c true if the implementation has the \a feature in the \a version. The only
    feature is \c XML, in version \c{1.0} or with no version.
*/
bool QDomImplementation::hasFeature(const QString &feature, const QString &version) const {
    return feature == QLatin1StringView("XML") && (version.isEmpty() || version == QLatin1StringView("1.0"));
}

/*!
    Creates a document type with the name \a qName, the public identifier \a publicId and the system
    identifier \a systemId. The document type belongs to no document until it is given to one with
    QDomDocument::QDomDocument() or createDocument(). Returns a null node if \a qName is not a valid name and
    the invalid data policy says so.

    \sa createDocument(), invalidDataPolicy()
*/
QDomDocumentType QDomImplementation::createDocumentType(const QString &qName, const QString &publicId,
                                                        const QString &systemId) {
    QString name = qName;
    if (!checkName(&name, true))
        return QDomDocumentType();
    auto *type = new QDomDocumentTypePrivate(QDomDocumentLinkPtr());
    type->name = name;
    type->publicId = publicId;
    type->systemId = systemId;
    return qDomWrap<QDomDocumentType>(type);
}

/*!
    Creates a document with the document type \a doctype and a document element with the qualified
    name \a qName in the namespace \a nsURI. Returns a null document if \a qName is not a valid name and the
    invalid data policy says so.

    \sa createDocumentType()
*/
QDomDocument QDomImplementation::createDocument(const QString &nsURI, const QString &qName,
                                                const QDomDocumentType &doctype) {
    QDomDocument document(doctype);
    const QDomElement root = document.createElementNS(nsURI, qName);
    if (root.isNull())
        return QDomDocument();
    document.appendChild(root);
    return document;
}

/*!
    Returns what the program has chosen to do with invalid data. It is
    \c AcceptInvalidData until setInvalidDataPolicy() is called.

    \sa setInvalidDataPolicy()
*/
QDomImplementation::InvalidDataPolicy QDomImplementation::invalidDataPolicy() {
    return static_cast<InvalidDataPolicy>(g_invalidDataPolicy.load());
}

/*!
    Sets what the functions that make nodes do with invalid data to \a policy. The setting is for the whole program.

    \sa invalidDataPolicy()
*/
void QDomImplementation::setInvalidDataPolicy(InvalidDataPolicy policy) {
    g_invalidDataPolicy.store(static_cast<int>(policy));
}

/*!
    Returns \c true if the implementation is null: it is a default-constructed one.
*/
bool QDomImplementation::isNull() const { return impl == nullptr; }

/*!
    \class QDomNode
    \brief The QDomNode class is the base class for all the nodes in a DOM tree.

    Everything in a document is a node: the document itself, its elements, their attributes and text, the
    comments and the rest. QDomNode has the functions that work for all of them, such as parentNode(),
    firstChild() and nextSibling() to walk the tree, and appendChild() or removeChild() to change it. The
    subclasses add what is special to their kind of node; toElement(), toText() and the like turn a node
    into one.

    A QDomNode is a handle to a node, and a copy of it is a handle to the same node. A node stays alive as long
    as a handle refers to it or it is in a tree that is alive. A null node, which a default-constructed handle
    is and which the functions return when there is no such node, is safe to ask anything: the answer is an
    empty one.

    \code
    for (QDomNode n = document.documentElement().firstChild(); !n.isNull(); n = n.nextSibling()) {
        QDomElement e = n.toElement();
        if (!e.isNull())
            qDebug() << e.tagName();
    }
    \endcode

    \differences
    The attributes of an element keep the order in which they were added. When a document is destroyed
    while a handle to one of its nodes remains, the node goes on as the root of a tree of its own, and
    ownerDocument() is null. The functions that insert a node check the rules of XML (an attribute is not a
    child, a document has one element, a node is not put into itself) and return a null node if they are
    broken. A node from another document that is inserted becomes a node of this one, as if importNode() had
    been used, without a copy.
    \enddifferences

    \sa QDomDocument, QDomElement, QDomNodeList
*/

/*!
    \enum QDomNode::NodeType

    What kind of node a QDomNode is.

    \value ElementNode An element: QDomElement.
    \value AttributeNode An attribute of an element: QDomAttr.
    \value TextNode Text: QDomText.
    \value CDATASectionNode A CDATA section: QDomCDATASection.
    \value EntityReferenceNode A reference to an entity that the document does not define: QDomEntityReference.
    \value EntityNode An entity that the document type declares: QDomEntity.
    \value ProcessingInstructionNode A processing instruction, which includes the XML declaration: QDomProcessingInstruction.
    \value CommentNode A comment: QDomComment.
    \value DocumentNode The document: QDomDocument.
    \value DocumentTypeNode The document type declaration: QDomDocumentType.
    \value DocumentFragmentNode A loose set of nodes that are meant to be inserted somewhere else: QDomDocumentFragment.
    \value NotationNode A notation that the document type declares: QDomNotation.
    \value BaseNode A null node: nodeType() of a node that is not there.
    \value CharacterDataNode Not the type of any node; QDomCharacterData is the base of text and comments.
*/

/*!
    \enum QDomNode::EncodingPolicy

    How save() takes the encoding of what it writes.

    \value EncodingFromDocument The encoding is the one that the encoding in the XML declaration of the document
           names, or UTF-8 if there is none. The encoding of the stream is set to it.
    \value EncodingFromTextStream The encoding is that of the stream; nothing is changed.
*/

/*!
    Constructs a null node.
*/
QDomNode::QDomNode() = default;

/*!
    \internal
*/
QDomNode::QDomNode(QDomNodePrivate *node) : impl(node) {
    if (impl)
        impl->ref();
}

/*!
    Constructs a handle to the node that \a node is a handle to.
*/
QDomNode::QDomNode(const QDomNode &node) : impl(node.impl) {
    if (impl)
        impl->ref();
}

/*!
    Moves \a node into the new handle, which leaves \a node null.
*/
QDomNode::QDomNode(QDomNode &&node) noexcept : impl(std::exchange(node.impl, nullptr)) {}

/*!
    Makes this a handle to the node that \a node is a handle to.
*/
QDomNode &QDomNode::operator=(const QDomNode &node) {
    if (this == &node)
        return *this;
    if (node.impl)
        node.impl->ref();
    if (impl)
        impl->deref();
    impl = node.impl;
    return *this;
}

/*!
    Moves \a node into this handle.
*/
QDomNode &QDomNode::operator=(QDomNode &&node) noexcept {
    std::swap(impl, node.impl);
    return *this;
}

/*!
    Returns \c true if \a node and this node are the same node, not just two nodes that look alike.
*/
bool QDomNode::operator==(const QDomNode &node) const { return impl == node.impl; }

/*!
    Returns \c true if \a node and this node are not the same node.
*/
bool QDomNode::operator!=(const QDomNode &node) const { return impl != node.impl; }

/*!
    Destroys the handle. The node goes with it if nothing else refers to it.
*/
QDomNode::~QDomNode() {
    if (impl)
        impl->deref();
}

/*!
    Inserts \a newChild before the child \a refChild, and returns \a newChild. If \a refChild is null, \a newChild is
    appended. A node that is a child of another node is moved. If \a newChild is a QDomDocumentFragment, its children
    are inserted and the fragment is left empty. Returns a null node, and changes nothing, if \a refChild is not a child of
    this node, if this node cannot have \a newChild as a child, or if \a newChild is this node or one of its ancestors.

    \sa insertAfter(), appendChild(), replaceChild()
*/
QDomNode QDomNode::insertBefore(const QDomNode &newChild, const QDomNode &refChild) {
    if (!impl || !newChild.impl)
        return QDomNode();
    return QDomNode(impl->insertBefore(newChild.impl, refChild.impl, nullptr));
}

/*!
    Inserts \a newChild after the child \a refChild, and returns \a newChild. If \a refChild is null, \a newChild is
    inserted as the first child. The rules are those of insertBefore().

    \sa insertBefore(), prependChild()
*/
QDomNode QDomNode::insertAfter(const QDomNode &newChild, const QDomNode &refChild) {
    if (!impl || !newChild.impl)
        return QDomNode();
    return QDomNode(impl->insertAfter(newChild.impl, refChild.impl));
}

/*!
    Replaces the child \a oldChild with \a newChild, and returns \a oldChild. Returns a null node, and changes
    nothing, if \a oldChild is not a child of this node or \a newChild cannot take its place.

    \sa insertBefore(), removeChild()
*/
QDomNode QDomNode::replaceChild(const QDomNode &newChild, const QDomNode &oldChild) {
    if (!impl || !newChild.impl || !oldChild.impl)
        return QDomNode();
    return impl->replaceChild(newChild.impl, oldChild.impl) ? oldChild : QDomNode();
}

/*!
    Removes the child \a oldChild from this node, and returns it. Returns a null node if \a oldChild is not a child of this
    node. The node that is removed is deleted when the last handle to it goes.

    \sa appendChild(), replaceChild()
*/
QDomNode QDomNode::removeChild(const QDomNode &oldChild) {
    if (!impl || !oldChild.impl)
        return QDomNode();
    return impl->removeChild(oldChild.impl) ? oldChild : QDomNode();
}

/*!
    Appends \a newChild to the children of this node, and returns it. The rules are those of insertBefore().

    \sa prependChild(), insertBefore()
*/
QDomNode QDomNode::appendChild(const QDomNode &newChild) {
    if (!impl || !newChild.impl)
        return QDomNode();
    return QDomNode(impl->insertBefore(newChild.impl, nullptr, nullptr));
}

/*!
    Inserts \a newChild as the first child of this node, and returns it. The rules are those of insertBefore().

    \sa appendChild(), insertAfter()
*/
QDomNode QDomNode::prependChild(const QDomNode &newChild) {
    if (!impl || !newChild.impl)
        return QDomNode();
    if (impl->first == newChild.impl)
        return newChild;
    return QDomNode(impl->insertBefore(newChild.impl, impl->first, nullptr));
}

/*!
    Returns \c true if the node has children.

    \sa firstChild(), childNodes()
*/
bool QDomNode::hasChildNodes() const { return impl && impl->first; }

/*!
    Returns a copy of the node, which has no parent. A copy of an element has the attributes of the element. If \a deep
    is \c true, the copy has copies of all the children, and their children, too; otherwise it has none.

    \sa QDomDocument::importNode()
*/
QDomNode QDomNode::cloneNode(bool deep) const {
    if (!impl)
        return QDomNode();
    return qDomWrap<QDomNode>(qDomClone(impl, deep, impl->link));
}

/*!
    Puts the node and everything under it in normal form: the text nodes that are next to each other become
    one, and those that are empty are removed. CDATA sections are left as they are.
*/
void QDomNode::normalize() {
    if (impl)
        impl->normalize();
}

/*!
    Returns \c true if the \a feature in the \a version is supported; see QDomImplementation::hasFeature().
*/
bool QDomNode::isSupported(const QString &feature, const QString &version) const {
    return QDomImplementation().hasFeature(feature, version);
}

/*!
    Returns the name of the node. It is the tag name of an element, the name of an attribute, the target of
    a processing instruction, the name of a document type, entity, notation or entity reference, and for the others
    a name in the form of \c{#text}, \c{#cdata-section}, \c{#comment}, \c{#document} or \c{#document-fragment}.

    \sa nodeValue(), nodeType()
*/
QString QDomNode::nodeName() const { return impl ? impl->nodeName() : QString(); }

/*!
    Returns the type of the node, or \c BaseNode if it is null.

    \sa toElement(), isElement()
*/
QDomNode::NodeType QDomNode::nodeType() const { return impl ? impl->type : BaseNode; }

/*!
    Returns the parent of the node, or a null node if it has none. An attribute has no parent: see
    QDomAttr::ownerElement().

    \sa firstChild(), ownerDocument()
*/
QDomNode QDomNode::parentNode() const { return impl ? qDomWrap<QDomNode>(impl->parent) : QDomNode(); }

/*!
    Returns the children of the node, as a list that changes with the tree: a child that is added or removed
    later is added to or removed from the list. The attributes of an element are not among them.

    \sa firstChild(), attributes()
*/
QDomNodeList QDomNode::childNodes() const {
    if (!impl)
        return QDomNodeList();
    return tagList(*this, QDomNodeListPrivate::Children, QString(), QString());
}

/*!
    Returns the first child of the node, or a null node if it has none.

    \sa lastChild(), nextSibling()
*/
QDomNode QDomNode::firstChild() const { return impl ? qDomWrap<QDomNode>(impl->first) : QDomNode(); }

/*!
    Returns the last child of the node, or a null node if it has none.

    \sa firstChild(), previousSibling()
*/
QDomNode QDomNode::lastChild() const { return impl ? qDomWrap<QDomNode>(impl->last) : QDomNode(); }

/*!
    Returns the child of the parent that is before this node, or a null node if there is none.

    \sa nextSibling(), lastChild()
*/
QDomNode QDomNode::previousSibling() const { return impl ? qDomWrap<QDomNode>(impl->prev) : QDomNode(); }

/*!
    Returns the child of the parent that is after this node, or a null node if there is none.

    \sa previousSibling(), firstChild()
*/
QDomNode QDomNode::nextSibling() const { return impl ? qDomWrap<QDomNode>(impl->next) : QDomNode(); }

/*!
    Returns the attributes of the node if it is an element, and an empty map for any other node.

    \sa QDomElement::attributes(), hasAttributes()
*/
QDomNamedNodeMap QDomNode::attributes() const {
    if (!isElement())
        return QDomNamedNodeMap();
    return QDomPrivateAccess::makeAttributeMap(*this);
}

/*!
    Returns the document that the node belongs to. For a document, it is the document itself. It is null for a
    node whose document has been destroyed, and for a document type that no document has been given.

    \sa QDomDocument
*/
QDomDocument QDomNode::ownerDocument() const {
    if (!impl)
        return QDomDocument();
    if (impl->isDocument())
        return qDomWrap<QDomDocument>(impl);
    if (impl->link && impl->link->document)
        return qDomWrap<QDomDocument>(impl->link->document);
    return QDomDocument();
}

/*!
    Returns the namespace URI of an element or attribute that was made with a namespace in mind, or a null string.

    \sa prefix(), localName(), QDomDocument::createElementNS()
*/
QString QDomNode::namespaceURI() const {
    if (impl && (impl->isElement() || impl->isAttr()))
        return static_cast<const QDomNamespacedPrivate *>(impl)->namespaceUri;
    return QString();
}

/*!
    Returns the local name of an element or attribute that was made with a namespace in mind: its name
    without the prefix. For any other node it is a null string.

    \sa prefix(), namespaceURI()
*/
QString QDomNode::localName() const {
    if (impl && (impl->isElement() || impl->isAttr()))
        return static_cast<const QDomNamespacedPrivate *>(impl)->localName;
    return QString();
}

/*!
    Returns \c true if the node is an element that has attributes.

    \sa attributes()
*/
bool QDomNode::hasAttributes() const {
    return impl && impl->isElement() && !static_cast<const QDomElementPrivate *>(impl)->attributes.isEmpty();
}

/*!
    Returns the value of the node: the text of a text node, comment or CDATA section, the value of an
    attribute, the data of a processing instruction, the replacement text of an entity. For the other nodes
    it is a null string.

    \sa setNodeValue(), nodeName()
*/
QString QDomNode::nodeValue() const { return impl ? impl->value : QString(); }

/*!
    Sets the value of the node to \a value, if it is a kind of node that has one; see nodeValue(). The invalid data
    policy applies to \a value.

    \sa nodeValue(), QDomImplementation::setInvalidDataPolicy()
*/
void QDomNode::setNodeValue(const QString &value) {
    if (impl)
        assignValue(impl, value);
}

/*!
    Returns the prefix of an element or attribute that was made with a namespace in mind, or a null string.

    \sa setPrefix(), namespaceURI(), localName()
*/
QString QDomNode::prefix() const {
    if (impl && (impl->isElement() || impl->isAttr()))
        return static_cast<const QDomNamespacedPrivate *>(impl)->prefix;
    return QString();
}

/*!
    Sets the prefix of an element or attribute that was made with a namespace in mind to \a pre, which changes its
    name. It does nothing for any other node. The prefix is not checked.

    \sa prefix()
*/
void QDomNode::setPrefix(const QString &pre) {
    if (!impl || !(impl->isElement() || impl->isAttr()))
        return;
    auto *node = static_cast<QDomNamespacedPrivate *>(impl);
    if (node->localName.isNull())
        return;
    node->prefix = pre;
    node->name = pre.isEmpty() ? node->localName : pre + QLatin1Char(':') + node->localName;
    qDomBumpStamp();
}

/*!
    Returns \c true if the node is an attribute.
*/
bool QDomNode::isAttr() const { return impl && impl->type == AttributeNode; }

/*!
    Returns \c true if the node is a CDATA section.
*/
bool QDomNode::isCDATASection() const { return impl && impl->type == CDATASectionNode; }

/*!
    Returns \c true if the node is a document fragment.
*/
bool QDomNode::isDocumentFragment() const { return impl && impl->type == DocumentFragmentNode; }

/*!
    Returns \c true if the node is a document.
*/
bool QDomNode::isDocument() const { return impl && impl->type == DocumentNode; }

/*!
    Returns \c true if the node is a document type.
*/
bool QDomNode::isDocumentType() const { return impl && impl->type == DocumentTypeNode; }

/*!
    Returns \c true if the node is an element.
*/
bool QDomNode::isElement() const { return impl && impl->type == ElementNode; }

/*!
    Returns \c true if the node is an entity reference.
*/
bool QDomNode::isEntityReference() const { return impl && impl->type == EntityReferenceNode; }

/*!
    Returns \c true if the node is text or a CDATA section, which is a kind of text.
*/
bool QDomNode::isText() const { return impl && impl->isText(); }

/*!
    Returns \c true if the node is an entity.
*/
bool QDomNode::isEntity() const { return impl && impl->type == EntityNode; }

/*!
    Returns \c true if the node is a notation.
*/
bool QDomNode::isNotation() const { return impl && impl->type == NotationNode; }

/*!
    Returns \c true if the node is a processing instruction.
*/
bool QDomNode::isProcessingInstruction() const { return impl && impl->type == ProcessingInstructionNode; }

/*!
    Returns \c true if the node is character data: text, a CDATA section or a comment.
*/
bool QDomNode::isCharacterData() const { return impl && impl->isCharacterData(); }

/*!
    Returns \c true if the node is a comment.
*/
bool QDomNode::isComment() const { return impl && impl->type == CommentNode; }

/*!
    Returns the first child of the node whose nodeName() is \a name, or a null node if there is none.

    \sa firstChildElement()
*/
QDomNode QDomNode::namedItem(const QString &name) const {
    if (!impl)
        return QDomNode();
    for (QDomNodePrivate *c = impl->first; c; c = c->next) {
        if (c->nodeName() == name)
            return qDomWrap<QDomNode>(c);
    }
    return QDomNode();
}

/*!
    Returns \c true if this is a null node: a handle to nothing.

    \sa clear()
*/
bool QDomNode::isNull() const { return impl == nullptr; }

/*!
    Makes this a null node. The node that it was a handle to is deleted if nothing else refers to it.
*/
void QDomNode::clear() {
    if (impl)
        impl->deref();
    impl = nullptr;
}

/*!
    Returns the node as an attribute, or a null QDomAttr if it is not one.
*/
QDomAttr QDomNode::toAttr() const { return isAttr() ? qDomWrap<QDomAttr>(impl) : QDomAttr(); }

/*!
    Returns the node as a CDATA section, or a null QDomCDATASection if it is not one.
*/
QDomCDATASection QDomNode::toCDATASection() const {
    return isCDATASection() ? qDomWrap<QDomCDATASection>(impl) : QDomCDATASection();
}

/*!
    Returns the node as a document fragment, or a null QDomDocumentFragment if it is not one.
*/
QDomDocumentFragment QDomNode::toDocumentFragment() const {
    return isDocumentFragment() ? qDomWrap<QDomDocumentFragment>(impl) : QDomDocumentFragment();
}

/*!
    Returns the node as a document, or a null QDomDocument if it is not one.
*/
QDomDocument QDomNode::toDocument() const { return isDocument() ? qDomWrap<QDomDocument>(impl) : QDomDocument(); }

/*!
    Returns the node as a document type, or a null QDomDocumentType if it is not one.
*/
QDomDocumentType QDomNode::toDocumentType() const {
    return isDocumentType() ? qDomWrap<QDomDocumentType>(impl) : QDomDocumentType();
}

/*!
    Returns the node as an element, or a null QDomElement if it is not one.
*/
QDomElement QDomNode::toElement() const { return isElement() ? qDomWrap<QDomElement>(impl) : QDomElement(); }

/*!
    Returns the node as an entity reference, or a null QDomEntityReference if it is not one.
*/
QDomEntityReference QDomNode::toEntityReference() const {
    return isEntityReference() ? qDomWrap<QDomEntityReference>(impl) : QDomEntityReference();
}

/*!
    Returns the node as text, or a null QDomText if it is not text. A CDATA section is text.
*/
QDomText QDomNode::toText() const { return isText() ? qDomWrap<QDomText>(impl) : QDomText(); }

/*!
    Returns the node as an entity, or a null QDomEntity if it is not one.
*/
QDomEntity QDomNode::toEntity() const { return isEntity() ? qDomWrap<QDomEntity>(impl) : QDomEntity(); }

/*!
    Returns the node as a notation, or a null QDomNotation if it is not one.
*/
QDomNotation QDomNode::toNotation() const { return isNotation() ? qDomWrap<QDomNotation>(impl) : QDomNotation(); }

/*!
    Returns the node as a processing instruction, or a null QDomProcessingInstruction if it is not one.
*/
QDomProcessingInstruction QDomNode::toProcessingInstruction() const {
    return isProcessingInstruction() ? qDomWrap<QDomProcessingInstruction>(impl) : QDomProcessingInstruction();
}

/*!
    Returns the node as character data, or a null QDomCharacterData if it is not text, a CDATA section or a comment.
*/
QDomCharacterData QDomNode::toCharacterData() const {
    return isCharacterData() ? qDomWrap<QDomCharacterData>(impl) : QDomCharacterData();
}

/*!
    Returns the node as a comment, or a null QDomComment if it is not one.
*/
QDomComment QDomNode::toComment() const { return isComment() ? qDomWrap<QDomComment>(impl) : QDomComment(); }

/*!
    Writes the node and what is under it to \a stream as XML. Each level is indented by \a indent spaces; with 0 the
    lines are not indented, and with -1 there are no line breaks. A document is written from the start,
    with its XML declaration if it has one, and the encoding of \a stream is set as \a encodingPolicy says; any other node
    is written where it is, with a first level of \a indent spaces.

    \code
    QFile file("out.xml");
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream stream(&file);
        document.save(stream, 2);
    }
    \endcode

    \sa QDomDocument::toString(), QDomDocument::toByteArray()
*/
void QDomNode::save(QTextStream &stream, int indent, EncodingPolicy encodingPolicy) const {
    if (!impl)
        return;
    if (impl->isDocument())
        qDomSaveDocument(impl, stream, indent, encodingPolicy);
    else
        qDomSaveNode(impl, stream, 1, indent);
}

/*!
    Returns the first child element of the node. If \a tagName is not empty, the element has that tag name,
    and if \a namespaceURI is not empty, it is in that namespace. Returns a null element if there is none.

    \sa lastChildElement(), nextSiblingElement()
*/
QDomElement QDomNode::firstChildElement(const QString &tagName, const QString &namespaceURI) const {
    if (!impl)
        return QDomElement();
    for (QDomNodePrivate *c = impl->first; c; c = c->next) {
        if (c->isElement() && (tagName.isEmpty() || c->name == tagName) &&
            (namespaceURI.isEmpty() || static_cast<const QDomElementPrivate *>(c)->namespaceUri == namespaceURI))
            return qDomWrap<QDomElement>(c);
    }
    return QDomElement();
}

/*!
    Returns the last child element of the node, with the \a tagName and \a namespaceURI as for
    firstChildElement().

    \sa firstChildElement(), previousSiblingElement()
*/
QDomElement QDomNode::lastChildElement(const QString &tagName, const QString &namespaceURI) const {
    if (!impl)
        return QDomElement();
    for (QDomNodePrivate *c = impl->last; c; c = c->prev) {
        if (c->isElement() && (tagName.isEmpty() || c->name == tagName) &&
            (namespaceURI.isEmpty() || static_cast<const QDomElementPrivate *>(c)->namespaceUri == namespaceURI))
            return qDomWrap<QDomElement>(c);
    }
    return QDomElement();
}

/*!
    Returns the element before this node among the children of the parent, with the \a tagName and
    \a namespaceURI as for firstChildElement().

    \sa nextSiblingElement(), lastChildElement()
*/
QDomElement QDomNode::previousSiblingElement(const QString &tagName, const QString &namespaceURI) const {
    if (!impl)
        return QDomElement();
    for (QDomNodePrivate *c = impl->prev; c; c = c->prev) {
        if (c->isElement() && (tagName.isEmpty() || c->name == tagName) &&
            (namespaceURI.isEmpty() || static_cast<const QDomElementPrivate *>(c)->namespaceUri == namespaceURI))
            return qDomWrap<QDomElement>(c);
    }
    return QDomElement();
}

/*!
    Returns the element after this node among the children of the parent, with the \a tagName and
    \a namespaceURI as for firstChildElement().

    \code
    for (QDomElement e = list.firstChildElement("item"); !e.isNull(); e = e.nextSiblingElement("item"))
        process(e);
    \endcode

    \sa previousSiblingElement(), firstChildElement()
*/
QDomElement QDomNode::nextSiblingElement(const QString &tagName, const QString &namespaceURI) const {
    if (!impl)
        return QDomElement();
    for (QDomNodePrivate *c = impl->next; c; c = c->next) {
        if (c->isElement() && (tagName.isEmpty() || c->name == tagName) &&
            (namespaceURI.isEmpty() || static_cast<const QDomElementPrivate *>(c)->namespaceUri == namespaceURI))
            return qDomWrap<QDomElement>(c);
    }
    return QDomElement();
}

/*!
    Returns the line where the node starts in the document that it was read from, counting from 1, or -1 if the node
    was not read from a document.

    \sa columnNumber()
*/
int QDomNode::lineNumber() const { return impl ? impl->line : -1; }

/*!
    Returns the column where the node starts in its line in the document that it was read from, counting
    from 1, or -1 if the node was not read from a document.

    \sa lineNumber()
*/
int QDomNode::columnNumber() const { return impl ? impl->column : -1; }

/*!
    \class QDomNodeList
    \brief The QDomNodeList class is a list of QDomNode objects.

    The lists that QDomNode::childNodes(), QDomDocument::elementsByTagName() and QDomElement::elementsByTagName()
    return are live: they show the tree as it is when they are asked, not as it was when they were made. A change to
    a tree makes a list look again at its next use, so that code which changes the tree while it goes through a list
    asks for a new look at every step.

    \code
    QDomNodeList items = document.elementsByTagName("item");
    for (int i = 0; i < items.size(); ++i)
        qDebug() << items.at(i).toElement().text();
    \endcode

    \differences
    The list has no iterators.
    \enddifferences

    \sa QDomNode::childNodes(), QDomNamedNodeMap
*/

/*!
    Constructs an empty list.
*/
QDomNodeList::QDomNodeList() = default;

QDomNodeList::QDomNodeList(std::shared_ptr<QDomNodeListPrivate> list) : d(std::move(list)) {}

/*!
    Constructs a list that is the same list as \a list.
*/
QDomNodeList::QDomNodeList(const QDomNodeList &list) = default;

/*!
    Makes this the same list as \a list.
*/
QDomNodeList &QDomNodeList::operator=(const QDomNodeList &list) = default;

/*!
    Destroys the list. The nodes that it lists are not affected.
*/
QDomNodeList::~QDomNodeList() = default;

/*!
    Returns \c true if \a list is this list, or a copy of it.
*/
bool QDomNodeList::operator==(const QDomNodeList &list) const { return d == list.d; }

/*!
    Returns \c true if \a list is another list than this.
*/
bool QDomNodeList::operator!=(const QDomNodeList &list) const { return d != list.d; }

/*!
    Returns the node at position \a index, counting from 0, or a null node if \a index is out of range.

    \sa length()
*/
QDomNode QDomNodeList::item(int index) const {
    if (!d || index < 0)
        return QDomNode();
    const QList<QDomNodePrivate *> &items = d->items();
    return index < items.size() ? qDomWrap<QDomNode>(items.at(index)) : QDomNode();
}

/*!
    Returns the node at position \a index, as item() does.
*/
QDomNode QDomNodeList::at(int index) const { return item(index); }

/*!
    Returns the number of nodes in the list.

    \sa item(), isEmpty()
*/
int QDomNodeList::length() const { return d ? static_cast<int>(d->items().size()) : 0; }

/*!
    Returns the number of nodes in the list, as length() does.
*/
int QDomNodeList::count() const { return length(); }

/*!
    Returns the number of nodes in the list, as length() does.
*/
int QDomNodeList::size() const { return length(); }

/*!
    Returns \c true if the list has no nodes.
*/
bool QDomNodeList::isEmpty() const { return length() == 0; }

/*!
    \class QDomNamedNodeMap
    \brief The QDomNamedNodeMap class contains a collection of nodes that can be accessed by name.

    The attributes of an element are a map from name to attribute, which QDomElement::attributes() returns,
    and so are the entities and notations of a QDomDocumentType. The nodes can also be reached by their
    position. A map is live, like QDomNodeList: it shows its element or document type as it is.

    \code
    QDomNamedNodeMap attributes = element.attributes();
    for (int i = 0; i < attributes.count(); ++i) {
        QDomAttr attribute = attributes.item(i).toAttr();
        qDebug() << attribute.name() << attribute.value();
    }
    \endcode

    The map of the entities and notations cannot be changed.

    \differences
    The nodes of a map of attributes are in the order that the attributes were added.
    \enddifferences

    \sa QDomNode::attributes(), QDomDocumentType, QDomNodeList
*/

/*!
    Constructs an empty map.
*/
QDomNamedNodeMap::QDomNamedNodeMap() = default;

QDomNamedNodeMap::QDomNamedNodeMap(const QDomNode &owner, Kind kind) : m_owner(owner), m_kind(kind) {}

/*!
    Constructs a map that is the same map as \a map.
*/
QDomNamedNodeMap::QDomNamedNodeMap(const QDomNamedNodeMap &map) = default;

/*!
    Makes this the same map as \a map.
*/
QDomNamedNodeMap &QDomNamedNodeMap::operator=(const QDomNamedNodeMap &map) = default;

/*!
    Destroys the map. The nodes in it are not affected.
*/
QDomNamedNodeMap::~QDomNamedNodeMap() = default;

/*!
    Returns \c true if \a map is a map of the same nodes as this: of the same element or document type.
*/
bool QDomNamedNodeMap::operator==(const QDomNamedNodeMap &map) const {
    return m_owner == map.m_owner && (m_owner.isNull() || m_kind == map.m_kind);
}

/*!
    Returns \c true if \a map is not a map of the same nodes as this.
*/
bool QDomNamedNodeMap::operator!=(const QDomNamedNodeMap &map) const { return !(*this == map); }

QList<QDomNodePrivate *> *QDomNamedNodeMap::nodes() const {
    QDomNodePrivate *owner = P(m_owner);
    if (!owner)
        return nullptr;
    switch (m_kind) {
    case AttributeMap:
        return owner->isElement() ? &static_cast<QDomElementPrivate *>(owner)->attributes : nullptr;
    case EntityMap:
        return owner->type == QDomNode::DocumentTypeNode ? &static_cast<QDomDocumentTypePrivate *>(owner)->entities
                                                         : nullptr;
    case NotationMap:
        return owner->type == QDomNode::DocumentTypeNode ? &static_cast<QDomDocumentTypePrivate *>(owner)->notations
                                                         : nullptr;
    }
    return nullptr;
}

/*!
    Returns the node whose name is \a name, or a null node if there is none. The name of an attribute is
    its qualified name.

    \sa namedItemNS(), contains()
*/
QDomNode QDomNamedNodeMap::namedItem(const QString &name) const {
    const QList<QDomNodePrivate *> *list = nodes();
    if (!list)
        return QDomNode();
    for (QDomNodePrivate *n : *list) {
        if (n->name == name)
            return qDomWrap<QDomNode>(n);
    }
    return QDomNode();
}

/*!
    Inserts the attribute \a newNode into the map, which puts it into the element, and returns the attribute that it
    replaced, or a null node if there was none. An attribute replaces the one with the same name. Returns a null node
    and changes nothing if the map is not one of attributes, \a newNode is not an attribute, or it belongs to an element.

    \sa setNamedItemNS(), removeNamedItem()
*/
QDomNode QDomNamedNodeMap::setNamedItem(const QDomNode &newNode) {
    if (m_kind != AttributeMap || !nodes() || !P(newNode))
        return QDomNode();
    return putAttrNode(static_cast<QDomElementPrivate *>(P(m_owner)), P(newNode), false);
}

/*!
    Removes the node whose name is \a name from the map, which removes it from the element, and returns it. Returns a
    null node if there is none or the map cannot be changed.

    \sa setNamedItem(), removeNamedItemNS()
*/
QDomNode QDomNamedNodeMap::removeNamedItem(const QString &name) {
    if (m_kind != AttributeMap || !nodes())
        return QDomNode();
    auto *element = static_cast<QDomElementPrivate *>(P(m_owner));
    QDomNode old = qDomWrap<QDomNode>(element->findAttribute(name));
    if (!old.isNull())
        element->dropAttribute(P(old));
    return old;
}

/*!
    Returns the node at position \a index, counting from 0, or a null node if \a index is out of range.

    \sa length()
*/
QDomNode QDomNamedNodeMap::item(int index) const {
    const QList<QDomNodePrivate *> *list = nodes();
    if (!list || index < 0 || index >= list->size())
        return QDomNode();
    return qDomWrap<QDomNode>(list->at(index));
}

/*!
    Returns the attribute with the local name \a localName in the namespace \a nsURI, or a null node if there
    is none.

    \sa namedItem()
*/
QDomNode QDomNamedNodeMap::namedItemNS(const QString &nsURI, const QString &localName) const {
    if (m_kind != AttributeMap || !nodes())
        return QDomNode();
    return qDomWrap<QDomNode>(static_cast<QDomElementPrivate *>(P(m_owner))->findAttributeNS(nsURI, localName));
}

/*!
    Inserts the attribute \a newNode into the map as setNamedItem() does, but it replaces the attribute that has
    the same local name in the same namespace.

    \sa setNamedItem()
*/
QDomNode QDomNamedNodeMap::setNamedItemNS(const QDomNode &newNode) {
    if (m_kind != AttributeMap || !nodes() || !P(newNode))
        return QDomNode();
    return putAttrNode(static_cast<QDomElementPrivate *>(P(m_owner)), P(newNode), true);
}

/*!
    Removes the attribute with the local name \a localName in the namespace \a nsURI from the map, and returns it.
    Returns a null node if there is none.

    \sa removeNamedItem()
*/
QDomNode QDomNamedNodeMap::removeNamedItemNS(const QString &nsURI, const QString &localName) {
    if (m_kind != AttributeMap || !nodes())
        return QDomNode();
    auto *element = static_cast<QDomElementPrivate *>(P(m_owner));
    QDomNode old = qDomWrap<QDomNode>(element->findAttributeNS(nsURI, localName));
    if (!old.isNull())
        element->dropAttribute(P(old));
    return old;
}

/*!
    Returns the number of nodes in the map.

    \sa item()
*/
int QDomNamedNodeMap::length() const {
    const QList<QDomNodePrivate *> *list = nodes();
    return list ? static_cast<int>(list->size()) : 0;
}

/*!
    Returns the number of nodes in the map, as length() does.
*/
int QDomNamedNodeMap::count() const { return length(); }

/*!
    Returns the number of nodes in the map, as length() does.
*/
int QDomNamedNodeMap::size() const { return length(); }

/*!
    Returns \c true if the map has no nodes.
*/
bool QDomNamedNodeMap::isEmpty() const { return length() == 0; }

/*!
    Returns \c true if the map has a node whose name is \a name.

    \sa namedItem()
*/
bool QDomNamedNodeMap::contains(const QString &name) const { return !namedItem(name).isNull(); }

/*!
    \class QDomDocumentType
    \brief The QDomDocumentType class is the representation of the DTD in the document tree.

    The document type is the \c{<!DOCTYPE ...>} of a document: its name, the public and system
    identifiers of an external subset, and the internal subset in square brackets. A document that has
    one has it as a child, before the document element; QDomDocument::doctype() returns it.

    The entities and notations that the internal subset declares are in entities() and notations().
    The document type is not used to check the document, and entities are not read from the external subset.

    \differences
    QDomDocument::doctype() is null for a document that has no document type.
    \enddifferences

    \sa QDomDocument::doctype(), QDomImplementation::createDocumentType()
*/

/*!
    Constructs a null document type.
*/
QDomDocumentType::QDomDocumentType() = default;

/*!
    \internal
*/
QDomDocumentType::QDomDocumentType(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the name of the document type: the name of the root element that it is for.
*/
QString QDomDocumentType::name() const { return impl ? impl->name : QString(); }

/*!
    Returns the entities that the document type declares, as a map of QDomEntity nodes.

    \sa notations()
*/
QDomNamedNodeMap QDomDocumentType::entities() const {
    return impl ? QDomPrivateAccess::makeEntityMap(*this) : QDomNamedNodeMap();
}

/*!
    Returns the notations that the document type declares, as a map of QDomNotation nodes.

    \sa entities()
*/
QDomNamedNodeMap QDomDocumentType::notations() const {
    return impl ? QDomPrivateAccess::makeNotationMap(*this) : QDomNamedNodeMap();
}

/*!
    Returns the public identifier of the external subset, or a null string.

    \sa systemId()
*/
QString QDomDocumentType::publicId() const {
    return impl ? static_cast<const QDomDocumentTypePrivate *>(impl)->publicId : QString();
}

/*!
    Returns the system identifier of the external subset, or a null string.

    \sa publicId()
*/
QString QDomDocumentType::systemId() const {
    return impl ? static_cast<const QDomDocumentTypePrivate *>(impl)->systemId : QString();
}

/*!
    Returns the internal subset: what is between the square brackets, as it was written, or a null string if there are
    none.
*/
QString QDomDocumentType::internalSubset() const {
    return impl ? static_cast<const QDomDocumentTypePrivate *>(impl)->internalSubset : QString();
}

/*!
    Returns \c DocumentTypeNode.
*/
QDomNode::NodeType QDomDocumentType::nodeType() const { return DocumentTypeNode; }

/*!
    \class QDomDocument
    \brief The QDomDocument class represents an XML document.

    The document is the root of the tree, and it makes the other nodes: createElement(), createTextNode() and the
    rest make a node that belongs to the document but is not yet in it, which appendChild() of the parent puts
    there. setContent() reads a document from text, a byte array or a device, and toString() and toByteArray()
    write one.

    \code
    QDomDocument document;
    if (const auto result = document.setContent(&file); !result) {
        qWarning() << result.errorMessage << "at line" << result.errorLine;
        return;
    }
    QDomElement root = document.documentElement();
    for (QDomElement e = root.firstChildElement("book"); !e.isNull(); e = e.nextSiblingElement("book"))
        qDebug() << e.attribute("isbn") << e.text();
    \endcode

    A document made in code:

    \code
    QDomDocument document;
    document.appendChild(document.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""));
    QDomElement root = document.createElement("library");
    document.appendChild(root);
    QDomElement book = document.createElement("book");
    book.setAttribute("isbn", "0-201-63361-2");
    book.appendChild(document.createTextNode("Design Patterns"));
    root.appendChild(book);
    const QString xml = document.toString(2);
    \endcode

    A default-constructed document is null until a node is made with it or setContent() is called.

    \differences
    The text, and not the declaration, decides the encoding of what setContent() takes as a QString. A
    parse that fails leaves the document empty. Text that is only whitespace between elements is dropped,
    unless \c PreserveSpacingOnlyNodes is set; the setContent() functions with a bool for the
    namespaces are for code written for older versions of Qt. The XML declaration is read as a processing
    instruction with the target \c xml, which is the first child of the document, and is written
    again from it. The document type is a child of the document. elementById() always returns a null element.
    \enddifferences

    \sa QDomNode, QDomElement, QXmlStreamReader
*/

/*!
    \enum QDomDocument::ParseOption

    What setContent() does in addition to reading the document.

    \value Default Nothing: names are not split into prefix and local name, and text that is only spacing is dropped.
    \value UseNamespaceProcessing Prefixes are resolved: the elements and attributes know their namespace URI, prefix
           and local name, and the namespace declarations are attributes.
    \value PreserveSpacingOnlyNodes Text that is only spacing, such as the indentation between elements, is kept as text nodes.
*/

/*!
    \typedef QDomDocument::ParseOptions

    A QFlags of QDomDocument::ParseOption values.
*/

/*!
    \class QDomDocument::ParseResult
    \brief The ParseResult struct tells how setContent() went.

    A result converts to \c true if the document was read, and otherwise holds the message of the error and
    where it is.

    \sa QDomDocument::setContent()
*/

/*!
    \variable QDomDocument::ParseResult::errorMessage

    The message of the error, which is empty if there was none.
*/

/*!
    \variable QDomDocument::ParseResult::errorLine

    The line of the error, counting from 1.
*/

/*!
    \variable QDomDocument::ParseResult::errorColumn

    The column of the error.
*/

/*!
    \fn QDomDocument::ParseResult::operator bool() const

    Returns \c true if there was no error.
*/

/*!
    Constructs a null document.
*/
QDomDocument::QDomDocument() = default;

QDomDocument::QDomDocument(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Constructs a document that has a document type with the name \a name.
*/
QDomDocument::QDomDocument(const QString &name) {
    QDomDocumentPrivate *d = ensureDocument(*this);
    auto *type = new QDomDocumentTypePrivate(d->link);
    type->name = name;
    d->appendUnchecked(type);
}

/*!
    Constructs a document that has the document type \a doctype, if it is not null.
*/
QDomDocument::QDomDocument(const QDomDocumentType &doctype) {
    QDomDocumentPrivate *d = ensureDocument(*this);
    if (P(doctype))
        d->insertBefore(P(doctype), nullptr, nullptr);
}

/*!
    Creates an element with the tag name \a tagName, which belongs to this document but has no parent, and returns it.
    Returns a null element if \a tagName is not a valid name and the invalid data policy says so.

    \sa createElementNS(), QDomNode::appendChild()
*/
QDomElement QDomDocument::createElement(const QString &tagName) {
    QString name = tagName;
    if (!checkName(&name, false))
        return QDomElement();
    auto *e = new QDomElementPrivate(ensureDocument(*this)->link);
    e->name = name;
    return qDomWrap<QDomElement>(e);
}

/*!
    Creates an empty document fragment, which belongs to this document, and returns it.
*/
QDomDocumentFragment QDomDocument::createDocumentFragment() {
    return qDomWrap<QDomDocumentFragment>(qDomNewNode(QDomNode::DocumentFragmentNode, ensureDocument(*this)->link));
}

/*!
    Creates a text node with the text \a data, which belongs to this document but has no parent, and returns it. The
    invalid data policy applies to \a data.

    \sa QDomText
*/
QDomText QDomDocument::createTextNode(const QString &data) {
    QString text = data;
    if (!checkData(&text))
        return QDomText();
    QDomNodePrivate *n = qDomNewNode(QDomNode::TextNode, ensureDocument(*this)->link);
    n->value = text;
    return qDomWrap<QDomText>(n);
}

/*!
    Creates a comment with the text \a data, which belongs to this document but has no parent, and returns it. The
    invalid data policy applies to \a data.

    \sa QDomComment
*/
QDomComment QDomDocument::createComment(const QString &data) {
    QString text = data;
    if (!checkComment(&text))
        return QDomComment();
    QDomNodePrivate *n = qDomNewNode(QDomNode::CommentNode, ensureDocument(*this)->link);
    n->value = text;
    return qDomWrap<QDomComment>(n);
}

/*!
    Creates a CDATA section with the text \a data, which belongs to this document but has no parent, and returns it. The
    invalid data policy applies to \a data.

    \sa QDomCDATASection
*/
QDomCDATASection QDomDocument::createCDATASection(const QString &data) {
    QString text = data;
    if (!checkCDATA(&text))
        return QDomCDATASection();
    QDomNodePrivate *n = qDomNewNode(QDomNode::CDATASectionNode, ensureDocument(*this)->link);
    n->value = text;
    return qDomWrap<QDomCDATASection>(n);
}

/*!
    Creates a processing instruction with the target \a target and the data \a data, which belongs to this document but has
    no parent, and returns it. The target \c xml makes the XML declaration. The invalid data policy applies to both.

    \sa QDomProcessingInstruction
*/
QDomProcessingInstruction QDomDocument::createProcessingInstruction(const QString &target, const QString &data) {
    QString name = target;
    QString text = data;
    if (!checkName(&name, false) || !checkInstructionData(&text))
        return QDomProcessingInstruction();
    QDomNodePrivate *n = qDomNewNode(QDomNode::ProcessingInstructionNode, ensureDocument(*this)->link);
    n->name = name;
    n->value = text;
    return qDomWrap<QDomProcessingInstruction>(n);
}

/*!
    Creates an attribute with the name \a name, which belongs to this document but to no element, and returns it.
    QDomElement::setAttributeNode() puts it into an element, but QDomElement::setAttribute() is
    easier.

    \sa createAttributeNS()
*/
QDomAttr QDomDocument::createAttribute(const QString &name) {
    QString n = name;
    if (!checkName(&n, false))
        return QDomAttr();
    auto *a = new QDomAttrPrivate(ensureDocument(*this)->link);
    a->name = n;
    return qDomWrap<QDomAttr>(a);
}

/*!
    Creates a reference to the entity \a name, which belongs to this document but has no parent, and returns it.

    \sa QDomEntityReference
*/
QDomEntityReference QDomDocument::createEntityReference(const QString &name) {
    QString n = name;
    if (!checkName(&n, false))
        return QDomEntityReference();
    QDomNodePrivate *r = qDomNewNode(QDomNode::EntityReferenceNode, ensureDocument(*this)->link);
    r->name = n;
    return qDomWrap<QDomEntityReference>(r);
}

/*!
    Returns a list of the elements of the document that have the tag name \a tagname, in the order that they are in the
    document. The name \c{*} stands for all of them. The list is live; see QDomNodeList.

    \sa QDomElement::elementsByTagName(), elementsByTagNameNS()
*/
QDomNodeList QDomDocument::elementsByTagName(const QString &tagname) const {
    return tagList(*this, QDomNodeListPrivate::TagName, tagname, QString());
}

/*!
    Returns a copy of the node \a importedNode that belongs to this document and has no parent. If \a deep is \c true, the
    copy has copies of all that is under the node. A document or a document type cannot be imported: a null node is
    returned.

    \sa QDomNode::cloneNode()
*/
QDomNode QDomDocument::importNode(const QDomNode &importedNode, bool deep) {
    QDomNodePrivate *source = P(importedNode);
    if (!source || source->isDocument() || source->type == DocumentTypeNode)
        return QDomNode();
    return qDomWrap<QDomNode>(qDomClone(source, deep, ensureDocument(*this)->link));
}

/*!
    Creates an element with the qualified name \a qName in the namespace \a nsURI, which belongs to this document but has
    no parent, and returns it. The prefix and local name are what are before and after the colon of \a qName.
    Returns a null element if \a qName is not a valid name and the invalid data policy says so.

    \sa createElement(), QDomNode::namespaceURI()
*/
QDomElement QDomDocument::createElementNS(const QString &nsURI, const QString &qName) {
    QString name = qName;
    if (!checkName(&name, true))
        return QDomElement();
    auto *e = new QDomElementPrivate(ensureDocument(*this)->link);
    e->name = name;
    e->namespaceUri = nsURI;
    qDomSplitName(name, &e->prefix, &e->localName);
    return qDomWrap<QDomElement>(e);
}

/*!
    Creates an attribute with the qualified name \a qName in the namespace \a nsURI, which belongs to this document but to
    no element, and returns it. Returns a null attribute if \a qName is not a valid name and the invalid data policy
    says so.

    \sa createAttribute(), createElementNS()
*/
QDomAttr QDomDocument::createAttributeNS(const QString &nsURI, const QString &qName) {
    QString name = qName;
    if (!checkName(&name, true))
        return QDomAttr();
    auto *a = new QDomAttrPrivate(ensureDocument(*this)->link);
    a->name = name;
    a->namespaceUri = nsURI;
    qDomSplitName(name, &a->prefix, &a->localName);
    return qDomWrap<QDomAttr>(a);
}

/*!
    Returns a list of the elements of the document that have the local name \a localName in the namespace \a nsURI,
    in the order that they are in the document. The name \c{*} stands for any of either. The list is live; see
    QDomNodeList.

    \sa elementsByTagName()
*/
QDomNodeList QDomDocument::elementsByTagNameNS(const QString &nsURI, const QString &localName) const {
    return tagList(*this, QDomNodeListPrivate::TagNameNS, localName, nsURI);
}

/*!
    Returns a null element. The document does not keep track of the attributes that are of the type \c ID, so there
    is no element with the ID \a elementId to return.
*/
QDomElement QDomDocument::elementById(const QString &elementId) {
    Q_UNUSED(elementId)
    return QDomElement();
}

/*!
    Returns the document type of the document, or a null document type if it has none.

    \sa QDomImplementation::createDocumentType()
*/
QDomDocumentType QDomDocument::doctype() const {
    QDomDocumentPrivate *d = documentOf(*this);
    return d ? qDomWrap<QDomDocumentType>(d->doctype()) : QDomDocumentType();
}

/*!
    Returns the implementation of the document, or a null one if the document is null.
*/
QDomImplementation QDomDocument::implementation() const {
    QDomDocumentPrivate *d = documentOf(*this);
    return d ? QDomPrivateAccess::makeImplementation(d) : QDomImplementation();
}

/*!
    Returns the root element of the document, or a null element if it has none.
*/
QDomElement QDomDocument::documentElement() const {
    QDomDocumentPrivate *d = documentOf(*this);
    return d ? qDomWrap<QDomElement>(d->documentElement()) : QDomElement();
}

/*!
    Replaces the content of the document with the XML document in \a data, as UTF-8, UTF-16 or Latin-1 text with the
    encoding that its XML declaration names. \a options say what is read and how. The result converts to \c true if
    the document was read; otherwise it has the message of the error and its position, and the document is empty.

    \code
    const auto result = document.setContent(QByteArray("<a><b/></a>"));
    Q_ASSERT(result);
    \endcode

    \sa toString(), ParseResult
*/
QDomDocument::ParseResult QDomDocument::setContent(const QByteArray &data, ParseOptions options) {
    QXmlStreamReader reader(data);
    return setContent(&reader, options);
}

/*!
    \overload
    Replaces the content of the document with the XML document that is the text \a data. An encoding in the XML
    declaration does not matter for text.
*/
QDomDocument::ParseResult QDomDocument::setContent(QAnyStringView data, ParseOptions options) {
    QXmlStreamReader reader(data.toString());
    return setContent(&reader, options);
}

/*!
    \overload
    Replaces the content of the document with the XML document in the null-terminated string \a data, which is read as
    bytes. Without it, a string literal does not tell the byte array and the string view apart.
*/
QDomDocument::ParseResult QDomDocument::setContent(const char *data, ParseOptions options) {
    return setContent(QByteArray(data), options);
}

/*!
    \overload
    Replaces the content of the document with the XML document that \a device gives. A device that is not open is opened
    for reading.
*/
QDomDocument::ParseResult QDomDocument::setContent(QIODevice *device, ParseOptions options) {
    if (!device || (!device->isOpen() && !device->open(QIODevice::ReadOnly))) {
        ParseResult result;
        result.errorMessage = QString("The device cannot be read.");
        return result;
    }
    QXmlStreamReader reader(device);
    return setContent(&reader, options);
}

/*!
    \overload
    Replaces the content of the document with the XML document that \a reader reads, from the token that it is at, to
    the end of the document. The namespace processing of \a reader is set from \a options.
*/
QDomDocument::ParseResult QDomDocument::setContent(QXmlStreamReader *reader, ParseOptions options) {
    if (!reader) {
        ParseResult result;
        result.errorMessage = QString("There is no reader.");
        return result;
    }
    return qDomParse(ensureDocument(*this), reader, options);
}

/*!
    \overload
    Replaces the content of the document with the XML document in \a text. If \a namespaceProcessing is \c true, prefixes are
    resolved. Returns \c false if there was an error, and sets \a errorMsg, \a errorLine and \a errorColumn, if they are not
    null, to the message and position. Deprecated; use the function that returns a ParseResult.
*/
bool QDomDocument::setContent(const QByteArray &text, bool namespaceProcessing, QString *errorMsg, int *errorLine,
                              int *errorColumn) {
    return reportResult(
        setContent(text, namespaceProcessing ? ParseOption::UseNamespaceProcessing : ParseOption::Default), errorMsg,
        errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a byte array, for text. Deprecated.
*/
bool QDomDocument::setContent(const QString &text, bool namespaceProcessing, QString *errorMsg, int *errorLine,
                              int *errorColumn) {
    return reportResult(setContent(QAnyStringView(text),
                                   namespaceProcessing ? ParseOption::UseNamespaceProcessing : ParseOption::Default),
                        errorMsg, errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a byte array, for what the device \a dev gives. Deprecated.
*/
bool QDomDocument::setContent(QIODevice *dev, bool namespaceProcessing, QString *errorMsg, int *errorLine,
                              int *errorColumn) {
    return reportResult(
        setContent(dev, namespaceProcessing ? ParseOption::UseNamespaceProcessing : ParseOption::Default), errorMsg,
        errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a byte array, for what \a reader reads. Deprecated.
*/
bool QDomDocument::setContent(QXmlStreamReader *reader, bool namespaceProcessing, QString *errorMsg, int *errorLine,
                              int *errorColumn) {
    return reportResult(
        setContent(reader, namespaceProcessing ? ParseOption::UseNamespaceProcessing : ParseOption::Default), errorMsg,
        errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a byte array and a bool, with namespace processing off. Deprecated.
*/
bool QDomDocument::setContent(const QByteArray &text, QString *errorMsg, int *errorLine, int *errorColumn) {
    return reportResult(setContent(text, ParseOption::Default), errorMsg, errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a string and a bool, with namespace processing off. Deprecated.
*/
bool QDomDocument::setContent(const QString &text, QString *errorMsg, int *errorLine, int *errorColumn) {
    return reportResult(setContent(QAnyStringView(text), ParseOption::Default), errorMsg, errorLine, errorColumn);
}

/*!
    \overload
    As the function that takes a device and a bool, with namespace processing off. Deprecated.
*/
bool QDomDocument::setContent(QIODevice *dev, QString *errorMsg, int *errorLine, int *errorColumn) {
    return reportResult(setContent(dev, ParseOption::Default), errorMsg, errorLine, errorColumn);
}

/*!
    Returns \c DocumentNode.
*/
QDomNode::NodeType QDomDocument::nodeType() const { return DocumentNode; }

/*!
    Returns the document as XML text, with each level indented by \a indent spaces; see QDomNode::save().

    \sa toByteArray(), setContent()
*/
QString QDomDocument::toString(int indent) const {
    QString text;
    QTextStream stream(&text);
    save(stream, indent);
    stream.flush();
    return text;
}

/*!
    Returns the document as XML text in the encoding that its XML declaration names, or UTF-8, with each level indented
    by \a indent spaces; see QDomNode::save().

    \sa toString()
*/
QByteArray QDomDocument::toByteArray(int indent) const {
    QByteArray bytes;
    QTextStream stream(&bytes);
    save(stream, indent);
    stream.flush();
    return bytes;
}

/*!
    \class QDomDocumentFragment
    \brief The QDomDocumentFragment class is a tree of QDomNodes which is not usually a complete QDomDocument.

    A fragment is a container for nodes that are made to be inserted into a tree, for example a run of nodes that
    are taken out of one place to be put in another or in several. When a fragment is inserted, its children are
    inserted and the fragment is left empty.

    \code
    QDomDocumentFragment fragment = document.createDocumentFragment();
    fragment.appendChild(document.createElement("a"));
    fragment.appendChild(document.createElement("b"));
    parent.appendChild(fragment); // parent has the children a and b now
    \endcode

    \differences
    There is no difference.
    \enddifferences

    \sa QDomNode::insertBefore(), QDomDocument::createDocumentFragment()
*/

/*!
    Constructs a null document fragment.
*/
QDomDocumentFragment::QDomDocumentFragment() = default;

/*!
    \internal
*/
QDomDocumentFragment::QDomDocumentFragment(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns \c DocumentFragmentNode.
*/
QDomNode::NodeType QDomDocumentFragment::nodeType() const { return DocumentFragmentNode; }

/*!
    \class QDomCharacterData
    \brief The QDomCharacterData class represents a generic string in the DOM.

    Text, CDATA sections and comments are character data: a string of which this class gives the text and the
    functions to read and change a part of it. The positions and lengths count UTF-16 code units, as those of QString
    do.

    \differences
    There is no difference.
    \enddifferences

    \sa QDomText, QDomComment, QDomCDATASection
*/

/*!
    Constructs a null object.
*/
QDomCharacterData::QDomCharacterData() = default;

/*!
    \internal
*/
QDomCharacterData::QDomCharacterData(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the \a count characters of the data from position \a offset, or fewer if there are not as many. Returns a
    null string if \a offset is after the end.

    \sa data()
*/
QString QDomCharacterData::substringData(unsigned long offset, unsigned long count) const {
    if (!impl || offset > static_cast<unsigned long>(impl->value.size()))
        return QString();
    return impl->value.mid(static_cast<qsizetype>(offset), count > static_cast<unsigned long>(impl->value.size())
                                                               ? impl->value.size()
                                                               : static_cast<qsizetype>(count));
}

/*!
    Appends \a arg to the data.

    \sa insertData()
*/
void QDomCharacterData::appendData(const QString &arg) {
    if (impl)
        assignValue(impl, impl->value + arg);
}

/*!
    Inserts \a arg into the data at position \a offset. Does nothing if \a offset is after the end.

    \sa appendData(), replaceData()
*/
void QDomCharacterData::insertData(unsigned long offset, const QString &arg) {
    if (!impl || offset > static_cast<unsigned long>(impl->value.size()))
        return;
    QString text = impl->value;
    text.insert(static_cast<qsizetype>(offset), arg);
    assignValue(impl, text);
}

/*!
    Removes \a count characters from the data from position \a offset, or those that there are. Does nothing if \a offset is
    after the end.

    \sa replaceData()
*/
void QDomCharacterData::deleteData(unsigned long offset, unsigned long count) { replaceData(offset, count, QString()); }

/*!
    Replaces \a count characters of the data from position \a offset, or those that there are, with \a arg. Does nothing
    if \a offset is after the end.

    \sa insertData(), deleteData()
*/
void QDomCharacterData::replaceData(unsigned long offset, unsigned long count, const QString &arg) {
    if (!impl || offset > static_cast<unsigned long>(impl->value.size()))
        return;
    QString text = impl->value;
    const unsigned long available = static_cast<unsigned long>(text.size()) - offset;
    text.remove(static_cast<qsizetype>(offset), static_cast<qsizetype>(count < available ? count : available));
    text.insert(static_cast<qsizetype>(offset), arg);
    assignValue(impl, text);
}

/*!
    Returns the number of characters in the data.

    \sa data()
*/
int QDomCharacterData::length() const { return impl ? static_cast<int>(impl->value.size()) : 0; }

/*!
    Returns the data.

    \sa setData()
*/
QString QDomCharacterData::data() const { return impl ? impl->value : QString(); }

/*!
    Sets the data to \a data. The invalid data policy applies to it.

    \sa data()
*/
void QDomCharacterData::setData(const QString &data) {
    if (impl)
        assignValue(impl, data);
}

/*!
    Returns \c CharacterDataNode.
*/
QDomNode::NodeType QDomCharacterData::nodeType() const { return CharacterDataNode; }

/*!
    \class QDomAttr
    \brief The QDomAttr class represents one attribute of a QDomElement.

    An attribute has a name and a value. QDomElement::setAttribute() and QDomElement::attribute() are the easiest way to
    work with attributes; the class is for the code that needs the node, to move it or to ask for the element that
    it is in. An attribute is not a child of its element, so it has no parent; ownerElement() is the element.

    \differences
    An attribute that has not been given a value by the program has none: value() is empty. Defaults of the document type
    are not applied, so specified() is always \c true.
    \enddifferences

    \sa QDomElement, QDomNamedNodeMap
*/

/*!
    Constructs a null attribute.
*/
QDomAttr::QDomAttr() = default;

/*!
    \internal
*/
QDomAttr::QDomAttr(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the name of the attribute: its qualified name.

    \sa value()
*/
QString QDomAttr::name() const { return impl ? impl->name : QString(); }

/*!
    Returns \c true: all attributes are given a value in the document or by the program.
*/
bool QDomAttr::specified() const { return impl && static_cast<const QDomAttrPrivate *>(impl)->specified; }

/*!
    Returns the element that has the attribute, or a null element if there is none.
*/
QDomElement QDomAttr::ownerElement() const {
    return impl ? qDomWrap<QDomElement>(static_cast<const QDomAttrPrivate *>(impl)->owner) : QDomElement();
}

/*!
    Returns the value of the attribute.

    \sa setValue()
*/
QString QDomAttr::value() const { return impl ? impl->value : QString(); }

/*!
    Sets the value of the attribute to \a value. The invalid data policy applies to it.

    \sa value()
*/
void QDomAttr::setValue(const QString &value) {
    if (impl)
        assignValue(impl, value);
}

/*!
    Returns \c AttributeNode.
*/
QDomNode::NodeType QDomAttr::nodeType() const { return AttributeNode; }

/*!
    \class QDomElement
    \brief The QDomElement class represents one element in the DOM tree.

    An element has a tag name, attributes and children. The attributes are read and set with attribute() and
    setAttribute(), and the children, which are text, other elements and the rest, are reached through the functions of
    QDomNode. text() gives the text that is in the element.

    \code
    QDomElement book = document.createElement("book");
    book.setAttribute("year", 1994);
    book.appendChild(document.createTextNode("Design Patterns"));
    library.appendChild(book);

    const int year = book.attribute("year", "0").toInt();
    \endcode

    The functions with \c NS in their name take the namespace of the attribute and its local name, as
    created by QDomDocument::createAttributeNS() or read with UseNamespaceProcessing.

    \differences
    The attributes keep the order in which they were added. The numbers that setAttribute() takes as a float or a
    double are written with the fewest digits that give the same number when they are read.
    \enddifferences

    \sa QDomNode, QDomAttr, QDomDocument
*/

/*!
    Constructs a null element.
*/
QDomElement::QDomElement() = default;

/*!
    \internal
*/
QDomElement::QDomElement(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Sets the tag name of the element to \a name.

    \sa tagName()
*/
void QDomElement::setTagName(const QString &name) {
    if (!impl)
        return;
    auto *e = static_cast<QDomElementPrivate *>(impl);
    e->name = name;
    if (!e->localName.isNull())
        qDomSplitName(name, &e->prefix, &e->localName);
    qDomBumpStamp();
}

/*!
    Returns the tag name of the element: its qualified name, with the prefix if it has one.

    \sa setTagName(), QDomNode::localName()
*/
QString QDomElement::tagName() const { return impl ? impl->name : QString(); }

/*!
    Returns the attribute with the name \a name, or a null attribute if there is none.

    \sa attribute(), setAttributeNode()
*/
QDomAttr QDomElement::attributeNode(const QString &name) {
    return impl ? qDomWrap<QDomAttr>(static_cast<QDomElementPrivate *>(impl)->findAttribute(name)) : QDomAttr();
}

/*!
    Adds the attribute \a newAttr to the element, in the place of the one with the same name if there is one, and returns the
    attribute that it replaced, or a null attribute if there was none. Returns a null attribute, and does nothing, if
    \a newAttr belongs to an element already.

    \sa attributeNode(), removeAttributeNode(), setAttribute()
*/
QDomAttr QDomElement::setAttributeNode(const QDomAttr &newAttr) {
    if (!impl || !P(newAttr))
        return QDomAttr();
    return putAttrNode(static_cast<QDomElementPrivate *>(impl), P(newAttr), false).toAttr();
}

/*!
    Removes the attribute \a oldAttr from the element, and returns it. Returns a null attribute if \a oldAttr is not an
    attribute of the element.

    \sa setAttributeNode(), removeAttribute()
*/
QDomAttr QDomElement::removeAttributeNode(const QDomAttr &oldAttr) {
    if (!impl || !P(oldAttr))
        return QDomAttr();
    auto *e = static_cast<QDomElementPrivate *>(impl);
    if (e->attributes.indexOf(P(oldAttr)) < 0)
        return QDomAttr();
    e->dropAttribute(P(oldAttr));
    return oldAttr;
}

/*!
    Returns the attribute with the local name \a localName in the namespace \a nsURI, or a null attribute if there is
    none.

    \sa attributeNode(), attributeNS()
*/
QDomAttr QDomElement::attributeNodeNS(const QString &nsURI, const QString &localName) {
    return impl ? qDomWrap<QDomAttr>(static_cast<QDomElementPrivate *>(impl)->findAttributeNS(nsURI, localName))
                : QDomAttr();
}

/*!
    Adds the attribute \a newAttr to the element as setAttributeNode() does, but it replaces the attribute that has the
    same local name in the same namespace.

    \sa setAttributeNode(), setAttributeNS()
*/
QDomAttr QDomElement::setAttributeNodeNS(const QDomAttr &newAttr) {
    if (!impl || !P(newAttr))
        return QDomAttr();
    return putAttrNode(static_cast<QDomElementPrivate *>(impl), P(newAttr), true).toAttr();
}

/*!
    Returns the value of the attribute with the name \a name, or \a defValue if there is none.

    \sa setAttribute(), hasAttribute()
*/
QString QDomElement::attribute(const QString &name, const QString &defValue) const {
    if (!impl)
        return defValue;
    const QDomNodePrivate *a = static_cast<const QDomElementPrivate *>(impl)->findAttribute(name);
    return a ? a->value : defValue;
}

/*!
    Sets the attribute with the name \a name to \a value, and makes it if there is none. The name and the value are
    subject to the invalid data policy.

    \sa attribute(), removeAttribute()
*/
void QDomElement::setAttribute(const QString &name, const QString &value) {
    if (!impl)
        return;
    auto *e = static_cast<QDomElementPrivate *>(impl);
    if (QDomNodePrivate *existing = e->findAttribute(name)) {
        assignValue(existing, value);
        return;
    }
    QString checkedName = name;
    QString checkedValue = value;
    if (!checkName(&checkedName, false) || !checkData(&checkedValue))
        return;
    auto *a = new QDomAttrPrivate(e->link);
    a->name = checkedName;
    a->value = checkedValue;
    e->putAttribute(a, -1);
}

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value.
*/
void QDomElement::setAttribute(const QString &name, qlonglong value) { setAttribute(name, QString::number(value)); }

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value.
*/
void QDomElement::setAttribute(const QString &name, qulonglong value) { setAttribute(name, QString::number(value)); }

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value.
*/
void QDomElement::setAttribute(const QString &name, int value) { setAttribute(name, QString::number(value)); }

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value.
*/
void QDomElement::setAttribute(const QString &name, uint value) { setAttribute(name, QString::number(value)); }

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value, with the fewest digits that read back as the
    same number.
*/
void QDomElement::setAttribute(const QString &name, float value) { setAttribute(name, numberText(value)); }

/*!
    \overload
    Sets the attribute with the name \a name to the number \a value, with the fewest digits that read back as the
    same number.
*/
void QDomElement::setAttribute(const QString &name, double value) { setAttribute(name, numberText(value)); }

/*!
    Removes the attribute with the name \a name, if there is one.

    \sa setAttribute(), removeAttributeNode()
*/
void QDomElement::removeAttribute(const QString &name) {
    if (!impl)
        return;
    auto *e = static_cast<QDomElementPrivate *>(impl);
    if (QDomNodePrivate *a = e->findAttribute(name))
        e->dropAttribute(a);
}

/*!
    Returns \c true if the element has an attribute with the name \a name.

    \sa attribute()
*/
bool QDomElement::hasAttribute(const QString &name) const {
    return impl && static_cast<const QDomElementPrivate *>(impl)->findAttribute(name) != nullptr;
}

/*!
    Returns the value of the attribute with the local name \a localName in the namespace \a nsURI, or \a defValue if
    there is none.

    \sa attribute(), setAttributeNS()
*/
QString QDomElement::attributeNS(const QString &nsURI, const QString &localName, const QString &defValue) const {
    if (!impl)
        return defValue;
    const QDomNodePrivate *a = static_cast<const QDomElementPrivate *>(impl)->findAttributeNS(nsURI, localName);
    return a ? a->value : defValue;
}

/*!
    Sets the attribute with the local name that \a qName has in the namespace \a nsURI to \a value, and makes it if
    there is none; the attribute is then named \a qName, with its prefix. The name and the value are subject to the invalid
    data policy.

    \sa attributeNS(), setAttribute()
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, const QString &value) {
    if (!impl)
        return;
    auto *e = static_cast<QDomElementPrivate *>(impl);
    QString name = qName;
    QString checkedValue = value;
    if (!checkName(&name, true) || !checkData(&checkedValue))
        return;
    QString prefix;
    QString local;
    qDomSplitName(name, &prefix, &local);
    if (QDomNodePrivate *existing = e->findAttributeNS(nsURI, local)) {
        existing->value = checkedValue;
        return;
    }
    auto *a = new QDomAttrPrivate(e->link);
    a->name = name;
    a->value = checkedValue;
    a->namespaceUri = nsURI;
    a->prefix = prefix;
    a->localName = local;
    e->putAttribute(a, -1);
}

/*!
    \overload
    Sets the attribute to the number \a value.
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, int value) {
    setAttributeNS(nsURI, qName, QString::number(value));
}

/*!
    \overload
    Sets the attribute to the number \a value.
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, uint value) {
    setAttributeNS(nsURI, qName, QString::number(value));
}

/*!
    \overload
    Sets the attribute to the number \a value.
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, qlonglong value) {
    setAttributeNS(nsURI, qName, QString::number(value));
}

/*!
    \overload
    Sets the attribute to the number \a value.
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, qulonglong value) {
    setAttributeNS(nsURI, qName, QString::number(value));
}

/*!
    \overload
    Sets the attribute to the number \a value, with the fewest digits that read back as the same number.
*/
void QDomElement::setAttributeNS(const QString &nsURI, const QString &qName, double value) {
    setAttributeNS(nsURI, qName, numberText(value));
}

/*!
    Removes the attribute with the local name \a localName in the namespace \a nsURI, if there is one.

    \sa removeAttribute()
*/
void QDomElement::removeAttributeNS(const QString &nsURI, const QString &localName) {
    if (!impl)
        return;
    auto *e = static_cast<QDomElementPrivate *>(impl);
    if (QDomNodePrivate *a = e->findAttributeNS(nsURI, localName))
        e->dropAttribute(a);
}

/*!
    Returns \c true if the element has an attribute with the local name \a localName in the namespace \a nsURI.

    \sa hasAttribute()
*/
bool QDomElement::hasAttributeNS(const QString &nsURI, const QString &localName) const {
    return impl && static_cast<const QDomElementPrivate *>(impl)->findAttributeNS(nsURI, localName) != nullptr;
}

/*!
    Returns a list of the elements under this one, at any depth, that have the tag name \a tagname, in document order. The
    name \c{*} stands for all of them. The list is live; see QDomNodeList.

    \sa QDomDocument::elementsByTagName(), elementsByTagNameNS()
*/
QDomNodeList QDomElement::elementsByTagName(const QString &tagname) const {
    return tagList(*this, QDomNodeListPrivate::TagName, tagname, QString());
}

/*!
    Returns a list of the elements under this one, at any depth, that have the local name \a localName in the namespace
    \a nsURI, in document order. The name \c{*} stands for any of either. The list is live; see QDomNodeList.

    \sa elementsByTagName()
*/
QDomNodeList QDomElement::elementsByTagNameNS(const QString &nsURI, const QString &localName) const {
    return tagList(*this, QDomNodeListPrivate::TagNameNS, localName, nsURI);
}

/*!
    Returns the attributes of the element.

    \sa attribute(), QDomNamedNodeMap
*/
QDomNamedNodeMap QDomElement::attributes() const {
    return impl ? QDomPrivateAccess::makeAttributeMap(*this) : QDomNamedNodeMap();
}

/*!
    Returns \c ElementNode.
*/
QDomNode::NodeType QDomElement::nodeType() const { return ElementNode; }

/*!
    Returns the text that is in the element: the text and CDATA sections that are its children, and those of its child
    elements and theirs, joined. Comments and processing instructions do not count.

    \sa QDomNode::firstChild()
*/
QString QDomElement::text() const {
    if (!impl)
        return QString();
    QString out = QString("");
    for (const QDomNodePrivate *n = impl->first; n;) {
        if (n->isText())
            out += n->value;
        if (n->first) {
            n = n->first;
            continue;
        }
        while (n && n != impl && !n->next)
            n = n->parent;
        if (!n || n == impl)
            break;
        n = n->next;
    }
    return out;
}

/*!
    \class QDomText
    \brief The QDomText class represents text data in the parsed XML document.

    The text of a document is in text nodes, which are the children of elements. The text is that of
    \l{QDomCharacterData::data()}{data()}; the characters that XML needs to write as entities, such as \c{<}, are
    stored as they are and written as entities.

    \code
    QDomText text = document.createTextNode("a < b");
    element.appendChild(text);
    \endcode

    \differences
    There is no difference.
    \enddifferences

    \sa QDomCharacterData, QDomCDATASection
*/

/*!
    Constructs a null text node.
*/
QDomText::QDomText() = default;

/*!
    \internal
*/
QDomText::QDomText(QDomNodePrivate *node) : QDomCharacterData(node) {}

/*!
    Splits the text at position \a offset into two nodes: this one keeps the text before \a offset, and a new node, which
    is inserted after this one if it has a parent, has the rest. Returns the new node, or a null node if \a offset is out of
    range.

    \sa QDomCharacterData::substringData()
*/
QDomText QDomText::splitText(int offset) {
    if (!impl || offset < 0 || offset > impl->value.size())
        return QDomText();
    QDomNodePrivate *n = qDomNewNode(impl->type, impl->link);
    QDomText result = qDomWrap<QDomText>(n);
    n->value = impl->value.mid(offset);
    impl->value.truncate(offset);
    if (impl->parent)
        impl->parent->insertAfter(n, impl);
    return result;
}

/*!
    Returns \c TextNode.
*/
QDomNode::NodeType QDomText::nodeType() const { return TextNode; }

/*!
    \class QDomComment
    \brief The QDomComment class represents an XML comment.

    A comment is written as \c{<!-- -->} in the document, and its text, in \l{QDomCharacterData::data()}{data()}, is
    what is between the dashes. The text of a comment must not have two dashes in a row or end with one.

    \code
    element.appendChild(document.createComment("added by the importer"));
    \endcode

    \differences
    There is no difference.
    \enddifferences

    \sa QDomCharacterData
*/

/*!
    Constructs a null comment.
*/
QDomComment::QDomComment() = default;

/*!
    \internal
*/
QDomComment::QDomComment(QDomNodePrivate *node) : QDomCharacterData(node) {}

/*!
    Returns \c CommentNode.
*/
QDomNode::NodeType QDomComment::nodeType() const { return CommentNode; }

/*!
    \class QDomCDATASection
    \brief The QDomCDATASection class represents an XML CDATA section.

    A CDATA section is text that is written as it is, with no need for entities: \c{<![CDATA[a < b]]>}. In the tree it is
    a text node of its own kind: QDomNode::isText() is \c true for it, and its text is in
    \l{QDomCharacterData::data()}{data()}. The text must not have \c{]]>}. When a section that has it is written, the
    section is split in two so that the document is still well-formed.

    \differences
    There is no difference.
    \enddifferences

    \sa QDomText
*/

/*!
    Constructs a null CDATA section.
*/
QDomCDATASection::QDomCDATASection() = default;

/*!
    \internal
*/
QDomCDATASection::QDomCDATASection(QDomNodePrivate *node) : QDomText(node) {}

/*!
    Returns \c CDATASectionNode.
*/
QDomNode::NodeType QDomCDATASection::nodeType() const { return CDATASectionNode; }

/*!
    \class QDomNotation
    \brief The QDomNotation class represents an XML notation.

    A notation is declared in the document type with \c{<!NOTATION>}, to name the format of the data that an
    unparsed entity has. It has an identifier in the public id, the system id or both.

    \differences
    There is no difference.
    \enddifferences

    \sa QDomDocumentType::notations()
*/

/*!
    Constructs a null notation.
*/
QDomNotation::QDomNotation() = default;

/*!
    \internal
*/
QDomNotation::QDomNotation(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the public identifier of the notation, or a null string.
*/
QString QDomNotation::publicId() const {
    return impl ? static_cast<const QDomDeclarationPrivate *>(impl)->publicId : QString();
}

/*!
    Returns the system identifier of the notation, or a null string.
*/
QString QDomNotation::systemId() const {
    return impl ? static_cast<const QDomDeclarationPrivate *>(impl)->systemId : QString();
}

/*!
    Returns \c NotationNode.
*/
QDomNode::NodeType QDomNotation::nodeType() const { return NotationNode; }

/*!
    \class QDomEntity
    \brief The QDomEntity class represents an XML entity.

    An entity is declared in the document type with \c{<!ENTITY>}. QDomNode::nodeValue() is the replacement text of an
    internal entity. An external entity has a public id, a system id or both, and an unparsed one has the name of a
    notation too. References to the entities that are declared are replaced by their text when a document is read, so an
    entity of the document type is only for a program that wants to know about them.

    \differences
    The text of an external entity is not read.
    \enddifferences

    \sa QDomDocumentType::entities(), QDomEntityReference
*/

/*!
    Constructs a null entity.
*/
QDomEntity::QDomEntity() = default;

/*!
    \internal
*/
QDomEntity::QDomEntity(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the public identifier of the entity, or a null string.
*/
QString QDomEntity::publicId() const {
    return impl ? static_cast<const QDomDeclarationPrivate *>(impl)->publicId : QString();
}

/*!
    Returns the system identifier of the entity, or a null string.
*/
QString QDomEntity::systemId() const {
    return impl ? static_cast<const QDomDeclarationPrivate *>(impl)->systemId : QString();
}

/*!
    Returns the name of the notation of an unparsed entity, or a null string.
*/
QString QDomEntity::notationName() const {
    return impl ? static_cast<const QDomDeclarationPrivate *>(impl)->notationName : QString();
}

/*!
    Returns \c EntityNode.
*/
QDomNode::NodeType QDomEntity::nodeType() const { return EntityNode; }

/*!
    \class QDomEntityReference
    \brief The QDomEntityReference class represents an XML entity reference.

    An entity reference, \c{&name;}, is in the tree where the name is of an entity that the document does not
    declare and that the program did not resolve. The references to the five predefined entities, to characters and to
    the entities of the document type are replaced by their text when a document is read.

    \differences
    There is no difference.
    \enddifferences

    \sa QDomEntity, QDomDocument::createEntityReference()
*/

/*!
    Constructs a null entity reference.
*/
QDomEntityReference::QDomEntityReference() = default;

/*!
    \internal
*/
QDomEntityReference::QDomEntityReference(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns \c EntityReferenceNode.
*/
QDomNode::NodeType QDomEntityReference::nodeType() const { return EntityReferenceNode; }

/*!
    \class QDomProcessingInstruction
    \brief The QDomProcessingInstruction class represents an XML processing instruction.

    A processing instruction is for the application that reads the document, written as \c{<?target data?>}. The XML
    declaration, \c{<?xml version="1.0" encoding="UTF-8"?>}, is a processing instruction with the target \c xml that
    is the first child of the document; QDomNode::save() takes the encoding from it.

    \code
    document.insertBefore(document.createProcessingInstruction("xml", "version=\"1.0\" encoding=\"UTF-8\""),
                          document.firstChild());
    \endcode

    \differences
    There is no difference.
    \enddifferences

    \sa QDomDocument::createProcessingInstruction()
*/

/*!
    Constructs a null processing instruction.
*/
QDomProcessingInstruction::QDomProcessingInstruction() = default;

/*!
    \internal
*/
QDomProcessingInstruction::QDomProcessingInstruction(QDomNodePrivate *node) : QDomNode(node) {}

/*!
    Returns the target of the instruction.

    \sa data()
*/
QString QDomProcessingInstruction::target() const { return impl ? impl->name : QString(); }

/*!
    Returns the data of the instruction.

    \sa setData(), target()
*/
QString QDomProcessingInstruction::data() const { return impl ? impl->value : QString(); }

/*!
    Sets the data of the instruction to \a data. The invalid data policy applies to it.

    \sa data()
*/
void QDomProcessingInstruction::setData(const QString &data) {
    if (impl)
        assignValue(impl, data);
}

/*!
    Returns \c ProcessingInstructionNode.
*/
QDomNode::NodeType QDomProcessingInstruction::nodeType() const { return ProcessingInstructionNode; }

// Writes the node to the stream as XML, indented by one space for each level.
QTextStream &operator<<(QTextStream &stream, const QDomNode &node) {
    node.save(stream, 1);
    return stream;
}

// Writes the node to the debug stream as XML on one line.
QDebug operator<<(QDebug debug, const QDomNode &node) {
    QDebugStateSaver saver(debug);
    debug.nospace().noquote();
    if (node.isNull()) {
        debug << "QDomNode(null)";
        return debug;
    }
    QString text;
    QTextStream stream(&text);
    node.save(stream, -1);
    stream.flush();
    debug << "QDomNode(" << text << ')';
    return debug;
}
