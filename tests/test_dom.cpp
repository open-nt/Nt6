// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QBuffer>
#include <QtCore/QTextStream>
#include <QtCore/QXmlStreamReader>
#include <QtXml/QtXml>

#include <cstdio>
#include <vector>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

#define CHECK_EQ(actual, expected)                                                                                     \
    do {                                                                                                               \
        const QString a_ = (actual);                                                                                   \
        const QString e_ = (expected);                                                                                 \
        if (a_ != e_) {                                                                                                \
            std::printf("FAIL %s:%d  %s\n  got:      [%s]\n  expected: [%s]\n", __FILE__, __LINE__, #actual,           \
                        qPrintable(a_), qPrintable(e_));                                                               \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using Opt = QDomDocument::ParseOption;

static const char *kLibrary = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<!-- c -->\n"
                              "<lib x=\"1\" y=\"2\">\n"
                              "  <book id=\"a\">One &amp; &lt;two&gt;</book>\n"
                              "  <book id=\"b\"/>\n"
                              "</lib>\n";

static QDomDocument parse(const QByteArray &xml, QDomDocument::ParseOptions options = Opt::Default) {
    QDomDocument doc;
    const auto result = doc.setContent(xml, options);
    if (!result)
        std::printf("parse error: %s (%lld:%lld) in %s\n", qPrintable(result.errorMessage),
                    static_cast<long long>(result.errorLine), static_cast<long long>(result.errorColumn),
                    xml.constData());
    return doc;
}

static void testParseAndNavigate() {
    QDomDocument doc = parse(kLibrary);
    CHECK(!doc.isNull());
    CHECK(doc.nodeType() == QDomNode::DocumentNode);
    CHECK(doc.isDocument());
    CHECK_EQ(doc.nodeName(), "#document");

    const QDomNode declaration = doc.firstChild();
    CHECK(declaration.isProcessingInstruction());
    const QDomProcessingInstruction pi = declaration.toProcessingInstruction();
    CHECK_EQ(pi.target(), "xml");
    CHECK_EQ(pi.data(), "version=\"1.0\" encoding=\"UTF-8\"");
    CHECK(declaration.nextSibling().isComment());
    CHECK_EQ(declaration.nextSibling().toComment().data(), " c ");

    const QDomElement root = doc.documentElement();
    CHECK_EQ(root.tagName(), "lib");
    CHECK(root.parentNode() == doc);
    CHECK(root.ownerDocument() == doc);
    CHECK_EQ(root.attribute("x"), "1");
    CHECK_EQ(root.attribute("missing", "dflt"), "dflt");
    CHECK(root.hasAttribute("y"));
    CHECK(!root.hasAttribute("z"));
    CHECK(root.hasAttributes());
    CHECK(root.hasChildNodes());
    // The indentation between the elements is dropped.
    CHECK(root.childNodes().size() == 2);
    CHECK(root.childNodes().length() == 2);
    CHECK(root.childNodes().count() == 2);
    CHECK(!root.childNodes().isEmpty());

    const QDomElement first = root.firstChildElement("book");
    CHECK(first == root.firstChild().toElement());
    CHECK_EQ(first.attribute("id"), "a");
    CHECK_EQ(first.text(), "One & <two>");
    CHECK(first.firstChild().isText());
    CHECK_EQ(first.firstChild().nodeValue(), "One & <two>");
    CHECK_EQ(first.firstChild().nodeName(), "#text");
    const QDomElement second = first.nextSiblingElement("book");
    CHECK_EQ(second.attribute("id"), "b");
    CHECK(second.previousSiblingElement() == first);
    CHECK(second.nextSiblingElement().isNull());
    CHECK(root.lastChildElement() == second);
    CHECK(root.lastChildElement("nope").isNull());
    CHECK(root.firstChild().previousSibling().isNull());
    CHECK(root.lastChild() == second);
    CHECK_EQ(second.text(), "");
    CHECK(!second.text().isNull());

    // Positions.
    CHECK(root.lineNumber() == 3);
    CHECK(first.lineNumber() == 4);
    QDomDocument built;
    CHECK(built.createElement("a").lineNumber() == -1);

    // The nodes of a document that was read keep their order.
    const QDomNamedNodeMap attributes = root.attributes();
    CHECK(attributes.length() == 2);
    CHECK_EQ(attributes.item(0).nodeName(), "x");
    CHECK_EQ(attributes.item(1).nodeName(), "y");
    CHECK(attributes.item(2).isNull());
    CHECK(attributes.contains("x"));
    CHECK(!attributes.contains("q"));
    CHECK_EQ(attributes.namedItem("y").toAttr().value(), "2");
    CHECK(attributes.namedItem("q").isNull());
}

static void testWhitespace() {
    QDomDocument doc = parse("<a> <b/> text <c>  </c></a>");
    QDomElement a = doc.documentElement();
    CHECK(a.childNodes().size() == 3);
    CHECK(a.childNodes().at(1).isText());
    CHECK_EQ(a.childNodes().at(1).nodeValue(), " text ");
    CHECK(a.lastChild().firstChild().isNull());

    QDomDocument kept = parse("<a> <b/> text <c>  </c></a>", Opt::PreserveSpacingOnlyNodes);
    a = kept.documentElement();
    CHECK(a.childNodes().size() == 4);
    CHECK_EQ(a.firstChild().nodeValue(), " ");
    CHECK_EQ(a.lastChild().firstChild().nodeValue(), "  ");

    // A CDATA section is never dropped.
    QDomDocument cdata = parse("<a><![CDATA[  ]]></a>");
    CHECK(cdata.documentElement().firstChild().isCDATASection());
    CHECK(cdata.documentElement().firstChild().isText());
    CHECK_EQ(cdata.documentElement().text(), "  ");
}

static void testSave() {
    QDomDocument doc = parse(kLibrary);
    CHECK_EQ(doc.toString(2), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<!-- c -->\n"
                              "<lib x=\"1\" y=\"2\">\n"
                              "  <book id=\"a\">One &amp; &lt;two&gt;</book>\n"
                              "  <book id=\"b\"/>\n"
                              "</lib>\n");
    CHECK_EQ(doc.toString(-1), "<?xml version=\"1.0\" encoding=\"UTF-8\"?><!-- c --><lib x=\"1\" y=\"2\">"
                               "<book id=\"a\">One &amp; &lt;two&gt;</book><book id=\"b\"/></lib>");
    CHECK_EQ(doc.toString(0), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                              "<!-- c -->\n"
                              "<lib x=\"1\" y=\"2\">\n"
                              "<book id=\"a\">One &amp; &lt;two&gt;</book>\n"
                              "<book id=\"b\"/>\n"
                              "</lib>\n");
    CHECK_EQ(doc.toString(), "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                             "<!-- c -->\n"
                             "<lib x=\"1\" y=\"2\">\n"
                             " <book id=\"a\">One &amp; &lt;two&gt;</book>\n"
                             " <book id=\"b\"/>\n"
                             "</lib>\n");
    CHECK(doc.toByteArray(2) == doc.toString(2).toUtf8());

    // What is written reads back as the same document.
    QDomDocument again = parse(doc.toByteArray(4));
    CHECK_EQ(again.toString(-1), doc.toString(-1));

    // A node that is not a document is written where it is.
    QString text;
    QTextStream stream(&text);
    doc.documentElement().firstChildElement().save(stream, 0);
    stream.flush();
    CHECK_EQ(text, "<book id=\"a\">One &amp; &lt;two&gt;</book>\n");
    text.clear();
    stream << doc.documentElement().lastChildElement();
    stream.flush();
    CHECK_EQ(text, " <book id=\"b\"/>\n");

    QDomDocument null;
    CHECK_EQ(null.toString(), "");
    CHECK(null.toByteArray().isEmpty());
}

static void testEscaping() {
    QDomDocument doc;
    QDomElement root = doc.createElement("r");
    doc.appendChild(root);
    root.setAttribute("q", "a\"b'<c>&\n\t\r");
    root.appendChild(doc.createTextNode("x<y>&\"'\r"));
    CHECK_EQ(doc.toString(-1), "<r q=\"a&quot;b&apos;&lt;c&gt;&amp;&#xa;&#x9;&#xd;\">x&lt;y&gt;&amp;\"'&#xd;</r>");
    QDomDocument again = parse(doc.toByteArray(-1));
    CHECK_EQ(again.documentElement().attribute("q"), "a\"b'<c>&\n\t\r");
    CHECK_EQ(again.documentElement().text(), "x<y>&\"'\r");

    // A CDATA section that has the end of one is written as two.
    QDomDocument cdata;
    QDomElement c = cdata.createElement("c");
    cdata.appendChild(c);
    c.appendChild(cdata.createCDATASection("a]]>b"));
    CHECK_EQ(cdata.toString(-1), "<c><![CDATA[a]]]]><![CDATA[>b]]></c>");
    CHECK_EQ(parse(cdata.toByteArray(-1)).documentElement().text(), "a]]>b");
}

static void testNamespaces() {
    const QByteArray xml = "<a xmlns=\"urn:d\" xmlns:p=\"urn:p\" p:attr=\"v\"><p:b/><c xmlns=\"\"/></a>";
    QDomDocument doc = parse(xml, Opt::UseNamespaceProcessing);
    QDomElement a = doc.documentElement();
    CHECK_EQ(a.namespaceURI(), "urn:d");
    CHECK_EQ(a.localName(), "a");
    CHECK(a.prefix().isEmpty());
    CHECK_EQ(a.attributeNS("urn:p", "attr"), "v");
    CHECK(a.hasAttributeNS("urn:p", "attr"));
    CHECK(!a.hasAttributeNS("urn:q", "attr"));
    CHECK_EQ(a.attribute("p:attr"), "v");
    CHECK_EQ(a.attribute("xmlns:p"), "urn:p");
    CHECK_EQ(a.attribute("xmlns"), "urn:d");
    CHECK_EQ(a.attributeNode("p:attr").namespaceURI(), "urn:p");
    CHECK_EQ(a.attributeNode("p:attr").prefix(), "p");
    CHECK_EQ(a.attributeNode("p:attr").localName(), "attr");

    QDomElement b = a.firstChildElement("p:b");
    CHECK(!b.isNull());
    CHECK_EQ(b.namespaceURI(), "urn:p");
    CHECK_EQ(b.prefix(), "p");
    CHECK_EQ(b.localName(), "b");
    CHECK_EQ(b.tagName(), "p:b");
    CHECK(a.firstChildElement("b", "urn:p").isNull()); // the tag name is the qualified name
    CHECK(a.firstChildElement(QString(), "urn:p") == b);
    CHECK(a.lastChildElement().namespaceURI().isEmpty());

    // The same document comes out.
    CHECK_EQ(doc.toString(-1), QString::fromUtf8(xml));

    // Without namespace processing a name is a name.
    QDomDocument plain = parse(xml);
    CHECK(plain.documentElement().namespaceURI().isNull());
    CHECK(plain.documentElement().localName().isNull());
    CHECK_EQ(plain.documentElement().attribute("xmlns:p"), "urn:p");
    CHECK_EQ(plain.toString(-1), QString::fromUtf8(xml));

    // The lists by namespace.
    CHECK(doc.elementsByTagNameNS("urn:p", "b").size() == 1);
    CHECK(doc.elementsByTagNameNS("*", "b").size() == 1);
    CHECK(doc.elementsByTagNameNS("urn:p", "*").size() == 1);
    CHECK(doc.elementsByTagNameNS("*", "*").size() == 3);
    CHECK(doc.elementsByTagNameNS("urn:q", "b").isEmpty());
    CHECK(plain.elementsByTagNameNS("*", "*").isEmpty());

    // Nodes that are made get their declarations when they are written.
    QDomDocument made;
    QDomElement root = made.createElementNS("urn:r", "r");
    made.appendChild(root);
    QDomElement child = made.createElementNS("urn:x", "x:e");
    root.appendChild(child);
    child.setAttributeNS("urn:y", "y:at", "1");
    QDomElement same = made.createElementNS("urn:x", "x:f");
    child.appendChild(same);
    QDomElement plainChild = made.createElementNS(QString(), "g");
    root.appendChild(plainChild);
    CHECK_EQ(made.toString(-1), "<r xmlns=\"urn:r\"><x:e xmlns:x=\"urn:x\" xmlns:y=\"urn:y\" y:at=\"1\"><x:f/></x:e>"
                                "<g xmlns=\"\"/></r>");
    CHECK_EQ(child.localName(), "e");
    CHECK_EQ(child.prefix(), "x");
    child.setPrefix("z");
    CHECK_EQ(child.tagName(), "z:e");
    CHECK_EQ(child.attributeNS("urn:y", "at"), "1");
    child.setAttributeNS("urn:y", "w:at", "2");
    CHECK_EQ(child.attributeNS("urn:y", "at"), "2");
    CHECK(child.attributes().size() == 1);
    child.removeAttributeNS("urn:y", "at");
    CHECK(!child.hasAttributeNS("urn:y", "at"));
    CHECK_EQ(child.attributeNS("urn:y", "at", "dflt"), "dflt");

    // A plain element is not touched.
    QDomDocument raw;
    QDomElement e = raw.createElement("p:e");
    raw.appendChild(e);
    CHECK_EQ(raw.toString(-1), "<p:e/>");
    CHECK(e.namespaceURI().isNull());
}

static void testAttributes() {
    QDomDocument doc;
    QDomElement e = doc.createElement("e");
    doc.appendChild(e);
    e.setAttribute("s", "text");
    e.setAttribute("i", 7);
    e.setAttribute("u", 8u);
    e.setAttribute("l", static_cast<qlonglong>(-9000000000LL));
    e.setAttribute("ul", static_cast<qulonglong>(18000000000000000000ULL));
    e.setAttribute("f", 0.1f);
    e.setAttribute("d", 0.1);
    e.setAttribute("d2", 1.0 / 3.0);
    CHECK_EQ(e.attribute("i"), "7");
    CHECK_EQ(e.attribute("u"), "8");
    CHECK_EQ(e.attribute("l"), "-9000000000");
    CHECK_EQ(e.attribute("ul"), "18000000000000000000");
    CHECK_EQ(e.attribute("f"), "0.1");
    CHECK_EQ(e.attribute("d"), "0.1");
    CHECK(e.attribute("d2").toDouble() == 1.0 / 3.0);
    CHECK(e.attributes().size() == 8);
    CHECK_EQ(e.attributes().item(0).nodeName(), "s");
    CHECK_EQ(e.attributes().item(7).nodeName(), "d2");

    // Setting again keeps the place.
    e.setAttribute("s", "other");
    CHECK(e.attributes().size() == 8);
    CHECK_EQ(e.attributes().item(0).toAttr().value(), "other");
    e.removeAttribute("i");
    CHECK(!e.hasAttribute("i"));
    CHECK(e.attributes().size() == 7);
    e.removeAttribute("i"); // nothing to do

    // Nodes.
    QDomAttr a = e.attributeNode("s");
    CHECK(!a.isNull());
    CHECK(a.nodeType() == QDomNode::AttributeNode);
    CHECK_EQ(a.name(), "s");
    CHECK_EQ(a.value(), "other");
    CHECK(a.specified());
    CHECK(a.ownerElement() == e);
    CHECK(a.parentNode().isNull());
    a.setValue("changed");
    CHECK_EQ(e.attribute("s"), "changed");
    CHECK(e.attributeNode("zzz").isNull());

    QDomAttr n = doc.createAttribute("n");
    n.setValue("v");
    CHECK(n.ownerElement().isNull());
    CHECK(e.setAttributeNode(n).isNull()); // nothing was replaced
    CHECK(n.ownerElement() == e);
    CHECK_EQ(e.attribute("n"), "v");
    CHECK(e.attributes().size() == 8);
    QDomElement other = doc.createElement("o");
    CHECK(other.setAttributeNode(n).isNull()); // it belongs to e
    CHECK(!other.hasAttribute("n"));

    QDomAttr replacement = doc.createAttribute("n");
    replacement.setValue("w");
    QDomAttr old = e.setAttributeNode(replacement);
    CHECK(old == n);
    CHECK(old.ownerElement().isNull());
    CHECK_EQ(old.value(), "v");
    CHECK_EQ(e.attribute("n"), "w");
    CHECK(e.attributes().size() == 8);

    CHECK(e.removeAttributeNode(old).isNull()); // not there any more
    CHECK(e.removeAttributeNode(replacement) == replacement);
    CHECK(!e.hasAttribute("n"));

    // Through the map.
    QDomNamedNodeMap map = e.attributes();
    QDomAttr viaMap = doc.createAttribute("m");
    viaMap.setValue("1");
    CHECK(map.setNamedItem(viaMap).isNull());
    CHECK_EQ(e.attribute("m"), "1");
    CHECK(map.removeNamedItem("m") == viaMap);
    CHECK(!e.hasAttribute("m"));
    CHECK(map.removeNamedItem("m").isNull());
    CHECK(map.setNamedItem(doc.createTextNode("t")).isNull());

    // A map and a list are views.
    QDomNamedNodeMap map2 = e.attributes();
    CHECK(map == map2);
    CHECK(!(map != map2));
    CHECK(map != QDomNamedNodeMap());
    e.setAttribute("late", "1");
    CHECK(map.contains("late"));

    QDomNamedNodeMap empty;
    CHECK(empty.isEmpty());
    CHECK(empty.item(0).isNull());
    CHECK(empty.namedItem("a").isNull());
    CHECK(QDomNode().attributes().isEmpty());
    CHECK(doc.attributes().isEmpty());
}

static void testTree() {
    QDomDocument doc;
    QDomElement root = doc.createElement("root");
    CHECK(root.parentNode().isNull());
    CHECK(root.ownerDocument() == doc);
    CHECK(doc.appendChild(root) == root);
    CHECK(root.parentNode() == doc);

    QDomElement a = doc.createElement("a");
    QDomElement b = doc.createElement("b");
    QDomElement c = doc.createElement("c");
    CHECK(root.appendChild(b) == b);
    CHECK(root.insertBefore(a, b) == a);
    CHECK(root.insertAfter(c, b) == c);
    CHECK_EQ(doc.toString(-1), "<root><a/><b/><c/></root>");
    CHECK(a.nextSibling() == b);
    CHECK(c.previousSibling() == b);
    CHECK(root.firstChild() == a);
    CHECK(root.lastChild() == c);

    // A node that has a parent is moved.
    CHECK(root.appendChild(a) == a);
    CHECK_EQ(doc.toString(-1), "<root><b/><c/><a/></root>");
    CHECK(root.prependChild(a) == a);
    CHECK(root.prependChild(a) == a); // already there
    CHECK_EQ(doc.toString(-1), "<root><a/><b/><c/></root>");
    CHECK(root.insertAfter(a, QDomNode()) == a); // the first
    CHECK(root.insertAfter(c, QDomNode()) == c);
    CHECK_EQ(doc.toString(-1), "<root><c/><a/><b/></root>");
    CHECK(root.insertBefore(b, QDomNode()) == b); // the last
    CHECK(root.insertBefore(c, a) == c);
    CHECK_EQ(doc.toString(-1), "<root><c/><a/><b/></root>");
    CHECK(root.insertAfter(b, a) == b); // already after it
    CHECK_EQ(doc.toString(-1), "<root><c/><a/><b/></root>");

    // The mistakes.
    CHECK(root.insertBefore(a, a).isNull());
    CHECK(root.insertBefore(doc.createElement("x"), doc.createElement("y")).isNull()); // y is not a child
    CHECK(root.appendChild(QDomNode()).isNull());
    CHECK(QDomNode().appendChild(a).isNull());
    CHECK(root.appendChild(root).isNull());
    CHECK(a.appendChild(root).isNull()); // an ancestor
    CHECK(root.appendChild(doc).isNull());
    CHECK(root.appendChild(doc.createAttribute("x")).isNull());
    CHECK(doc.appendChild(doc.createElement("second")).isNull()); // a document has one element
    CHECK(doc.appendChild(doc.createTextNode("text")).isNull());
    QDomText text = doc.createTextNode("t");
    CHECK(text.appendChild(doc.createElement("x")).isNull()); // text has no children
    CHECK(doc.appendChild(doc.createComment("fine")) != QDomNode());
    CHECK_EQ(doc.toString(-1), "<root><c/><a/><b/></root><!--fine-->");

    // Replacing and removing.
    QDomElement d = doc.createElement("d");
    CHECK(root.replaceChild(d, a) == a);
    CHECK(a.parentNode().isNull());
    CHECK_EQ(doc.toString(-1), "<root><c/><d/><b/></root><!--fine-->");
    CHECK(root.replaceChild(d, d) == d);
    CHECK(root.replaceChild(a, a).isNull()); // a is not a child
    CHECK(root.replaceChild(doc.createElement("e"), QDomNode()).isNull());
    CHECK(doc.replaceChild(doc.createElement("newroot"), root) == root); // still one element
    CHECK_EQ(doc.toString(-1), "<newroot/><!--fine-->");
    CHECK(root.parentNode().isNull());
    CHECK_EQ(root.firstChild().toElement().tagName(), "c"); // the tree that was removed lives on
    CHECK(doc.removeChild(doc.lastChild()).isComment());
    CHECK(doc.removeChild(doc.lastChild()).isElement());
    CHECK(doc.removeChild(doc.lastChild()).isNull());
    CHECK(doc.removeChild(root).isNull());

    // A fragment gives its children.
    QDomElement parent = doc.createElement("p");
    QDomDocumentFragment fragment = doc.createDocumentFragment();
    CHECK(fragment.nodeType() == QDomNode::DocumentFragmentNode);
    CHECK_EQ(fragment.nodeName(), "#document-fragment");
    fragment.appendChild(doc.createElement("f1"));
    fragment.appendChild(doc.createElement("f2"));
    parent.appendChild(doc.createElement("first"));
    CHECK(parent.appendChild(fragment) == fragment);
    CHECK(fragment.firstChild().isNull());
    parent.insertBefore(fragment, parent.firstChild()); // empty
    CHECK_EQ(parent.firstChild().nodeName(), "first");
    fragment.appendChild(doc.createElement("f3"));
    parent.insertBefore(fragment, parent.firstChild());
    CHECK_EQ(parent.childNodes().at(0).nodeName(), "f3");
    CHECK(parent.childNodes().size() == 4);
    QDomDocumentFragment second = doc.createDocumentFragment();
    second.appendChild(doc.createElement("s1"));
    CHECK(parent.replaceChild(second, parent.childNodes().at(1)).nodeName() == "first");
    CHECK_EQ(parent.childNodes().at(1).nodeName(), "s1");
    // Two elements in a fragment do not fit into a document that has one.
    QDomDocument one;
    one.appendChild(one.createElement("only"));
    QDomDocumentFragment two = one.createDocumentFragment();
    two.appendChild(one.createElement("x"));
    CHECK(one.appendChild(two).isNull());
    CHECK(one.childNodes().size() == 1);
    CHECK(two.hasChildNodes());

    // Entity references and processing instructions are fine under an element.
    parent.appendChild(doc.createEntityReference("ref"));
    parent.appendChild(doc.createProcessingInstruction("target", "data"));
    CHECK(parent.lastChild().isProcessingInstruction());
    CHECK(parent.childNodes().at(parent.childNodes().size() - 2).isEntityReference());
}

static void testNodeLists() {
    QDomDocument doc = parse("<r><a id=\"1\"><a id=\"2\"/></a><b/><a id=\"3\"/></r>");
    QDomNodeList all = doc.elementsByTagName("a");
    CHECK(all.size() == 3);
    CHECK_EQ(all.at(0).toElement().attribute("id"), "1");
    CHECK_EQ(all.at(1).toElement().attribute("id"), "2");
    CHECK_EQ(all.at(2).toElement().attribute("id"), "3");
    CHECK(all.at(3).isNull());
    CHECK(all.at(-1).isNull());
    CHECK(doc.elementsByTagName("*").size() == 5);
    CHECK(doc.elementsByTagName("zz").isEmpty());

    QDomElement root = doc.documentElement();
    QDomNodeList children = root.childNodes();
    QDomNodeList inRoot = root.elementsByTagName("a");
    CHECK(children.size() == 3);
    CHECK(inRoot.size() == 3);
    CHECK(root.firstChildElement("a").elementsByTagName("a").size() == 1); // not itself
    CHECK(children == QDomNodeList(children));
    CHECK(children != inRoot);
    QDomNodeList copy = all;
    CHECK(copy == all);

    // The lists follow the tree.
    QDomElement added = doc.createElement("a");
    root.appendChild(added);
    CHECK(all.size() == 4);
    CHECK(children.size() == 4);
    root.removeChild(root.firstChildElement("a"));
    CHECK(all.size() == 2);
    CHECK(children.size() == 3);
    added.setTagName("other");
    CHECK(all.size() == 1);
    CHECK(doc.elementsByTagName("other").size() == 1);
    CHECK(QDomNodeList().isEmpty());
    CHECK(QDomNodeList().item(0).isNull());

    // A list keeps its root alive.
    QDomNodeList detached;
    {
        QDomDocument temporary = parse("<t><u/><u/></t>");
        detached = temporary.documentElement().elementsByTagName("u");
    }
    CHECK(detached.size() == 2);
    CHECK_EQ(detached.at(0).nodeName(), "u");

    // Taking nodes out of a list while it is used.
    QDomDocument many;
    QDomElement top = many.createElement("top");
    many.appendChild(top);
    for (int i = 0; i < 50; ++i)
        top.appendChild(many.createElement("x"));
    QDomNodeList xs = many.elementsByTagName("x");
    while (xs.size() > 10)
        top.removeChild(xs.at(0));
    CHECK(xs.size() == 10);
}

static void testClone() {
    QDomDocument doc = parse("<r k=\"v\"><c>text<d/></c></r>");
    QDomElement r = doc.documentElement();
    QDomNode deep = r.cloneNode();
    QDomNode shallow = r.cloneNode(false);
    CHECK(deep != r);
    CHECK(deep.parentNode().isNull());
    CHECK_EQ(deep.toElement().attribute("k"), "v");
    CHECK_EQ(deep.toElement().text(), "text");
    CHECK(deep.toElement().firstChildElement("c") != r.firstChildElement("c"));
    CHECK(shallow.toElement().hasAttribute("k"));
    CHECK(!shallow.hasChildNodes());
    CHECK(deep.ownerDocument() == doc);
    // A copy is its own.
    deep.toElement().setAttribute("k", "changed");
    CHECK_EQ(r.attribute("k"), "v");
    CHECK_EQ(r.attributeNode("k").ownerElement().tagName(), "r");

    // A copy of a document.
    QDomNode docCopy = doc.cloneNode();
    CHECK(docCopy.isDocument());
    CHECK(docCopy != doc);
    CHECK_EQ(docCopy.toDocument().toString(-1), doc.toString(-1));
    CHECK(docCopy.toDocument().documentElement().ownerDocument() == docCopy.toDocument());
    CHECK(!doc.cloneNode(false).hasChildNodes());

    // Importing.
    QDomDocument other;
    QDomNode imported = other.importNode(r, true);
    CHECK(imported.ownerDocument() == other);
    CHECK(imported.parentNode().isNull());
    CHECK(other.appendChild(imported) == imported);
    CHECK_EQ(other.toString(-1), "<r k=\"v\"><c>text<d/></c></r>");
    CHECK(other.importNode(doc, true).isNull());
    CHECK(other.importNode(QDomNode(), true).isNull());
    CHECK(!other.importNode(r, false).hasChildNodes());
    CHECK(other.importNode(r.attributeNode("k"), true).isAttr());

    // A node that is inserted into another document belongs to it.
    QDomDocument third;
    third.createComment("makes the document");
    QDomElement moved = doc.createElement("moved");
    CHECK(moved.ownerDocument() == doc);
    third.appendChild(moved);
    CHECK(moved.ownerDocument() == third);
    QDomElement inner = doc.createElement("inner");
    inner.setAttribute("a", "1");
    moved.appendChild(inner);
    CHECK(inner.ownerDocument() == third);
    CHECK(inner.attributeNode("a").ownerDocument() == third);
}

static void testText() {
    QDomDocument doc;
    QDomElement e = doc.createElement("e");
    doc.appendChild(e);
    QDomText t = doc.createTextNode("Hello, world");
    e.appendChild(t);
    CHECK(t.nodeType() == QDomNode::TextNode);
    CHECK(t.length() == 12);
    CHECK_EQ(t.data(), "Hello, world");
    CHECK_EQ(t.substringData(7, 5), "world");
    CHECK_EQ(t.substringData(7, 100), "world");
    CHECK(t.substringData(13, 1).isNull());
    t.appendData("!");
    CHECK_EQ(t.data(), "Hello, world!");
    t.insertData(5, " there");
    CHECK_EQ(t.data(), "Hello there, world!");
    t.insertData(100, "no");
    CHECK_EQ(t.data(), "Hello there, world!");
    t.deleteData(5, 6);
    CHECK_EQ(t.data(), "Hello, world!");
    t.replaceData(7, 5, "there");
    CHECK_EQ(t.data(), "Hello, there!");
    t.deleteData(5, 1000);
    CHECK_EQ(t.data(), "Hello");
    t.setData("a b");
    CHECK_EQ(t.nodeValue(), "a b");
    t.setNodeValue("a b c");
    CHECK_EQ(t.data(), "a b c");

    QDomText tail = t.splitText(3);
    CHECK_EQ(t.data(), "a b");
    CHECK_EQ(tail.data(), " c");
    CHECK(t.nextSibling() == tail);
    CHECK(e.childNodes().size() == 2);
    CHECK(t.splitText(100).isNull());
    CHECK(t.splitText(-1).isNull());
    CHECK_EQ(e.text(), "a b c");

    // Normalizing joins the pieces and removes empty text.
    e.appendChild(doc.createTextNode(""));
    e.appendChild(doc.createElement("x"));
    e.appendChild(doc.createTextNode("1"));
    e.appendChild(doc.createTextNode("2"));
    e.appendChild(doc.createCDATASection("3"));
    e.appendChild(doc.createTextNode("4"));
    e.normalize();
    CHECK(e.childNodes().size() == 5);
    CHECK_EQ(e.firstChild().nodeValue(), "a b c");
    CHECK_EQ(e.childNodes().at(2).nodeValue(), "12");
    CHECK(e.childNodes().at(3).isCDATASection());
    CHECK_EQ(e.text(), "a b c1234");

    // Comments, and text that is deeper.
    QDomComment comment = doc.createComment("note");
    CHECK(comment.nodeType() == QDomNode::CommentNode);
    CHECK(comment.isCharacterData());
    CHECK(!comment.isText());
    CHECK_EQ(comment.nodeName(), "#comment");
    CHECK_EQ(comment.toCharacterData().data(), "note");
    e.appendChild(comment);
    e.firstChildElement("x").appendChild(doc.createTextNode("deep"));
    CHECK_EQ(e.text(), "a b c" + QString("deep") + "1234");
    CHECK(QDomCharacterData().nodeType() == QDomNode::CharacterDataNode);
    CHECK_EQ(QDomCharacterData().data(), "");
    CHECK(QDomCharacterData().length() == 0);

    // The kinds.
    QDomCDATASection cdata = doc.createCDATASection("x");
    CHECK(cdata.nodeType() == QDomNode::CDATASectionNode);
    CHECK_EQ(cdata.nodeName(), "#cdata-section");
    CHECK(cdata.isText());
    CHECK(cdata.toText().nodeType() == QDomNode::TextNode);
    CHECK(t.toText() == t);
    CHECK(t.toCDATASection().isNull());
    CHECK(t.toComment().isNull());
    CHECK(t.toElement().isNull());
    QDomText nullText;
    CHECK(nullText.isNull());
    CHECK(nullText.splitText(0).isNull());
}

static void testDocumentType() {
    const QByteArray xml = "<?xml version=\"1.0\"?>\n"
                           "<!DOCTYPE note PUBLIC \"-//X[//\" \"note.dtd\" [\n"
                           "<!ENTITY who \"Nt\">\n"
                           "<!ENTITY logo SYSTEM \"logo.gif\" NDATA gif>\n"
                           "<!NOTATION gif SYSTEM \"image/gif\">\n"
                           "]>\n"
                           "<note>Hello &who;</note>\n";
    QDomDocument doc = parse(xml);
    QDomDocumentType type = doc.doctype();
    CHECK(!type.isNull());
    CHECK(type.nodeType() == QDomNode::DocumentTypeNode);
    CHECK(type.isDocumentType());
    CHECK_EQ(type.name(), "note");
    CHECK_EQ(type.publicId(), "-//X[//");
    CHECK_EQ(type.systemId(), "note.dtd");
    CHECK_EQ(type.internalSubset().trimmed().left(14), "<!ENTITY who \"");
    CHECK(type.internalSubset().contains("<!NOTATION gif"));
    CHECK(type.parentNode() == doc);
    CHECK(doc.documentElement().previousSibling() == type);
    CHECK_EQ(doc.documentElement().text(), "Hello Nt");

    // Only the entities that are not parsed are kept as nodes; the text of the others is in the content.
    QDomNamedNodeMap entities = type.entities();
    CHECK(entities.size() == 1);
    QDomEntity logo = entities.namedItem("logo").toEntity();
    CHECK(!logo.isNull());
    CHECK(logo.nodeType() == QDomNode::EntityNode);
    CHECK_EQ(logo.systemId(), "logo.gif");
    CHECK_EQ(logo.notationName(), "gif");
    CHECK(logo.publicId().isEmpty());
    CHECK(entities.setNamedItem(logo).isNull());
    CHECK(entities.removeNamedItem("logo").isNull());
    CHECK(entities.size() == 1);
    QDomNamedNodeMap notations = type.notations();
    CHECK(notations.size() == 1);
    QDomNotation gif = notations.namedItem("gif").toNotation();
    CHECK(gif.nodeType() == QDomNode::NotationNode);
    CHECK_EQ(gif.systemId(), "image/gif");
    CHECK(gif.publicId().isEmpty());

    // It is written again, with the internal subset as it was.
    const QString out = doc.toString(-1);
    CHECK(out.startsWith("<?xml version=\"1.0\"?><!DOCTYPE note PUBLIC \"-//X[//\" \"note.dtd\" [\n<!ENTITY who"));
    CHECK(out.endsWith("]><note>Hello Nt</note>"));

    // A document without one.
    QDomDocument none = parse("<a/>");
    CHECK(none.doctype().isNull());
    CHECK(none.doctype().name().isEmpty());
    CHECK(none.doctype().entities().isEmpty());

    // Documents that are made.
    QDomImplementation implementation;
    CHECK(implementation.isNull());
    QDomDocumentType made = implementation.createDocumentType("svg", "-//W3C//DTD SVG 1.1//EN", "svg11.dtd");
    CHECK(made.ownerDocument().isNull());
    QDomDocument svg = implementation.createDocument("http://www.w3.org/2000/svg", "svg", made);
    CHECK(svg.implementation() != implementation);
    CHECK(!svg.implementation().isNull());
    CHECK(svg.implementation() == svg.implementation());
    CHECK(made.ownerDocument() == svg);
    CHECK_EQ(svg.toString(-1), "<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.1//EN\" \"svg11.dtd\"><svg "
                               "xmlns=\"http://www.w3.org/2000/svg\"/>");
    CHECK(svg.doctype() == made);
    CHECK_EQ(svg.documentElement().namespaceURI(), "http://www.w3.org/2000/svg");
    CHECK(implementation.hasFeature("XML", "1.0"));
    CHECK(implementation.hasFeature("XML", QString()));
    CHECK(!implementation.hasFeature("XML", "9.0"));
    CHECK(!implementation.hasFeature("HTML", "1.0"));
    CHECK(svg.isSupported("XML", "1.0"));

    QDomDocument named("mydocument");
    named.appendChild(named.createElement("root"));
    CHECK_EQ(named.toString(-1), "<!DOCTYPE mydocument><root/>");
    CHECK_EQ(named.doctype().name(), "mydocument");
    QDomDocument withSystem(implementation.createDocumentType("x", QString(), "x.dtd"));
    withSystem.appendChild(withSystem.createElement("x"));
    CHECK_EQ(withSystem.toString(-1), "<!DOCTYPE x SYSTEM \"x.dtd\"><x/>");
}

static void testErrors() {
    QDomDocument doc;
    auto result = doc.setContent(QByteArray("<a>\n <b></c>\n</a>"));
    CHECK(!result);
    CHECK(!result.errorMessage.isEmpty());
    CHECK(result.errorLine == 2);
    CHECK(result.errorColumn > 0);
    CHECK(doc.documentElement().isNull());
    CHECK(!doc.hasChildNodes());

    // A failed read empties a document that had something.
    doc.setContent(QByteArray("<ok/>"));
    CHECK(!doc.documentElement().isNull());
    CHECK(!doc.setContent(QByteArray("<a>")));
    CHECK(doc.documentElement().isNull());
    CHECK(!doc.setContent(QByteArray()));
    CHECK(!doc.setContent(QByteArray("text")));
    CHECK(!doc.setContent(QByteArray("<a/><b/>")));
    CHECK(!doc.setContent(QByteArray("<a x=\"1\" x=\"2\"/>")));
    CHECK(!doc.setContent(QByteArray("<a>&undefined;</a>")));
    CHECK(doc.setContent(QByteArray("<a/>")));

    // A text instead of bytes.
    CHECK(doc.setContent(QString("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><a>\xC3\xA9</a>")));
    result = doc.setContent(QStringView(u"<a>\u00e9</a>"));
    CHECK(result);
    CHECK_EQ(doc.documentElement().text(), QString::fromUtf8("\xC3\xA9"));
    CHECK(!doc.setContent(QStringView(u"<a>")));
    CHECK(doc.setContent("<literal/>"));
    CHECK_EQ(doc.documentElement().tagName(), "literal");

    // Nesting.
    QByteArray deep;
    for (int i = 0; i < 1024; ++i)
        deep += "<a>";
    for (int i = 0; i < 1024; ++i)
        deep += "</a>";
    CHECK(doc.setContent(deep));
    deep = "<a>" + deep + "</a>";
    result = doc.setContent(deep);
    CHECK(!result);
    CHECK(!result.errorMessage.isEmpty());
    CHECK(!doc.hasChildNodes());

    // The device.
    QBuffer buffer;
    buffer.setData("<from><device/></from>");
    CHECK(doc.setContent(&buffer));
    CHECK_EQ(doc.documentElement().tagName(), "from");
    QBuffer closed;
    CHECK(!doc.setContent(&closed)); // it cannot be read: opened for reading, nothing there
    CHECK(!doc.setContent(static_cast<QIODevice *>(nullptr)));
    CHECK(!doc.setContent(static_cast<QXmlStreamReader *>(nullptr)));

    // The reader.
    QXmlStreamReader reader(QByteArray("<r><s/></r>"));
    CHECK(doc.setContent(&reader, Opt::UseNamespaceProcessing));
    CHECK_EQ(doc.documentElement().localName(), "r");
    QXmlStreamReader advanced(QByteArray("<r><s/></r>"));
    advanced.readNext();
    advanced.readNext();
    CHECK(advanced.isStartElement());
    CHECK(doc.setContent(&advanced));
    CHECK_EQ(doc.documentElement().tagName(), "r");

    // The functions that older code uses.
    QString message;
    int line = 0;
    int column = 0;
    CHECK(doc.setContent(QString("<a><b/></a>"), &message, &line, &column));
    CHECK(!doc.setContent(QString("<a>\n<b></a>"), &message, &line, &column));
    CHECK(!message.isEmpty());
    CHECK(line == 2);
    CHECK(column > 0);
    CHECK(!doc.setContent(QByteArray("<a>"), &message));
    CHECK(doc.setContent(QByteArray("<p:a xmlns:p=\"urn:p\"/>"), true, &message, &line, &column));
    CHECK_EQ(doc.documentElement().localName(), "a");
    CHECK(doc.setContent(QString("<p:a xmlns:p=\"urn:p\"/>"), false));
    CHECK(doc.documentElement().localName().isNull());
    QBuffer old;
    old.setData("<old/>");
    CHECK(doc.setContent(&old, &message));
    old.close();
    CHECK(doc.setContent(&old, true, &message));
    QXmlStreamReader oldReader(QByteArray("<old/>"));
    CHECK(doc.setContent(&oldReader, false));
}

static void testDestroyedDocument() {
    QDomElement root;
    QDomElement child;
    QDomText text;
    QDomAttr attribute;
    QDomNodeList list;
    {
        QDomDocument doc = parse("<r a=\"1\"><c>t</c></r>");
        root = doc.documentElement();
        child = root.firstChildElement();
        text = child.firstChild().toText();
        attribute = root.attributeNode("a");
        list = root.elementsByTagName("c");
    }
    CHECK_EQ(root.tagName(), "r");
    CHECK(root.parentNode().isNull());
    CHECK(root.ownerDocument().isNull());
    CHECK(child.ownerDocument().isNull());
    CHECK(child.parentNode() == root);
    CHECK_EQ(text.data(), "t");
    CHECK_EQ(attribute.value(), "1");
    CHECK(attribute.ownerElement() == root);
    CHECK(list.size() == 1);
    CHECK_EQ(root.text(), "t");

    // It can go on to another document.
    QDomDocument other;
    other.createComment("makes the document");
    other.appendChild(root);
    CHECK(root.ownerDocument() == other);
    CHECK(child.ownerDocument() == other);
    CHECK_EQ(other.toString(-1), "<r a=\"1\"><c>t</c></r>");

    // A document that is kept alive by its elements alone does not exist.
    QDomElement orphan;
    {
        QDomDocument doc;
        orphan = doc.createElement("o");
        doc.appendChild(orphan);
    }
    CHECK(orphan.parentNode().isNull());
    CHECK(orphan.ownerDocument().isNull());

    // Assignments and moves.
    QDomNode a = root;
    QDomNode b;
    b = a;
    CHECK(a == b);
    QDomNode moved = std::move(b);
    CHECK(b.isNull());
    CHECK(moved == root);
    a = a;
    CHECK(a == root);
    a.clear();
    CHECK(a.isNull());
    CHECK(!moved.isNull());
    QDomNode swapped;
    swapped = std::move(moved);
    CHECK(swapped == root);
}

static void testNullHandles() {
    QDomNode n;
    CHECK(n.isNull());
    CHECK(n.nodeType() == QDomNode::BaseNode);
    CHECK(n.nodeName().isEmpty());
    CHECK(n.nodeValue().isNull());
    CHECK(n.namespaceURI().isNull());
    CHECK(n.localName().isNull());
    CHECK(n.prefix().isNull());
    CHECK(n.parentNode().isNull());
    CHECK(n.firstChild().isNull());
    CHECK(n.lastChild().isNull());
    CHECK(n.previousSibling().isNull());
    CHECK(n.nextSibling().isNull());
    CHECK(n.childNodes().isEmpty());
    CHECK(n.ownerDocument().isNull());
    CHECK(!n.hasChildNodes());
    CHECK(!n.hasAttributes());
    CHECK(!n.isElement());
    CHECK(!n.isText());
    CHECK(!n.isDocument());
    CHECK(n.namedItem("a").isNull());
    CHECK(n.cloneNode().isNull());
    CHECK(n.firstChildElement().isNull());
    CHECK(n.lastChildElement().isNull());
    CHECK(n.nextSiblingElement().isNull());
    CHECK(n.previousSiblingElement().isNull());
    CHECK(n.lineNumber() == -1);
    CHECK(n.columnNumber() == -1);
    CHECK(n.toElement().isNull());
    CHECK(n.toDocument().isNull());
    CHECK(n == QDomNode());
    n.setNodeValue("x");
    n.setPrefix("x");
    n.normalize();

    QDomElement e;
    CHECK(e.tagName().isNull());
    CHECK(e.attribute("a", "d") == "d");
    CHECK(e.attributeNS("u", "a", "d") == "d");
    CHECK(!e.hasAttribute("a"));
    CHECK(!e.hasAttributeNS("u", "a"));
    CHECK(e.text().isNull());
    CHECK(e.elementsByTagName("a").isEmpty());
    CHECK(e.elementsByTagNameNS("*", "*").isEmpty());
    CHECK(e.attributes().isEmpty());
    CHECK(e.attributeNode("a").isNull());
    CHECK(e.attributeNodeNS("u", "a").isNull());
    CHECK(e.nodeType() == QDomNode::ElementNode);
    e.setAttribute("a", "b");
    e.setAttributeNS("u", "p:a", "b");
    e.removeAttribute("a");
    e.removeAttributeNS("u", "a");
    e.setTagName("x");
    CHECK(e.isNull());
    CHECK(e.setAttributeNode(QDomAttr()).isNull());
    CHECK(e.removeAttributeNode(QDomAttr()).isNull());

    QDomAttr a;
    CHECK(a.name().isNull());
    CHECK(a.value().isNull());
    CHECK(!a.specified());
    CHECK(a.ownerElement().isNull());
    a.setValue("x");

    QDomProcessingInstruction pi;
    CHECK(pi.target().isNull());
    CHECK(pi.data().isNull());
    pi.setData("x");

    QDomDocument d;
    CHECK(d.isNull());
    CHECK(d.documentElement().isNull());
    CHECK(d.doctype().isNull());
    CHECK(d.implementation().isNull());
    CHECK(d.elementsByTagName("a").isEmpty());
    CHECK(d.elementById("a").isNull());
    CHECK(d.nodeType() == QDomNode::DocumentNode);
    CHECK(d.importNode(QDomNode(), true).isNull());
    CHECK(d.isNull()); // the functions that only ask do not make one
    d.createElement("x");
    CHECK(!d.isNull());

    CHECK(QDomDocumentType().name().isNull());
    CHECK(QDomDocumentType().entities().isEmpty());
    CHECK(QDomDocumentType().notations().isEmpty());
    CHECK(QDomDocumentType().publicId().isNull());
    CHECK(QDomDocumentType().systemId().isNull());
    CHECK(QDomDocumentType().internalSubset().isNull());
    CHECK(QDomEntity().publicId().isNull());
    CHECK(QDomEntity().notationName().isNull());
    CHECK(QDomNotation().systemId().isNull());
    CHECK(QDomNotation().nodeType() == QDomNode::NotationNode);
    CHECK(QDomEntityReference().nodeType() == QDomNode::EntityReferenceNode);
    CHECK(QDomDocumentFragment().nodeType() == QDomNode::DocumentFragmentNode);
    CHECK(QDomComment().nodeType() == QDomNode::CommentNode);
    CHECK(QDomCDATASection().nodeType() == QDomNode::CDATASectionNode);
    CHECK(QDomProcessingInstruction().nodeType() == QDomNode::ProcessingInstructionNode);
    CHECK(QDomAttr().nodeType() == QDomNode::AttributeNode);

    QString text;
    QTextStream stream(&text);
    n.save(stream, 1);
    stream << n;
    stream.flush();
    CHECK(text.isEmpty());
}

static void testInvalidData() {
    CHECK(QDomImplementation::invalidDataPolicy() == QDomImplementation::AcceptInvalidData);
    QDomDocument doc;
    // Accepted as they are.
    CHECK(!doc.createElement("1bad name").isNull());
    CHECK(!doc.createComment("a--b").isNull());
    CHECK(!doc.createTextNode(QString(u"a\u0001b")).isNull());
    CHECK(!doc.createCDATASection("a]]>b").isNull());

    QDomImplementation::setInvalidDataPolicy(QDomImplementation::ReturnNullNode);
    CHECK(QDomImplementation::invalidDataPolicy() == QDomImplementation::ReturnNullNode);
    CHECK(doc.createElement("1bad").isNull());
    CHECK(doc.createElement("a b").isNull());
    CHECK(doc.createElement("").isNull());
    CHECK(doc.createElement("a:b:c").isNull() == false); // a name can have colons for the old functions
    CHECK(doc.createElementNS("urn:x", "a:b:c").isNull());
    CHECK(doc.createElementNS("urn:x", ":b").isNull());
    CHECK(doc.createElementNS("urn:x", "a:").isNull());
    CHECK(doc.createElementNS("urn:x", "a:b").isNull() == false);
    CHECK(QDomImplementation().createDocument(QString(), "1bad", QDomDocumentType()).isNull());
    CHECK(doc.createAttribute("2").isNull());
    CHECK(doc.createAttributeNS("u", "a:b:c").isNull());
    CHECK(doc.createEntityReference("a b").isNull());
    CHECK(doc.createComment("a--b").isNull());
    CHECK(doc.createComment("a-").isNull());
    CHECK(!doc.createComment("a-b").isNull());
    CHECK(doc.createTextNode(QString(u"a\u0001b")).isNull());
    CHECK(!doc.createTextNode(QString(u"a\tb\n\r\u00e9\U0001F600")).isNull());
    CHECK(doc.createTextNode(QString(u"a\xD800")).isNull()); // a lone surrogate
    CHECK(doc.createCDATASection("a]]>b").isNull());
    CHECK(doc.createProcessingInstruction("t", "a?>b").isNull());
    CHECK(doc.createProcessingInstruction("1", "x").isNull());
    CHECK(!doc.createProcessingInstruction("xml", "version=\"1.0\"").isNull());
    QDomElement e = doc.createElement("e");
    e.setAttribute("1bad", "v");
    e.setAttribute("ok", QString(u"\u0001"));
    CHECK(e.attributes().isEmpty());
    e.setAttribute("ok", "v");
    e.setAttribute("ok", QString(u"\u0001")); // it stays
    CHECK_EQ(e.attribute("ok"), "v");
    CHECK(QDomImplementation().createDocumentType("1", QString(), QString()).isNull());
    QDomText t = doc.createTextNode("fine");
    t.setData(QString(u"\u0002"));
    CHECK_EQ(t.data(), "fine");
    t.appendData(QString(u"\u0002"));
    CHECK_EQ(t.data(), "fine");

    QDomImplementation::setInvalidDataPolicy(QDomImplementation::DropInvalidData);
    CHECK_EQ(doc.createElement("1ba d!").tagName(), "bad");
    CHECK(doc.createElement("123").isNull());
    CHECK_EQ(doc.createElementNS("u", "a:b").tagName(), "a:b");
    CHECK_EQ(doc.createComment("a--b-").toComment().data(), "a- -b");
    CHECK_EQ(doc.createTextNode(QString(u"a\u0001b\U0001F600")).data(), QString(u"ab\U0001F600"));
    CHECK_EQ(doc.createCDATASection("a]]>b").data(), "ab");
    CHECK_EQ(doc.createProcessingInstruction("t", "a?>b").data(), "ab");
    CHECK_EQ(doc.createTextNode(QString(u"x\xD800y")).data(), "xy");
    QDomElement dropped = doc.createElement("d");
    dropped.setAttribute("a", QString(u"1\u00012"));
    CHECK_EQ(dropped.attribute("a"), "12");
    QDomImplementation::setInvalidDataPolicy(QDomImplementation::AcceptInvalidData);
}

static void testEncodings() {
    // The encoding of the declaration is used for the bytes.
    QDomDocument doc = parse("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><a>\xE9</a>");
    CHECK_EQ(doc.documentElement().text(), QString::fromUtf8("\xC3\xA9"));
    const QByteArray latin1 = doc.toByteArray(-1);
    CHECK(latin1 == QByteArray("<?xml version=\"1.0\" encoding=\"ISO-8859-1\"?><a>\xE9</a>"));

    QDomDocument utf8 = parse("<?xml version=\"1.0\"?><a>\xC3\xA9</a>");
    CHECK(utf8.toByteArray(-1) == QByteArray("<?xml version=\"1.0\"?><a>\xC3\xA9</a>"));
    QDomDocument noDeclaration;
    noDeclaration.appendChild(noDeclaration.createElement("e"))
        .appendChild(noDeclaration.createTextNode(QString(u"\u00e9")));
    CHECK(noDeclaration.toByteArray(-1) == QByteArray("<e>\xC3\xA9</e>"));

    // The stream's own encoding.
    QByteArray bytes;
    QTextStream stream(&bytes);
    stream.setEncoding(QStringConverter::Latin1);
    utf8.save(stream, -1, QDomNode::EncodingFromTextStream);
    stream.flush();
    CHECK(bytes == QByteArray("<?xml version=\"1.0\"?><a>\xE9</a>"));

    // The declaration is quoted either way.
    QDomDocument quoted = parse("<?xml version='1.0' encoding='ISO-8859-1' standalone='yes'?><a>\xE9</a>");
    CHECK_EQ(quoted.firstChild().toProcessingInstruction().data(),
             "version=\"1.0\" encoding=\"ISO-8859-1\" standalone=\"yes\"");
    CHECK(quoted.toByteArray(-1).contains("\xE9"));
    QDomDocument odd;
    odd.appendChild(odd.createProcessingInstruction("xml", "version=\"1.0\" encoding = 'nonesuch'"));
    odd.appendChild(odd.createElement("e"));
    CHECK(odd.toByteArray(-1) == QByteArray("<?xml version=\"1.0\" encoding = 'nonesuch'?><e/>"));
}

static void testMisc() {
    QDomDocument doc;
    QDomProcessingInstruction pi = doc.createProcessingInstruction("target", "some data");
    CHECK_EQ(pi.nodeName(), "target");
    CHECK_EQ(pi.nodeValue(), "some data");
    pi.setData("other");
    CHECK_EQ(pi.data(), "other");
    QDomProcessingInstruction empty = doc.createProcessingInstruction("t", QString());
    doc.appendChild(doc.createElement("r")).appendChild(empty);
    CHECK_EQ(doc.toString(-1), "<r><?t?></r>");

    QDomEntityReference ref = doc.createEntityReference("nbsp");
    CHECK_EQ(ref.nodeName(), "nbsp");
    QDomDocument withReference = parse("<!DOCTYPE a [<!ENTITY ext SYSTEM \"x.xml\">]><a>&ext;</a>");
    CHECK(withReference.documentElement().firstChild().isEntityReference());
    CHECK_EQ(withReference.documentElement().firstChild().nodeName(), "ext");
    CHECK_EQ(withReference.documentElement().firstChild().toEntityReference().nodeName(), "ext");
    CHECK(withReference.documentElement().firstChild().toEntityReference().nodeType() == QDomNode::EntityReferenceNode);
    CHECK(withReference.toString(-1).contains("<a>&ext;</a>"));

    QDomElement e = doc.createElement("e");
    CHECK(!e.isSupported("XML", "2.0"));
    CHECK(e.isSupported("XML", "1.0"));
    CHECK(doc.elementsByTagName("e").isEmpty());

    // The names of the nodes.
    CHECK(doc.createTextNode("x").nodeType() == QDomNode::TextNode);
    CHECK(doc.createComment("x").isComment());
    CHECK(doc.createAttribute("x").isAttr());
    CHECK(doc.createCDATASection("x").isCDATASection());
    CHECK(doc.createEntityReference("x").isEntityReference());
    CHECK(doc.createDocumentFragment().isDocumentFragment());
    CHECK(doc.createElement("x").toElement().nodeType() == QDomNode::ElementNode);
    CHECK(QDomNode(doc.createElement("x")).namedItem("y").isNull());
    QDomElement p = doc.createElement("p");
    p.appendChild(doc.createElement("named"));
    CHECK(p.namedItem("named").isElement());
    CHECK(p.namedItem("other").isNull());

    // Debug output.
    QString text;
    {
        QDebug debug(&text);
        debug << p << QDomNode();
    }
    // Spaced as Qt spaces items: the state saver gives back the space that nospace() held back.
    CHECK_EQ(text, "QDomNode(<p><named/></p>) QDomNode(null) ");
}

// The links of every node agree with those of its neighbours, and no node is its own ancestor.
static bool consistent(const std::vector<QDomElement> &nodes) {
    for (const QDomElement &n : nodes) {
        int depth = 0;
        for (QDomNode p = n.parentNode(); !p.isNull(); p = p.parentNode()) {
            if (p == n || ++depth > 1000)
                return false;
        }
        if (!n.previousSibling().isNull() && n.previousSibling().nextSibling() != n)
            return false;
        if (!n.nextSibling().isNull() && n.nextSibling().previousSibling() != n)
            return false;
        if (n.hasChildNodes()) {
            if (!n.firstChild().previousSibling().isNull() || !n.lastChild().nextSibling().isNull())
                return false;
            int count = 0;
            for (QDomNode c = n.firstChild(); !c.isNull(); c = c.nextSibling()) {
                if (c.parentNode() != n)
                    return false;
                ++count;
            }
            if (count != n.childNodes().size())
                return false;
        } else if (!n.firstChild().isNull() || !n.lastChild().isNull()) {
            return false;
        }
        if (n.parentNode().isNull() && (!n.previousSibling().isNull() || !n.nextSibling().isNull()))
            return false;
    }
    return true;
}

static void testRandomTree() {
    QDomDocument doc;
    doc.createComment("makes the document");
    std::vector<QDomElement> nodes;
    for (int i = 0; i < 40; ++i)
        nodes.push_back(doc.createElement("n" + QString::number(i)));
    quint32 state = 12345;
    const auto random = [&state](int n) {
        state = state * 1664525u + 1013904223u;
        return static_cast<int>((state >> 8) % static_cast<quint32>(n));
    };
    bool ok = true;
    for (int step = 0; step < 20000 && ok; ++step) {
        QDomElement a = nodes[static_cast<std::size_t>(random(40))];
        QDomElement b = nodes[static_cast<std::size_t>(random(40))];
        switch (random(7)) {
        case 0:
            a.appendChild(b);
            break;
        case 1:
            a.prependChild(b);
            break;
        case 2:
            a.insertBefore(b, random(2) ? a.firstChild() : a.lastChild());
            break;
        case 3:
            a.insertAfter(b, random(2) ? a.firstChild() : QDomNode());
            break;
        case 4:
            a.removeChild(b);
            break;
        case 5:
            a.replaceChild(b, a.firstChild());
            break;
        case 6:
            a.removeChild(a.lastChild());
            break;
        }
        if (step % 50 == 0)
            ok = consistent(nodes);
    }
    CHECK(ok);
    CHECK(consistent(nodes));
    int attached = 0;
    for (const QDomElement &n : nodes)
        attached += n.parentNode().isNull() ? 0 : 1;
    CHECK(attached > 5);
    // Whatever is left, it is deleted without trouble.
    nodes.clear();
}

static void testHostileDocuments() {
    // An entity that expands to ten to the tenth characters is refused.
    QByteArray bomb = "<!DOCTYPE a [<!ENTITY e0 \"aaaaaaaaaa\">";
    for (int i = 1; i < 10; ++i) {
        bomb += "<!ENTITY e" + QByteArray::number(i) + " \"";
        for (int j = 0; j < 10; ++j)
            bomb += "&e" + QByteArray::number(i - 1) + ";";
        bomb += "\">";
    }
    bomb += "]><a>&e9;</a>";
    QDomDocument doc;
    CHECK(!doc.setContent(bomb));
    CHECK(!doc.hasChildNodes());

    // Many attributes in one element.
    QByteArray wide = "<a";
    for (int i = 0; i < 5000; ++i)
        wide += " a" + QByteArray::number(i) + "=\"" + QByteArray::number(i) + "\"";
    wide += "/>";
    CHECK(doc.setContent(wide));
    CHECK(doc.documentElement().attributes().size() == 5000);
    CHECK_EQ(doc.documentElement().attribute("a4999"), "4999");
}

static void testStress() {
    // A large flat document and a deep one are read, listed and destroyed.
    QByteArray xml = "<r>";
    for (int i = 0; i < 20000; ++i)
        xml += "<i n=\"" + QByteArray::number(i) + "\">v</i>";
    xml += "</r>";
    {
        QDomDocument doc = parse(xml);
        CHECK(doc.documentElement().childNodes().size() == 20000);
        QDomNodeList items = doc.elementsByTagName("i");
        CHECK(items.size() == 20000);
        qint64 sum = 0;
        for (int i = 0; i < items.size(); ++i)
            sum += items.at(i).toElement().attribute("n").toInt();
        CHECK(sum == 199990000LL);
        int count = 0;
        for (QDomElement e = doc.documentElement().firstChildElement(); !e.isNull(); e = e.nextSiblingElement())
            ++count;
        CHECK(count == 20000);
        CHECK(doc.toString(-1).size() == xml.size());
    }
    QDomDocument doc;
    QDomElement e = doc.createElement("l0");
    doc.appendChild(e);
    for (int i = 1; i < 1000; ++i) {
        QDomElement child = doc.createElement("l");
        e.appendChild(child);
        e = child;
    }
    CHECK(doc.elementsByTagName("l").size() == 999);
    CHECK(doc.toString(-1).size() > 1000);
    CHECK(doc.cloneNode().toDocument().elementsByTagName("l").size() == 999);
}

int main() {
    testParseAndNavigate();
    testWhitespace();
    testSave();
    testEscaping();
    testNamespaces();
    testAttributes();
    testTree();
    testNodeLists();
    testClone();
    testText();
    testDocumentType();
    testErrors();
    testDestroyedDocument();
    testNullHandles();
    testInvalidData();
    testEncodings();
    testMisc();
    testRandomTree();
    testHostileDocuments();
    testStress();

    if (g_failures) {
        std::printf("%d DOM test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all DOM tests passed");
    return 0;
}
