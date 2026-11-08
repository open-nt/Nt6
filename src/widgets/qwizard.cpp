// MIT License - see LICENSE
#include <QtWidgets/qwizard.h>

#include <QtCore/qdebug.h>
#include <QtWidgets/qabstractslider.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qframe.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qplaintextedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>
#include <QtWidgets/qstackedlayout.h>

#include <algorithm>
#include <functional>

namespace {

// The property of a field and how its changes are noticed, for the widgets that Qt knows.
const char *defaultProperty(const QWidget *widget) {
    if (dynamic_cast<const QLineEdit *>(widget))
        return "text";
    if (dynamic_cast<const QAbstractButton *>(widget))
        return "checked";
    if (dynamic_cast<const QSpinBox *>(widget) || dynamic_cast<const QDoubleSpinBox *>(widget) ||
        dynamic_cast<const QAbstractSlider *>(widget))
        return "value";
    if (dynamic_cast<const QComboBox *>(widget))
        return "currentIndex";
    if (dynamic_cast<const QPlainTextEdit *>(widget))
        return "plainText";
    if (dynamic_cast<const QListWidget *>(widget))
        return "currentRow";
    return nullptr;
}

void connectChanges(QWidget *widget, QWizardPage *page, const std::function<void()> &changed) {
    if (auto *w = dynamic_cast<QLineEdit *>(widget))
        QObject::connect(w, &QLineEdit::textChanged, page, [changed] { changed(); });
    else if (auto *b = dynamic_cast<QAbstractButton *>(widget))
        QObject::connect(b, &QAbstractButton::toggled, page, [changed] { changed(); });
    else if (auto *s = dynamic_cast<QSpinBox *>(widget))
        QObject::connect(s, &QSpinBox::valueChanged, page, [changed] { changed(); });
    else if (auto *d = dynamic_cast<QDoubleSpinBox *>(widget))
        QObject::connect(d, &QDoubleSpinBox::valueChanged, page, [changed] { changed(); });
    else if (auto *a = dynamic_cast<QAbstractSlider *>(widget))
        QObject::connect(a, &QAbstractSlider::valueChanged, page, [changed] { changed(); });
    else if (auto *c = dynamic_cast<QComboBox *>(widget))
        QObject::connect(c, &QComboBox::currentIndexChanged, page, [changed] { changed(); });
    else if (auto *p = dynamic_cast<QPlainTextEdit *>(widget))
        QObject::connect(p, &QPlainTextEdit::textChanged, page, [changed] { changed(); });
    else if (auto *l = dynamic_cast<QListWidget *>(widget))
        QObject::connect(l, &QListWidget::currentRowChanged, page, [changed] { changed(); });
}

bool isEmptyValue(const QVariant &value) {
    if (!value.isValid())
        return true;
    if (value.typeId() == QMetaType::QString)
        return value.toString().isEmpty();
    return false;
}

} // namespace

/*!
    \class QWizard
    \brief The QWizard class leads the user through a task in a sequence of pages.

    A wizard shows one QWizardPage at a time, with a title, and buttons to go Back, to the Next page, to Finish, and
    to Cancel. Add the pages in order, or give them ids with setPage() and choose the next page in
    QWizardPage::nextId():

    \code
    QWizard wizard(window);
    wizard.setWindowTitle("New Project");
    wizard.addPage(new IntroPage);
    wizard.addPage(new DetailsPage);
    if (wizard.exec() == QDialog::Accepted)
        create(wizard.field("name").toString());
    \endcode

    Pages register fields: widgets whose value the wizard can read by name from any page. A mandatory field, whose
    name ends with "*", must be changed from its starting value before the user can go on.

    The Next button is enabled when the current page is complete, and is replaced by Commit on a commit page, after
    which the user cannot go back. Finish shows on the last page.

    \differences
    All the styles look the same: a title and a subtitle above the page, and the buttons in a row at the bottom.
    There are no pixmaps (\c setPixmap(), the watermark and the logo), no \c setButtonLayout(), no
    \c setTitleFormat() or \c setSubTitleFormat() (the text is plain), and no \c setDefaultProperty(). A field of a
    widget that Nt does not know needs its property; a changed signal given to \c registerField() is connected with
    the string-based syntax. The texts are not translated.
    \enddifferences

    \sa QWizardPage, QDialog
*/

/*!
    \enum QWizard::WizardButton

    The buttons of a wizard.

    \value BackButton Back.
    \value NextButton Next.
    \value CommitButton Commit, instead of Next on a commit page.
    \value FinishButton Finish.
    \value CancelButton Cancel.
    \value HelpButton Help, with HaveHelpButton.
    \value CustomButton1 A custom button, with HaveCustomButton1.
    \value CustomButton2 A custom button, with HaveCustomButton2.
    \value CustomButton3 A custom button, with HaveCustomButton3.
    \value Stretch Accepted for compatibility; there is no setButtonLayout().
    \value NoButton No button.
    \omitvalue NStandardButtons
    \omitvalue NButtons
*/

/*!
    \enum QWizard::WizardStyle

    The looks of a wizard; recorded only, all look the same in Nt.

    \value ClassicStyle Classic.
    \value ModernStyle Modern.
    \value MacStyle macOS.
    \value AeroStyle Windows Aero.
    \omitvalue NStyles
*/

/*!
    \enum QWizard::WizardOption

    Options of a wizard. WizardOptions is a typedef for \c{QFlags<WizardOption>}.

    \value IndependentPages Going back does not clean up the page, and a page is initialized only the first time.
    \value IgnoreSubTitles The subtitles are not shown.
    \value ExtendedWatermarkPixmap Accepted for compatibility; there are no pixmaps.
    \value NoDefaultButton Next and Finish are not default buttons.
    \value NoBackButtonOnStartPage No Back button on the first page.
    \value NoBackButtonOnLastPage No Back button on the last page.
    \value DisabledBackButtonOnLastPage A disabled Back button on the last page.
    \value HaveNextButtonOnLastPage A disabled Next button on the last page.
    \value HaveFinishButtonOnEarlyPages A Finish button on every page.
    \value NoCancelButton No Cancel button.
    \value CancelButtonOnLeft The Cancel button at the left.
    \value HaveHelpButton A Help button.
    \value HelpButtonOnRight The Help button at the right.
    \value HaveCustomButton1 The first custom button.
    \value HaveCustomButton2 The second custom button.
    \value HaveCustomButton3 The third custom button.
    \value NoCancelButtonOnLastPage No Cancel button on the last page.
*/

/*!
    Constructs a wizard with the given \a parent and window \a flags.
*/
QWizard::QWizard(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags) {
    auto *main = new QVBoxLayout(this);
    m_header = new QWidget(this);
    auto *headerLayout = new QVBoxLayout(m_header);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    m_title = new QLabel(m_header);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_subTitle = new QLabel(m_header);
    m_subTitle->setWordWrap(true);
    headerLayout->addWidget(m_title);
    headerLayout->addWidget(m_subTitle);
    main->addWidget(m_header);
    m_body = new QHBoxLayout;
    main->addLayout(m_body, 1);
    auto *pages = new QWidget(this);
    m_stack = new QStackedLayout(pages);
    m_body->addWidget(pages, 1);
    auto *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    main->addWidget(line);
    m_buttonRow = new QHBoxLayout;
    main->addLayout(m_buttonRow);
    for (int i = 0; i < NButtons; ++i)
        setButton(static_cast<WizardButton>(i), nullptr);
    updateButtons();
}

/*!
    Destroys the wizard and its pages.
*/
QWizard::~QWizard() {
    for (QWizardPage *page : m_pages)
        page->m_wizard = nullptr;
}

/*!
    Adds \a page with an id one greater than the largest id so far, or 0 for the first page, and returns the id. The
    wizard takes the page as a child.

    \sa setPage()
*/
int QWizard::addPage(QWizardPage *page) {
    const int id = m_pages.isEmpty() ? 0 : m_pages.lastKey() + 1;
    setPage(id, page);
    return id;
}

/*!
    Adds \a page with the given \a id. An id that is negative or taken is refused.

    \sa addPage(), removePage()
*/
void QWizard::setPage(int id, QWizardPage *page) {
    if (!page || id < 0 || m_pages.contains(id))
        return;
    m_pages.insert(id, page);
    page->m_wizard = this;
    m_stack->addWidget(page);
    QObject::connect(page, &QWizardPage::completeChanged, this, [this, page] {
        if (page == currentPage())
            updateButtons();
    });
    updateButtons();
    emit pageAdded(id);
}

/*!
    Removes the page with the given \a id, without deleting it. The page no longer has a parent.

    \sa page()
*/
void QWizard::removePage(int id) {
    QWizardPage *page = m_pages.value(id, nullptr);
    if (!page)
        return;
    if (id == m_current) {
        if (m_history.size() > 1) {
            back();
        } else {
            m_current = -1;
            m_history.clear();
        }
    }
    m_history.erase(std::remove(m_history.begin(), m_history.end(), id), m_history.end());
    m_pages.remove(id);
    m_stack->removeWidget(page);
    QObject::disconnect(page, nullptr, this, nullptr);
    page->m_wizard = nullptr;
    page->setParent(nullptr);
    if (m_start == id)
        m_start = -1;
    updateButtons();
    emit pageRemoved(id);
}

/*!
    Returns the page with the given \a id, or \c nullptr.
*/
QWizardPage *QWizard::page(int id) const { return m_pages.value(id, nullptr); }

/*!
    Returns whether the user went through the page with the given \a id on the way to the current page.

    \sa visitedIds()
*/
bool QWizard::hasVisitedPage(int id) const {
    return std::find(m_history.begin(), m_history.end(), id) != m_history.end();
}

/*!
    Returns the ids of the pages the user went through to the current page, in order, the current one included.
*/
QList<int> QWizard::visitedIds() const {
    QList<int> ids;
    for (int id : m_history)
        ids.append(id);
    return ids;
}

/*!
    Returns the ids of the pages, in increasing order.
*/
QList<int> QWizard::pageIds() const { return m_pages.keys(); }

/*!
    Makes the page with the given \a id the first page. By default it is the page with the smallest id.

    \sa startId()
*/
void QWizard::setStartId(int id) {
    if (id != -1 && !m_pages.contains(id))
        return;
    m_start = id;
}

/*!
    Returns the id of the first page, or -1 if there are no pages.

    \sa setStartId()
*/
int QWizard::startId() const { return m_start >= 0 ? m_start : (m_pages.isEmpty() ? -1 : m_pages.firstKey()); }

/*!
    Returns the current page, or \c nullptr before the wizard is shown.
*/
QWizardPage *QWizard::currentPage() const { return page(m_current); }

/*!
    \fn int QWizard::currentId() const

    Returns the id of the current page, or -1 before the wizard is shown.
*/

/*!
    Returns whether the user may leave the current page with Next, Commit or Finish. The default implementation asks
    QWizardPage::validatePage().
*/
bool QWizard::validateCurrentPage() {
    QWizardPage *current = currentPage();
    return !current || current->validatePage();
}

/*!
    Returns the id of the page after the current one, or -1 if it is the last. The default implementation asks
    QWizardPage::nextId().
*/
int QWizard::nextId() const {
    const QWizardPage *current = currentPage();
    return current ? current->nextId() : -1;
}

/*!
    Sets the field \a name of any page to \a value.

    \sa field()
*/
void QWizard::setField(const QString &name, const QVariant &value) {
    for (QWizardPage *p : m_pages) {
        if (const QWizardPage::Field *f = p->findField(name)) {
            if (f->widget)
                f->widget->setProperty(f->property.constData(), value);
            return;
        }
    }
}

/*!
    Returns the value of the field \a name of any page, or an invalid QVariant if there is no such field.

    \sa setField(), QWizardPage::registerField()
*/
QVariant QWizard::field(const QString &name) const {
    for (const QWizardPage *p : m_pages)
        if (const QWizardPage::Field *f = p->findField(name))
            return f->widget ? f->widget->property(f->property.constData()) : QVariant();
    return QVariant();
}

/*!
    \fn void QWizard::setWizardStyle(WizardStyle style)

    Sets the look of the wizard to \a style; recorded only.
*/

/*!
    \fn QWizard::WizardStyle QWizard::wizardStyle() const

    Returns the look of the wizard, ClassicStyle by default.
*/

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.

    \sa testOption()
*/
void QWizard::setOption(WizardOption option, bool on) {
    WizardOptions options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QWizard::testOption(WizardOption option) const

    Returns whether \a option is set.
*/

/*!
    Sets the options of the wizard to \a options.

    \sa options()
*/
void QWizard::setOptions(WizardOptions options) {
    m_options = options;
    layoutButtons();
    updateButtons();
    updateHeader();
}

/*!
    \fn QWizard::WizardOptions QWizard::options() const

    Returns the options of the wizard; none by default.
*/

/*!
    Sets the text of the button \a which to \a text, for all the pages that do not set their own.

    \sa buttonText(), QWizardPage::setButtonText()
*/
void QWizard::setButtonText(WizardButton which, const QString &text) {
    if (which < 0 || which >= NButtons)
        return;
    m_buttonTexts[which] = text;
    updateButtons();
}

/*!
    Returns the text of the button \a which on the current page.

    \sa setButtonText()
*/
QString QWizard::buttonText(WizardButton which) const {
    if (which < 0 || which >= NButtons)
        return QString();
    if (const QWizardPage *current = currentPage(); current && current->m_buttonTexts.contains(which))
        return current->m_buttonTexts.value(which);
    return m_buttonTexts[which].isEmpty() ? defaultButtonText(which) : m_buttonTexts[which];
}

QString QWizard::defaultButtonText(WizardButton which) const {
    switch (which) {
    case BackButton:
        return QStringLiteral("< Back");
    case NextButton:
        return QStringLiteral("Next >");
    case CommitButton:
        return QStringLiteral("Commit");
    case FinishButton:
        return QStringLiteral("Finish");
    case CancelButton:
        return QStringLiteral("Cancel");
    case HelpButton:
        return QStringLiteral("Help");
    default:
        return QString();
    }
}

/*!
    Replaces the button \a which with \a button, or with a new push button if it is \c nullptr. The wizard takes the
    button as a child and deletes the one it had.

    \sa button()
*/
void QWizard::setButton(WizardButton which, QAbstractButton *button) {
    if (which < 0 || which >= NButtons)
        return;
    if (!button) {
        auto *push = new QPushButton(this);
        push->setAutoDefault(false);
        button = push;
    }
    if (m_buttons[which] == button)
        return;
    delete m_buttons[which];
    m_buttons[which] = button;
    button->setParent(this);
    QObject::connect(button, &QAbstractButton::clicked, this, [this, which] {
        switch (which) {
        case BackButton:
            back();
            break;
        case NextButton:
        case CommitButton:
            next();
            break;
        case FinishButton:
            if (validateCurrentPage())
                accept();
            break;
        case CancelButton:
            reject();
            break;
        case HelpButton:
            emit helpRequested();
            break;
        default:
            emit customButtonClicked(which);
            break;
        }
    });
    layoutButtons();
    updateButtons();
}

/*!
    Returns the button \a which, or \c nullptr.

    \sa setButton()
*/
QAbstractButton *QWizard::button(WizardButton which) const {
    return which >= 0 && which < NButtons ? m_buttons[which] : nullptr;
}

/*!
    Puts \a widget at the left of the pages, such as a list of the steps, or removes it with \c nullptr. The wizard
    takes the widget as a child and deletes the one it had.

    \sa sideWidget()
*/
void QWizard::setSideWidget(QWidget *widget) {
    if (m_side == widget)
        return;
    delete m_side.data();
    m_side = widget;
    if (widget) {
        widget->setParent(this);
        m_body->insertWidget(0, widget);
        widget->show();
    }
}

/*!
    \fn QWidget *QWizard::sideWidget() const

    Returns the widget at the left of the pages, or \c nullptr.
*/

/*!
    \reimp

    Showing the wizard the first time goes to the first page.
*/
void QWizard::setVisible(bool visible) {
    if (visible && m_current < 0)
        restart();
    QDialog::setVisible(visible);
}

/*!
    Goes back to the page before the current one. The current page is cleaned up, unless the IndependentPages option
    is set.

    \sa next()
*/
void QWizard::back() {
    if (m_history.size() < 2)
        return;
    const int leaving = m_current;
    m_history.pop_back();
    if (!testOption(IndependentPages))
        cleanupPage(leaving);
    switchTo(m_history.back(), false);
}

/*!
    Goes to the next page if the current page is valid. The next page is initialized first.

    \sa back(), validateCurrentPage()
*/
void QWizard::next() {
    if (m_current < 0) {
        restart();
        return;
    }
    if (!validateCurrentPage())
        return;
    const int id = nextId();
    if (id < 0 || !m_pages.contains(id))
        return;
    if (!testOption(IndependentPages) || !hasVisitedPage(id))
        initializePage(id);
    m_history.push_back(id);
    switchTo(id, true);
}

/*!
    Goes to the page with the given \a id by going forward with next() until it is reached, if it is ahead, or back
    if it was visited.

    \sa currentId()
*/
void QWizard::setCurrentId(int id) {
    if (!m_pages.contains(id) || id == m_current)
        return;
    if (hasVisitedPage(id)) {
        while (m_current != id && m_history.size() > 1)
            back();
        return;
    }
    // Go forward while the pages lead there.
    std::vector<int> seen;
    while (m_current != id) {
        const int before = m_current;
        if (std::find(seen.begin(), seen.end(), before) != seen.end())
            return;
        seen.push_back(before);
        next();
        if (m_current == before)
            return;
    }
}

/*!
    Goes back to the first page, cleaning up the visited pages, and initializes it.
*/
void QWizard::restart() {
    while (!m_history.empty()) {
        if (!testOption(IndependentPages))
            cleanupPage(m_history.back());
        m_history.pop_back();
    }
    m_current = -1;
    const int start = startId();
    if (start < 0) {
        updateButtons();
        return;
    }
    initializePage(start);
    m_history.push_back(start);
    switchTo(start, true);
}

/*!
    \reimp

    Canceling resets the wizard to its first page for the next time; finishing checks the current page first.
*/
void QWizard::done(int result) {
    if (result != Rejected && !validateCurrentPage())
        return;
    QDialog::done(result);
    if (result == Rejected) {
        m_history.clear();
        m_current = -1;
    }
}

/*!
    Is called before the page with the given \a id is shown when the user goes to it. The default implementation calls
    QWizardPage::initializePage().
*/
void QWizard::initializePage(int id) {
    if (QWizardPage *p = page(id))
        p->initializePage();
}

/*!
    Is called when the user leaves the page with the given \a id with Back. The default implementation calls
    QWizardPage::cleanupPage().
*/
void QWizard::cleanupPage(int id) {
    if (QWizardPage *p = page(id))
        p->cleanupPage();
}

void QWizard::switchTo(int id, bool forward) {
    (void)forward;
    m_current = id;
    m_stack->setCurrentWidget(page(id));
    updateHeader();
    updateButtons();
    emit currentIdChanged(id);
}

void QWizard::updateHeader() {
    const QWizardPage *current = currentPage();
    const QString title = current ? current->title() : QString();
    const QString subTitle = current && !testOption(IgnoreSubTitles) ? current->subTitle() : QString();
    m_title->setText(title);
    m_subTitle->setText(subTitle);
    m_subTitle->setVisible(!subTitle.isEmpty());
    m_header->setVisible(!title.isEmpty() || !subTitle.isEmpty());
}

// Puts the buttons in their row: Help, a stretch, then Back, Next, Commit, Finish and Cancel.
void QWizard::layoutButtons() {
    if (!m_buttonRow)
        return;
    while (QLayoutItem *item = m_buttonRow->takeAt(0))
        delete item;
    const auto add = [this](WizardButton which) {
        if (m_buttons[which])
            m_buttonRow->addWidget(m_buttons[which]);
    };
    if (testOption(CancelButtonOnLeft))
        add(CancelButton);
    if (!testOption(HelpButtonOnRight))
        add(HelpButton);
    m_buttonRow->addStretch();
    add(CustomButton1);
    add(CustomButton2);
    add(CustomButton3);
    add(BackButton);
    add(NextButton);
    add(CommitButton);
    add(FinishButton);
    if (!testOption(CancelButtonOnLeft))
        add(CancelButton);
    if (testOption(HelpButtonOnRight))
        add(HelpButton);
}

void QWizard::updateButtons() {
    for (QAbstractButton *b : m_buttons)
        if (!b)
            return; // still being constructed
    for (int i = 0; i < NButtons; ++i)
        m_buttons[i]->setText(buttonText(static_cast<WizardButton>(i)));
    const QWizardPage *current = currentPage();
    const bool complete = current && current->isComplete();
    const int next = current ? nextId() : -1;
    const bool last = current && (next < 0 || current->isFinalPage());
    const bool commit = current && current->isCommitPage();
    const bool start = m_history.size() <= 1;
    // The pages before a commit page cannot be gone back to.
    bool afterCommit = false;
    for (std::size_t i = 0; i + 1 < m_history.size(); ++i)
        if (const QWizardPage *p = page(m_history[i]); p && p->isCommitPage())
            afterCommit = true;

    m_buttons[BackButton]->setVisible(!(start && testOption(NoBackButtonOnStartPage)) &&
                                      !(last && next < 0 && testOption(NoBackButtonOnLastPage)));
    m_buttons[BackButton]->setEnabled(!start && !afterCommit &&
                                      !(last && next < 0 && testOption(DisabledBackButtonOnLastPage)));
    const bool showNext = !commit && (next >= 0 || testOption(HaveNextButtonOnLastPage));
    m_buttons[NextButton]->setVisible(showNext);
    m_buttons[NextButton]->setEnabled(complete && next >= 0);
    m_buttons[CommitButton]->setVisible(commit && next >= 0);
    m_buttons[CommitButton]->setEnabled(complete);
    m_buttons[FinishButton]->setVisible(last || testOption(HaveFinishButtonOnEarlyPages));
    m_buttons[FinishButton]->setEnabled(complete && (last || testOption(HaveFinishButtonOnEarlyPages)));
    m_buttons[CancelButton]->setVisible(!testOption(NoCancelButton) &&
                                        !(last && next < 0 && testOption(NoCancelButtonOnLastPage)));
    m_buttons[HelpButton]->setVisible(testOption(HaveHelpButton));
    m_buttons[CustomButton1]->setVisible(testOption(HaveCustomButton1));
    m_buttons[CustomButton2]->setVisible(testOption(HaveCustomButton2));
    m_buttons[CustomButton3]->setVisible(testOption(HaveCustomButton3));
    // The default button: Commit or Next, or Finish on the last page.
    if (!testOption(NoDefaultButton)) {
        QAbstractButton *preferred = m_buttons[commit ? CommitButton : (last ? FinishButton : NextButton)];
        if (auto *push = dynamic_cast<QPushButton *>(preferred))
            push->setDefault(true);
    }
}

/*!
    \class QWizardPage
    \brief The QWizardPage class is a page of a QWizard.

    A wizard page is a widget with a title and a subtitle. Subclass it to fill in its widgets, and register the fields
    that later pages and the wizard read:

    \code
    class DetailsPage : public QWizardPage {
    public:
        DetailsPage() {
            setTitle("Details");
            auto *name = new QLineEdit;
            registerField("name*", name); // mandatory
            auto *layout = new QVBoxLayout(this);
            layout->addWidget(name);
        }
    };
    \endcode

    Reimplement initializePage() to fill the page from the fields of earlier pages, validatePage() to check it before
    the user goes on, isComplete() to say when Next is enabled, and nextId() to choose the next page.

    \differences
    See QWizard.
    \enddifferences

    \sa QWizard
*/

/*!
    Constructs a wizard page with the given \a parent.
*/
QWizardPage::QWizardPage(QWidget *parent) : QWidget(parent) {}

/*!
    Destroys the page; a wizard that has it forgets it.
*/
QWizardPage::~QWizardPage() {
    if (m_wizard) {
        for (int id : m_wizard->pageIds()) {
            if (m_wizard->page(id) == this) {
                m_wizard->m_pages.remove(id);
                m_wizard->m_history.erase(std::remove(m_wizard->m_history.begin(), m_wizard->m_history.end(), id),
                                          m_wizard->m_history.end());
                if (m_wizard->m_current == id)
                    m_wizard->m_current = m_wizard->m_history.empty() ? -1 : m_wizard->m_history.back();
            }
        }
    }
}

/*!
    Sets the title of the page to \a title, which the wizard shows above it.

    \sa title()
*/
void QWizardPage::setTitle(const QString &title) {
    m_title = title;
    if (m_wizard && m_wizard->currentPage() == this)
        m_wizard->updateHeader();
}

/*!
    \fn QString QWizardPage::title() const

    Returns the title of the page.
*/

/*!
    Sets the subtitle of the page to \a subTitle, which the wizard shows under the title unless IgnoreSubTitles is set.

    \sa subTitle()
*/
void QWizardPage::setSubTitle(const QString &subTitle) {
    m_subTitle = subTitle;
    if (m_wizard && m_wizard->currentPage() == this)
        m_wizard->updateHeader();
}

/*!
    \fn QString QWizardPage::subTitle() const

    Returns the subtitle of the page.
*/

/*!
    Sets whether the page is a last page, with a Finish button, even if nextId() leads on, to \a finalPage.

    \sa isFinalPage()
*/
void QWizardPage::setFinalPage(bool finalPage) {
    m_final = finalPage;
    if (m_wizard && m_wizard->currentPage() == this)
        m_wizard->updateButtons();
}

/*!
    Returns whether the page is a last page: it was set so, or nextId() returns -1.
*/
bool QWizardPage::isFinalPage() const {
    return m_final || (m_wizard && m_wizard->currentPage() == this && nextId() < 0);
}

/*!
    Sets whether the page is a commit page to \a commitPage: its Next button is a Commit button, and the user cannot go
    back past it.

    \sa isCommitPage()
*/
void QWizardPage::setCommitPage(bool commitPage) {
    m_commit = commitPage;
    if (m_wizard && m_wizard->currentPage() == this)
        m_wizard->updateButtons();
}

/*!
    \fn bool QWizardPage::isCommitPage() const

    Returns whether the page is a commit page.
*/

/*!
    Sets the text of the button \a which on this page to \a text.

    \sa buttonText()
*/
void QWizardPage::setButtonText(QWizard::WizardButton which, const QString &text) {
    m_buttonTexts.insert(which, text);
    if (m_wizard && m_wizard->currentPage() == this)
        m_wizard->updateButtons();
}

/*!
    Returns the text of the button \a which on this page.

    \sa setButtonText()
*/
QString QWizardPage::buttonText(QWizard::WizardButton which) const {
    if (m_buttonTexts.contains(which))
        return m_buttonTexts.value(which);
    return m_wizard ? m_wizard->buttonText(which) : QString();
}

/*!
    Is called before the page is shown when the user goes to it. The default implementation does nothing.

    \sa cleanupPage()
*/
void QWizardPage::initializePage() {}

/*!
    Is called when the user leaves the page with Back. The default implementation sets the fields of the page back to
    their starting values.

    \sa initializePage()
*/
void QWizardPage::cleanupPage() {
    for (const Field &f : m_fields)
        if (f.widget)
            f.widget->setProperty(f.property.constData(), f.initial);
}

/*!
    Returns whether the user may leave the page with Next, Commit or Finish. The default implementation returns
    \c true.
*/
bool QWizardPage::validatePage() { return true; }

/*!
    Returns whether the page is complete, which enables Next, Commit and Finish. The default implementation returns
    \c true if every mandatory field has been changed from its starting value and is not an empty text.

    \sa completeChanged()
*/
bool QWizardPage::isComplete() const {
    for (const Field &f : m_fields) {
        if (!f.mandatory || !f.widget)
            continue;
        const QVariant value = f.widget->property(f.property.constData());
        if (isEmptyValue(value) || value == f.initial)
            return false;
    }
    return true;
}

/*!
    Returns the id of the page after this one, or -1 if it is the last. The default implementation returns the next
    larger id of the wizard.
*/
int QWizardPage::nextId() const {
    if (!m_wizard)
        return -1;
    const QList<int> ids = m_wizard->pageIds();
    bool found = false;
    for (int id : ids) {
        if (found)
            return id;
        found = m_wizard->page(id) == this;
    }
    return -1;
}

/*!
    Sets the field \a name, of this page or another page of the wizard, to \a value.

    \sa field()
*/
void QWizardPage::setField(const QString &name, const QVariant &value) {
    if (m_wizard) {
        m_wizard->setField(name, value);
    } else if (const Field *f = findField(name); f && f->widget) {
        f->widget->setProperty(f->property.constData(), value);
    }
}

/*!
    Returns the value of the field \a name, of this page or another page of the wizard.

    \sa setField(), registerField()
*/
QVariant QWizardPage::field(const QString &name) const {
    if (m_wizard)
        return m_wizard->field(name);
    const Field *f = findField(name);
    return f && f->widget ? f->widget->property(f->property.constData()) : QVariant();
}

/*!
    Registers the field \a name for the \a property of \a widget, which the wizard reads with QWizard::field(). A name
    that ends with "*" is mandatory: the page is not complete until it changes. Without \a property, the property of
    the widget type is used: the text of a QLineEdit, the checked state of a button, the value of a spin box or a
    slider, the current index of a combo box, the plain text of a QPlainTextEdit and the current row of a QListWidget.
    \a changedSignal, given with \c SIGNAL(), tells the page when the property changes; it is needed only for other
    widgets.
*/
void QWizardPage::registerField(const QString &name, QWidget *widget, const char *property, const char *changedSignal) {
    if (!widget || name.isEmpty())
        return;
    const bool mandatory = name.endsWith(QLatin1Char('*'));
    const QString plain = mandatory ? name.left(name.size() - 1) : name;
    const char *prop = property ? property : defaultProperty(widget);
    if (!prop) {
        qWarning("QWizardPage::registerField: no property for the field");
        return;
    }
    m_fields.push_back({plain, widget, QByteArray(prop), widget->property(prop), mandatory});
    if (changedSignal)
        QObject::connect(widget, changedSignal, this, SIGNAL(completeChanged()));
    else
        connectChanges(widget, this, [this] { emit completeChanged(); });
}

/*!
    \fn QWizard *QWizardPage::wizard() const

    Returns the wizard that the page is in, or \c nullptr.
*/

const QWizardPage::Field *QWizardPage::findField(const QString &name) const {
    for (const Field &f : m_fields)
        if (f.name == name)
            return &f;
    return nullptr;
}

#include "moc_qwizard.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
