// MIT License - see LICENSE
#include <QtGui/qpagedpaintdevice.h>

/*!
    \class QPagedPaintDevice
    \inmodule NtGui
    \since 6.0
    \brief The QPagedPaintDevice class represents a paint device that supports multiple pages.

    A paged device paints on pages of the size, orientation and margins of
    its pageLayout(). Painting starts on the first page; newPage() ends it and
    starts another, with the page layout of that moment. QPdfWriter is one.

    \differences
    There are no page ranges (\c setPageRanges()) and no
    \c PdfVersion_X4.
    \enddifferences

    \sa QPdfWriter, QPageLayout
*/

/*!
    \enum QPagedPaintDevice::PdfVersion

    This enum gives the version of PDF that a QPdfWriter writes.

    \value PdfVersion_1_4 PDF 1.4, the default.
    \value PdfVersion_A1b PDF/A-1b; Nt writes PDF 1.4, without the metadata that conformance needs.
    \value PdfVersion_1_6 PDF 1.6.
*/

/*!
    Constructs a device with an A4 portrait page and margins of 10 points.
*/
QPagedPaintDevice::QPagedPaintDevice()
    : m_layout(QPageSize(QPageSize::A4), QPageLayout::Portrait, QMarginsF(10, 10, 10, 10)) {}

/*!
    Destroys the device.
*/
QPagedPaintDevice::~QPagedPaintDevice() = default;

/*!
    \fn bool QPagedPaintDevice::newPage()

    Ends the current page and starts a new one, and returns \c true if it
    could.
*/

/*!
    Sets the page layout to \a pageLayout, from the next page on, and returns
    \c true if it is valid.

    \sa pageLayout()
*/
bool QPagedPaintDevice::setPageLayout(const QPageLayout &pageLayout) {
    if (!pageLayout.isValid())
        return false;
    m_layout = pageLayout;
    return true;
}

/*!
    Sets the page size to \a pageSize, keeping the orientation and margins as
    far as the size allows, and returns \c true if it is valid.
*/
bool QPagedPaintDevice::setPageSize(const QPageSize &pageSize) {
    if (!pageSize.isValid())
        return false;
    m_layout.setPageSize(pageSize, m_layout.minimumMargins());
    return true;
}

/*!
    Sets the orientation of the pages to \a orientation and returns \c true.
*/
bool QPagedPaintDevice::setPageOrientation(QPageLayout::Orientation orientation) {
    m_layout.setOrientation(orientation);
    return true;
}

/*!
    Sets the margins to \a margins in \a units, and returns \c true if they
    fit the page.
*/
bool QPagedPaintDevice::setPageMargins(const QMarginsF &margins, QPageLayout::Unit units) {
    QPageLayout layout = m_layout;
    layout.setUnits(units);
    if (!layout.setMargins(margins))
        return false;
    m_layout = layout;
    return true;
}

/*!
    \fn QPageLayout QPagedPaintDevice::pageLayout() const

    Returns the page layout of the device.
*/
