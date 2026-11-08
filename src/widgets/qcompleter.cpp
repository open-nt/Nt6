// MIT License - see LICENSE
#include <QtWidgets/qcompleter.h>

#include "qwidget_p.h"

#include <QtGui/qevent.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistwidget.h>

#include <algorithm>

namespace {

// The list of completions under the widget: it does not take the focus, and it closes when the mouse is pressed
// outside it.
class CompleterPopup : public QListWidget {
public:
    explicit CompleterPopup(QWidget *parent) : QListWidget(parent) {
        setFocusPolicy(Qt::NoFocus);
        hide();
    }

protected:
    void hideEvent(QHideEvent *event) override {
        QtPrivate::WidgetAccess::setPopup(this, nullptr);
        QListWidget::hideEvent(event);
    }
};

} // namespace

/*!
    \class QCompleter
    \brief The QCompleter class completes what the user types from a list of words.

    Give a line edit a completer, and as the user types it shows the words that start with the text in a list under
    the line edit. The arrow keys move in the list, and Return or a click takes the word:

    \code
    auto *completer = new QCompleter(QStringList{"apple", "apricot", "banana"}, edit);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    edit->setCompleter(completer);
    \endcode

    In InlineCompletion mode the rest of the first matching word is put after the text, selected, so that typing on
    replaces it. The filter mode chooses whether the words must start with the text, contain it, or end with it.

    \differences
    The words come from the QStringList given to the constructor: there is no \c setModel(), \c model(),
    \c completionModel(), \c setCompletionColumn() or \c setCompletionRole(), and \c setModelSorting() is recorded only.
    The popup is a list inside the window, not a window of its own, and \c setPopup() is missing. Only QLineEdit uses
    a completer by itself.
    \enddifferences

    \sa QLineEdit::setCompleter()
*/

/*!
    \enum QCompleter::CompletionMode

    How the completions are offered.

    \value PopupCompletion The words that match are shown in a list.
    \value UnfilteredPopupCompletion All the words are shown in a list, with the first match current.
    \value InlineCompletion The rest of the first match is put after the text, selected.
*/

/*!
    \enum QCompleter::ModelSorting

    How the words are sorted; recorded only.

    \value UnsortedModel Not sorted.
    \value CaseSensitivelySortedModel Sorted, case matters.
    \value CaseInsensitivelySortedModel Sorted, case does not matter.
*/

/*!
    Constructs a completer without words, with the given \a parent.
*/
QCompleter::QCompleter(QObject *parent) : QObject(parent) {}

/*!
    Constructs a completer of the words in \a completions, with the given \a parent.
*/
QCompleter::QCompleter(const QStringList &completions, QObject *parent) : QObject(parent), m_completions(completions) {}

/*!
    Destroys the completer and its popup.
*/
QCompleter::~QCompleter() { delete m_popup; }

/*!
    Makes \a widget the widget that the completer completes and shows its popup under. QLineEdit::setCompleter() calls
    it.

    \sa widget()
*/
void QCompleter::setWidget(QWidget *widget) {
    hidePopup();
    m_widget = widget;
}

/*!
    \fn QWidget *QCompleter::widget() const

    Returns the widget that the completer completes, or \c nullptr.
*/

/*!
    \fn void QCompleter::setCompletionMode(CompletionMode mode)

    Sets how the completions are offered to \a mode.
*/

/*!
    \fn QCompleter::CompletionMode QCompleter::completionMode() const

    Returns how the completions are offered, PopupCompletion by default.
*/

/*!
    Sets how the words are matched to \a filterMode: Qt::MatchStartsWith, Qt::MatchContains or Qt::MatchEndsWith.
    Other modes are refused.

    \sa filterMode()
*/
void QCompleter::setFilterMode(Qt::MatchFlags filterMode) {
    const int kind = static_cast<int>(filterMode & Qt::MatchTypeMask);
    if (kind != Qt::MatchStartsWith && kind != Qt::MatchContains && kind != Qt::MatchEndsWith)
        return;
    m_filter = filterMode;
    filter();
}

/*!
    \fn Qt::MatchFlags QCompleter::filterMode() const

    Returns how the words are matched, Qt::MatchStartsWith by default.
*/

/*!
    Sets whether case matters when the words are matched to \a caseSensitivity.

    \sa caseSensitivity()
*/
void QCompleter::setCaseSensitivity(Qt::CaseSensitivity caseSensitivity) {
    m_case = caseSensitivity;
    filter();
}

/*!
    \fn Qt::CaseSensitivity QCompleter::caseSensitivity() const

    Returns whether case matters when the words are matched, Qt::CaseSensitive by default.
*/

/*!
    \fn void QCompleter::setModelSorting(ModelSorting sorting)

    Sets how the words are sorted to \a sorting; recorded only.
*/

/*!
    \fn QCompleter::ModelSorting QCompleter::modelSorting() const

    Returns how the words are sorted, UnsortedModel by default.
*/

/*!
    Sets how many words the popup shows before it scrolls to \a maxItems.

    \sa maxVisibleItems()
*/
void QCompleter::setMaxVisibleItems(int maxItems) { m_maxVisible = std::max(1, maxItems); }

/*!
    \fn int QCompleter::maxVisibleItems() const

    Returns how many words the popup shows before it scrolls, 7 by default.
*/

/*!
    \fn void QCompleter::setWrapAround(bool wrap)

    Sets whether moving past the end of the popup goes round to the start to \a wrap.
*/

/*!
    \fn bool QCompleter::wrapAround() const

    Returns whether moving past the end of the popup goes round to the start. The default is \c true.
*/

/*!
    \fn QString QCompleter::completionPrefix() const

    Returns the text that the words are matched against.

    \sa setCompletionPrefix()
*/

/*!
    \fn int QCompleter::completionCount() const

    Returns the number of words that match the prefix, or of all the words in UnfilteredPopupCompletion mode.
*/

/*!
    Makes the word at \a row of the completions current and returns whether there is such a row.

    \sa currentRow(), currentCompletion()
*/
bool QCompleter::setCurrentRow(int row) {
    if (row < 0 || row >= completionCount())
        return false;
    m_row = row;
    if (m_popup && m_popup->isVisible())
        m_popup->setCurrentRow(row);
    return true;
}

/*!
    \fn int QCompleter::currentRow() const

    Returns the row of the current completion, or -1.
*/

/*!
    Returns the current completion, or an empty string.
*/
QString QCompleter::currentCompletion() const {
    return m_row >= 0 && m_row < completionCount() ? m_matches.at(m_row) : QString();
}

/*!
    Returns the list that shows the completions, creating it if there is none yet.
*/
QAbstractItemView *QCompleter::popup() const {
    if (!m_popup) {
        auto *self = const_cast<QCompleter *>(this);
        self->m_popup = new CompleterPopup(m_widget ? m_widget->window() : nullptr);
        QObject::connect(self->m_popup, &QListWidget::itemClicked, self,
                         [self](QListWidgetItem *item) { self->choose(self->m_popup->row(item)); });
        QObject::connect(self->m_popup, &QListWidget::currentRowChanged, self, [self](int row) {
            if (row >= 0 && row < self->completionCount()) {
                self->m_row = row;
                emit self->highlighted(self->m_matches.at(row));
            }
        });
    }
    return m_popup;
}

/*!
    Sets the text that the words are matched against to \a prefix. The first match becomes current.

    \sa complete()
*/
void QCompleter::setCompletionPrefix(const QString &prefix) {
    m_prefix = prefix;
    filter();
}

/*!
    Offers the completions of the prefix: shows the popup under the widget, or under \a rect if it is valid, in the
    coordinates of the widget. In InlineCompletion mode nothing is shown.

    \sa setCompletionPrefix()
*/
void QCompleter::complete(const QRect &rect) {
    filter();
    if (m_mode == InlineCompletion || completionCount() == 0) {
        hidePopup();
        return;
    }
    showPopup(rect);
}

bool QCompleter::matches(const QString &candidate) const {
    switch (static_cast<int>(m_filter & Qt::MatchTypeMask)) {
    case Qt::MatchContains:
        return candidate.contains(m_prefix, m_case);
    case Qt::MatchEndsWith:
        return candidate.endsWith(m_prefix, m_case);
    default:
        return candidate.startsWith(m_prefix, m_case);
    }
}

void QCompleter::filter() {
    m_matches.clear();
    m_row = -1;
    for (const QString &word : m_completions) {
        if (m_mode == UnfilteredPopupCompletion || matches(word))
            m_matches.append(word);
    }
    if (m_mode == UnfilteredPopupCompletion) {
        for (int i = 0; i < completionCount(); ++i) {
            if (matches(m_matches.at(i))) {
                m_row = i;
                break;
            }
        }
    } else if (!m_matches.isEmpty()) {
        m_row = 0;
    }
}

void QCompleter::showPopup(const QRect &rect) {
    if (!m_widget)
        return;
    popup();
    QWidget *top = m_widget->window();
    if (m_popup->parentWidget() != top)
        m_popup->setParent(top);
    const int row = m_row; // filling the list moves its current row
    m_popup->clear();
    m_popup->addItems(m_matches);
    m_row = row;
    const QRect area = rect.isValid() ? rect : m_widget->rect();
    const int rows = std::min(completionCount(), m_maxVisible);
    const int height = rows * m_popup->rowHeight() + 4;
    QPoint at = m_widget->mapTo(top, area.bottomLeft() + QPoint(0, 1));
    if (at.y() + height > top->height())
        at.setY(std::max(0, m_widget->mapTo(top, area.topLeft()).y() - height));
    m_popup->setGeometry(QRect(at, QSize(area.width(), height)));
    m_popup->setCurrentRow(m_row);
    m_popup->raise();
    m_popup->show();
    QtPrivate::WidgetAccess::setPopup(m_popup, m_popup);
    QtPrivate::WidgetAccess::data(m_popup).popupAnchor = m_widget.data();
}

void QCompleter::hidePopup() {
    if (m_popup && m_popup->isVisible())
        m_popup->hide();
}

// The user took the completion at `row`.
void QCompleter::choose(int row) {
    if (row < 0 || row >= completionCount())
        return;
    const QString text = m_matches.at(row);
    hidePopup();
    if (auto *edit = dynamic_cast<QLineEdit *>(m_widget.data()))
        edit->setText(text);
    emit activated(text);
}

bool QCompleter::handleKey(QKeyEvent *event) {
    if (!m_popup || !m_popup->isVisible())
        return false;
    const int count = completionCount();
    int row = m_row;
    switch (event->key()) {
    case Qt::Key_Up:
        row = row <= 0 ? (m_wrap ? count - 1 : 0) : row - 1;
        break;
    case Qt::Key_Down:
        row = row >= count - 1 ? (m_wrap ? 0 : count - 1) : row + 1;
        break;
    case Qt::Key_PageUp:
        row = std::max(0, row - m_maxVisible);
        break;
    case Qt::Key_PageDown:
        row = std::min(count - 1, row + m_maxVisible);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
    case Qt::Key_Tab:
        if (m_row >= 0) {
            choose(m_row);
            event->accept();
            return true;
        }
        hidePopup();
        return false;
    case Qt::Key_Escape:
        hidePopup();
        event->accept();
        return true;
    default:
        return false;
    }
    setCurrentRow(row);
    if (m_row >= 0)
        emit highlighted(m_matches.at(m_row));
    event->accept();
    return true;
}

void QCompleter::textEdited(const QString &edited, bool deleting) {
    // A copy: `edited` may be the text of the line edit, which setText() below changes.
    const QString text = edited; // NOLINT(performance-unnecessary-copy-initialization): see above
    setCompletionPrefix(text);
    if (text.isEmpty()) {
        hidePopup();
        return;
    }
    if (m_mode == InlineCompletion) {
        auto *edit = dynamic_cast<QLineEdit *>(m_widget.data());
        if (!edit || m_row < 0 || deleting || (m_filter & Qt::MatchTypeMask) != Qt::MatchStartsWith)
            return;
        const QString completion = m_matches.at(m_row);
        if (completion.size() <= text.size())
            return;
        // Put the rest of the word after the text, selected.
        edit->setText(text + completion.mid(text.size()));
        edit->setSelection(static_cast<int>(text.size()), static_cast<int>(completion.size() - text.size()));
        return;
    }
    complete();
}

#include "moc_qcompleter.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
