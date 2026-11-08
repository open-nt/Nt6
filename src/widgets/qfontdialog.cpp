// MIT License - see LICENSE
#include <QtWidgets/qfontdialog.h>

#include "gui/qfontengine_p.h"

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qlistwidget.h>
#include <QtWidgets/qspinbox.h>

namespace {

const char *const kStyles[] = {"Regular", "Italic", "Bold", "Bold Italic"};
const int kSizes[] = {6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72};

// The generic families that fontconfig always resolves, before the installed ones.
QStringList familyList() {
    QStringList families = {QStringLiteral("Sans Serif"), QStringLiteral("Serif"), QStringLiteral("Monospace")};
    for (const QString &family : QFontEngine::families())
        if (!families.contains(family))
            families.append(family);
    return families;
}

int rowOf(QListWidget *list, const QString &text) {
    for (int i = 0; i < list->count(); ++i)
        if (list->item(i)->text().compare(text, Qt::CaseInsensitive) == 0)
            return i;
    return -1;
}

} // namespace

/*!
    \class QFontDialog
    \brief The QFontDialog class lets the user choose a font.

    The dialog has a list of font families, of styles and of sizes, and a sample text in the current font. getFont()
    shows it and waits:

    \code
    bool ok = false;
    const QFont font = QFontDialog::getFont(&ok, editor->font(), window);
    if (ok)
        editor->setFont(font);
    \endcode

    The families are the generic Sans Serif, Serif and Monospace, then the families that fontconfig finds.

    \differences
    The dialog is always drawn by Nt. The styles are Regular, Italic, Bold and Bold Italic whatever the family has, and
    there are no effects (underline, strike out) and no writing systems. The options that filter the families
    (\c ScalableFonts, \c MonospacedFonts and the others) are recorded but do not filter. There is no \c open() with a
    receiver. The texts are not translated.
    \enddifferences

    \sa QFont
*/

/*!
    \enum QFontDialog::FontDialogOption

    Options for the dialog. FontDialogOptions is a typedef for \c{QFlags<FontDialogOption>}.

    \value NoButtons No OK and Cancel buttons, for a dialog that is used for its currentFontChanged() signal.
    \value DontUseNativeDialog Accepted for compatibility; the dialog is never native.
    \value ScalableFonts Show scalable fonts; recorded only.
    \value NonScalableFonts Show bitmap fonts; recorded only.
    \value MonospacedFonts Show monospaced fonts; recorded only.
    \value ProportionalFonts Show proportional fonts; recorded only.
*/

/*!
    Constructs a font dialog with the given \a parent. The current font is the default font of the application.
*/
QFontDialog::QFontDialog(QWidget *parent) : QDialog(parent) { init(QFont()); }

/*!
    Constructs a font dialog with the given \a parent and the \a initial current font.
*/
QFontDialog::QFontDialog(const QFont &initial, QWidget *parent) : QDialog(parent) { init(initial); }

/*!
    Destroys the font dialog.
*/
QFontDialog::~QFontDialog() = default;

void QFontDialog::init(const QFont &initial) {
    setWindowTitle(QStringLiteral("Select Font"));
    auto *main = new QVBoxLayout(this);
    auto *grid = new QGridLayout;
    main->addLayout(grid);
    grid->addWidget(new QLabel(QStringLiteral("Font"), this), 0, 0);
    grid->addWidget(new QLabel(QStringLiteral("Font style"), this), 0, 1);
    grid->addWidget(new QLabel(QStringLiteral("Size"), this), 0, 2);
    m_families = new QListWidget(this);
    m_families->addItems(familyList());
    m_styles = new QListWidget(this);
    for (const char *style : kStyles)
        m_styles->addItem(QString::fromUtf8(style));
    m_size = new QSpinBox(this);
    m_size->setRange(1, 512);
    m_sizes = new QListWidget(this);
    for (int size : kSizes)
        m_sizes->addItem(QString::number(size));
    auto *sizeColumn = new QVBoxLayout;
    sizeColumn->addWidget(m_size);
    sizeColumn->addWidget(m_sizes);
    grid->addWidget(m_families, 1, 0);
    grid->addWidget(m_styles, 1, 1);
    grid->addLayout(sizeColumn, 1, 2);
    grid->setColumnStretch(0, 3);
    grid->setColumnStretch(1, 2);
    grid->setColumnStretch(2, 1);
    main->addWidget(new QLabel(QStringLiteral("Sample"), this));
    m_sample = new QLineEdit(QStringLiteral("AaBbYyZz"), this);
    main->addWidget(m_sample);
    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    main->addWidget(m_buttons);
    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    QObject::connect(m_families, &QListWidget::currentRowChanged, this, [this] { fromControls(); });
    QObject::connect(m_styles, &QListWidget::currentRowChanged, this, [this] { fromControls(); });
    QObject::connect(m_sizes, &QListWidget::currentTextChanged, this, [this](const QString &text) {
        if (!m_updating && !text.isEmpty())
            m_size->setValue(text.toInt());
    });
    QObject::connect(m_size, &QSpinBox::valueChanged, this, [this] { fromControls(); });
    setCurrentFont(initial);
}

/*!
    Sets the current font to \a font, and selects its family, style and size in the lists.

    \sa currentFont()
*/
void QFontDialog::setCurrentFont(const QFont &font) {
    const bool changed = font != m_font;
    m_font = font;
    syncControls();
    if (changed)
        emit currentFontChanged(m_font);
}

/*!
    \fn QFont QFontDialog::currentFont() const

    Returns the font that the user is picking.

    \sa setCurrentFont(), selectedFont()
*/

/*!
    \fn QFont QFontDialog::selectedFont() const

    Returns the font that was chosen when the dialog was accepted.

    \sa currentFont()
*/

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.

    \sa testOption()
*/
void QFontDialog::setOption(FontDialogOption option, bool on) {
    FontDialogOptions options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QFontDialog::testOption(FontDialogOption option) const

    Returns whether \a option is set.
*/

/*!
    Sets the options of the dialog to \a options.

    \sa options()
*/
void QFontDialog::setOptions(FontDialogOptions options) {
    m_options = options;
    m_buttons->setVisible(!options.testFlag(NoButtons));
}

/*!
    \fn QFontDialog::FontDialogOptions QFontDialog::options() const

    Returns the options of the dialog.
*/

/*!
    \reimp

    An accepted dialog records the selected font and emits fontSelected().
*/
void QFontDialog::done(int result) {
    if (result == Accepted) {
        m_selected = m_font;
        emit fontSelected(m_selected);
    }
    QDialog::done(result);
}

void QFontDialog::syncControls() {
    m_updating = true;
    int family = rowOf(m_families, m_font.family());
    if (family < 0 && !m_font.family().isEmpty()) {
        m_families->addItem(m_font.family());
        family = m_families->count() - 1;
    }
    m_families->setCurrentRow(family < 0 ? 0 : family);
    m_styles->setCurrentRow((m_font.bold() ? 2 : 0) + (m_font.italic() ? 1 : 0));
    const int size = m_font.pointSize() > 0 ? m_font.pointSize() : 10;
    m_size->setValue(size);
    m_sizes->setCurrentRow(rowOf(m_sizes, QString::number(size)));
    m_sample->setFont(m_font);
    m_updating = false;
}

void QFontDialog::fromControls() {
    if (m_updating)
        return;
    QFont font = m_font;
    if (QListWidgetItem *item = m_families->currentItem())
        font.setFamily(item->text());
    const int style = std::max(0, m_styles->currentRow());
    font.setItalic(style % 2 == 1);
    font.setBold(style >= 2);
    font.setPointSize(m_size->value());
    m_updating = true;
    m_sizes->setCurrentRow(rowOf(m_sizes, QString::number(m_size->value())));
    m_updating = false;
    m_sample->setFont(font);
    if (font != m_font) {
        m_font = font;
        emit currentFontChanged(m_font);
    }
}

/*!
    Shows a font dialog with the given \a parent, window \a title and \a options, starting at the \a initial font, and
    waits for the user. Returns the chosen font, or \a initial if the user canceled; \a ok, if given, is set to whether
    the user accepted.
*/
QFont QFontDialog::getFont(bool *ok, const QFont &initial, QWidget *parent, const QString &title,
                           FontDialogOptions options) {
    QFontDialog dialog(initial, parent);
    if (!title.isEmpty())
        dialog.setWindowTitle(title);
    dialog.setOptions(options);
    const bool accepted = dialog.exec() == Accepted;
    if (ok)
        *ok = accepted;
    return accepted ? dialog.selectedFont() : initial;
}

/*!
    \overload

    Shows a font dialog with the given \a parent, starting at the default font. \a ok, if given, is set to whether the
    user accepted.
*/
QFont QFontDialog::getFont(bool *ok, QWidget *parent) { return getFont(ok, QFont(), parent); }

#include "moc_qfontdialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
