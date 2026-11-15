// MIT License - see LICENSE
//
// Overview page about Nt XML for the NDoc documentation. This file holds only a
// documentation comment and is not part of the library.

/*!
    \page nt-xml.html
    \title Using Nt XML
    \brief How to read an XML document into a tree, walk and change the tree, and write it out.

    Nt XML holds a whole XML document in memory as a tree of nodes: the document is a QDomDocument,
    its elements are QDomElement, their text is QDomText, and all of them are a QDomNode. A program
    walks the tree, searches it, changes it and writes it out. The library is \c Nt::Xml and needs only
    Nt Core:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(library main.cpp)
    target_link_libraries(library PRIVATE Nt::Xml)
    \endcode

    The tree is all in memory at once. A document that is large and read from beginning to end, once, is
    better read with QXmlStreamReader of Nt Core, which needs no more memory than the one token that it is at;
    QXmlStreamWriter is the way to write such a document. QDomDocument reads with QXmlStreamReader, so what
    the reader accepts, the document does.

    \section1 Reading

    QDomDocument::setContent() reads a document from a byte array, a string, a QIODevice or a
    QXmlStreamReader. It returns a result that is \c true if the document was read, and otherwise has the message
    of the error and where it is:

    \code
    QFile file("library.xml");
    QDomDocument document;
    if (const auto result = document.setContent(&file); !result) {
        qWarning() << "library.xml:" << result.errorLine << result.errorColumn << result.errorMessage;
        return false;
    }
    \endcode

    A document that cannot be read is left empty. Bytes are decoded as the encoding of the XML declaration says;
    a string is text already, and the declaration does not matter.

    Text that is only spacing, such as the indentation between elements, is not in the tree, unless
    \c PreserveSpacingOnlyNodes is given to setContent(). Names are not taken apart unless
    \c UseNamespaceProcessing is given; see Namespaces below. The XML declaration is the first child of the
    document: a QDomProcessingInstruction with the target \c xml. A document type is a QDomDocumentType, a child
    before the document element. Elements nested deeper than 1024 levels are an error.

    \section1 Walking the Tree

    QDomNode::firstChild(), nextSibling(), parentNode() and the rest walk the tree node by node;
    QDomNode::firstChildElement() and nextSiblingElement(), with an optional tag name, skip what is not an element.
    A node is turned into its kind with \c toElement() and the others, which return a null node if it is not of that kind.
    QDomElement::attribute() gives an attribute, with a default for when there is none, and QDomElement::text() the
    text that is in the element:

    \code
    const QDomElement root = document.documentElement();
    for (QDomElement book = root.firstChildElement("book"); !book.isNull(); book = book.nextSiblingElement("book")) {
        const QString isbn = book.attribute("isbn");
        const QString title = book.firstChildElement("title").text();
        const int year = book.attribute("year", "0").toInt();
        process(isbn, title, year);
    }
    \endcode

    Every function can be used on a null node and gives an empty answer, so a chain such as the one for the
    title needs no check at each step. QDomDocument::elementsByTagName() and QDomElement::elementsByTagName()
    find the elements with a name at any depth, and the attributes of an element are in QDomElement::attributes():

    \code
    const QDomNodeList titles = document.elementsByTagName("title");
    for (int i = 0; i < titles.size(); ++i)
        qDebug() << titles.at(i).toElement().text();
    \endcode

    The lists, and the maps of attributes, are live: they show the tree as it is when they are asked. The attributes
    of an element are in the order in which they were added.

    \section1 Changing the Tree

    The nodes are made by the document, and are put into the tree by their parent:

    \code
    QDomElement book = document.createElement("book");
    book.setAttribute("isbn", "0-201-63361-2");
    book.setAttribute("year", 1994);
    QDomElement title = document.createElement("title");
    title.appendChild(document.createTextNode("Design Patterns"));
    book.appendChild(title);
    root.appendChild(book);
    \endcode

    QDomNode::insertBefore(), insertAfter(), replaceChild(), removeChild(), appendChild() and prependChild() change the
    children of a node. A node that is already in a tree is moved, and a QDomDocumentFragment gives its children
    when it is inserted. The functions return a null node, and change nothing, when the XML rules are broken:
    an attribute is not a child, a document has one element, and a node is not put into itself or into what it
    contains. Those that remove a node return it; it is deleted when the last handle to it goes.

    QDomNode::cloneNode() copies a node and, if wanted, all that is under it. A node that is put in a tree of another document
    becomes a node of that document.

    \section1 Writing

    QDomDocument::toString() and toByteArray() write the document, with each level indented by the number of spaces
    that is given (1 by default; 0 for lines without indentation; -1 for no line breaks at all).
    QDomNode::save() writes to a QTextStream, and takes the encoding from the XML declaration if the
    stream is a device:

    \code
    QFile out("library.xml");
    if (out.open(QIODevice::WriteOnly)) {
        QTextStream stream(&out);
        document.save(stream, 2);
    }
    \endcode

    The characters that XML needs entities for are written as entities, so that what is written reads back as the
    same document.

    \section1 Namespaces

    With \c UseNamespaceProcessing, an element or attribute knows its QDomNode::namespaceURI(), QDomNode::prefix() and
    QDomNode::localName(), and the declarations (\c xmlns and \c{xmlns:p}) are attributes of the element. QDomDocument::createElementNS()
    and the other functions with \c NS in their names make such nodes by hand, and the declarations that
    they need are written when the document is:

    \code
    QDomDocument document;
    QDomElement svg = document.createElementNS("http://www.w3.org/2000/svg", "svg");
    document.appendChild(svg);
    QDomElement link = document.createElementNS("http://www.w3.org/1999/xlink", "xlink:a");
    svg.appendChild(link);
    // <svg xmlns="http://www.w3.org/2000/svg"><xlink:a xmlns:xlink="http://www.w3.org/1999/xlink"/></svg>
    \endcode

    The functions without \c NS see only the names as written, with the prefix.

    \section1 Invalid Data

    XML does not allow every string as a name or as text: a name does not start with a digit, and text has
    no control characters. QDomImplementation::setInvalidDataPolicy() says what the functions that make nodes do
    with such data. The default, \c AcceptInvalidData, uses it as it is, which means that the document that
    is written may not be well-formed; \c ReturnNullNode makes no node, and \c DropInvalidData leaves out what is not allowed.
    A program that makes documents from data that it does not control should not use the default.

    \section1 Lifetime

    A QDomNode is a handle: a copy of it is a handle to the same node. A node is deleted when nothing
    refers to it, neither a handle nor a parent. A handle to a node of a document that is destroyed keeps the node, which is then the root
    of a tree of its own and has no QDomNode::ownerDocument().

    \section1 What Is Missing

    The classes of the SAX interface (\c QXmlSimpleReader, \c QXmlContentHandler and the rest), which Qt 6 has in
    Core5Compat, and the document type as more than a record of what the document declares: it is not used to check a
    document. The class pages list what differs from Qt in each class.

    \sa QDomDocument, QDomNode, QDomElement, QXmlStreamReader, QXmlStreamWriter
*/
