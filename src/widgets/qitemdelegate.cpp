// MIT License - see LICENSE
#include <QtWidgets/qitemdelegate.h>

#include <QtGui/qevent.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qstyle.h>
#include <QtWidgets/qstyleditemdelegate.h>
#include <QtWidgets/qwidget.h>

#include <algorithm>

/*!
    \class QItemDelegate
    \brief The QItemDelegate class provides display and editing facilities for data items from a model.

    QItemDelegate draws an item in parts, each with a function of its own that a subclass can reimplement:
    drawBackground(), drawCheck(), drawDecoration(), drawDisplay() and drawFocus(). doLayout() says where the
    parts go. It is the older of the two delegates; new code uses QStyledItemDelegate, which draws through the
    style. Code that draws its items in its own way often derives from QItemDelegate:

    \code
    class Money : public QItemDelegate {
    protected:
        void drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                         const QString &text) const override {
            QStyleOptionViewItem right = option;
            right.displayAlignment = Qt::AlignRight | Qt::AlignVCenter;
            QItemDelegate::drawDisplay(painter, right, rect, text + " €");
        }
    };

    view->setItemDelegateForColumn(2, new Money);
    \endcode

    The editors are those of QStyledItemDelegate: a line edit, spin box, combo box or date editor by the type
    of the value, or any widget through its user property.

    \differences
    There is no \c QItemEditorFactory: \c itemEditorFactory() and \c setItemEditorFactory() are missing. Text
    is drawn on one line, and the decoration is not tinted when the item is selected.
    \enddifferences

    \sa QStyledItemDelegate, QAbstractItemDelegate, QAbstractItemView
*/

namespace {
constexpr int kMargin = 3; // between the parts of an item, and around them

template <typename T> bool holds(const QVariant &value) { return value.typeId() == QMetaType::fromType<T>().id(); }

const QStyle *styleOf(const QStyleOptionViewItem &option) {
    return option.widget ? option.widget->style() : QApplication::style();
}

QPalette::ColorGroup groupOf(const QStyleOptionViewItem &option) {
    if (!(option.state & QStyle::State_Enabled))
        return QPalette::Disabled;
    return (option.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
}
} // namespace

/*!
    Constructs a delegate with the given \a parent.
*/
QItemDelegate::QItemDelegate(QObject *parent)
    : QAbstractItemDelegate(parent), m_editors(new QStyledItemDelegate(this)) {
    // The editors are watched by the helper; what it says, this delegate says.
    connect(m_editors, &QAbstractItemDelegate::commitData, this, [this](QWidget *editor) { emit commitData(editor); });
    connect(m_editors, &QAbstractItemDelegate::closeEditor, this,
            [this](QWidget *editor, QAbstractItemDelegate::EndEditHint hint) { emit closeEditor(editor, hint); });
}

/*!
    Destroys the delegate.
*/
QItemDelegate::~QItemDelegate() = default;

/*!
    \fn bool QItemDelegate::hasClipping() const

    Returns whether an item is clipped to its rectangle when it is drawn. The default is \c true.

    \sa setClipping()
*/

/*!
    Sets whether an item is clipped to its rectangle when it is drawn to \a clip.

    \sa hasClipping()
*/
void QItemDelegate::setClipping(bool clip) { m_clipping = clip; }

/*!
    Returns \a option with what the model says about the item \a index: its font, the alignment of its text
    and the color of its text. paint() draws with what this returns.
*/
QStyleOptionViewItem QItemDelegate::setOptions(const QModelIndex &index, const QStyleOptionViewItem &option) const {
    QStyleOptionViewItem opt = option;
    opt.index = index;
    QVariant value = index.data(Qt::FontRole);
    if (holds<QFont>(value)) {
        opt.font = value.value<QFont>();
        opt.fontMetrics = QFontMetrics(opt.font);
    }
    value = index.data(Qt::TextAlignmentRole);
    if (value.isValid() && !value.isNull()) {
        Qt::Alignment alignment(static_cast<Qt::AlignmentFlag>(value.toInt()));
        if (!(alignment & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)))
            alignment |= Qt::AlignVCenter;
        opt.displayAlignment = alignment;
    }
    value = index.data(Qt::ForegroundRole);
    if (holds<QBrush>(value))
        opt.palette.setBrush(QPalette::Text, value.value<QBrush>());
    else if (holds<QColor>(value))
        opt.palette.setColor(QPalette::Text, value.value<QColor>());
    return opt;
}

/*!
    Returns the pixmap that \a variant, the decoration of an item, is drawn as: a QPixmap as it is, a QImage
    made into one, and a QColor as a square of that color in the decoration size of \a option.
*/
QPixmap QItemDelegate::decoration(const QStyleOptionViewItem &option, const QVariant &variant) const {
    if (holds<QPixmap>(variant))
        return variant.value<QPixmap>();
    if (holds<QImage>(variant))
        return QPixmap::fromImage(variant.value<QImage>());
    if (holds<QColor>(variant)) {
        const QSize size =
            option.decorationSize.isValid() && !option.decorationSize.isEmpty() ? option.decorationSize : QSize(16, 16);
        QPixmap swatch(size);
        swatch.fill(variant.value<QColor>());
        return swatch;
    }
    return QPixmap();
}

QString QItemDelegate::textOf(const QModelIndex &index, const QStyleOptionViewItem &option) const {
    const QVariant value = index.data(Qt::DisplayRole);
    return value.isValid() && !value.isNull() ? m_editors->displayText(value, option.locale) : QString();
}

/*!
    Returns the size of the part of the item \a index that shows the role \a role: for Qt::CheckStateRole the
    check box, for Qt::DecorationRole the decoration, and for Qt::DisplayRole the text, in the font of
    \a option. The rectangle starts at (0, 0); doLayout() places it. It is empty if the item has no such part.
*/
QRect QItemDelegate::rect(const QStyleOptionViewItem &option, const QModelIndex &index, int role) const {
    const QVariant value = index.data(role);
    if (!value.isValid() || value.isNull())
        return QRect();
    switch (role) {
    case Qt::CheckStateRole: {
        const int size = styleOf(option)->pixelMetric(QStyle::PM_IndicatorWidth, &option, option.widget);
        return QRect(0, 0, size, size);
    }
    case Qt::DecorationRole: {
        if (option.decorationSize.isValid() && !option.decorationSize.isEmpty())
            return QRect(QPoint(0, 0), option.decorationSize);
        const QPixmap pixmap = decoration(option, value);
        return pixmap.isNull() ? QRect() : QRect(QPoint(0, 0), pixmap.size());
    }
    case Qt::DisplayRole: {
        const QString text = m_editors->displayText(value, option.locale);
        return QRect(0, 0, option.fontMetrics.horizontalAdvance(text) + 2 * kMargin, option.fontMetrics.height());
    }
    default:
        return QRect();
    }
}

/*!
    Places the parts of an item: on entry \a checkRect, \a pixmapRect and \a textRect have the sizes of the
    check box, the decoration and the text (from rect()), and an empty one for a part that the item does not
    have; on return they are where the parts go. The check box is on the left, and the decoration on the side
    of the text that the QStyleOptionViewItem::decorationPosition of \a option says.

    If \a hint is \c true, the parts are placed in the smallest rectangle that holds them, starting at (0, 0),
    which is how sizeHint() measures an item; otherwise in the rectangle of \a option, where the text takes
    what is left.
*/
void QItemDelegate::doLayout(const QStyleOptionViewItem &option, QRect *checkRect, QRect *pixmapRect, QRect *textRect,
                             bool hint) const {
    const QSize check = checkRect && checkRect->isValid() ? checkRect->size() : QSize(0, 0);
    const QSize deco = pixmapRect && pixmapRect->isValid() ? pixmapRect->size() : QSize(0, 0);
    const QSize text = textRect && textRect->isValid() ? textRect->size() : QSize(0, 0);
    const bool stacked = option.decorationPosition == QStyleOptionViewItem::Top ||
                         option.decorationPosition == QStyleOptionViewItem::Bottom;
    QRect area = option.rect;
    if (hint) {
        // The smallest rectangle that holds the parts and the margins.
        int width = kMargin, height = 0;
        if (!check.isEmpty()) {
            width += check.width() + kMargin;
            height = check.height();
        }
        if (stacked) {
            width += std::max(deco.width(), text.width());
            height = std::max(height, deco.height() + (deco.isEmpty() || text.isEmpty() ? 0 : kMargin) + text.height());
        } else {
            width += deco.width() + (deco.isEmpty() ? 0 : kMargin) + text.width();
            height = std::max({height, deco.height(), text.height()});
        }
        area = QRect(0, 0, width + kMargin, height + 2 * kMargin);
    }
    int left = area.left() + kMargin;
    if (checkRect) {
        *checkRect =
            check.isEmpty() ? QRect() : QRect(QPoint(left, area.top() + (area.height() - check.height()) / 2), check);
        if (!check.isEmpty())
            left += check.width() + kMargin;
    }
    QRect rest(left, area.top(), std::max(0, area.right() - kMargin - left + 1), area.height());
    QRect decoRect;
    if (!deco.isEmpty()) {
        switch (option.decorationPosition) {
        case QStyleOptionViewItem::Left:
            decoRect = QRect(QPoint(rest.left(), rest.top() + (rest.height() - deco.height()) / 2), deco);
            rest.setLeft(rest.left() + deco.width() + kMargin);
            break;
        case QStyleOptionViewItem::Right:
            decoRect =
                QRect(QPoint(rest.right() + 1 - deco.width(), rest.top() + (rest.height() - deco.height()) / 2), deco);
            rest.setRight(rest.right() - deco.width() - kMargin);
            break;
        case QStyleOptionViewItem::Top:
            decoRect = QRect(QPoint(rest.left() + (rest.width() - deco.width()) / 2, rest.top() + kMargin), deco);
            rest.setTop(decoRect.bottom() + 1 + kMargin);
            break;
        case QStyleOptionViewItem::Bottom:
            decoRect = QRect(
                QPoint(rest.left() + (rest.width() - deco.width()) / 2, rest.bottom() + 1 - kMargin - deco.height()),
                deco);
            rest.setBottom(decoRect.top() - 1 - kMargin);
            break;
        }
    }
    if (pixmapRect)
        *pixmapRect = decoRect;
    if (textRect) {
        if (hint && !text.isEmpty())
            *textRect = QRect(rest.topLeft(), QSize(std::min(text.width(), rest.width()), rest.height()));
        else
            *textRect = text.isEmpty() && hint ? QRect() : rest;
    }
}

/*!
    \reimp

    Draws the item \a index with \a painter in the rectangle of \a option: its background, check box,
    decoration, text and focus frame, through drawBackground(), drawCheck(), drawDecoration(), drawDisplay()
    and drawFocus().
*/
void QItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    const QStyleOptionViewItem opt = setOptions(index, option);
    painter->save();
    if (m_clipping)
        painter->setClipRect(opt.rect);
    const QString text = textOf(index, opt);
    const QPixmap pixmap = decoration(opt, index.data(Qt::DecorationRole));
    const QVariant checkValue = index.data(Qt::CheckStateRole);
    QRect checkRect = rect(opt, index, Qt::CheckStateRole);
    QRect pixmapRect = pixmap.isNull() ? QRect() : rect(opt, index, Qt::DecorationRole);
    QRect textRect = rect(opt, index, Qt::DisplayRole);
    doLayout(opt, &checkRect, &pixmapRect, &textRect, false);
    drawBackground(painter, opt, index);
    if (checkValue.isValid() && !checkValue.isNull())
        drawCheck(painter, opt, checkRect, static_cast<Qt::CheckState>(checkValue.toInt()));
    drawDecoration(painter, opt, pixmapRect, pixmap);
    drawDisplay(painter, opt, textRect, text);
    drawFocus(painter, opt, textRect);
    painter->restore();
}

/*!
    \reimp

    Returns the size that the item \a index needs: what the model gives for Qt::SizeHintRole, or the size of
    its check box, decoration and text as doLayout() places them, in the font of \a option.
*/
QSize QItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    const QVariant hint = index.data(Qt::SizeHintRole);
    if (holds<QSize>(hint))
        return hint.toSize();
    const QStyleOptionViewItem opt = setOptions(index, option);
    QRect checkRect = rect(opt, index, Qt::CheckStateRole);
    QRect pixmapRect = rect(opt, index, Qt::DecorationRole);
    QRect textRect = rect(opt, index, Qt::DisplayRole);
    doLayout(opt, &checkRect, &pixmapRect, &textRect, true);
    QRect all = checkRect.united(pixmapRect).united(textRect);
    return QSize(all.right() + 1 + kMargin, all.bottom() + 1 + kMargin).expandedTo(QSize(1, opt.fontMetrics.height()));
}

/*!
    Fills the background of the item \a index with \a painter: with the highlight if the item is selected and
    the whole item shows it (QStyleOptionViewItem::showDecorationSelected of \a option), or else with the brush that the
    model returns for Qt::BackgroundRole.
*/
void QItemDelegate::drawBackground(QPainter *painter, const QStyleOptionViewItem &option,
                                   const QModelIndex &index) const {
    if (option.showDecorationSelected && (option.state & QStyle::State_Selected)) {
        painter->fillRect(option.rect, option.palette.color(groupOf(option), QPalette::Highlight));
        return;
    }
    const QVariant value = index.data(Qt::BackgroundRole);
    if (holds<QBrush>(value))
        painter->fillRect(option.rect, value.value<QBrush>());
    else if (holds<QColor>(value))
        painter->fillRect(option.rect, value.value<QColor>());
}

/*!
    Draws \a text with \a painter in \a rect, aligned and elided as \a option says. A selected item has its
    text on the highlight.
*/
void QItemDelegate::drawDisplay(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                                const QString &text) const {
    const QPalette::ColorGroup group = groupOf(option);
    const bool selected = option.state & QStyle::State_Selected;
    if (selected)
        painter->fillRect(rect, option.palette.color(group, QPalette::Highlight));
    if (text.isEmpty() || rect.isEmpty())
        return;
    const QRect inner = rect.adjusted(kMargin, 0, -kMargin, 0);
    painter->save();
    painter->setFont(option.font);
    painter->setPen(option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
    const QString shown = option.textElideMode == Qt::ElideNone
                              ? text
                              : option.fontMetrics.elidedText(text, option.textElideMode, inner.width());
    painter->drawText(inner, static_cast<int>(option.displayAlignment) | Qt::TextSingleLine, shown);
    painter->restore();
}

/*!
    Draws \a pixmap with \a painter in \a rect, scaled to it. Nothing is drawn for a null pixmap; \a option is
    not used.
*/
void QItemDelegate::drawDecoration(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                                   const QPixmap &pixmap) const {
    static_cast<void>(option);
    if (pixmap.isNull() || rect.isEmpty())
        return;
    painter->drawPixmap(rect, pixmap);
}

/*!
    Draws the focus frame of the style around \a rect with \a painter, if the item has the focus in \a option.
*/
void QItemDelegate::drawFocus(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect) const {
    if (!(option.state & QStyle::State_HasFocus) || rect.isEmpty())
        return;
    QStyleOption focus;
    focus.rect = rect;
    focus.palette = option.palette;
    focus.state = option.state;
    styleOf(option)->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, painter, option.widget);
}

/*!
    Draws a check box in the state \a state with \a painter in \a rect, in the palette and state of \a option.
*/
void QItemDelegate::drawCheck(QPainter *painter, const QStyleOptionViewItem &option, const QRect &rect,
                              Qt::CheckState state) const {
    if (rect.isEmpty())
        return;
    QStyleOptionButton box;
    box.rect = rect;
    box.palette = option.palette;
    box.state = option.state & ~QStyle::State(QStyle::State_HasFocus | QStyle::State_Selected);
    box.state |= state == Qt::Unchecked ? QStyle::State_Off : QStyle::State_On;
    styleOf(option)->drawPrimitive(QStyle::PE_IndicatorCheckBox, &box, painter, option.widget);
}

/*!
    \reimp

    Returns the editor of QStyledItemDelegate for the item \a index, as a child of \a parent; \a option is
    passed on.
*/
QWidget *QItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                     const QModelIndex &index) const {
    return m_editors->createEditor(parent, option, index);
}

/*!
    \reimp

    Shows the value of the item \a index in \a editor, as QStyledItemDelegate does.
*/
void QItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    m_editors->setEditorData(editor, index);
}

/*!
    \reimp

    Writes what \a editor shows to the item \a index of \a model, as QStyledItemDelegate does.
*/
void QItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    m_editors->setModelData(editor, model, index);
}

/*!
    \reimp

    Gives \a editor the part of the rectangle of \a option where the text of the item \a index is.
*/
void QItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                         const QModelIndex &index) const {
    if (!editor)
        return;
    const QStyleOptionViewItem opt = setOptions(index, option);
    QRect checkRect = rect(opt, index, Qt::CheckStateRole);
    QRect pixmapRect = rect(opt, index, Qt::DecorationRole);
    QRect textRect = rect(opt, index, Qt::DisplayRole);
    doLayout(opt, &checkRect, &pixmapRect, &textRect, false);
    QRect r = option.rect;
    r.setLeft(std::max(option.rect.left(), textRect.left()));
    r.setRight(std::min(option.rect.right(), std::max(textRect.right(), r.left())));
    editor->setGeometry(r);
}

/*!
    \reimp

    Watches the editor \a object as QStyledItemDelegate does: Return stores the value of the editor and closes
    it, Escape closes it, Tab moves to the next item, and losing the focus stores the value. \a event is the
    event of the editor.
*/
bool QItemDelegate::eventFilter(QObject *object, QEvent *event) {
    return static_cast<QObject *>(m_editors)->eventFilter(object, event);
}

/*!
    \reimp

    Toggles the check box of the item \a index of \a model when it is clicked, or when Space is pressed on the
    item, if the item is enabled and Qt::ItemIsUserCheckable; \a option has the rectangle of the item. Returns
    \c true if \a event did that.
*/
bool QItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                                const QModelIndex &index) {
    if (!model || !index.isValid())
        return false;
    const Qt::ItemFlags flags = model->flags(index);
    if (!(flags & Qt::ItemIsUserCheckable) || !(flags & Qt::ItemIsEnabled) || !(option.state & QStyle::State_Enabled))
        return false;
    const QVariant value = index.data(Qt::CheckStateRole);
    if (!value.isValid() || value.isNull())
        return false;
    if (event->type() == QEvent::MouseButtonRelease) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const QStyleOptionViewItem opt = setOptions(index, option);
        QRect checkRect = rect(opt, index, Qt::CheckStateRole);
        QRect pixmapRect = rect(opt, index, Qt::DecorationRole);
        QRect textRect = rect(opt, index, Qt::DisplayRole);
        doLayout(opt, &checkRect, &pixmapRect, &textRect, false);
        if (mouse->button() != Qt::LeftButton || !checkRect.contains(mouse->pos()))
            return false;
    } else if (event->type() == QEvent::KeyPress) {
        if (static_cast<QKeyEvent *>(event)->key() != Qt::Key_Space)
            return false;
    } else {
        return false;
    }
    const auto state = static_cast<Qt::CheckState>(value.toInt());
    return model->setData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
}

#include "moc_qitemdelegate.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
