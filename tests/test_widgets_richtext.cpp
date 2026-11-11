// MIT License - see LICENSE
//
// Tests for rich text in Nt Widgets on the offscreen platform: QLabel with HTML and links, QTextEdit with
// setHtml() and toHtml(), and the links of QTextBrowser. The tests play the window system through
// QWindowSystemInterface.
#include <QtCore>
#include <QtGui/qevent.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtGui/qtextdocument.h>
#include <QtGui/qwindow.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qscrollbar.h>
#include <QtWidgets/qtextbrowser.h>
#include <QtWidgets/qtextedit.h>

#include "platforms/offscreen/qoffscreenintegration_p.h"
#include "qpa/qwindowsysteminterface_p.h"

#include <cstdio>

static int g_failures = 0;
#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                                                \
            ++g_failures;                                                                                              \
        }                                                                                                              \
    } while (0)

namespace {

void settle(QWidget *w) {
    for (int i = 0; i < 3; ++i) {
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QCoreApplication::sendPostedEvents(w);
        if (w->windowHandle())
            QCoreApplication::sendPostedEvents(w->windowHandle());
    }
}

QImage grab(QWidget *w) {
    settle(w->window());
    return QOffscreenBackingStore::imageOf(w->window()->windowHandle());
}

Qt::CursorShape shown(QWidget *w) { return QOffscreenWindow::cursorOf(w->window()->windowHandle()); }

void mouse(QWidget *w, QEvent::Type type, QPoint local, Qt::MouseButton button = Qt::LeftButton,
           Qt::MouseButtons buttons = Qt::LeftButton) {
    QWindow *win = w->window()->windowHandle();
    const QPoint inWindow = w->mapTo(w->window(), local);
    QWindowSystemInterface::handleMouseEvent(win, QPointF(inWindow), QPointF(inWindow + w->window()->pos()), buttons,
                                             button, type, Qt::NoModifier);
}

void move(QWidget *w, QPoint local) { mouse(w, QEvent::MouseMove, local, Qt::NoButton, Qt::NoButton); }

void click(QWidget *w, QPoint local) {
    mouse(w, QEvent::MouseButtonPress, local, Qt::LeftButton, Qt::LeftButton);
    mouse(w, QEvent::MouseButtonRelease, local, Qt::LeftButton, Qt::NoButton);
}

bool isBlue(QRgb p) { return qBlue(p) > 150 && qRed(p) < 100 && qGreen(p) < 100; }
bool isRed(QRgb p) { return qRed(p) > 150 && qBlue(p) < 100 && qGreen(p) < 100; }

int count(const QImage &image, bool (*test)(QRgb)) {
    int n = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (test(image.pixel(x, y)))
                ++n;
        }
    }
    return n;
}

// The middle of the blue pixels of the image: a point of the link that is drawn there, with its underline.
QPoint linkPoint(const QImage &image) {
    qint64 sx = 0, sy = 0, n = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (isBlue(image.pixel(x, y))) {
                sx += x;
                sy += y;
                ++n;
            }
        }
    }
    return n > 0 ? QPoint(static_cast<int>(sx / n), static_cast<int>(sy / n)) : QPoint(-1, -1);
}

// Whether text is drawn at all: without a font (a bare container, say) the glyphs are not, while underlines and
// backgrounds still are, so the checks of the ink of letters are left out then.
bool fontsDraw() {
    static const bool draws = [] {
        QImage image(60, 40, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.drawText(QPoint(2, 28), QStringLiteral("MWQ"));
        painter.end();
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                if ((image.pixel(x, y) & 0x00ffffffU) != 0x00ffffffU)
                    return true;
            }
        }
        return false;
    }();
    return draws;
}

void whiten(QWidget *w) {
    QPalette p = w->palette();
    p.setColor(QPalette::Window, Qt::white);
    p.setColor(QPalette::Base, Qt::white);
    p.setColor(QPalette::WindowText, Qt::black);
    p.setColor(QPalette::Text, Qt::black);
    w->setPalette(p);
}

void testLabel() {
    QLabel plain(QStringLiteral("a < b"));
    CHECK(plain.textFormat() == Qt::AutoText);
    CHECK(!plain.openExternalLinks());
    const QSize plainHint = plain.sizeHint();
    CHECK(plainHint.width() == plain.fontMetrics().horizontalAdvance(QStringLiteral("a < b")));

    // Rich text is as wide as what it shows, not as its source.
    QLabel rich(QStringLiteral("<b>a</b>"));
    CHECK(rich.text() == QStringLiteral("<b>a</b>"));
    CHECK(rich.sizeHint().width() < rich.fontMetrics().horizontalAdvance(QStringLiteral("<b>a</b>")));
    CHECK(rich.sizeHint().height() >= rich.fontMetrics().height());
    rich.setTextFormat(Qt::PlainText);
    CHECK(rich.textFormat() == Qt::PlainText);
    CHECK(rich.sizeHint().width() == rich.fontMetrics().horizontalAdvance(QStringLiteral("<b>a</b>")));
    rich.setTextFormat(Qt::RichText);
    rich.setText(QStringLiteral("one<br>two<br>three"));
    CHECK(rich.sizeHint().height() >= 3 * rich.fontMetrics().height());
    CHECK(rich.setProperty("openExternalLinks", true) && rich.openExternalLinks());

    QWidget top;
    whiten(&top);
    top.resize(320, 120);
    QLabel label(&top);
    label.setGeometry(10, 10, 300, 100);
    label.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label.setText(QStringLiteral("Plain <font color=\"red\">red</font> and a <a href=\"doc/page.html\">link</a> here"));
    QStringList activated, hovered;
    QObject::connect(&label, &QLabel::linkActivated, [&](const QString &link) { activated << link; });
    QObject::connect(&label, &QLabel::linkHovered, [&](const QString &link) { hovered << link; });
    top.show();
    settle(&top);
    QImage image = grab(&top);
    CHECK(!fontsDraw() || count(image, isRed) > 5);
    CHECK(count(image, isBlue) > 5);
    const QPoint link = linkPoint(image) - label.pos();
    CHECK(label.rect().contains(link));
    CHECK(link.y() < 30); // at the top

    CHECK(label.hasMouseTracking());
    move(&label, QPoint(2, 5));
    CHECK(hovered.isEmpty());
    CHECK(shown(&label) == Qt::ArrowCursor);
    move(&label, link);
    CHECK(hovered == QStringList{QStringLiteral("doc/page.html")});
    CHECK(shown(&label) == Qt::PointingHandCursor);
    move(&label, link + QPoint(1, 0));
    CHECK(hovered.size() == 1);
    click(&label, link);
    CHECK(activated == QStringList{QStringLiteral("doc/page.html")});
    move(&label, QPoint(2, 5));
    CHECK(hovered.size() == 2 && hovered.last().isEmpty());
    CHECK(shown(&label) == Qt::ArrowCursor);
    click(&label, QPoint(2, 5));
    CHECK(activated.size() == 1);
    // A press on the link and a release beside it is no click.
    mouse(&label, QEvent::MouseButtonPress, link);
    mouse(&label, QEvent::MouseButtonRelease, QPoint(2, 80), Qt::LeftButton, Qt::NoButton);
    CHECK(activated.size() == 1);

    // The alignment of the label places the rich text.
    label.setAlignment(Qt::AlignRight | Qt::AlignBottom);
    image = grab(&top);
    const QPoint moved = linkPoint(image) - label.pos();
    CHECK(moved.x() > link.x() + 20 && moved.y() > link.y() + 40);
    click(&label, link);
    CHECK(activated.size() == 1);
    click(&label, moved);
    CHECK(activated.size() == 2);

    // Word wrap breaks the rich text at the width of the label.
    label.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    label.setText(QStringLiteral("<font color=\"red\">word word word word word word word word word word word word word "
                                 "word word word word word word word word word word word</font>"));
    image = grab(&top);
    int lowest = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (isRed(image.pixel(x, y)))
                lowest = y;
        }
    }
    label.setWordWrap(true);
    image = grab(&top);
    int wrappedLowest = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (isRed(image.pixel(x, y)))
                wrappedLowest = y;
        }
    }
    CHECK(!fontsDraw() || wrappedLowest > lowest + 8);

    // Plain text again: no document, no links.
    label.setText(QStringLiteral("just text"));
    image = grab(&top);
    CHECK(count(image, isRed) == 0 && count(image, isBlue) == 0);
    label.setTextFormat(Qt::PlainText);
    label.setText(QStringLiteral("<a href=\"x\">shown as it is</a>"));
    image = grab(&top);
    CHECK(count(image, isBlue) == 0);
    click(&label, QPoint(40, 8));
    CHECK(activated.size() == 2);
    label.clear();
    CHECK(label.text().isEmpty());
}

void testTextEdit() {
    QTextEdit edit;
    whiten(&edit);
    edit.resize(300, 200);
    CHECK(edit.document() != nullptr);
    CHECK(edit.toPlainText().isEmpty());
    int changes = 0;
    QObject::connect(&edit, &QTextEdit::textChanged, [&changes] { ++changes; });

    edit.setHtml(QStringLiteral("<title>Doc</title><h1>Title</h1><p>Some <font color=\"red\">red</font> text with a "
                                "<a href=\"target.html\">link</a>.</p>"));
    CHECK(changes == 1);
    CHECK(edit.toPlainText() == QStringLiteral("Title\nSome red text with a link."));
    CHECK(edit.documentTitle() == QStringLiteral("Doc"));
    CHECK(edit.document()->blockCount() == 2);
    CHECK(edit.document()->firstBlock().blockFormat().headingLevel() == 1);
    const QString html = edit.toHtml();
    CHECK(html.contains(QStringLiteral("<h1")) && html.contains(QStringLiteral("href=\"target.html\"")));
    CHECK(edit.property("html").toString() == html);

    edit.show();
    settle(&edit);
    QImage image = grab(&edit);
    CHECK(!fontsDraw() || count(image, isRed) > 5);
    CHECK(count(image, isBlue) > 5);
    const QPoint link = edit.viewport()->mapFrom(&edit, linkPoint(image));
    CHECK(edit.anchorAt(link) == QStringLiteral("target.html"));
    CHECK(edit.anchorAt(QPoint(280, 180)).isEmpty());
    CHECK(edit.document()->textWidth() == edit.viewport()->width());

    // A long document scrolls.
    QString longHtml;
    for (int i = 0; i < 60; ++i)
        longHtml += QStringLiteral("<p>Paragraph %1 <a href=\"p%1\">go</a></p>").arg(i);
    edit.setHtml(longHtml);
    settle(&edit);
    image = grab(&edit);
    CHECK(edit.verticalScrollBar()->maximum() > 500);
    CHECK(edit.verticalScrollBar()->maximum() ==
          static_cast<int>(edit.document()->size().height()) - edit.viewport()->height());
    const QPoint first = edit.viewport()->mapFrom(&edit, linkPoint(image));
    edit.verticalScrollBar()->setValue(edit.verticalScrollBar()->maximum());
    image = grab(&edit);
    CHECK(count(image, isBlue) > 5);
    QString last;
    for (int y = edit.viewport()->height() - 1; y > 0 && last.isEmpty(); --y) {
        for (int x = 0; x < edit.viewport()->width() && last.isEmpty(); ++x)
            last = edit.anchorAt(QPoint(x, y));
    }
    CHECK(last == QStringLiteral("p59"));
    CHECK(!edit.anchorAt(first).startsWith(QStringLiteral("p0")) || edit.anchorAt(first) != QStringLiteral("p0"));

    // A narrower editor wraps the text again.
    edit.setHtml(QStringLiteral("<p>word word word word word word word word word word word word word word word word</p>"));
    settle(&edit);
    const qreal wide = edit.document()->size().height();
    edit.resize(120, 200);
    settle(&edit);
    static_cast<void>(grab(&edit));
    CHECK(edit.document()->size().height() > wide);
    edit.resize(300, 200);
    settle(&edit);

    // The document can be given other contents, and the editor shows them.
    changes = 0;
    edit.document()->setHtml(QStringLiteral("<p><font color=\"red\">fresh</font></p>"));
    CHECK(edit.toPlainText() == QStringLiteral("fresh"));
    CHECK(changes == 1);
    CHECK(!fontsDraw() || count(grab(&edit), isRed) > 5);

    // Changing the text in another way leaves plain text.
    edit.append(QStringLiteral("more"));
    CHECK(edit.toPlainText() == QStringLiteral("fresh\nmore"));
    CHECK(count(grab(&edit), isRed) == 0);
    CHECK(edit.document()->toPlainText() == QStringLiteral("fresh\nmore"));
    CHECK(edit.document()->firstBlock().textFormats().isEmpty());
    CHECK(edit.toHtml().contains(QStringLiteral("<p>fresh</p>")));
    CHECK(edit.anchorAt(QPoint(10, 10)).isEmpty());

    // setText() takes both kinds of text.
    edit.setText(QStringLiteral("a < b"));
    CHECK(edit.toPlainText() == QStringLiteral("a < b"));
    edit.setText(QStringLiteral("<b>bold</b> text"));
    CHECK(edit.toPlainText() == QStringLiteral("bold text"));
    CHECK(edit.document()->firstBlock().textFormats().size() == 1);
    edit.setPlainText(QStringLiteral("<b>not bold</b>"));
    CHECK(edit.toPlainText() == QStringLiteral("<b>not bold</b>"));
    CHECK(edit.toHtml().contains(QStringLiteral("&lt;b&gt;not bold&lt;/b&gt;")));
    edit.setDocumentTitle(QStringLiteral("Named"));
    CHECK(edit.documentTitle() == QStringLiteral("Named"));

    QTextEdit with(QStringLiteral("<i>x</i>"));
    CHECK(with.toPlainText() == QStringLiteral("x"));
    CHECK(with.acceptRichText());
}

void testTextBrowser() {
    QTemporaryDir temp;
    const QString root = temp.path();
    const auto write = [&](const QString &name, const char *text) {
        QFile file(root + QChar('/') + name);
        CHECK(file.open(QIODevice::WriteOnly));
        file.write(text);
    };
    write(QStringLiteral("index.html"),
          "<html><head><title>Index</title></head><body><h2>Help</h2><p>Go to the <a href=\"page.html\">page</a>.</p>"
          "</body></html>");
    write(QStringLiteral("page.html"), "<p>The page, with <a href=\"http://example.com/\">a site</a>.</p>");
    write(QStringLiteral("notes.txt"), "<not html> at all");

    QTextBrowser browser;
    whiten(&browser);
    browser.resize(320, 200);
    browser.show();
    settle(&browser);
    QList<QUrl> clicked, highlighted;
    QObject::connect(&browser, &QTextBrowser::anchorClicked, [&](const QUrl &url) { clicked << url; });
    QObject::connect(&browser, &QTextBrowser::highlighted, [&](const QUrl &url) { highlighted << url; });

    browser.setSource(QUrl::fromLocalFile(root + QStringLiteral("/index.html")));
    CHECK(browser.toPlainText() == QStringLiteral("Help\nGo to the page."));
    CHECK(browser.documentTitle() == QStringLiteral("Index"));
    QImage image = grab(&browser);
    QWidget *viewport = browser.viewport();
    QPoint link = viewport->mapFrom(&browser, linkPoint(image));
    CHECK(browser.anchorAt(link) == QStringLiteral("page.html"));

    move(viewport, QPoint(250, 150));
    CHECK(highlighted.isEmpty());
    move(viewport, link);
    CHECK(highlighted == QList<QUrl>{QUrl(QStringLiteral("page.html"))});
    CHECK(shown(&browser) == Qt::PointingHandCursor);
    move(viewport, QPoint(250, 150));
    CHECK(highlighted.size() == 2 && highlighted.last().isEmpty());
    CHECK(shown(&browser) != Qt::PointingHandCursor);

    // A click follows the link, and the history knows.
    click(viewport, link);
    CHECK(clicked == QList<QUrl>{QUrl(QStringLiteral("page.html"))});
    CHECK(browser.toPlainText() == QStringLiteral("The page, with a site."));
    CHECK(browser.source() == QUrl::fromLocalFile(root + QStringLiteral("/page.html")));
    CHECK(browser.isBackwardAvailable());

    // A link to a site is told about, and not followed.
    image = grab(&browser);
    link = viewport->mapFrom(&browser, linkPoint(image));
    CHECK(browser.anchorAt(link) == QStringLiteral("http://example.com/"));
    click(viewport, link);
    CHECK(clicked.size() == 2 && clicked.last() == QUrl(QStringLiteral("http://example.com/")));
    CHECK(browser.toPlainText() == QStringLiteral("The page, with a site."));

    browser.backward();
    CHECK(browser.toPlainText() == QStringLiteral("Help\nGo to the page."));
    image = grab(&browser);
    link = viewport->mapFrom(&browser, linkPoint(image));

    // Without openLinks() the click is only told about.
    browser.setOpenLinks(false);
    click(viewport, link);
    CHECK(clicked.size() == 3);
    CHECK(browser.toPlainText() == QStringLiteral("Help\nGo to the page."));
    browser.setOpenLinks(true);

    // A file that is not HTML is shown as it is.
    browser.setSource(QUrl(QStringLiteral("notes.txt")));
    CHECK(browser.toPlainText() == QStringLiteral("<not html> at all"));
    CHECK(count(grab(&browser), isBlue) == 0);
}

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    testLabel();
    testTextEdit();
    testTextBrowser();
    if (g_failures == 0)
        std::printf("all widget rich text tests passed\n");
    return g_failures == 0 ? 0 : 1;
}
