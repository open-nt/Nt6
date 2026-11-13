// MIT License - see LICENSE
#include <QtCore>
#include <QtCore/QXmlStreamReader>
#include <QtCore/QXmlStreamWriter>

#include <cstdio>
#include <string>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

using R = QXmlStreamReader;

// The tokens of a document as one line of text: what kind each is and what it carries.
static QString dump(QXmlStreamReader &xml) {
    QString out;
    while (!xml.atEnd()) {
        switch (xml.readNext()) {
        case R::StartDocument:
            out += "SD(" + xml.documentVersion().toString() + "," + xml.documentEncoding().toString() + ")";
            break;
        case R::EndDocument:
            out += "ED";
            break;
        case R::StartElement: {
            out += "<" + xml.qualifiedName().toString();
            if (!xml.namespaceUri().isEmpty())
                out += "{" + xml.namespaceUri().toString() + "}";
            const QXmlStreamAttributes attributes = xml.attributes();
            for (const QXmlStreamAttribute &a : attributes) {
                out += " " + a.qualifiedName().toString();
                if (!a.namespaceUri().isEmpty())
                    out += "{" + a.namespaceUri().toString() + "}";
                out += "=[" + a.value().toString() + "]";
            }
            out += ">";
            break;
        }
        case R::EndElement:
            out += "</" + xml.qualifiedName().toString() + ">";
            break;
        case R::Characters:
            out += QString(xml.isCDATA() ? "CD" : xml.isWhitespace() ? "WS" : "T") + "[" + xml.text().toString() + "]";
            break;
        case R::Comment:
            out += "C[" + xml.text().toString() + "]";
            break;
        case R::DTD:
            out += "DTD[" + xml.dtdName().toString() + "]";
            break;
        case R::EntityReference:
            out += "E[" + xml.name().toString() + "]";
            break;
        case R::ProcessingInstruction:
            out += "PI[" + xml.processingInstructionTarget().toString() + "|" +
                   xml.processingInstructionData().toString() + "]";
            break;
        case R::Invalid:
            out += xml.error() == R::PrematureEndOfDocumentError ? "!PREMATURE" : "!ERROR";
            break;
        case R::NoToken:
            out += "!NOTOKEN";
            break;
        }
    }
    return out;
}

static QString dump(const QByteArray &document) {
    QXmlStreamReader xml(document);
    return dump(xml);
}

static bool malformed(const QByteArray &document) {
    QXmlStreamReader xml(document);
    while (!xml.atEnd())
        xml.readNext();
    return xml.error() == R::NotWellFormedError && xml.tokenType() == R::Invalid && !xml.errorString().isEmpty();
}

static bool premature(const QByteArray &document) {
    QXmlStreamReader xml(document);
    while (!xml.atEnd())
        xml.readNext();
    return xml.error() == R::PrematureEndOfDocumentError;
}

static void testTokens() {
    CHECK(dump("<a/>") == "SD(,)<a></a>ED");
    CHECK(dump("<a></a>") == "SD(,)<a></a>ED");
    CHECK(dump("<?xml version=\"1.0\"?><a/>") == "SD(1.0,)<a></a>ED");
    CHECK(dump("<?xml version='1.0' encoding='UTF-8' standalone='yes'?>\n<a/>\n") == "SD(1.0,UTF-8)<a></a>ED");
    CHECK(dump("<a>text</a>") == "SD(,)<a>T[text]</a>ED");
    CHECK(dump("<a> \n\t</a>") == "SD(,)<a>WS[ \n\t]</a>ED");
    CHECK(dump("<a>x<b/>y<c>z</c> </a>") == "SD(,)<a>T[x]<b></b>T[y]<c>T[z]</c>WS[ ]</a>ED");
    CHECK(dump("<a k=\"v\" other='w'/>") == "SD(,)<a k=[v] other=[w]></a>ED");
    CHECK(dump("<a k = \"v\"\n  l='' ></a >") == "SD(,)<a k=[v] l=[]></a>ED");
    CHECK(dump("<!-- one --><a><!--two--></a><!---->") == "SD(,)C[ one ]<a>C[two]</a>C[]ED");
    CHECK(dump("<?before data here?><a><?in?></a><?after  x ?>") ==
          "SD(,)PI[before|data here]<a>PI[in|]</a>PI[after|x ]ED");
    CHECK(dump("<a><![CDATA[<raw> & ]] text]]></a>") == "SD(,)<a>CD[<raw> & ]] text]</a>ED");
    CHECK(dump("<a>x<![CDATA[]]>y</a>") == "SD(,)<a>T[x]CD[]T[y]</a>ED");
    CHECK(dump("  \n<a/>  \n ") == "SD(,)<a></a>ED");

    // References
    CHECK(dump("<a>&lt;&gt;&amp;&apos;&quot;</a>") == "SD(,)<a>T[<>&'\"]</a>ED");
    CHECK(dump("<a>&#65;&#x42;&#x63;&#10;</a>") == "SD(,)<a>T[ABc\n]</a>ED");
    CHECK(dump("<a>&#32;&#9;</a>") == "SD(,)<a>WS[ \t]</a>ED");
    CHECK(dump("<a k='&lt;&#65;&amp;&quot;'/>") == "SD(,)<a k=[<A&\"]></a>ED");
    {
        QXmlStreamReader xml(QByteArray("<a>&#x1F600;</a>"));
        xml.readNextStartElement();
        const QString text = xml.readElementText();
        CHECK(text.size() == 2 && text.at(0).isHighSurrogate() && text == QString::fromUtf8("\xf0\x9f\x98\x80"));
    }
    // White space in the value of an attribute becomes spaces, but not what a reference stands for
    CHECK(dump("<a k='x\ty\nz&#10;'/>") == "SD(,)<a k=[x y z\n]></a>ED");
    // Line ends become line feeds
    CHECK(dump("<a>one\r\ntwo\rthree\n</a>") == "SD(,)<a>T[one\ntwo\nthree\n]</a>ED");

    // Names with dots, dashes, digits and letters from beyond ASCII
    CHECK(dump(QString::fromUtf8("<r\xc3\xa9sum\xc3\xa9.v-2_x a\xc3\xb1o='1'/>").toUtf8()) ==
          QString::fromUtf8("SD(,)<r\xc3\xa9sum\xc3\xa9.v-2_x a\xc3\xb1o=[1]></r\xc3\xa9sum\xc3\xa9.v-2_x>ED"));
}

static void testNamespaces() {
    CHECK(dump("<a xmlns='urn:d'><b/></a>") == "SD(,)<a{urn:d}><b{urn:d}></b></a>ED");
    CHECK(dump("<p:a xmlns:p='urn:p' p:k='1' k='2'><b/></p:a>") ==
          "SD(,)<p:a{urn:p} p:k{urn:p}=[1] k=[2]><b></b></p:a>ED");
    // A declaration holds for the element and what is inside it; an inner one hides it
    CHECK(dump("<a xmlns='urn:1'><b xmlns='urn:2'><c/></b><d xmlns=''/><e/></a>") ==
          "SD(,)<a{urn:1}><b{urn:2}><c{urn:2}></c></b><d></d><e{urn:1}></e></a>ED");
    CHECK(dump("<a xmlns:p='urn:1'><p:b xmlns:p='urn:2'/><p:c/></a>") ==
          "SD(,)<a><p:b{urn:2}></p:b><p:c{urn:1}></p:c></a>ED");
    CHECK(dump("<a xml:lang='en'/>") == "SD(,)<a xml:lang{http://www.w3.org/XML/1998/namespace}=[en]></a>ED");
    CHECK(malformed("<p:a/>") && malformed("<a p:k='1'/>") && malformed("<a xmlns:p=''/>"));
    CHECK(malformed("<a:b:c xmlns:a='urn:a'/>") && malformed("<:a/>") && malformed("<a: xmlns:a='urn:a'/>"));
    CHECK(malformed("<a xmlns:p='urn:1' xmlns:q='urn:1' p:k='1' q:k='2'/>")); // the same attribute twice
    CHECK(!malformed("<a xmlns:p='urn:1' xmlns:q='urn:2' p:k='1' q:k='2' k='3'/>"));
    CHECK(malformed("<a xmlns:xmlns='urn:x'/>") && malformed("<a xmlns:xml='urn:other'/>"));

    {
        QXmlStreamReader xml(
            QByteArray("<p:root xmlns:p='urn:p' xmlns='urn:d' p:attr='v' plain='w'><child/></p:root>"));
        CHECK(xml.namespaceProcessing());
        CHECK(xml.readNextStartElement());
        CHECK(xml.name() == u"root" && xml.prefix() == u"p" && xml.qualifiedName() == u"p:root");
        CHECK(xml.namespaceUri() == u"urn:p");
        const QXmlStreamNamespaceDeclarations declarations = xml.namespaceDeclarations();
        CHECK(declarations.size() == 2);
        CHECK(declarations.at(0) == QXmlStreamNamespaceDeclaration("p", "urn:p"));
        CHECK(declarations.at(1).prefix().isEmpty() && declarations.at(1).namespaceUri() == u"urn:d");
        CHECK(declarations.at(0) != declarations.at(1));
        const QXmlStreamAttributes attributes = xml.attributes();
        CHECK(attributes.size() == 2);
        CHECK(attributes.at(0).name() == u"attr" && attributes.at(0).prefix() == u"p");
        CHECK(attributes.at(0).qualifiedName() == u"p:attr" && attributes.at(0).namespaceUri() == u"urn:p");
        CHECK(attributes.at(0).value() == u"v" && !attributes.at(0).isDefault());
        CHECK(attributes.at(1).name() == u"plain" && attributes.at(1).prefix().isEmpty());
        CHECK(attributes.at(1).namespaceUri().isEmpty());
        CHECK(attributes.value("urn:p", "attr") == u"v" && attributes.value("p:attr") == u"v");
        CHECK(attributes.value("plain") == u"w" && attributes.value("", "plain") == u"w");
        CHECK(attributes.value("missing").isNull() && attributes.value("urn:d", "plain").isNull());
        CHECK(attributes.hasAttribute("plain") && attributes.hasAttribute("urn:p", "attr"));
        CHECK(!attributes.hasAttribute("attr") && !attributes.hasAttribute("urn:x", "attr"));
        CHECK(xml.readNextStartElement() && xml.name() == u"child" && xml.namespaceUri() == u"urn:d");
        CHECK(xml.prefix().isEmpty() && xml.namespaceDeclarations().isEmpty() && xml.attributes().isEmpty());
        xml.skipCurrentElement();
        CHECK(xml.isEndElement() && xml.name() == u"child" && xml.namespaceUri() == u"urn:d");
        CHECK(xml.readNext() == R::EndElement && xml.name() == u"root" && xml.namespaceUri() == u"urn:p");
        CHECK(xml.prefix() == u"p");
    }
    {
        // Without namespace processing a name is what is written
        QXmlStreamReader xml;
        xml.setNamespaceProcessing(false);
        CHECK(!xml.namespaceProcessing());
        xml.addData(QByteArray("<p:a xmlns:p='urn:p' q:k='1'/>"));
        CHECK(xml.readNextStartElement() && xml.name() == u"p:a" && xml.qualifiedName() == u"p:a");
        CHECK(xml.prefix().isEmpty() && xml.namespaceUri().isEmpty() && xml.namespaceDeclarations().isEmpty());
        CHECK(xml.attributes().size() == 2 && xml.attributes().hasAttribute("xmlns:p") &&
              xml.attributes().value("q:k") == u"1");
        CHECK(xml.readNext() == R::EndElement && xml.readNext() == R::EndDocument && !xml.hasError());
    }
    {
        // A fragment whose prefixes are declared elsewhere
        QXmlStreamReader xml;
        xml.addExtraNamespaceDeclaration(QXmlStreamNamespaceDeclaration("ext", "urn:ext"));
        xml.addExtraNamespaceDeclarations(
            {QXmlStreamNamespaceDeclaration("two", "urn:two"), QXmlStreamNamespaceDeclaration("", "urn:def")});
        xml.addData(QByteArray("<ext:a two:k='1'><b/></ext:a>"));
        CHECK(dump(xml) == "SD(,)<ext:a{urn:ext} two:k{urn:two}=[1]><b{urn:def}></b></ext:a>ED");
    }
}

static void testAttributeValue() {
    // An attribute with an empty value is there
    QXmlStreamReader xml(QByteArray("<a empty='' full='x'/>"));
    CHECK(xml.readNextStartElement());
    const QXmlStreamAttributes attributes = xml.attributes();
    CHECK(attributes.hasAttribute("empty") && attributes.value("empty").isEmpty() &&
          !attributes.value("empty").isNull());
    CHECK(!attributes.hasAttribute("none") && attributes.value("none").isNull());

    QXmlStreamAttributes made;
    made.append("first", "1");
    made.append("urn:n", "second", "2");
    made.append(QXmlStreamAttribute("p:third", ""));
    CHECK(made.size() == 3 && made.value("first") == u"1" && made.value("urn:n", "second") == u"2");
    CHECK(made.hasAttribute("p:third") && made.at(2).name() == u"third" && made.at(2).prefix() == u"p");
    CHECK(made.at(1).namespaceUri() == u"urn:n" && made.at(1).qualifiedName() == u"second" &&
          made.at(1).prefix().isEmpty());
    CHECK(QXmlStreamAttribute("k", "v") == QXmlStreamAttribute("k", "v") &&
          QXmlStreamAttribute("k", "v") != QXmlStreamAttribute("k", "w"));
    CHECK(QXmlStreamAttribute("k", "v") != QXmlStreamAttribute("l", "v"));
    CHECK(QXmlStreamAttribute("urn:n", "k", "v") == QXmlStreamAttribute("urn:n", "k", "v"));
    CHECK(QXmlStreamAttribute("urn:n", "k", "v") != QXmlStreamAttribute("urn:m", "k", "v"));
    const QXmlStreamAttribute empty;
    CHECK(empty.name().isEmpty() && empty.value().isEmpty() && empty.qualifiedName().isEmpty() &&
          empty.prefix().isEmpty());
    const QXmlStreamNamespaceDeclaration none;
    CHECK(none.prefix().isEmpty() && none.namespaceUri().isEmpty() && none == QXmlStreamNamespaceDeclaration());
    CHECK(QXmlStreamNotationDeclaration() == QXmlStreamNotationDeclaration() &&
          QXmlStreamNotationDeclaration().name().isEmpty());
    CHECK(QXmlStreamEntityDeclaration() == QXmlStreamEntityDeclaration() &&
          QXmlStreamEntityDeclaration().value().isEmpty());
}

static void testErrors() {
    // Not well-formed
    for (const char *document : {"<a><b></a></b>",
                                 "<a></b>",
                                 "<a/><b/>",
                                 "<a/>text",
                                 "text<a/>",
                                 "<a k='1' k='2'/>",
                                 "<a k=v/>",
                                 "<a k/>",
                                 "<a k='<'/>",
                                 "<a 1='x'/>",
                                 "<1a/>",
                                 "<a>&undeclared;</a>",
                                 "<a>&#0;</a>",
                                 "<a>&#xFFFF;</a>",
                                 "<a>&#xD800;</a>",
                                 "<a>&#x110000;</a>",
                                 "<a>&;</a>",
                                 "<a>&#;</a>",
                                 "<a>&#x;</a>",
                                 "<a>&#12a;</a>",
                                 "<a>]]></a>",
                                 "<a><!-- a -- b --></a>",
                                 "<a><?xml version='1.0'?></a>",
                                 " <?xml version='1.0'?><a/>",
                                 "<?xml version='1.1'?><a/>",
                                 "<?xml encoding='UTF-8'?><a/>",
                                 "<?xml version='1.0' standalone='maybe'?><a/>",
                                 "<?xml version='1.0' bogus='1'?><a/>",
                                 "<?xml version='1.0' encoding='no such'?><a/>",
                                 "<?xml?><a/>",
                                 "<a k='1'l='2'/>",
                                 "<a/ >",
                                 "<a><![CDATA[x]]></a><![CDATA[y]]>",
                                 "<![CDATA[x]]><a/>",
                                 "<a></a></a>",
                                 "</a>",
                                 "<a>\x01</a>",
                                 "<a k='&lt'/>",
                                 "<a k='&nope;'/>",
                                 "<a><!bogus></a>",
                                 "<a/><!DOCTYPE a>",
                                 "<!DOCTYPE a><!DOCTYPE a><a/>",
                                 "<a><!DOCTYPE a></a>",
                                 "<?xml version='1.0' encoding='KOI8-R'?><a/>"}) {
        const bool bad = malformed(document);
        CHECK(bad);
        if (!bad)
            std::printf("  accepted: %s\n", document);
    }
    // The input ends too early
    for (const char *document : {"",
                                 " ",
                                 "<",
                                 "<a",
                                 "<a>",
                                 "<a><b></b>",
                                 "<a>text",
                                 "<a k='v",
                                 "<a k=",
                                 "<!-- c",
                                 "<?pi x",
                                 "<a><![CDATA[x",
                                 "<?xml version='1.0'?>",
                                 "<a>&am",
                                 "<!DOCTYPE a [",
                                 "<a></",
                                 "<a></a",
                                 "<a>]",
                                 "<a>]]",
                                 "<!",
                                 "<!DOCTYPE a [<!ENTITY e 'x'"}) {
        const bool early = premature(document);
        CHECK(early);
        if (!early)
            std::printf("  not premature: \"%s\" gave %s\n", document, dump(document).toUtf8().constData());
    }
    // Well-formed, where a careless parser might object
    for (const char *document :
         {"<a>]</a>", "<a>]]</a>", "<a>] ]></a>", "<a>a]]b</a>", "<a k='>'/>", "<a k=\"'\" l='\"'/>",
          "<a><!-- - --></a>", "<a><?p ?></a>", "<a><?xml-stylesheet href='x'?></a>", "<?xml version = \"1.0\" ?><a/>",
          "<a\n/>", "<a><b\t/></a\n>", "<a>&#x10FFFF;&#xD7FF;&#xE000;</a>", "<!--c--><?p?><a/><?p?><!--c-->"}) {
        const bool good = !malformed(document) && !premature(document);
        CHECK(good);
        if (!good)
            std::printf("  rejected: %s\n", document);
    }

    {
        // The message and the place
        QXmlStreamReader xml(QByteArray("<a>\n  <b>\n  </c>\n</a>"));
        CHECK(!xml.hasError() && xml.error() == R::NoError && xml.errorString().isEmpty());
        CHECK(xml.tokenType() == R::NoToken && xml.tokenString() == "NoToken" && !xml.atEnd());
        CHECK(xml.lineNumber() == 1 && xml.columnNumber() == 0 && xml.characterOffset() == 0);
        while (!xml.atEnd())
            xml.readNext();
        CHECK(xml.hasError() && xml.error() == R::NotWellFormedError && xml.tokenString() == "Invalid");
        CHECK(xml.errorString() == "Opening and ending tag mismatch.");
        CHECK(xml.lineNumber() == 3 && xml.columnNumber() == 2);
        // The error stays
        CHECK(xml.readNext() == R::Invalid && xml.error() == R::NotWellFormedError && xml.atEnd());
        CHECK(!xml.readNextStartElement());
        CHECK(xml.name().isEmpty() && xml.text().isEmpty() && xml.attributes().isEmpty());
    }
    {
        // An error of the program
        QXmlStreamReader xml(QByteArray("<a><b/></a>"));
        CHECK(xml.readNextStartElement() && xml.name() == u"a");
        xml.raiseError("not what was expected");
        CHECK(xml.hasError() && xml.error() == R::CustomError && xml.errorString() == "not what was expected");
        CHECK(xml.atEnd() && xml.tokenType() == R::Invalid && xml.readNext() == R::Invalid);
        QXmlStreamReader other(QByteArray("<a/>"));
        other.raiseError();
        CHECK(other.error() == R::CustomError && !other.errorString().isEmpty());
    }
    {
        // After the end there is nothing, and no error
        QXmlStreamReader xml(QByteArray("<a/>"));
        while (!xml.atEnd())
            xml.readNext();
        CHECK(xml.isEndDocument() && !xml.hasError() && xml.tokenString() == "EndDocument");
        CHECK(xml.readNext() == R::Invalid && !xml.hasError());
    }
}

static void testPositions() {
    QXmlStreamReader xml(QByteArray("<?xml version='1.0'?>\n<a>\r\n  <b k='v'/>text\n</a>\n"));
    CHECK(xml.readNext() == R::StartDocument && xml.isStartDocument());
    CHECK(xml.lineNumber() == 1 && xml.columnNumber() == 21 && xml.characterOffset() == 21);
    CHECK(xml.readNext() == R::StartElement && xml.lineNumber() == 2 && xml.columnNumber() == 3 &&
          xml.characterOffset() == 25);
    CHECK(xml.readNext() == R::Characters && xml.isWhitespace() && xml.lineNumber() == 3 && xml.columnNumber() == 2);
    CHECK(xml.characterOffset() == 28); // the line end counts as one character
    CHECK(xml.readNext() == R::StartElement && xml.columnNumber() == 12);
    CHECK(xml.readNext() == R::EndElement && xml.columnNumber() == 12 && xml.isEndElement());
    CHECK(xml.readNext() == R::Characters && xml.text() == u"text\n" && xml.lineNumber() == 4 &&
          xml.columnNumber() == 0);
    CHECK(xml.isCharacters() && !xml.isWhitespace() && !xml.isCDATA());
    CHECK(xml.readNext() == R::EndElement && xml.columnNumber() == 4);
    CHECK(xml.readNext() == R::EndDocument && xml.lineNumber() == 5 && xml.isEndDocument());

    // The names of the tokens
    QXmlStreamReader names(QByteArray("<!DOCTYPE a SYSTEM 'a.dtd'><a>t<!--c--><?p?>&e;</a>"));
    QStringList seen;
    while (!names.atEnd()) {
        names.readNext();
        seen.append(names.tokenString());
    }
    CHECK(seen == QStringList({"StartDocument", "DTD", "StartElement", "Characters", "Comment", "ProcessingInstruction",
                               "EntityReference", "EndElement", "EndDocument"}));
}

static void testDocumentStart() {
    {
        QXmlStreamReader xml(QByteArray("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><a/>"));
        CHECK(xml.readNext() == R::StartDocument && xml.documentVersion() == u"1.0" &&
              xml.documentEncoding() == u"UTF-8");
        CHECK(!xml.isStandaloneDocument() && xml.hasStandaloneDeclaration());
    }
    {
        QXmlStreamReader xml(QByteArray("<?xml version='1.0' standalone='yes'?><a/>"));
        CHECK(xml.readNext() == R::StartDocument && xml.isStandaloneDocument() && xml.hasStandaloneDeclaration());
        CHECK(xml.documentEncoding().isEmpty());
    }
    {
        QXmlStreamReader xml(QByteArray("<a/>"));
        CHECK(xml.readNext() == R::StartDocument && xml.documentVersion().isEmpty());
        CHECK(!xml.isStandaloneDocument() && !xml.hasStandaloneDeclaration());
    }
}

static void testEncodings() {
    const QString expected = QString::fromUtf8("SD(1.0,UTF-8)<a k=[\xc3\xa4]>T[\xe2\x82\xac \xf0\x9f\x98\x80]</a>ED");
    const QByteArray utf8("<?xml version='1.0' encoding='UTF-8'?><a k='\xc3\xa4'>\xe2\x82\xac \xf0\x9f\x98\x80</a>");
    CHECK(dump(utf8) == expected);
    CHECK(dump(QByteArray("\xef\xbb\xbf") + utf8) == expected); // with a byte order mark

    // UTF-16, with a byte order mark and without
    const QString text = QString::fromUtf8(utf8).replace("UTF-8", "UTF-16");
    const QString expected16 = QString(expected).replace("UTF-8", "UTF-16");
    QByteArray le, be;
    for (const QChar c : text) {
        le.append(static_cast<char>(c.unicode() & 0xff));
        le.append(static_cast<char>(c.unicode() >> 8));
        be.append(static_cast<char>(c.unicode() >> 8));
        be.append(static_cast<char>(c.unicode() & 0xff));
    }
    CHECK(dump(QByteArray("\xff\xfe") + le) == expected16 && dump(QByteArray("\xfe\xff") + be) == expected16);
    CHECK(dump(le) == expected16 && dump(be) == expected16);

    // Latin-1 by its declaration
    CHECK(dump("<?xml version='1.0' encoding='ISO-8859-1'?><a>\xe4\xf6\xfc</a>") ==
          QString::fromUtf8("SD(1.0,ISO-8859-1)<a>T[\xc3\xa4\xc3\xb6\xc3\xbc]</a>ED"));
    // Text is text: the encoding it names is only reported
    QXmlStreamReader fromString(QString::fromUtf8("<?xml version='1.0' encoding='ISO-8859-1'?><a>\xc3\xa4</a>"));
    CHECK(dump(fromString) == QString::fromUtf8("SD(1.0,ISO-8859-1)<a>T[\xc3\xa4]</a>ED"));
    QXmlStreamReader fromLiteral("<a>literal</a>");
    CHECK(dump(fromLiteral) == "SD(,)<a>T[literal]</a>ED");
}

static void testIncremental() {
    const QByteArray document("<?xml version='1.0' encoding='UTF-8'?>\r\n<!DOCTYPE r [<!ENTITY e 'ent'>]>\r\n"
                              "<r a='1' b=\"two &amp; three\"><!-- note --><c>\xc3\xa4\xe2\x82\xac&e;&#65;]</c>"
                              "<![CDATA[raw]]><?pi data?><d/>tail\r\n</r>\n");
    const QString whole = dump(document);
    CHECK(whole == QString::fromUtf8("SD(1.0,UTF-8)DTD[r]<r a=[1] b=[two & three]>C[ note ]<c>T[\xc3\xa4\xe2\x82\xac"
                                     "entA]]</c>CD[raw]PI[pi|data]<d></d>T[tail\n]</r>ED"));

    // Byte by byte: every token comes out the same, whatever the pieces
    for (int chunk : {1, 2, 3, 7, 16}) {
        QXmlStreamReader xml;
        QString out;
        int prematureEnds = 0;
        for (qsizetype at = 0; at <= document.size(); at += chunk) {
            if (at < document.size())
                xml.addData(document.mid(at, chunk));
            out += dump(xml);
            if (xml.error() == R::PrematureEndOfDocumentError) {
                ++prematureEnds;
                CHECK(out.endsWith("!PREMATURE"));
                out.chop(10);
                CHECK(xml.atEnd() && xml.tokenType() == R::Invalid);
            }
        }
        CHECK(out == whole && !xml.hasError() && xml.isEndDocument());
        CHECK(prematureEnds > 0);
        if (out != whole)
            std::printf("  chunk %d: %s\n", chunk, out.toUtf8().constData());
    }

    {
        // Pieces of text, and bytes before text
        QXmlStreamReader xml;
        CHECK(xml.readNext() == R::Invalid && xml.error() == R::PrematureEndOfDocumentError && xml.atEnd());
        xml.addData(QString("<a>one"));
        CHECK(!xml.atEnd());
        CHECK(xml.readNext() == R::StartDocument && xml.readNext() == R::StartElement);
        CHECK(xml.readNext() == R::Invalid && xml.error() == R::PrematureEndOfDocumentError);
        xml.addData(" two</a>");
        CHECK(xml.readNext() == R::Characters && xml.text() == u"one two" && !xml.hasError());
        CHECK(xml.readNext() == R::EndElement && xml.readNext() == R::EndDocument && xml.atEnd());
    }
    {
        // clear() starts over, with the settings kept
        QXmlStreamReader xml(QByteArray("<a><b>"));
        xml.setNamespaceProcessing(false);
        xml.setEntityExpansionLimit(100);
        while (!xml.atEnd())
            xml.readNext();
        CHECK(xml.error() == R::PrematureEndOfDocumentError);
        xml.clear();
        CHECK(!xml.hasError() && xml.tokenType() == R::NoToken && xml.lineNumber() == 1 && xml.characterOffset() == 0);
        CHECK(!xml.namespaceProcessing() && xml.entityExpansionLimit() == 100 && xml.device() == nullptr);
        xml.addData(QByteArray("<x:y/>"));
        CHECK(dump(xml) == "SD(,)<x:y></x:y>ED");
    }
}

static void testDevice() {
    QByteArray document("<?xml version='1.0'?>\n<list>\n");
    for (int i = 0; i < 20000; ++i)
        document +=
            "  <item id='" + QByteArray::number(i) + "'>value " + QByteArray::number(i * 7) + " \xc3\xa4</item>\n";
    document += "</list>\n";
    CHECK(document.size() > 600 * 1024); // several of the reader's chunks

    QBuffer buffer(&document);
    CHECK(buffer.open(QIODevice::ReadOnly));
    QXmlStreamReader xml(&buffer);
    CHECK(xml.device() == &buffer);
    CHECK(xml.readNextStartElement() && xml.name() == u"list");
    int count = 0;
    bool right = true;
    while (xml.readNextStartElement()) {
        right = right && xml.name() == u"item" && xml.attributes().value("id").toInt() == count;
        right =
            right && xml.readElementText() == "value " + QString::number(count * 7) + QString::fromUtf8(" \xc3\xa4");
        ++count;
    }
    CHECK(count == 20000 && right && !xml.hasError());
    CHECK(xml.isEndElement() && xml.name() == u"list");
    CHECK(xml.readNext() == R::EndDocument && xml.atEnd() && xml.lineNumber() == 20004);
    CHECK(xml.characterOffset() == QString::fromUtf8(document).size());

    // Data cannot be added to a reader with a device; setDevice() starts a new document
    QByteArray small("<s>1</s>");
    QBuffer second(&small);
    CHECK(second.open(QIODevice::ReadOnly));
    xml.setDevice(&second);
    CHECK(xml.device() == &second && xml.tokenType() == R::NoToken && !xml.atEnd());
    CHECK(dump(xml) == "SD(,)<s>T[1]</s>ED");

    // A device that ends in the middle
    QByteArray cut("<a><b>text");
    QBuffer third(&cut);
    CHECK(third.open(QIODevice::ReadOnly));
    QXmlStreamReader early(&third);
    CHECK(dump(early) == "SD(,)<a><b>!PREMATURE" && early.error() == R::PrematureEndOfDocumentError);
}

static void testHelpers() {
    const QByteArray document(
        "<root><title>The <b>bold</b> one<!-- c --><?p?> here</title><skip><deep><deeper/></deep>x</skip>"
        "<empty/><last>end</last></root>");
    {
        QXmlStreamReader xml(document);
        CHECK(xml.readNextStartElement() && xml.name() == u"root");
        CHECK(xml.readNextStartElement() && xml.name() == u"title");
        CHECK(xml.readElementText(R::IncludeChildElements) == "The bold one here");
        CHECK(xml.isEndElement() && xml.name() == u"title");
        CHECK(xml.readNextStartElement() && xml.name() == u"skip");
        xml.skipCurrentElement();
        CHECK(xml.isEndElement() && xml.name() == u"skip");
        CHECK(xml.readNextStartElement() && xml.name() == u"empty");
        CHECK(xml.readElementText().isEmpty() && xml.isEndElement() && xml.name() == u"empty");
        CHECK(xml.readNextStartElement() && xml.name() == u"last" && xml.readElementText() == "end");
        CHECK(!xml.readNextStartElement() && xml.isEndElement() && xml.name() == u"root");
        CHECK(!xml.readNextStartElement() && xml.isEndDocument() && !xml.hasError());
    }
    {
        QXmlStreamReader xml(document);
        xml.readNextStartElement();
        xml.readNextStartElement();
        CHECK(xml.readElementText(R::SkipChildElements) == "The  one here" && xml.name() == u"title" &&
              !xml.hasError());
    }
    {
        QXmlStreamReader xml(document);
        xml.readNextStartElement();
        xml.readNextStartElement();
        CHECK(xml.readElementText() == "The " && xml.error() == R::UnexpectedElementError);
        CHECK(xml.errorString() == "Expected character data." && xml.atEnd() && xml.readNext() == R::Invalid);
    }
    {
        // Not at a start element
        QXmlStreamReader xml(document);
        CHECK(xml.readElementText().isEmpty() && xml.error() == R::CustomError);
    }
    {
        // CDATA and references are part of the text
        QXmlStreamReader xml(QByteArray("<a>one <![CDATA[<two>]]> &amp; three</a>"));
        xml.readNextStartElement();
        CHECK(xml.readElementText() == "one <two> & three");
    }
    {
        // The document ends inside the element
        QXmlStreamReader xml(QByteArray("<a>one"));
        xml.readNextStartElement();
        xml.readElementText();
        CHECK(xml.error() == R::PrematureEndOfDocumentError);
        QXmlStreamReader skip(QByteArray("<a><b>"));
        skip.readNextStartElement();
        skip.skipCurrentElement();
        CHECK(skip.error() == R::PrematureEndOfDocumentError);
    }
}

namespace {

class Resolver : public QXmlStreamEntityResolver {
public:
    QString resolveUndeclaredEntity(const QString &name) override {
        asked.append(name);
        if (name == "nbsp")
            return QString(QChar(0xa0));
        if (name == "markup")
            return QString("<i>made</i>");
        return QString();
    }
    QStringList asked;
};

} // namespace

static void testDtd() {
    {
        QXmlStreamReader xml(QByteArray("<!DOCTYPE doc PUBLIC \"-//Nt//DTD Test//EN\" 'test.dtd' [\n"
                                        "  <!-- a comment with > and ' in it -->\n"
                                        "  <?pi with > ?>\n"
                                        "  <!ELEMENT doc (#PCDATA|p)*>\n"
                                        "  <!ATTLIST doc kind CDATA \"a > b\">\n"
                                        "  <!ENTITY name \"N&#116;\">\n"
                                        "  <!ENTITY name \"ignored\">\n"
                                        "  <!ENTITY nested 'the &name; library'>\n"
                                        "  <!ENTITY markup '<p>para</p> and &lt;'>\n"
                                        "  <!ENTITY % param 'ignored'>\n"
                                        "  <!ENTITY logo SYSTEM \"logo.png\" NDATA png>\n"
                                        "  <!ENTITY chapter PUBLIC 'pub-id' 'chapter.xml'>\n"
                                        "  <!NOTATION png SYSTEM \"image/png\">\n"
                                        "  <!NOTATION gif PUBLIC 'gif-id'>\n"
                                        "  <!NOTATION jpg PUBLIC 'jpg-id' \"image/jpeg\">\n"
                                        "]>\n"
                                        "<doc a='&name; &nested;'>&name;, &nested;: &markup;&chapter;</doc>"));
        CHECK(xml.readNext() == R::StartDocument);
        CHECK(xml.readNext() == R::DTD && xml.isDTD());
        CHECK(xml.dtdName() == u"doc" && xml.dtdPublicId() == u"-//Nt//DTD Test//EN" &&
              xml.dtdSystemId() == u"test.dtd");
        CHECK(xml.text().startsWith(u"<!DOCTYPE doc PUBLIC") && xml.text().endsWith(u"]>"));
        const QXmlStreamNotationDeclarations notations = xml.notationDeclarations();
        CHECK(notations.size() == 3);
        CHECK(notations.at(0).name() == u"png" && notations.at(0).systemId() == u"image/png" &&
              notations.at(0).publicId().isEmpty());
        CHECK(notations.at(1).name() == u"gif" && notations.at(1).publicId() == u"gif-id" &&
              notations.at(1).systemId().isEmpty());
        CHECK(notations.at(2).publicId() == u"jpg-id" && notations.at(2).systemId() == u"image/jpeg");
        CHECK(notations.at(0) != notations.at(1) && notations.at(0) == notations.at(0));
        const QXmlStreamEntityDeclarations unparsed = xml.entityDeclarations();
        CHECK(unparsed.size() == 1 && unparsed.at(0).name() == u"logo" && unparsed.at(0).notationName() == u"png");
        CHECK(unparsed.at(0).systemId() == u"logo.png" && unparsed.at(0).publicId().isEmpty() &&
              unparsed.at(0).value().isEmpty());
        CHECK(unparsed.at(0) == unparsed.at(0) && unparsed.at(0) != QXmlStreamEntityDeclaration());

        // Entities in the value of an attribute and in text, one inside another, and one with an element in it
        CHECK(xml.readNext() == R::StartElement && xml.attributes().value("a") == u"Nt the Nt library");
        CHECK(xml.readNext() == R::Characters && xml.text() == u"Nt, the Nt library: ");
        CHECK(xml.readNext() == R::StartElement && xml.name() == u"p");
        CHECK(xml.readNext() == R::Characters && xml.text() == u"para");
        CHECK(xml.readNext() == R::EndElement && xml.name() == u"p");
        CHECK(xml.readNext() == R::Characters && xml.text() == u" and <");
        // An external entity is not read: the reference is reported
        CHECK(xml.readNext() == R::EntityReference && xml.isEntityReference() && xml.name() == u"chapter" &&
              xml.text().isEmpty());
        CHECK(xml.readNext() == R::EndElement && xml.readNext() == R::EndDocument && !xml.hasError());
        // The declarations stay known
        CHECK(xml.notationDeclarations().size() == 3 && xml.dtdName() == u"doc");
    }
    CHECK(dump("<!DOCTYPE a><a/>") == "SD(,)DTD[a]<a></a>ED");
    CHECK(dump("<?xml version='1.0'?>\n<!DOCTYPE a SYSTEM \"a.dtd\" >\n<a/>") == "SD(1.0,)DTD[a]<a></a>ED");
    CHECK(dump("<!DOCTYPE a [ ] ><a/>") == "SD(,)DTD[a]<a></a>ED");
    // With an external subset or parameter entities an entity may be declared out of sight
    CHECK(dump("<!DOCTYPE a SYSTEM 'a.dtd'><a>x&unknown;y</a>") == "SD(,)DTD[a]<a>T[x]E[unknown]T[y]</a>ED");
    CHECK(dump("<!DOCTYPE a [<!ENTITY % p SYSTEM 'p.ent'> %p;]><a>&unknown;</a>") == "SD(,)DTD[a]<a>E[unknown]</a>ED");
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e 'x'>]><a>&unknown;</a>"));
    // An external entity has no place in the value of an attribute
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e SYSTEM 'e.xml'>]><a k='&e;'/>"));
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e '<b/>'>]><a k='&e;'/>"));
    for (const char *document :
         {"<!DOCTYPE><a/>", "<!DOCTYPE a PUBLIC 'only-public'><a/>", "<!DOCTYPE a [<!BOGUS x>]><a/>",
          "<!DOCTYPE a [<!ENTITY e>]><a/>", "<!DOCTYPE a [<!ENTITY e 'x' 'y'>]><a/>",
          "<!DOCTYPE a [<!NOTATION n>]><a/>", "<!DOCTYPE a [text]><a/>", "<!DOCTYPE a SYSTEM><a/>",
          "<!DOCTYPE a [<!ENTITY e '&#0;'>]><a/>", "<!DOCTYPE a [<!ENTITY e 'x'>] junk><a/>"}) {
        const bool bad = malformed(document);
        CHECK(bad);
        if (!bad)
            std::printf("  accepted: %s\n", document);
    }

    // Entities that never stop growing
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e '&e;'>]><a>&e;</a>"));
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e '&f;'><!ENTITY f '&e;'>]><a>&e;</a>"));
    CHECK(malformed("<!DOCTYPE a [<!ENTITY e '&e;'>]><a k='&e;'/>"));
    const QByteArray bomb(
        "<!DOCTYPE a [<!ENTITY a '0123456789'><!ENTITY b '&a;&a;&a;&a;&a;&a;&a;&a;&a;&a;'>"
        "<!ENTITY c '&b;&b;&b;&b;&b;&b;&b;&b;&b;&b;'><!ENTITY d '&c;&c;&c;&c;&c;&c;&c;&c;&c;&c;'>]><a>&d;</a>");
    CHECK(malformed(bomb)); // 10000 characters from one reference
    {
        QXmlStreamReader xml;
        CHECK(xml.entityExpansionLimit() == 4096);
        xml.setEntityExpansionLimit(20000);
        CHECK(xml.entityExpansionLimit() == 20000);
        xml.addData(bomb);
        xml.readNextStartElement();
        CHECK(xml.readElementText().size() == 10000 && !xml.hasError());
    }
    {
        // The limit is for each reference, not for the document
        QByteArray many("<!DOCTYPE a [<!ENTITY k '" + QByteArray(1000, 'x') + "'>]><a>");
        for (int i = 0; i < 50; ++i)
            many += "&k; ";
        many += "</a>";
        QXmlStreamReader xml(many);
        xml.readNextStartElement();
        CHECK(xml.readElementText().size() == 50 * 1001 && !xml.hasError());
    }

    {
        // The resolver stands in for a document type that is not there
        Resolver resolver;
        QXmlStreamReader xml(QByteArray("<a>x&nbsp;y &markup; &other;</a>"));
        CHECK(xml.entityResolver() == nullptr);
        xml.setEntityResolver(&resolver);
        CHECK(xml.entityResolver() == &resolver);
        CHECK(dump(xml) == QString::fromUtf8("SD(,)<a>T[x\xc2\xa0y ]<i>T[made]</i>WS[ ]!ERROR"));
        CHECK(xml.errorString() == "Entity 'other' not declared." && resolver.asked.contains("other"));
        CHECK(resolver.resolveEntity("p", "s").isEmpty());
        QXmlStreamEntityResolver plain;
        CHECK(plain.resolveUndeclaredEntity("x").isEmpty() && plain.resolveEntity("p", "s").isEmpty());
    }
}

// QXmlStreamWriter

static void testWriter() {
    {
        QString out;
        QXmlStreamWriter xml(&out);
        CHECK(!xml.autoFormatting() && xml.autoFormattingIndent() == 4 && xml.device() == nullptr && !xml.hasError());
        xml.writeStartDocument();
        xml.writeStartElement("root");
        xml.writeAttribute("k", "a<b>&\"c'\n\t");
        xml.writeCharacters("x < y & \"z\" > w\n\t'");
        xml.writeEmptyElement("empty");
        xml.writeAttribute("e", "1");
        xml.writeTextElement("t", "text");
        xml.writeStartElement("open");
        xml.writeEndElement();
        xml.writeComment(" note ");
        xml.writeProcessingInstruction("target", "some data");
        xml.writeProcessingInstruction("bare");
        xml.writeCDATA("raw <&> ]]> more");
        xml.writeEntityReference("ent");
        xml.writeEndDocument();
        CHECK(out == "<?xml version=\"1.0\"?><root k=\"a&lt;b&gt;&amp;&quot;c'&#10;&#9;\">x &lt; y &amp; &quot;z&quot; "
                     "&gt; w\n\t'"
                     "<empty e=\"1\"/><t>text</t><open/><!-- note --><?target some data?><?bare?>"
                     "<![CDATA[raw <&> ]]]]><![CDATA[> more]]>&ent;</root>");
        CHECK(!xml.hasError());
    }
    {
        // Bytes are UTF-8, and say so
        QByteArray out;
        QXmlStreamWriter xml(&out);
        xml.writeStartDocument("1.0", true);
        xml.writeTextElement("a", QString::fromUtf8("\xc3\xa4\xe2\x82\xac"));
        xml.writeEndDocument();
        CHECK(out == "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><a>\xc3\xa4\xe2\x82\xac</a>");
        QByteArray no;
        QXmlStreamWriter other(&no);
        other.writeStartDocument("1.0", false);
        other.writeDTD("<!DOCTYPE a>");
        other.writeEmptyElement("a");
        other.writeEndDocument();
        CHECK(no == "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"no\"?><!DOCTYPE a><a/>");
        QByteArray versioned;
        QXmlStreamWriter third(&versioned);
        third.writeStartDocument("1.1");
        CHECK(versioned == "<?xml version=\"1.1\" encoding=\"UTF-8\"?>");
    }
    {
        // A device, and no device
        QBuffer buffer;
        CHECK(buffer.open(QIODevice::WriteOnly));
        QXmlStreamWriter xml(&buffer);
        CHECK(xml.device() == &buffer);
        xml.writeStartElement("a");
        xml.writeCharacters("b");
        xml.writeEndElement();
        CHECK(buffer.data() == "<a>b</a>" && !xml.hasError());
        QBuffer closed;
        xml.setDevice(&closed);
        CHECK(xml.device() == &closed);
        xml.writeEmptyElement("c");
        xml.writeEndDocument();
        CHECK(xml.hasError() && closed.data().isEmpty());
        QXmlStreamWriter nowhere;
        CHECK(!nowhere.hasError());
        nowhere.writeEmptyElement("a");
        nowhere.writeEndDocument();
        CHECK(nowhere.hasError());
    }
    {
        // Auto formatting: a line for each element, but none inside one that has text
        QString out;
        QXmlStreamWriter xml(&out);
        xml.setAutoFormatting(true);
        CHECK(xml.autoFormatting());
        xml.writeStartDocument();
        xml.writeComment("top");
        xml.writeStartElement("root");
        xml.writeStartElement("group");
        xml.writeAttribute("id", "1");
        xml.writeTextElement("name", "first");
        xml.writeEmptyElement("flag");
        xml.writeStartElement("mixed");
        xml.writeCharacters("a ");
        xml.writeTextElement("b", "bold");
        xml.writeCharacters(" c");
        xml.writeEndElement();
        xml.writeStartElement("none");
        xml.writeEndElement();
        xml.writeEndElement();
        xml.writeProcessingInstruction("pi");
        xml.writeEndDocument();
        CHECK(out == "<?xml version=\"1.0\"?>\n"
                     "<!--top-->\n"
                     "<root>\n"
                     "    <group id=\"1\">\n"
                     "        <name>first</name>\n"
                     "        <flag/>\n"
                     "        <mixed>a <b>bold</b> c</mixed>\n"
                     "        <none/>\n"
                     "    </group>\n"
                     "    <?pi?>\n"
                     "</root>\n");
        if (out.size() < 400)
            CHECK(dump(out.toUtf8()).contains("<mixed>T[a ]<b>T[bold]</b>T[ c]</mixed>"));

        QString tabs;
        QXmlStreamWriter tabbed(&tabs);
        tabbed.setAutoFormatting(true);
        tabbed.setAutoFormattingIndent(-1);
        CHECK(tabbed.autoFormattingIndent() == -1);
        tabbed.writeStartElement("a");
        tabbed.writeStartElement("b");
        tabbed.writeEmptyElement("c");
        tabbed.writeEndDocument();
        CHECK(tabs == "<a>\n\t<b>\n\t\t<c/>\n\t</b>\n</a>\n");
        QString two;
        QXmlStreamWriter narrow(&two);
        narrow.setAutoFormatting(true);
        narrow.setAutoFormattingIndent(2);
        narrow.writeStartElement("a");
        narrow.writeEmptyElement("b");
        narrow.writeEndDocument();
        CHECK(two == "<a>\n  <b/>\n</a>\n");
    }
    {
        // Namespaces: declared by the program, or made up by the writer
        QString out;
        QXmlStreamWriter xml(&out);
        xml.writeStartElement("root");
        xml.writeDefaultNamespace("urn:default");
        xml.writeNamespace("urn:p", "p");
        xml.writeStartElement("urn:p", "child");
        xml.writeAttribute("urn:p", "attr", "1");
        xml.writeAttribute("urn:default", "other", "2"); // an attribute needs a prefix, which then is the latest
        xml.writeEmptyElement("urn:default", "plain");
        xml.writeTextElement("urn:new", "made", "t");
        xml.writeEmptyElement("urn:new", "again");
        xml.writeEndElement();
        xml.writeEmptyElement("urn:p", "after");
        xml.writeEndDocument();
        CHECK(out == "<root xmlns=\"urn:default\" xmlns:p=\"urn:p\"><p:child p:attr=\"1\" xmlns:n1=\"urn:default\" "
                     "n1:other=\"2\">"
                     "<n1:plain/><n2:made xmlns:n2=\"urn:new\">t</n2:made><n3:again xmlns:n3=\"urn:new\"/></p:child>"
                     "<p:after/></root>");
        // What the writer wrote reads back with the same names
        CHECK(dump(out.toUtf8()) == "SD(,)<root{urn:default}><p:child{urn:p} p:attr{urn:p}=[1] "
                                    "n1:other{urn:default}=[2]><n1:plain{urn:default}></n1:plain>"
                                    "<n2:made{urn:new}>T[t]</n2:made><n3:again{urn:new}></n3:again></"
                                    "p:child><p:after{urn:p}></p:after></root>ED");
    }
    {
        // A namespace that is declared before the element goes into the element
        QString out;
        QXmlStreamWriter xml(&out);
        xml.writeNamespace("urn:a", "a");
        xml.writeNamespace("urn:made");
        xml.writeStartElement("urn:a", "root");
        xml.writeEmptyElement("urn:made", "child");
        xml.writeEndDocument();
        CHECK(out == "<a:root xmlns:a=\"urn:a\" xmlns:n1=\"urn:made\"><n1:child/></a:root>");
    }
    {
        QString out;
        QXmlStreamWriter xml(&out);
        xml.writeStartElement("a");
        QXmlStreamAttributes attributes;
        attributes.append("plain", "1");
        attributes.append("urn:n", "spaced", "2");
        xml.writeAttributes(attributes);
        xml.writeAttribute(QXmlStreamAttribute("last", "3"));
        xml.writeAttribute("", "nons", "4");
        xml.writeEndElement();
        xml.writeAttribute("late", "ignored"); // no start element to put it in
        CHECK(out == "<a plain=\"1\" xmlns:n1=\"urn:n\" n1:spaced=\"2\" last=\"3\" nons=\"4\"/>");
        xml.writeEndElement(); // nothing is open
        CHECK(out.endsWith("/>"));
    }
}

static void testCopy() {
    // Reading a document and writing each token gives the document back
    const QByteArray document(
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><!DOCTYPE root [<!ENTITY ext SYSTEM \"e.xml\">]>"
        "<root xmlns=\"urn:d\" xmlns:p=\"urn:p\" a=\"1 &lt; 2\"><!--c--><p:one p:k=\"v\">text &amp; more</p:one>"
        "<two><![CDATA[<raw>]]></two><?pi d?>&ext;<three xmlns=\"urn:other\"><four/></three><five/></root>");
    QByteArray copy;
    {
        QXmlStreamReader reader(document);
        QXmlStreamWriter writer(&copy);
        while (!reader.atEnd()) {
            reader.readNext();
            writer.writeCurrentToken(reader);
        }
        CHECK(!reader.hasError() && !writer.hasError());
    }
    CHECK(copy == document);
    if (copy != document)
        std::printf("  %s\n", copy.constData());
    CHECK(dump(copy) == dump(document));

    // Without a declaration none is written; a string gets no encoding
    QString text;
    QXmlStreamReader reader(QByteArray("<a><b>x</b></a>"));
    QXmlStreamWriter writer(&text);
    while (!reader.atEnd()) {
        reader.readNext();
        writer.writeCurrentToken(reader);
    }
    CHECK(text == "<a><b>x</b></a>");

    // Whatever the writer is given comes back from the reader
    const QString awkward = QString::fromUtf8("<&>\"' \t\n ]]> \xc3\xa4\xf0\x9f\x98\x80 &amp; &#65; <!-- -->");
    QByteArray bytes;
    {
        QXmlStreamWriter xml(&bytes);
        xml.writeStartDocument();
        xml.writeStartElement("a");
        xml.writeAttribute("k", awkward);
        xml.writeTextElement("t", awkward);
        xml.writeStartElement("c");
        xml.writeCDATA(awkward);
        xml.writeEndDocument();
    }
    QXmlStreamReader back(bytes);
    CHECK(back.readNextStartElement() && back.attributes().value("k") == QStringView(awkward));
    CHECK(back.readNextStartElement() && back.readElementText() == awkward);
    CHECK(back.readNextStartElement() && back.readElementText() == awkward);
    CHECK(!back.hasError());
}

static void testWriterErrors() {
    // A document without errors.
    QString out;
    QXmlStreamWriter ok(&out);
    ok.writeTextElement(QStringLiteral("a"), QStringLiteral("tab\tand\nnewline"));
    CHECK(!ok.hasError() && ok.error() == QXmlStreamWriter::Error::None && ok.errorString().isEmpty());
    CHECK(!ok.stopWritingOnError());

    // Characters that XML does not allow, and lone surrogates.
    QString bad;
    QXmlStreamWriter control(&bad);
    control.writeTextElement(QStringLiteral("a"), QString(QChar(0x1)));
    CHECK(control.hasError() && control.error() == QXmlStreamWriter::Error::InvalidCharacter);
    CHECK(control.errorString().contains(QStringLiteral("U+0001")));
    control.writeTextElement(QStringLiteral("b"), QString(QChar(0xd800))); // the first error is kept
    CHECK(control.error() == QXmlStreamWriter::Error::InvalidCharacter);
    CHECK(bad.contains(QStringLiteral("<b>"))); // it goes on writing by default

    QString surrogate;
    QXmlStreamWriter encoding(&surrogate);
    encoding.writeCharacters(QString(QChar(0xdc00)));
    CHECK(encoding.error() == QXmlStreamWriter::Error::Encoding);
    QString pair;
    QXmlStreamWriter fine(&pair);
    fine.writeCharacters(QString::fromUtf8("\xf0\x9f\x98\x80")); // a surrogate pair is fine
    CHECK(!fine.hasError());

    // Stopping at the first error.
    QString stopped;
    QXmlStreamWriter stopping(&stopped);
    stopping.setStopWritingOnError(true);
    CHECK(stopping.stopWritingOnError());
    stopping.writeStartElement(QStringLiteral("root"));
    stopping.writeCharacters(QString(QChar(0xffff)));
    stopping.writeEndElement();
    CHECK(stopping.error() == QXmlStreamWriter::Error::InvalidCharacter && stopped == QStringLiteral("<root>"));

    // Custom errors, and writing to nothing.
    QString custom;
    QXmlStreamWriter raising(&custom);
    raising.raiseError(QStringLiteral("not allowed here"));
    CHECK(raising.error() == QXmlStreamWriter::Error::Custom &&
          raising.errorString() == QStringLiteral("not allowed here"));
    QXmlStreamWriter nowhere;
    nowhere.writeStartElement(QStringLiteral("x"));
    CHECK(nowhere.error() == QXmlStreamWriter::Error::IO);
    QBuffer closed;
    QXmlStreamWriter unopened(&closed);
    unopened.writeTextElement(QStringLiteral("x"), QStringLiteral("y"));
    CHECK(unopened.error() == QXmlStreamWriter::Error::IO && !unopened.errorString().isEmpty());
}

static void testRawInnerData() {
    QXmlStreamReader xml(QStringLiteral("<doc><note a='1'>Call <b>now</b>! &amp; <!--c--><i/></note><after/></doc>"));
    CHECK(xml.readNextStartElement() && xml.name() == QStringLiteral("doc"));
    CHECK(xml.readNextStartElement() && xml.name() == QStringLiteral("note"));
    CHECK(xml.readRawInnerData() == QStringLiteral("Call <b>now</b>! &amp; <!--c--><i/>"));
    CHECK(xml.tokenType() == QXmlStreamReader::EndElement && xml.name() == QStringLiteral("note"));
    CHECK(xml.readNextStartElement() && xml.name() == QStringLiteral("after"));
    CHECK(xml.readRawInnerData().isEmpty() && !xml.hasError());
    // Not at a start element: an error.
    QXmlStreamReader text(QStringLiteral("<a>x</a>"));
    text.readNext();
    CHECK(text.readRawInnerData().isEmpty() && text.hasError());
}

int main() {
    testTokens();
    testNamespaces();
    testAttributeValue();
    testErrors();
    testPositions();
    testDocumentStart();
    testEncodings();
    testIncremental();
    testDevice();
    testHelpers();
    testDtd();
    testWriter();
    testCopy();
    testWriterErrors();
    testRawInnerData();

    if (g_failures) {
        std::printf("%d XML stream test check(s) failed\n", g_failures);
        return 1;
    }
    std::puts("all XML stream tests passed");
    return 0;
}
