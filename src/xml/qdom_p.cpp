// MIT License - see LICENSE
#include "qdom_p.h"

#include <QtCore/qstringconverter.h>
#include <QtCore/qtextstream.h>

#include <optional>
#include <utility>
#include <vector>

namespace {

std::atomic<quint64> g_stamp{1};

bool isStar(const QString &s) { return s.size() == 1 && s.at(0) == QLatin1Char('*'); }

} // namespace

quint64 qDomStamp() { return g_stamp.load(std::memory_order_relaxed); }

void qDomBumpStamp() { g_stamp.fetch_add(1, std::memory_order_relaxed); }

QDomNodePrivate::~QDomNodePrivate() {
    QDomNodePrivate *child = first;
    while (child) {
        QDomNodePrivate *following = child->next;
        child->parent = child->prev = child->next = nullptr;
        child->deref();
        child = following;
    }
}

bool QDomNodePrivate::canHaveChildren() const {
    switch (type) {
    case QDomNode::ElementNode:
    case QDomNode::DocumentNode:
    case QDomNode::DocumentFragmentNode:
    case QDomNode::EntityReferenceNode:
        return true;
    default:
        return false;
    }
}

QString QDomNodePrivate::nodeName() const {
    switch (type) {
    case QDomNode::TextNode:
        return QString("#text");
    case QDomNode::CDATASectionNode:
        return QString("#cdata-section");
    case QDomNode::CommentNode:
        return QString("#comment");
    case QDomNode::DocumentNode:
        return QString("#document");
    case QDomNode::DocumentFragmentNode:
        return QString("#document-fragment");
    default:
        return name;
    }
}

void QDomNodePrivate::unlink(QDomNodePrivate *child) {
    if (child->prev)
        child->prev->next = child->next;
    else
        first = child->next;
    if (child->next)
        child->next->prev = child->prev;
    else
        last = child->prev;
    child->parent = child->prev = child->next = nullptr;
}

void QDomNodePrivate::linkBefore(QDomNodePrivate *child, QDomNodePrivate *refChild) {
    child->parent = this;
    if (refChild) {
        child->next = refChild;
        child->prev = refChild->prev;
        if (refChild->prev)
            refChild->prev->next = child;
        else
            first = child;
        refChild->prev = child;
    } else {
        child->prev = last;
        child->next = nullptr;
        if (last)
            last->next = child;
        else
            first = child;
        last = child;
    }
}

void QDomNodePrivate::appendUnchecked(QDomNodePrivate *child) {
    child->ref();
    linkBefore(child, nullptr);
}

// The kinds of children that each kind of node takes. A document takes one element, one document type, and
// comments and instructions. replaced is a child that is about to go, so that it does not count.
bool QDomNodePrivate::acceptsChild(const QDomNodePrivate *child, const QDomNodePrivate *replaced) const {
    if (!canHaveChildren())
        return false;
    const bool inDocument = type == QDomNode::DocumentNode;
    int elements = 0;
    int doctypes = 0;
    if (inDocument) {
        for (const QDomNodePrivate *c = first; c; c = c->next) {
            if (c == replaced || c == child)
                continue;
            elements += c->isElement() ? 1 : 0;
            doctypes += c->type == QDomNode::DocumentTypeNode ? 1 : 0;
        }
    }
    const auto accepts = [&](const QDomNodePrivate *n) {
        switch (n->type) {
        case QDomNode::ElementNode:
            ++elements;
            return true;
        case QDomNode::DocumentTypeNode:
            ++doctypes;
            return inDocument;
        case QDomNode::CommentNode:
        case QDomNode::ProcessingInstructionNode:
            return true;
        case QDomNode::TextNode:
        case QDomNode::CDATASectionNode:
        case QDomNode::EntityReferenceNode:
            return !inDocument;
        default:
            return false;
        }
    };
    bool ok = true;
    if (child->type == QDomNode::DocumentFragmentNode) {
        for (const QDomNodePrivate *c = child->first; c; c = c->next)
            ok = accepts(c) && ok;
    } else {
        ok = accepts(child);
    }
    return ok && (!inDocument || (elements <= 1 && doctypes <= 1));
}

// Inserts newChild before refChild, or at the end if that is null. A fragment is emptied into the node. The
// child that is to be replaced is left out of the rules about what a node takes.
QDomNodePrivate *QDomNodePrivate::insertBefore(QDomNodePrivate *newChild, QDomNodePrivate *refChild,
                                               QDomNodePrivate *replaced) {
    if (!newChild || newChild == refChild)
        return nullptr;
    if (refChild && refChild->parent != this)
        return nullptr;
    if (!acceptsChild(newChild, replaced))
        return nullptr;
    for (const QDomNodePrivate *p = this; p; p = p->parent) {
        if (p == newChild)
            return nullptr;
    }

    if (newChild->type == QDomNode::DocumentFragmentNode) {
        QDomNodePrivate *child = newChild->first;
        while (child) {
            QDomNodePrivate *following = child->next;
            newChild->unlink(child); // the fragment's reference passes to this node
            linkBefore(child, refChild);
            if (link && child->link != link)
                qDomAdopt(child, link);
            child = following;
        }
        qDomBumpStamp();
        return newChild;
    }

    if (newChild->parent)
        newChild->parent->unlink(newChild);
    else
        newChild->ref();
    linkBefore(newChild, refChild);
    if (link && newChild->link != link)
        qDomAdopt(newChild, link);
    qDomBumpStamp();
    return newChild;
}

QDomNodePrivate *QDomNodePrivate::insertAfter(QDomNodePrivate *newChild, QDomNodePrivate *refChild) {
    if (!newChild || newChild == refChild)
        return nullptr;
    if (refChild && refChild->parent != this)
        return nullptr;
    QDomNodePrivate *before = refChild ? refChild->next : first;
    if (before == newChild)
        return newChild;
    return insertBefore(newChild, before, nullptr);
}

QDomNodePrivate *QDomNodePrivate::replaceChild(QDomNodePrivate *newChild, QDomNodePrivate *oldChild) {
    if (!newChild || !oldChild || oldChild->parent != this)
        return nullptr;
    if (newChild == oldChild)
        return oldChild;
    if (!insertBefore(newChild, oldChild, oldChild))
        return nullptr;
    removeChild(oldChild);
    return oldChild;
}

// The caller must hold a handle to oldChild: removing the last reference deletes it.
QDomNodePrivate *QDomNodePrivate::removeChild(QDomNodePrivate *oldChild) {
    if (!oldChild || oldChild->parent != this)
        return nullptr;
    unlink(oldChild);
    qDomBumpStamp();
    oldChild->deref();
    return oldChild;
}

void QDomNodePrivate::removeAllChildren() {
    QDomNodePrivate *child = first;
    first = last = nullptr;
    while (child) {
        QDomNodePrivate *following = child->next;
        child->parent = child->prev = child->next = nullptr;
        child->deref();
        child = following;
    }
    qDomBumpStamp();
}

void QDomNodePrivate::normalize() {
    QDomNodePrivate *p = first;
    while (p) {
        QDomNodePrivate *following = p->next;
        if (p->type == QDomNode::TextNode) {
            while (following && following->type == QDomNode::TextNode) {
                p->value += following->value;
                QDomNodePrivate *merged = following;
                following = following->next;
                removeChild(merged);
            }
            if (p->value.isEmpty())
                removeChild(p);
        } else {
            p->normalize();
        }
        p = following;
    }
}

QDomElementPrivate::~QDomElementPrivate() {
    for (QDomNodePrivate *a : attributes) {
        static_cast<QDomAttrPrivate *>(a)->owner = nullptr;
        a->deref();
    }
}

QDomNodePrivate *QDomElementPrivate::findAttribute(const QString &qualifiedName) const {
    for (QDomNodePrivate *a : attributes) {
        if (a->name == qualifiedName)
            return a;
    }
    return nullptr;
}

QDomNodePrivate *QDomElementPrivate::findAttributeNS(const QString &uri, const QString &local) const {
    for (QDomNodePrivate *a : attributes) {
        const auto *attribute = static_cast<const QDomAttrPrivate *>(a);
        if (!attribute->localName.isNull() && attribute->localName == local && attribute->namespaceUri == uri)
            return a;
    }
    return nullptr;
}

void QDomElementPrivate::putAttribute(QDomNodePrivate *attribute, qsizetype index) {
    attribute->ref();
    static_cast<QDomAttrPrivate *>(attribute)->owner = this;
    if (index >= 0 && index < attributes.size()) {
        QDomNodePrivate *old = attributes.at(index);
        static_cast<QDomAttrPrivate *>(old)->owner = nullptr;
        attributes[index] = attribute;
        old->deref();
    } else {
        attributes.append(attribute);
    }
}

void QDomElementPrivate::dropAttribute(QDomNodePrivate *attribute) {
    const qsizetype index = attributes.indexOf(attribute);
    if (index < 0)
        return;
    attributes.removeAt(index);
    static_cast<QDomAttrPrivate *>(attribute)->owner = nullptr;
    attribute->deref();
}

QDomDocumentTypePrivate::~QDomDocumentTypePrivate() {
    for (QDomNodePrivate *n : entities)
        n->deref();
    for (QDomNodePrivate *n : notations)
        n->deref();
}

QDomDocumentPrivate::QDomDocumentPrivate()
    : QDomNodePrivate(QDomNode::DocumentNode, std::make_shared<QDomDocumentLink>()) {
    link->document = this;
}

QDomDocumentPrivate::~QDomDocumentPrivate() { link->document = nullptr; }

QDomNodePrivate *QDomDocumentPrivate::documentElement() const {
    for (QDomNodePrivate *c = first; c; c = c->next) {
        if (c->isElement())
            return c;
    }
    return nullptr;
}

QDomNodePrivate *QDomDocumentPrivate::doctype() const {
    for (QDomNodePrivate *c = first; c; c = c->next) {
        if (c->type == QDomNode::DocumentTypeNode)
            return c;
    }
    return nullptr;
}

const QList<QDomNodePrivate *> &QDomNodeListPrivate::items() {
    const quint64 now = qDomStamp();
    if (built && stamp == now)
        return m_items;
    built = true;
    stamp = now;
    m_items.clear();
    QDomNodePrivate *top = QDomPrivateAccess::impl(root);
    if (!top)
        return m_items;
    if (kind == Children) {
        for (QDomNodePrivate *c = top->first; c; c = c->next)
            m_items.append(c);
        return m_items;
    }
    QDomNodePrivate *n = top->first;
    while (n) {
        if (n->isElement()) {
            bool matches;
            if (kind == TagName) {
                matches = isStar(tag) || n->name == tag;
            } else {
                const auto *e = static_cast<const QDomElementPrivate *>(n);
                matches = !e->localName.isNull() && (isStar(namespaceUri) || e->namespaceUri == namespaceUri) &&
                          (isStar(tag) || e->localName == tag);
            }
            if (matches)
                m_items.append(n);
        }
        if (n->first) {
            n = n->first;
            continue;
        }
        while (n != top && !n->next)
            n = n->parent;
        if (n == top)
            break;
        n = n->next;
    }
    return m_items;
}

QDomDocumentPrivate *qDomNewDocument() { return new QDomDocumentPrivate; }

QDomNodePrivate *qDomNewNode(QDomNode::NodeType type, const QDomDocumentLinkPtr &link) {
    switch (type) {
    case QDomNode::ElementNode:
        return new QDomElementPrivate(link);
    case QDomNode::AttributeNode:
        return new QDomAttrPrivate(link);
    case QDomNode::EntityNode:
    case QDomNode::NotationNode:
        return new QDomDeclarationPrivate(type, link);
    case QDomNode::DocumentTypeNode:
        return new QDomDocumentTypePrivate(link);
    case QDomNode::DocumentNode:
        return qDomNewDocument();
    default:
        return new QDomNodePrivate(type, link);
    }
}

QDomNodePrivate *qDomClone(const QDomNodePrivate *src, bool deep, const QDomDocumentLinkPtr &link) {
    QDomNodePrivate *copy = qDomNewNode(src->type, link);
    copy->name = src->name;
    copy->value = src->value;
    copy->line = src->line;
    copy->column = src->column;
    switch (src->type) {
    case QDomNode::ElementNode:
    case QDomNode::AttributeNode: {
        const auto *from = static_cast<const QDomNamespacedPrivate *>(src);
        auto *to = static_cast<QDomNamespacedPrivate *>(copy);
        to->namespaceUri = from->namespaceUri;
        to->prefix = from->prefix;
        to->localName = from->localName;
        if (src->isAttr()) {
            static_cast<QDomAttrPrivate *>(copy)->specified = static_cast<const QDomAttrPrivate *>(src)->specified;
        } else {
            auto *element = static_cast<QDomElementPrivate *>(copy);
            for (const QDomNodePrivate *a : static_cast<const QDomElementPrivate *>(src)->attributes)
                element->putAttribute(qDomClone(a, true, copy->link), -1);
        }
        break;
    }
    case QDomNode::EntityNode:
    case QDomNode::NotationNode: {
        const auto *from = static_cast<const QDomDeclarationPrivate *>(src);
        auto *to = static_cast<QDomDeclarationPrivate *>(copy);
        to->publicId = from->publicId;
        to->systemId = from->systemId;
        to->notationName = from->notationName;
        break;
    }
    case QDomNode::DocumentTypeNode: {
        const auto *from = static_cast<const QDomDocumentTypePrivate *>(src);
        auto *to = static_cast<QDomDocumentTypePrivate *>(copy);
        to->publicId = from->publicId;
        to->systemId = from->systemId;
        to->internalSubset = from->internalSubset;
        for (const QDomNodePrivate *n : from->entities) {
            QDomNodePrivate *c = qDomClone(n, true, copy->link);
            c->ref();
            to->entities.append(c);
        }
        for (const QDomNodePrivate *n : from->notations) {
            QDomNodePrivate *c = qDomClone(n, true, copy->link);
            c->ref();
            to->notations.append(c);
        }
        break;
    }
    default:
        break;
    }
    if (deep) {
        for (const QDomNodePrivate *c = src->first; c; c = c->next)
            copy->appendUnchecked(qDomClone(c, true, copy->link));
    }
    return copy;
}

void qDomAdopt(QDomNodePrivate *node, const QDomDocumentLinkPtr &link) {
    node->link = link;
    if (node->isElement()) {
        for (QDomNodePrivate *a : static_cast<QDomElementPrivate *>(node)->attributes)
            a->link = link;
    }
    for (QDomNodePrivate *c = node->first; c; c = c->next)
        qDomAdopt(c, link);
}

void qDomSplitName(const QString &qualifiedName, QString *prefix, QString *localName) {
    const qsizetype colon = qualifiedName.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        *prefix = QString();
        *localName = qualifiedName;
    } else {
        *prefix = qualifiedName.left(colon);
        *localName = qualifiedName.mid(colon + 1);
    }
}

// Writing

namespace {

// The characters that would be taken for markup, or lost when the text is read again.
QString encodeText(const QString &text, bool attribute) {
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '<':
            out += QString("&lt;");
            break;
        case '>':
            out += QString("&gt;");
            break;
        case '&':
            out += QString("&amp;");
            break;
        case '\r':
            out += QString("&#xd;");
            break;
        case '"':
            out += attribute ? QString("&quot;") : QString(c);
            break;
        case '\'':
            out += attribute ? QString("&apos;") : QString(c);
            break;
        case '\n':
            out += attribute ? QString("&#xa;") : QString(c);
            break;
        case '\t':
            out += attribute ? QString("&#x9;") : QString(c);
            break;
        default:
            out += c;
            break;
        }
    }
    return out;
}

QString quoted(const QString &value) {
    const QChar quote = value.contains(QLatin1Char('"')) ? QLatin1Char('\'') : QLatin1Char('"');
    return QString(quote) + value + QString(quote);
}

struct SaveContext {
    QTextStream &stream;
    int indent;
    std::vector<std::pair<QString, QString>> scope; // the prefixes in scope and their URIs, the innermost last

    const QString *lookup(const QString &prefix) const {
        for (auto it = scope.rbegin(); it != scope.rend(); ++it) {
            if (it->first == prefix)
                return &it->second;
        }
        return nullptr;
    }
    void pad(int depth) {
        if (indent >= 1)
            stream << QString(static_cast<qsizetype>(depth) * indent, QLatin1Char(' '));
    }
    void newline() {
        if (indent != -1)
            stream << QChar(u'\n');
    }
};

bool afterText(const QDomNodePrivate *n) { return n->prev && n->prev->isText(); }

bool beforeText(const QDomNodePrivate *n) { return n->next && n->next->isText(); }

bool isNamespaceDeclaration(const QDomNodePrivate *attribute) {
    return attribute->name == QLatin1StringView("xmlns") || attribute->name.startsWith(QLatin1StringView("xmlns:"));
}

void saveNode(const QDomNodePrivate *n, SaveContext &c, int depth);

void saveElement(const QDomElementPrivate *e, SaveContext &c, int depth) {
    QTextStream &s = c.stream;
    if (!afterText(e))
        c.pad(depth);

    const std::size_t mark = c.scope.size();
    for (const QDomNodePrivate *a : e->attributes) {
        if (a->name == QLatin1StringView("xmlns"))
            c.scope.emplace_back(QString(), a->value);
        else if (a->name.startsWith(QLatin1StringView("xmlns:")))
            c.scope.emplace_back(a->name.mid(6), a->value);
    }

    // A name in a namespace is written with the declaration that it needs, unless one is in scope.
    QString declarations;
    const auto need = [&](const QString &prefix, const QString &uri, bool forElement) {
        if (prefix == QLatin1StringView("xml"))
            return;
        const QString *bound = c.lookup(prefix);
        if (uri.isEmpty()) {
            if (forElement && prefix.isEmpty() && bound && !bound->isEmpty()) {
                declarations += QString(" xmlns=\"\"");
                c.scope.emplace_back(QString(), QString());
            }
            return;
        }
        if (bound && *bound == uri)
            return;
        declarations += prefix.isEmpty() ? QString(" xmlns=\"") : QString(" xmlns:") + prefix + QString("=\"");
        declarations += encodeText(uri, true) + QString("\"");
        c.scope.emplace_back(prefix, uri);
    };
    if (!e->localName.isNull())
        need(e->prefix, e->namespaceUri, true);
    for (const QDomNodePrivate *a : e->attributes) {
        const auto *attribute = static_cast<const QDomAttrPrivate *>(a);
        if (!attribute->localName.isNull() && !attribute->prefix.isEmpty() && !isNamespaceDeclaration(a))
            need(attribute->prefix, attribute->namespaceUri, false);
    }

    s << QChar(u'<') << e->name << declarations;
    for (const QDomNodePrivate *a : e->attributes)
        s << QChar(u' ') << a->name << QString("=\"") << encodeText(a->value, true) << QChar(u'"');

    if (e->last) {
        s << QChar(u'>');
        if (!e->first->isText())
            c.newline();
        for (const QDomNodePrivate *child = e->first; child; child = child->next)
            saveNode(child, c, depth + 1);
        if (!e->last->isText())
            c.pad(depth);
        s << QString("</") << e->name << QChar(u'>');
    } else {
        s << QString("/>");
    }
    if (!beforeText(e))
        c.newline();
    c.scope.resize(mark);
}

void saveNode(const QDomNodePrivate *n, SaveContext &c, int depth) {
    QTextStream &s = c.stream;
    switch (n->type) {
    case QDomNode::ElementNode:
        saveElement(static_cast<const QDomElementPrivate *>(n), c, depth);
        break;
    case QDomNode::AttributeNode:
        s << n->name << QString("=\"") << encodeText(n->value, true) << QChar(u'"');
        break;
    case QDomNode::TextNode:
        s << encodeText(n->value, false);
        break;
    case QDomNode::CDATASectionNode: {
        QString data = n->value;
        data.replace(QString("]]>"), QString("]]]]><![CDATA[>"));
        s << QString("<![CDATA[") << data << QString("]]>");
        break;
    }
    case QDomNode::CommentNode:
        if (!afterText(n))
            c.pad(depth);
        s << QString("<!--") << n->value << QString("-->");
        if (!beforeText(n))
            c.newline();
        break;
    case QDomNode::ProcessingInstructionNode:
        if (!afterText(n))
            c.pad(depth);
        s << QString("<?") << n->name;
        if (!n->value.isEmpty())
            s << QChar(u' ') << n->value;
        s << QString("?>");
        if (!beforeText(n))
            c.newline();
        break;
    case QDomNode::EntityReferenceNode:
        s << QChar(u'&') << n->name << QChar(u';');
        break;
    case QDomNode::DocumentFragmentNode:
    case QDomNode::DocumentNode:
        for (const QDomNodePrivate *child = n->first; child; child = child->next)
            saveNode(child, c, depth);
        break;
    case QDomNode::DocumentTypeNode: {
        const auto *t = static_cast<const QDomDocumentTypePrivate *>(n);
        if (t->name.isEmpty())
            break;
        s << QString("<!DOCTYPE ") << t->name;
        if (!t->publicId.isNull())
            s << QString(" PUBLIC ") << quoted(t->publicId) << QChar(u' ') << quoted(t->systemId);
        else if (!t->systemId.isNull())
            s << QString(" SYSTEM ") << quoted(t->systemId);
        if (!t->internalSubset.isEmpty())
            s << QString(" [") << t->internalSubset << QChar(u']');
        s << QChar(u'>');
        c.newline();
        break;
    }
    case QDomNode::EntityNode: {
        const auto *d = static_cast<const QDomDeclarationPrivate *>(n);
        s << QString("<!ENTITY ") << d->name;
        if (!d->publicId.isNull())
            s << QString(" PUBLIC ") << quoted(d->publicId) << QChar(u' ') << quoted(d->systemId);
        else if (!d->systemId.isNull())
            s << QString(" SYSTEM ") << quoted(d->systemId);
        else
            s << QChar(u' ') << quoted(d->value);
        if (!d->notationName.isEmpty())
            s << QString(" NDATA ") << d->notationName;
        s << QChar(u'>');
        c.newline();
        break;
    }
    case QDomNode::NotationNode: {
        const auto *d = static_cast<const QDomDeclarationPrivate *>(n);
        s << QString("<!NOTATION ") << d->name;
        if (!d->publicId.isNull()) {
            s << QString(" PUBLIC ") << quoted(d->publicId);
            if (!d->systemId.isNull())
                s << QChar(u' ') << quoted(d->systemId);
        } else if (!d->systemId.isNull()) {
            s << QString(" SYSTEM ") << quoted(d->systemId);
        }
        s << QChar(u'>');
        c.newline();
        break;
    }
    default:
        break;
    }
}

// The value of encoding in the text of the XML declaration, or an empty string.
QString declaredEncoding(const QString &declaration) {
    qsizetype i = declaration.indexOf(QLatin1StringView("encoding"));
    if (i < 0)
        return QString();
    i += 8;
    while (i < declaration.size() && declaration.at(i).isSpace())
        ++i;
    if (i >= declaration.size() || declaration.at(i) != QLatin1Char('='))
        return QString();
    ++i;
    while (i < declaration.size() && declaration.at(i).isSpace())
        ++i;
    if (i >= declaration.size())
        return QString();
    const QChar quote = declaration.at(i);
    if (quote != QLatin1Char('"') && quote != QLatin1Char('\''))
        return QString();
    const qsizetype end = declaration.indexOf(quote, i + 1);
    return end < 0 ? QString() : declaration.mid(i + 1, end - i - 1);
}

} // namespace

void qDomSaveNode(const QDomNodePrivate *node, QTextStream &stream, int depth, int indent) {
    SaveContext context{stream, indent, {}};
    saveNode(node, context, depth);
}

void qDomSaveDocument(const QDomNodePrivate *document, QTextStream &stream, int indent,
                      QDomNode::EncodingPolicy policy) {
    if (policy == QDomNode::EncodingFromDocument && !stream.string()) {
        QString encoding;
        const QDomNodePrivate *first = document->first;
        if (first && first->type == QDomNode::ProcessingInstructionNode && first->name == QLatin1StringView("xml"))
            encoding = declaredEncoding(first->value);
        std::optional<QStringConverter::Encoding> converter;
        if (!encoding.isEmpty())
            converter = QStringConverter::encodingForName(QAnyStringView(encoding));
        stream.setEncoding(converter.value_or(QStringConverter::Utf8));
    }
    SaveContext context{stream, indent, {}};
    saveNode(document, context, 0);
}
