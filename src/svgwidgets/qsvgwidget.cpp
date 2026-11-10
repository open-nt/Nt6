// MIT License - see LICENSE
#include <QtSvgWidgets/qsvgwidget.h>

#include <QtGui/qpainter.h>
#include <QtSvg/qsvgrenderer.h>

/*!
    \module NtSvgWidgets
    \title Nt SVG Widgets C++ Classes
    \brief A widget that shows SVG documents.
    \cmaketarget Nt::SvgWidgets

    Nt SVG Widgets has QSvgWidget, which shows an SVG document in a user interface made with Nt Widgets.
    See \l{Using Nt SVG}.

    Link with \c Nt::SvgWidgets and include \c{<QSvgWidget>}.
*/

/*!
    \class QSvgWidget
    \brief The QSvgWidget class is a widget that shows the contents of an SVG file.

    The widget draws a document scaled to its own size, so it stays sharp at any size and on screens of any
    resolution:

    \snippet svg/svg_viewer.cpp widget

    Its size hint is the default size of the document. The widget does not keep the proportions of the
    document by itself; to do so, set the aspect ratio mode of its renderer():

    \code
    widget->renderer()->setAspectRatioMode(Qt::KeepAspectRatio);
    \endcode

    \differences
    Documents are static, as with QSvgRenderer, which lists what is drawn. There is no \c QGraphicsSvgItem,
    as Nt Widgets has no graphics view.
    \enddifferences

    \sa QSvgRenderer, QWidget
*/

/*!
    Constructs a widget without a document, with the given \a parent.

    \sa load()
*/
QSvgWidget::QSvgWidget(QWidget *parent) : QWidget(parent), m_renderer(new QSvgRenderer(this)) {
    connect(m_renderer, &QSvgRenderer::repaintNeeded, this, [this] { update(); });
}

/*!
    Constructs a widget with the given \a parent that shows the SVG file \a file.
*/
QSvgWidget::QSvgWidget(const QString &file, QWidget *parent) : QSvgWidget(parent) { m_renderer->load(file); }

/*!
    Destroys the widget.
*/
QSvgWidget::~QSvgWidget() = default;

/*!
    Returns the renderer that draws the document. The widget owns it.
*/
QSvgRenderer *QSvgWidget::renderer() const { return m_renderer; }

/*!
    \reimp
    Returns the default size of the document, or 128 by 64 if none is loaded.
*/
QSize QSvgWidget::sizeHint() const { return m_renderer->isValid() ? m_renderer->defaultSize() : QSize(128, 64); }

/*!
    Returns the options of the renderer.

    \sa setOptions()
*/
QtSvg::Options QSvgWidget::options() const { return m_renderer->options(); }

/*!
    Sets the options of the renderer to \a options.

    \sa options(), QSvgRenderer::setOptions()
*/
void QSvgWidget::setOptions(QtSvg::Options options) { m_renderer->setOptions(options); }

/*!
    Loads the SVG file \a file, which may be a resource, and shows it. If the file cannot be loaded the
    widget shows nothing; renderer()->isValid() tells.
*/
void QSvgWidget::load(const QString &file) {
    m_renderer->load(file);
    updateGeometry();
    update();
}

/*!
    \overload

    Loads the SVG document in \a contents and shows it.
*/
void QSvgWidget::load(const QByteArray &contents) {
    m_renderer->load(contents);
    updateGeometry();
    update();
}

/*!
    \reimp
    Draws the document scaled to the widget. \a event is not used.
*/
void QSvgWidget::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    m_renderer->render(&painter, QRectF(rect()));
}

#include "moc_qsvgwidget.cpp"
