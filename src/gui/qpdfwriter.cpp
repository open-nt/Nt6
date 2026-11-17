// MIT License - see LICENSE
#include <QtGui/qpdfwriter.h>

#include "qpdfengine_p.h"

#include <QtCore/qdebug.h>

struct QPdfWriter::Data {
    QtPrivate::PdfSettings settings;
    std::unique_ptr<QtPrivate::PdfEngine> engine;
};

/*!
    \class QPdfWriter
    \inmodule NtGui
    \since 6.0
    \brief The QPdfWriter class is a class to generate PDFs that can be used as a paint device.

    Paint on a QPdfWriter as on any other device, call newPage() between
    pages, and the document is written when the painter ends. Coordinates
    are pixels at resolution(), from the top left of the part of the page
    inside the margins; width() and height() give its size:

    \code
    QPdfWriter writer("report.pdf");
    writer.setPageLayout(QPageLayout(QPageSize(QPageSize::A4), QPageLayout::Portrait,
                                     QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter));
    writer.setResolution(300);
    writer.setTitle("Report");

    QPainter painter(&writer);
    painter.setFont(QFont("Helvetica", 18, QFont::Bold));
    painter.drawText(QRect(0, 0, writer.width(), 200), Qt::AlignCenter, "Page one");
    writer.newPage();
    painter.drawRect(0, 0, writer.width() / 2, writer.height() / 2);
    \endcode

    Shapes, paths and pens are written as PDF paths, linear and radial
    gradients as PDF shadings, and images as compressed images with their
    alpha channel. Text is written as the outlines of its glyphs, so it
    stays sharp at any zoom and needs no fonts on the reader's side. Fonts
    have their point size at resolution(): a 12 point font is 50 pixels high
    at 300 dpi, as in Qt.

    \differences
    Text is drawn as glyph outlines, not as PDF text with an embedded font,
    so it cannot be selected, searched or copied. Text is laid out upright at
    the size of the font, as Nt's QPainter does elsewhere: a rotating or
    scaling transformation moves it but does not turn or scale it.
    Reflected and repeated gradients are padded, conical gradients and
    gradient stops with alpha are filled opaquely with their colors, and the
    composition modes other than SourceOver are ignored. PdfVersion_A1b
    writes plain PDF 1.4 rather than conforming PDF/A. There is no
    \c setDocumentXmpMetadata(), \c addFileAttachment(), \c setColorModel() or
    page ranges. Changing the page layout while painting takes effect on the
    next page, as in Qt.
    \enddifferences

    \sa QPagedPaintDevice, QPageLayout, QPainter
*/

/*!
    Constructs a writer that writes the PDF to the file \a filename when
    painting ends.
*/
QPdfWriter::QPdfWriter(const QString &filename) : m_d(std::make_unique<Data>()) {
    m_d->settings.fileName = filename;
    m_d->engine = std::make_unique<QtPrivate::PdfEngine>(&m_d->settings, this);
}

/*!
    Constructs a writer that writes the PDF to \a device when painting ends.
    The device is opened for writing if it is not open.
*/
QPdfWriter::QPdfWriter(QIODevice *device) : m_d(std::make_unique<Data>()) {
    m_d->settings.device = device;
    m_d->engine = std::make_unique<QtPrivate::PdfEngine>(&m_d->settings, this);
}

/*!
    Destroys the writer.
*/
QPdfWriter::~QPdfWriter() = default;

/*!
    Sets the version of PDF to write to \a version.
*/
void QPdfWriter::setPdfVersion(QPagedPaintDevice::PdfVersion version) { m_d->settings.version = version; }

/*!
    Returns the version of PDF that is written. The default is PdfVersion_1_4.
*/
QPagedPaintDevice::PdfVersion QPdfWriter::pdfVersion() const { return m_d->settings.version; }

/*!
    Returns the title of the document.
*/
QString QPdfWriter::title() const { return m_d->settings.title; }

/*!
    Sets the title of the document to \a title.
*/
void QPdfWriter::setTitle(const QString &title) { m_d->settings.title = title; }

/*!
    Returns the creator of the document.
*/
QString QPdfWriter::creator() const { return m_d->settings.creator; }

/*!
    Sets the creator of the document, the application that made it, to
    \a creator.
*/
void QPdfWriter::setCreator(const QString &creator) { m_d->settings.creator = creator; }

/*!
    Ends the current page and starts a new one with the current page layout.
    Returns \c false if no painter is active on the writer.
*/
bool QPdfWriter::newPage() { return m_d->engine->newPage(); }

/*!
    Sets the resolution, in dots per inch, that painter coordinates have on
    the page to \a resolution. Set it before painting begins.
*/
void QPdfWriter::setResolution(int resolution) {
    if (resolution <= 0) {
        qWarning("QPdfWriter::setResolution(): the resolution must be positive");
        return;
    }
    m_d->settings.resolution = resolution;
}

/*!
    Returns the resolution in dots per inch. The default is 1200.
*/
int QPdfWriter::resolution() const { return m_d->settings.resolution; }

/*!
    \reimp
*/
QPaintEngine *QPdfWriter::paintEngine() const { return m_d->engine.get(); }

/*!
    \reimp

    Reports the paint rectangle of the page layout in pixels at the
    resolution, the resolution as the logical and physical dots per inch, and
    the size of the paint rectangle in millimeters, for \a id.
*/
int QPdfWriter::metric(PaintDeviceMetric id) const {
    const int res = m_d->settings.resolution;
    const QPageLayout layout = pageLayout();
    switch (id) {
    case PdmWidth:
        return layout.paintRectPixels(res).width();
    case PdmHeight:
        return layout.paintRectPixels(res).height();
    case PdmWidthMM:
        return qRound(layout.paintRect(QPageLayout::Millimeter).width());
    case PdmHeightMM:
        return qRound(layout.paintRect(QPageLayout::Millimeter).height());
    case PdmDpiX:
    case PdmDpiY:
    case PdmPhysicalDpiX:
    case PdmPhysicalDpiY:
        return res;
    case PdmDepth:
        return 32;
    case PdmDevicePixelRatio:
        return 1;
    case PdmDevicePixelRatioScaled:
        return 65536;
    default:
        return QPaintDevice::metric(id);
    }
}

#include "moc_qpdfwriter.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
