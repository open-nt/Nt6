// MIT License - see LICENSE
#include "qdom_p.h"

#include <QtCore/qxmlstream.h>

namespace {

// Nesting this deep is no document that a person wrote, and a tree this deep is more than the recursion of the
// functions that visit it should have to take.
constexpr int kMaxDepth = 1024;

QString textOf(QStringView view) { return view.isEmpty() ? QString() : view.toString(); }

bool allSpace(const QString &text) {
    for (const QChar c : text) {
        if (c != QLatin1Char(' ') && c != QLatin1Char('\t') && c != QLatin1Char('\n') && c != QLatin1Char('\r'))
            return false;
    }
    return true;
}

// The part of a document type declaration between the square brackets, as it was written.
QString internalSubsetOf(const QString &declaration) {
    QChar quote;
    qsizetype open = -1;
    for (qsizetype i = 0; i < declaration.size(); ++i) {
        const QChar c = declaration.at(i);
        if (!quote.isNull()) {
            if (c == quote)
                quote = QChar();
        } else if (c == QLatin1Char('"') || c == QLatin1Char('\'')) {
            quote = c;
        } else if (c == QLatin1Char('[')) {
            open = i;
            break;
        }
    }
    const qsizetype close = declaration.lastIndexOf(QLatin1Char(']'));
    if (open < 0 || close <= open)
        return QString();
    return declaration.mid(open + 1, close - open - 1);
}

void addDeclarations(QDomDocumentTypePrivate *type, const QXmlStreamReader &reader, const QDomDocumentLinkPtr &link) {
    const QXmlStreamEntityDeclarations entities = reader.entityDeclarations();
    for (const QXmlStreamEntityDeclaration &declaration : entities) {
        auto *entity = new QDomDeclarationPrivate(QDomNode::EntityNode, link);
        entity->name = declaration.name().toString();
        entity->value = textOf(declaration.value());
        entity->publicId = textOf(declaration.publicId());
        entity->systemId = textOf(declaration.systemId());
        entity->notationName = textOf(declaration.notationName());
        entity->ref();
        type->entities.append(entity);
    }
    const QXmlStreamNotationDeclarations notations = reader.notationDeclarations();
    for (const QXmlStreamNotationDeclaration &declaration : notations) {
        auto *notation = new QDomDeclarationPrivate(QDomNode::NotationNode, link);
        notation->name = declaration.name().toString();
        notation->publicId = textOf(declaration.publicId());
        notation->systemId = textOf(declaration.systemId());
        notation->ref();
        type->notations.append(notation);
    }
}

} // namespace

QDomDocument::ParseResult qDomParse(QDomDocumentPrivate *doc, QXmlStreamReader *reader,
                                    QDomDocument::ParseOptions options) {
    doc->removeAllChildren();
    const bool namespaces = options.testFlag(QDomDocument::ParseOption::UseNamespaceProcessing);
    const bool keepSpace = options.testFlag(QDomDocument::ParseOption::PreserveSpacingOnlyNodes);
    reader->setNamespaceProcessing(namespaces);
    const QDomDocumentLinkPtr &link = doc->link;

    QDomNodePrivate *current = doc;
    int depth = 0;
    QString failure;

    // Text arrives in pieces; it becomes one node, or none if it is only spacing.
    QString text;
    bool hasText = false;
    int textLine = -1;
    int textColumn = -1;
    const auto flushText = [&] {
        if (hasText && current != doc && (keepSpace || !allSpace(text))) {
            QDomNodePrivate *node = qDomNewNode(QDomNode::TextNode, link);
            node->value = text;
            node->line = textLine;
            node->column = textColumn;
            current->appendUnchecked(node);
        }
        text.clear();
        hasText = false;
    };
    const auto addLeaf = [&](QDomNode::NodeType type, const QString &name, const QString &value) {
        QDomNodePrivate *node = qDomNewNode(type, link);
        node->name = name;
        node->value = value;
        node->line = static_cast<int>(reader->lineNumber());
        node->column = static_cast<int>(reader->columnNumber());
        current->appendUnchecked(node);
    };

    QXmlStreamReader::TokenType token = reader->tokenType();
    if (token == QXmlStreamReader::NoToken)
        token = reader->readNext();
    for (;; token = reader->readNext()) {
        if (token == QXmlStreamReader::EndDocument || token == QXmlStreamReader::Invalid ||
            token == QXmlStreamReader::NoToken || !failure.isEmpty())
            break;
        switch (token) {
        case QXmlStreamReader::StartDocument:
            if (current == doc && !reader->documentVersion().isEmpty()) {
                QString data = QString("version=\"") + reader->documentVersion().toString() + QString("\"");
                if (!reader->documentEncoding().isEmpty())
                    data += QString(" encoding=\"") + reader->documentEncoding().toString() + QString("\"");
                if (reader->hasStandaloneDeclaration())
                    data += QString(reader->isStandaloneDocument() ? " standalone=\"yes\"" : " standalone=\"no\"");
                addLeaf(QDomNode::ProcessingInstructionNode, QString("xml"), data);
            }
            break;
        case QXmlStreamReader::DTD: {
            flushText();
            auto *type = new QDomDocumentTypePrivate(link);
            type->name = reader->dtdName().toString();
            type->publicId = textOf(reader->dtdPublicId());
            type->systemId = textOf(reader->dtdSystemId());
            type->internalSubset = internalSubsetOf(reader->text().toString());
            addDeclarations(type, *reader, link);
            type->line = static_cast<int>(reader->lineNumber());
            type->column = static_cast<int>(reader->columnNumber());
            current->appendUnchecked(type);
            break;
        }
        case QXmlStreamReader::StartElement: {
            flushText();
            if (++depth > kMaxDepth) {
                failure = QString("The elements are nested too deeply.");
                break;
            }
            auto *element = new QDomElementPrivate(link);
            element->name = reader->qualifiedName().toString();
            element->line = static_cast<int>(reader->lineNumber());
            element->column = static_cast<int>(reader->columnNumber());
            if (namespaces) {
                element->localName = reader->name().toString();
                element->prefix = textOf(reader->prefix());
                element->namespaceUri = textOf(reader->namespaceUri());
                const QXmlStreamNamespaceDeclarations declarations = reader->namespaceDeclarations();
                for (const QXmlStreamNamespaceDeclaration &declaration : declarations) {
                    auto *attribute = new QDomAttrPrivate(link);
                    const QString prefix = declaration.prefix().toString();
                    attribute->name = prefix.isEmpty() ? QString("xmlns") : QString("xmlns:") + prefix;
                    attribute->namespaceUri = QString("http://www.w3.org/2000/xmlns/");
                    attribute->prefix = prefix.isEmpty() ? QString() : QString("xmlns");
                    attribute->localName = prefix.isEmpty() ? QString("xmlns") : prefix;
                    attribute->value = declaration.namespaceUri().toString();
                    element->putAttribute(attribute, -1);
                }
            }
            const QXmlStreamAttributes attributes = reader->attributes();
            for (const QXmlStreamAttribute &source : attributes) {
                auto *attribute = new QDomAttrPrivate(link);
                attribute->name = source.qualifiedName().toString();
                attribute->value = source.value().toString();
                if (namespaces) {
                    attribute->localName = source.name().toString();
                    attribute->prefix = textOf(source.prefix());
                    attribute->namespaceUri = textOf(source.namespaceUri());
                }
                element->putAttribute(attribute, -1);
            }
            current->appendUnchecked(element);
            current = element;
            break;
        }
        case QXmlStreamReader::EndElement:
            flushText();
            if (current != doc) {
                current = current->parent;
                --depth;
            }
            break;
        case QXmlStreamReader::Characters:
            if (reader->isCDATA()) {
                flushText();
                if (current != doc)
                    addLeaf(QDomNode::CDATASectionNode, QString(), reader->text().toString());
            } else {
                if (!hasText) {
                    hasText = true;
                    textLine = static_cast<int>(reader->lineNumber());
                    textColumn = static_cast<int>(reader->columnNumber());
                }
                text += reader->text().toString();
            }
            break;
        case QXmlStreamReader::Comment:
            flushText();
            addLeaf(QDomNode::CommentNode, QString(), reader->text().toString());
            break;
        case QXmlStreamReader::ProcessingInstruction:
            flushText();
            addLeaf(QDomNode::ProcessingInstructionNode, reader->processingInstructionTarget().toString(),
                    reader->processingInstructionData().toString());
            break;
        case QXmlStreamReader::EntityReference:
            flushText();
            if (current != doc)
                addLeaf(QDomNode::EntityReferenceNode, reader->name().toString(), QString());
            break;
        default:
            break;
        }
    }

    if (token == QXmlStreamReader::EndDocument && failure.isEmpty() && !reader->hasError()) {
        flushText();
        return {};
    }
    QDomDocument::ParseResult result;
    result.errorMessage = failure.isEmpty() ? reader->errorString() : failure;
    if (result.errorMessage.isEmpty())
        result.errorMessage = QString("The document ended before it was complete.");
    result.errorLine = static_cast<qsizetype>(reader->lineNumber());
    result.errorColumn = static_cast<qsizetype>(reader->columnNumber());
    doc->removeAllChildren();
    return result;
}
