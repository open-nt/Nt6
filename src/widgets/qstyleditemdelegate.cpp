// MIT License - see LICENSE
#include <QtWidgets/qstyleditemdelegate.h>

#include <QtCore/qdatetime.h>
#include <QtGui/qcolor.h>
#include <QtGui/qevent.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qdatetimeedit.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstyle.h>

#include <algorithm>
#include <limits>

/*!
    \class QStyledItemDelegate
    \brief The QStyledItemDelegate class provides display and editing facilities for data items from a model.

    This is the delegate that the views of Nt Widgets use unless they are
    given another one. It draws an item from what the model returns for
    the roles of the item:

    \list
    \li Qt::DisplayRole: the text. Numbers, dates and times are written
        with displayText().
    \li Qt::CheckStateRole: a check box before the text.
    \li Qt::DecorationRole: a QPixmap, a QImage or a QColor before the
        text.
    \li Qt::FontRole, Qt::ForegroundRole, Qt::BackgroundRole and
        Qt::TextAlignmentRole: the font, the colors and the alignment.
    \li Qt::SizeHintRole: the size of the item, instead of the one that
        its contents need.
    \endlist

    For editing it makes a widget that fits the type of the value of
    Qt::EditRole: a combo box for a \c bool, a spin box for an integer, a
    QDoubleSpinBox for a \c double, a QDateEdit, QTimeEdit or
    QDateTimeEdit for a date or a time, and a QLineEdit for everything
    else. Return or Enter in the editor stores the value, Escape gives the
    edit up, and Tab stores it and moves on to the next item; so does
    moving the focus elsewhere. A click on the check box of an item that is
    Qt::ItemIsUserCheckable toggles it without an editor.

    To draw or edit some items in another way, subclass the delegate and
    reimplement paint() and sizeHint(), or createEditor(), setEditorData()
    and setModelData():

    \code
    class RatingDelegate : public QStyledItemDelegate {
    public:
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override {
            auto *box = new QSpinBox(parent);
            box->setRange(0, 5);
            return box;
        }
    };

    view->setItemDelegateForColumn(2, new RatingDelegate);
    \endcode

    \differences
    The item is drawn by the delegate itself, not by the style, so a custom
    QStyle does not change how items look. The decoration is a pixmap, an
    image or a color: Nt has no QIcon. Text is drawn on one line. There is
    no \c QItemEditorFactory: the editor for a type is chosen by
    createEditor(). \c QItemDelegate is not provided.
    \enddifferences

    \sa QAbstractItemDelegate, QTreeView
*/

namespace {

constexpr int kMargin = 3;  // around the parts of an item
constexpr int kSpacing = 4; // between the check box, the decoration and the text

struct Parts {
    QRect check;
    QRect decoration;
    QRect text;
};

int indicatorSize(const QStyleOptionViewItem &option) {
    const QStyle *style = option.widget ? option.widget->style() : QApplication::style();
    return style->pixelMetric(QStyle::PM_IndicatorWidth, &option, option.widget);
}

QSize decorationSizeOf(const QStyleOptionViewItem &option) {
    if (!(option.features & QStyleOptionViewItem::HasDecoration))
        return QSize(0, 0);
    if (option.decorationSize.isValid() && !option.decorationSize.isEmpty())
        return option.decorationSize;
    return option.decoration.isNull() ? QSize(16, 16) : option.decoration.size();
}

// Where the check box, the decoration and the text of an item are, from left to right in its rectangle.
Parts layoutParts(const QStyleOptionViewItem &option) {
    Parts parts;
    const QRect r = option.rect;
    int x = r.left() + kMargin;
    if (option.features & QStyleOptionViewItem::HasCheckIndicator) {
        const int size = indicatorSize(option);
        parts.check = QRect(x, r.top() + (r.height() - size) / 2, size, size);
        x += size + kSpacing;
    }
    const QSize decoration = decorationSizeOf(option);
    if (!decoration.isEmpty()) {
        parts.decoration = QRect(x, r.top() + (r.height() - decoration.height()) / 2, decoration.width(),
                                 decoration.height());
        x += decoration.width() + kSpacing;
    }
    parts.text = QRect(x, r.top(), std::max(0, r.right() - kMargin - x + 1), r.height());
    return parts;
}

template <typename T> bool holds(const QVariant &value) { return value.typeId() == QMetaType::fromType<T>().id(); }

bool isInteger(int type) {
    switch (type) {
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::Short:
    case QMetaType::UShort:
    case QMetaType::Long:
    case QMetaType::ULong:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return true;
    default:
        return false;
    }
}

} // namespace

/*!
    Constructs a delegate with the given \a parent.
*/
QStyledItemDelegate::QStyledItemDelegate(QObject *parent) : QAbstractItemDelegate(parent) {}

/*!
    Destroys the delegate.
*/
QStyledItemDelegate::~QStyledItemDelegate() = default;

/*!
    Returns the text that the value \a value is shown as, written the way
    \a locale writes numbers, dates and times. A subclass reimplements this
    to show values in another way, with a unit for example.
*/
QString QStyledItemDelegate::displayText(const QVariant &value, const QLocale &locale) const {
    switch (value.typeId()) {
    case QMetaType::Float:
    case QMetaType::Double:
        return locale.toString(value.toDouble());
    case QMetaType::Int:
    case QMetaType::Short:
    case QMetaType::Long:
    case QMetaType::LongLong:
        return locale.toString(value.toLongLong());
    case QMetaType::UInt:
    case QMetaType::UShort:
    case QMetaType::ULong:
    case QMetaType::ULongLong:
        return locale.toString(value.toULongLong());
    case QMetaType::QDate:
        return locale.toString(value.toDate(), QLocale::ShortFormat);
    case QMetaType::QTime:
        return locale.toString(value.toTime(), QLocale::ShortFormat);
    case QMetaType::QDateTime:
        return locale.toString(value.toDateTime(), QLocale::ShortFormat);
    default:
        return value.toString();
    }
}

/*!
    Fills \a option with what the model says about the item \a index: its
    text, check state, decoration, font, colors and alignment. paint() and
    sizeHint() call this; a subclass that reimplements them calls it too,
    or reimplements it to change what they see.
*/
void QStyledItemDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const {
    option->index = index;
    QVariant value = index.data(Qt::FontRole);
    if (holds<QFont>(value)) {
        option->font = value.value<QFont>();
        option->fontMetrics = QFontMetrics(option->font);
    }
    value = index.data(Qt::TextAlignmentRole);
    if (value.isValid() && !value.isNull()) {
        // A horizontal alignment alone, as models often give it, keeps the text centered vertically.
        Qt::Alignment alignment(static_cast<Qt::AlignmentFlag>(value.toInt()));
        if (!(alignment & (Qt::AlignTop | Qt::AlignBottom | Qt::AlignVCenter)))
            alignment |= Qt::AlignVCenter;
        if (!(alignment & (Qt::AlignLeft | Qt::AlignRight | Qt::AlignHCenter)))
            alignment |= Qt::AlignLeft;
        option->displayAlignment = alignment;
    }
    value = index.data(Qt::ForegroundRole);
    if (holds<QBrush>(value))
        option->palette.setBrush(QPalette::Text, value.value<QBrush>());
    else if (holds<QColor>(value))
        option->palette.setColor(QPalette::Text, value.value<QColor>());
    value = index.data(Qt::CheckStateRole);
    if (value.isValid() && !value.isNull()) {
        option->features |= QStyleOptionViewItem::HasCheckIndicator;
        option->checkState = static_cast<Qt::CheckState>(value.toInt());
    }
    value = index.data(Qt::DecorationRole);
    if (holds<QPixmap>(value)) {
        option->decoration = value.value<QPixmap>();
        option->features |= QStyleOptionViewItem::HasDecoration;
    } else if (holds<QImage>(value)) {
        option->decoration = QPixmap::fromImage(value.value<QImage>());
        option->features |= QStyleOptionViewItem::HasDecoration;
    } else if (holds<QColor>(value)) {
        QPixmap swatch(16, 16);
        swatch.fill(value.value<QColor>());
        option->decoration = swatch;
        option->features |= QStyleOptionViewItem::HasDecoration;
    }
    value = index.data(Qt::DisplayRole);
    if (value.isValid() && !value.isNull()) {
        option->features |= QStyleOptionViewItem::HasDisplay;
        option->text = displayText(value, option->locale);
    }
    value = index.data(Qt::BackgroundRole);
    if (holds<QBrush>(value))
        option->backgroundBrush = value.value<QBrush>();
    else if (holds<QColor>(value))
        option->backgroundBrush = QBrush(value.value<QColor>());
}

/*!
    \reimp

    Draws the item \a index with \a painter in the rectangle of \a option:
    its background, or the highlight if it is selected, then its check
    box, its decoration and its text.
*/
void QStyledItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QPalette::ColorGroup group = !(opt.state & QStyle::State_Enabled) ? QPalette::Disabled
                                       : (opt.state & QStyle::State_Active) ? QPalette::Active
                                                                            : QPalette::Inactive;
    const bool selected = opt.state & QStyle::State_Selected;
    painter->save();
    painter->setClipRect(opt.rect);
    if (selected)
        painter->fillRect(opt.rect, opt.palette.color(group, QPalette::Highlight));
    else if (opt.backgroundBrush.style() != Qt::NoBrush)
        painter->fillRect(opt.rect, opt.backgroundBrush);

    const Parts parts = layoutParts(opt);
    if (opt.features & QStyleOptionViewItem::HasCheckIndicator) {
        QStyleOptionButton box;
        box.rect = parts.check;
        box.palette = opt.palette;
        box.state = opt.state & ~QStyle::State(QStyle::State_HasFocus | QStyle::State_Selected);
        // A partly checked box is drawn checked: the style has no third look.
        box.state |= opt.checkState == Qt::Unchecked ? QStyle::State_Off : QStyle::State_On;
        const QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &box, painter, opt.widget);
    }
    if (!parts.decoration.isEmpty() && !opt.decoration.isNull())
        painter->drawPixmap(parts.decoration, opt.decoration);
    if (opt.features & QStyleOptionViewItem::HasDisplay) {
        painter->setFont(opt.font);
        painter->setPen(opt.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text));
        const QString text = opt.textElideMode == Qt::ElideNone
                                 ? opt.text
                                 : opt.fontMetrics.elidedText(opt.text, opt.textElideMode, parts.text.width());
        painter->drawText(parts.text, int(opt.displayAlignment) | Qt::TextSingleLine, text);
    }
    painter->restore();
}

/*!
    \reimp

    Returns the size that the item \a index needs: what the model gives
    for Qt::SizeHintRole, or the size of its check box, decoration and
    text in the font of \a option.
*/
QSize QStyledItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
    const QVariant hint = index.data(Qt::SizeHintRole);
    if (holds<QSize>(hint))
        return hint.toSize();
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    int width = 2 * kMargin;
    int height = opt.fontMetrics.height();
    if (opt.features & QStyleOptionViewItem::HasCheckIndicator) {
        const int size = indicatorSize(opt);
        width += size + kSpacing;
        height = std::max(height, size);
    }
    const QSize decoration = decorationSizeOf(opt);
    if (!decoration.isEmpty()) {
        width += decoration.width() + kSpacing;
        height = std::max(height, decoration.height());
    }
    width += opt.fontMetrics.horizontalAdvance(opt.text);
    return QSize(width, height + 2 * kMargin);
}

/*!
    \reimp

    Returns an editor for the item \a index as a child of \a parent, of
    the kind that fits the type of the value that the model returns for
    Qt::EditRole; see the description of the class. \a option is not used.
*/
QWidget *QStyledItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                           const QModelIndex &index) const {
    static_cast<void>(option);
    if (!index.isValid())
        return nullptr;
    const QVariant value = index.data(Qt::EditRole);
    const int type = value.typeId();
    if (type == QMetaType::Bool) {
        auto *box = new QComboBox(parent);
        box->addItem(QStringLiteral("False"));
        box->addItem(QStringLiteral("True"));
        return box;
    }
    if (isInteger(type)) {
        auto *box = new QSpinBox(parent);
        const bool isUnsigned =
            type == QMetaType::UInt || type == QMetaType::UShort || type == QMetaType::ULong || type == QMetaType::ULongLong;
        box->setRange(isUnsigned ? 0 : std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
        return box;
    }
    if (type == QMetaType::Double || type == QMetaType::Float) {
        auto *box = new QDoubleSpinBox(parent);
        box->setDecimals(6);
        box->setRange(-std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
        return box;
    }
    if (type == QMetaType::QDate)
        return new QDateEdit(parent);
    if (type == QMetaType::QTime)
        return new QTimeEdit(parent);
    if (type == QMetaType::QDateTime)
        return new QDateTimeEdit(parent);
    auto *edit = new QLineEdit(parent);
    edit->setFrame(false);
    return edit;
}

/*!
    \reimp

    Shows the value that the model returns for Qt::EditRole of the item
    \a index in \a editor, one of the editors of createEditor().
*/
void QStyledItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const {
    const QVariant value = index.data(Qt::EditRole);
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        combo->setCurrentIndex(value.toBool() ? 1 : 0);
    } else if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        const qlonglong wide = value.toLongLong();
        spin->setValue(static_cast<int>(std::clamp<qlonglong>(wide, std::numeric_limits<int>::min(),
                                                               std::numeric_limits<int>::max())));
    } else if (auto *real = qobject_cast<QDoubleSpinBox *>(editor)) {
        real->setValue(value.toDouble());
    } else if (auto *date = qobject_cast<QDateEdit *>(editor)) {
        date->setDate(value.toDate());
    } else if (auto *time = qobject_cast<QTimeEdit *>(editor)) {
        time->setTime(value.toTime());
    } else if (auto *dateTime = qobject_cast<QDateTimeEdit *>(editor)) {
        dateTime->setDateTime(value.toDateTime());
    } else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
        line->setText(value.toString());
        line->selectAll();
    }
}

/*!
    \reimp

    Writes what \a editor shows to Qt::EditRole of the item \a index of
    \a model, as a value of the type that the item had.
*/
void QStyledItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
    if (!model || !index.isValid())
        return;
    const QVariant old = index.data(Qt::EditRole);
    QVariant value;
    if (auto *combo = qobject_cast<QComboBox *>(editor)) {
        value = combo->currentIndex() == 1;
    } else if (auto *spin = qobject_cast<QSpinBox *>(editor)) {
        value = spin->value();
    } else if (auto *real = qobject_cast<QDoubleSpinBox *>(editor)) {
        value = real->value();
    } else if (auto *date = qobject_cast<QDateEdit *>(editor)) {
        value = date->date();
    } else if (auto *time = qobject_cast<QTimeEdit *>(editor)) {
        value = time->time();
    } else if (auto *dateTime = qobject_cast<QDateTimeEdit *>(editor)) {
        value = dateTime->dateTime();
    } else if (auto *line = qobject_cast<QLineEdit *>(editor)) {
        value = line->text();
    } else {
        return;
    }
    // Keep the type of the item where the editor has a wider or a narrower one.
    if (old.isValid() && old.typeId() != value.typeId() && old.typeId() != QMetaType::QString)
        value.convert(old.metaType());
    model->setData(index, value, Qt::EditRole);
}

/*!
    \reimp

    Gives \a editor the rectangle of the item in \a option, less the room
    that the check box and the decoration of the item \a index take.
*/
void QStyledItemDelegate::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option,
                                               const QModelIndex &index) const {
    if (!editor)
        return;
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const Parts parts = layoutParts(opt);
    QRect r = option.rect;
    r.setLeft(std::max(option.rect.left(), parts.text.left() - kMargin));
    editor->setGeometry(r);
}

/*!
    \reimp

    Watches the editor \a object: Return and Enter in it store its value
    and close it, Escape closes it without storing, Tab and Shift+Tab
    store and move to the next and the previous item, and so does the
    editor losing the focus to another widget of its window. Other
    \a event are left to the editor.
*/
bool QStyledItemDelegate::eventFilter(QObject *object, QEvent *event) {
    auto *editor = qobject_cast<QWidget *>(object);
    if (!editor)
        return false;
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        switch (key->key()) {
        case Qt::Key_Return:
        case Qt::Key_Enter:
            emit commitData(editor);
            emit closeEditor(editor, SubmitModelCache);
            return true;
        case Qt::Key_Escape:
            emit closeEditor(editor, RevertModelCache);
            return true;
        case Qt::Key_Tab:
            emit commitData(editor);
            emit closeEditor(editor, EditNextItem);
            return true;
        case Qt::Key_Backtab:
            emit commitData(editor);
            emit closeEditor(editor, EditPreviousItem);
            return true;
        default:
            return false;
        }
    }
    if (event->type() == QEvent::FocusOut) {
        // The popup of a combo box or of a date editor takes the focus to a window of its own: the editor
        // is still in use then.
        QWidget *now = QApplication::focusWidget();
        if (now && now != editor && now->window() == editor->window()) {
            bool inside = false;
            for (QWidget *w = now; w && !inside; w = w->parentWidget())
                inside = w == editor;
            if (!inside) {
                emit commitData(editor);
                emit closeEditor(editor, NoHint);
            }
        }
    }
    return false;
}

/*!
    \reimp

    Toggles the check box of the item \a index of \a model when it is
    clicked, or when Space is pressed on the item, if the item is enabled
    and Qt::ItemIsUserCheckable; \a option has the rectangle of the item.
    Returns \c true if \a event did that.
*/
bool QStyledItemDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
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
        QStyleOptionViewItem opt = option;
        initStyleOption(&opt, index);
        if (mouse->button() != Qt::LeftButton || !layoutParts(opt).check.contains(mouse->pos()))
            return false;
    } else if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        if (key != Qt::Key_Space)
            return false;
    } else {
        return false;
    }
    const auto state = static_cast<Qt::CheckState>(value.toInt());
    return model->setData(index, state == Qt::Checked ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
}

#include "moc_qstyleditemdelegate.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
