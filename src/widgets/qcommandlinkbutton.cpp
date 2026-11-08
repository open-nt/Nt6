// MIT License - see LICENSE
#include <QtWidgets/qcommandlinkbutton.h>

#include <QtGui/qpainter.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleoption.h>

#include <algorithm>

namespace {

constexpr int kMargin = 10;
constexpr int kArrow = 20; // the room of the arrow at the left

} // namespace

/*!
    \class QCommandLinkButton
    \brief The QCommandLinkButton class provides a large flat button with an arrow, a title and a description.

    Command link buttons are the choices of a dialog that asks what to do, each with a line that explains it:

    \code
    auto *save = new QCommandLinkButton("Save", "Keep the changes in the file.", dialog);
    connect(save, &QPushButton::clicked, dialog, &QDialog::accept);
    \endcode

    The button is flat until the mouse is over it, and draws an arrow before its title, which is larger than the text
    of other buttons. Lines of the description are wrapped to the width of the button.

    \differences
    The arrow is drawn rather than taken from the icon theme, and there is no \c setIcon().
    \enddifferences

    \sa QPushButton
*/

/*!
    Constructs a command link button with the given \a parent.
*/
QCommandLinkButton::QCommandLinkButton(QWidget *parent) : QPushButton(parent) {
    setFlat(true);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

/*!
    Constructs a command link button with the given title \a text and \a parent.
*/
QCommandLinkButton::QCommandLinkButton(const QString &text, QWidget *parent) : QCommandLinkButton(parent) {
    setText(text);
}

/*!
    Constructs a command link button with the given title \a text, \a description and \a parent.
*/
QCommandLinkButton::QCommandLinkButton(const QString &text, const QString &description, QWidget *parent)
    : QCommandLinkButton(parent) {
    setText(text);
    setDescription(description);
}

/*!
    Destroys the button.
*/
QCommandLinkButton::~QCommandLinkButton() = default;

/*!
    \fn QString QCommandLinkButton::description() const

    Returns the description under the title.

    \sa setDescription()
*/

/*!
    Sets the description under the title to \a description.
*/
void QCommandLinkButton::setDescription(const QString &description) {
    m_description = description;
    updateGeometry();
    update();
}

QFont QCommandLinkButton::titleFont() const {
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() > 0 ? f.pointSizeF() * 1.2 : 12);
    f.setBold(true);
    return f;
}

int QCommandLinkButton::textWidth() const { return std::max(0, width() - kArrow - 2 * kMargin); }

/*!
    \reimp
*/
QSize QCommandLinkButton::sizeHint() const {
    const QFontMetrics title(titleFont());
    const int width = std::max(title.horizontalAdvance(text()), 200);
    int height = kMargin + title.height();
    if (!m_description.isEmpty()) {
        const QRect lines = fontMetrics().boundingRect(QRect(0, 0, width, 1000), Qt::TextWordWrap, m_description);
        height += 4 + lines.height();
    }
    return QSize(width + kArrow + 2 * kMargin, height + kMargin);
}

/*!
    \reimp
*/
QSize QCommandLinkButton::minimumSizeHint() const {
    const QFontMetrics title(titleFont());
    return QSize(title.horizontalAdvance(text()) + kArrow + 2 * kMargin, sizeHint().height());
}

/*!
    \reimp
*/
void QCommandLinkButton::paintEvent(QPaintEvent *event) {
    (void)event;
    QPainter painter(this);
    QStyleOptionButton option;
    option.initFrom(this);
    option.features |= QStyleOptionButton::Flat;
    if (isDefault() || (autoDefault() && hasFocus()))
        option.features |= QStyleOptionButton::DefaultButton;
    option.state |= isDown() ? QStyle::State_Sunken : QStyle::State_Raised;
    style()->drawPrimitive(QStyle::PE_PanelButtonCommand, &option, &painter, this);

    const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
    const int shift = isDown() ? 1 : 0;
    const QFontMetrics title(titleFont());
    // The arrow: a short line with a head, in the color of links.
    painter.setPen(palette().color(group, QPalette::Highlight));
    const int y = kMargin + title.height() / 2 + shift;
    const int x = kMargin + shift;
    painter.drawLine(x, y, x + 12, y);
    for (int d = 1; d <= 4; ++d) {
        painter.drawPoint(x + 12 - d, y - d);
        painter.drawPoint(x + 12 - d, y + d);
    }
    painter.setFont(titleFont());
    painter.setPen(palette().color(group, QPalette::ButtonText));
    painter.drawText(QRect(kMargin + kArrow + shift, kMargin + shift, textWidth(), title.height()),
                     Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, text());
    if (!m_description.isEmpty()) {
        painter.setFont(font());
        painter.drawText(QRect(kMargin + kArrow + shift, kMargin + title.height() + 4 + shift, textWidth(),
                               height() - kMargin - title.height() - 4),
                         Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, m_description);
    }
}

#include "moc_qcommandlinkbutton.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
