// MIT License - see LICENSE
#include <QtWidgets/qcolordialog.h>

#include <QtGui/qevent.h>
#include <QtGui/qimage.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qdialogbuttonbox.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qlineedit.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qspinbox.h>

#include <algorithm>
#include <array>
#include <functional>

namespace {

constexpr int kBasicRows = 6;
constexpr int kBasicColumns = 8;
constexpr int kCustomCount = 16;

// The basic colors: a row of grays, then rows of hues from strong to pale.
std::array<QRgb, kBasicRows * kBasicColumns> &basicColors() {
    static std::array<QRgb, kBasicRows * kBasicColumns> colors = [] {
        std::array<QRgb, kBasicRows * kBasicColumns> out{};
        for (int c = 0; c < kBasicColumns; ++c) {
            const int gray = 255 * c / (kBasicColumns - 1);
            out[static_cast<std::size_t>(c)] = qRgb(gray, gray, gray);
        }
        const int saturation[] = {255, 255, 160, 255, 90};
        const int value[] = {255, 170, 255, 110, 255};
        for (int r = 1; r < kBasicRows; ++r)
            for (int c = 0; c < kBasicColumns; ++c)
                out[static_cast<std::size_t>(r) * kBasicColumns + static_cast<std::size_t>(c)] =
                    QColor::fromHsv(360 * c / kBasicColumns, saturation[r - 1], value[r - 1]).rgb();
        return out;
    }();
    return colors;
}

std::array<QRgb, kCustomCount> &customColors() {
    static std::array<QRgb, kCustomCount> colors = [] {
        std::array<QRgb, kCustomCount> out{};
        out.fill(qRgb(255, 255, 255));
        return out;
    }();
    return colors;
}

// A grid of color cells; a click picks one.
class Swatches : public QWidget {
public:
    Swatches(QWidget *parent, QRgb *colors, int rows, int columns)
        : QWidget(parent), m_colors(colors), m_rows(rows), m_columns(columns) {
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    }

    std::function<void(QColor)> picked;
    int current() const { return m_current; }

    QSize sizeHint() const override { return QSize(m_columns * kCellW, m_rows * kCellH); }
    QSize minimumSizeHint() const override { return sizeHint(); }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        for (int i = 0; i < m_rows * m_columns; ++i) {
            const QRect cell = cellRect(i);
            painter.setPen(palette().color(group, QPalette::Mid));
            painter.setBrush(QColor(m_colors[i]));
            painter.drawRect(cell.adjusted(2, 2, -3, -3));
            if (i == m_current) {
                painter.setPen(palette().color(group, hasFocus() ? QPalette::Highlight : QPalette::Text));
                painter.setBrush(Qt::NoBrush);
                painter.drawRect(cell.adjusted(0, 0, -1, -1));
            }
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        const int column = event->pos().x() / kCellW;
        const int row = event->pos().y() / kCellH;
        if (column < 0 || column >= m_columns || row < 0 || row >= m_rows) {
            event->ignore();
            return;
        }
        m_current = row * m_columns + column;
        update();
        if (picked)
            picked(QColor(m_colors[m_current]));
        event->accept();
    }

private:
    static constexpr int kCellW = 24;
    static constexpr int kCellH = 20;
    QRect cellRect(int i) const { return QRect((i % m_columns) * kCellW, (i / m_columns) * kCellH, kCellW, kCellH); }

    QRgb *m_colors;
    int m_rows;
    int m_columns;
    int m_current = -1;
};

// Hue across, saturation down; a click or a drag picks both.
class HueSaturationField : public QWidget {
public:
    explicit HueSaturationField(QWidget *parent) : QWidget(parent) {
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    }

    std::function<void(int, int)> picked;
    void setHueSaturation(int hue, int saturation) {
        m_hue = hue;
        m_saturation = saturation;
        update();
    }

    QSize sizeHint() const override { return QSize(240, 160); }
    QSize minimumSizeHint() const override { return QSize(120, 80); }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        if (m_image.size() != size()) {
            m_image = QImage(size(), QImage::Format_RGB32);
            for (int y = 0; y < height(); ++y)
                for (int x = 0; x < width(); ++x)
                    m_image.setPixel(x, y, QColor::fromHsv(hueAt(x), saturationAt(y), 200).rgb());
        }
        QPainter painter(this);
        painter.drawImage(QPoint(0, 0), m_image);
        // A cross at the current hue and saturation.
        const int x = std::max(1, width() - 1) * m_hue / 359;
        const int y = std::max(1, height() - 1) * (255 - m_saturation) / 255;
        painter.setPen(QColor(Qt::black));
        painter.drawLine(x - 6, y, x - 2, y);
        painter.drawLine(x + 2, y, x + 6, y);
        painter.drawLine(x, y - 6, x, y - 2);
        painter.drawLine(x, y + 2, x, y + 6);
    }
    void mousePressEvent(QMouseEvent *event) override { pick(event); }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton)
            pick(event);
    }

private:
    int hueAt(int x) const { return std::clamp(x * 359 / std::max(1, width() - 1), 0, 359); }
    int saturationAt(int y) const { return std::clamp(255 - y * 255 / std::max(1, height() - 1), 0, 255); }
    void pick(QMouseEvent *event) {
        if (picked)
            picked(hueAt(event->pos().x()), saturationAt(event->pos().y()));
        event->accept();
    }

    QImage m_image;
    int m_hue = 0;
    int m_saturation = 0;
};

// The value of the current hue and saturation, from bright at the top to black at the bottom.
class ValueBar : public QWidget {
public:
    explicit ValueBar(QWidget *parent) : QWidget(parent) { setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding); }

    std::function<void(int)> picked;
    void setColor(int hue, int saturation, int value) {
        m_hue = hue;
        m_saturation = saturation;
        m_value = value;
        update();
    }

    QSize sizeHint() const override { return QSize(24, 160); }
    QSize minimumSizeHint() const override { return QSize(24, 80); }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        const int bar = width() - 8;
        for (int y = 0; y < height(); ++y)
            painter.fillRect(0, y, bar, 1, QColor::fromHsv(m_hue, m_saturation, valueAt(y)));
        const int y = std::max(1, height() - 1) * (255 - m_value) / 255;
        painter.setPen(palette().color(QPalette::Active, QPalette::WindowText));
        for (int d = 0; d < 4; ++d)
            painter.drawLine(bar + 1 + d, y - (3 - d), bar + 1 + d, y + (3 - d));
    }
    void mousePressEvent(QMouseEvent *event) override { pick(event); }
    void mouseMoveEvent(QMouseEvent *event) override {
        if (event->buttons() & Qt::LeftButton)
            pick(event);
    }

private:
    int valueAt(int y) const { return std::clamp(255 - y * 255 / std::max(1, height() - 1), 0, 255); }
    void pick(QMouseEvent *event) {
        if (picked)
            picked(valueAt(event->pos().y()));
        event->accept();
    }

    int m_hue = 0;
    int m_saturation = 0;
    int m_value = 0;
};

// The current color.
class ColorPreview : public QWidget {
public:
    explicit ColorPreview(QWidget *parent) : QWidget(parent) {}
    void setColor(const QColor &color) {
        m_color = color;
        update();
    }
    QSize sizeHint() const override { return QSize(60, 40); }

protected:
    void paintEvent(QPaintEvent *event) override {
        (void)event;
        QPainter painter(this);
        painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
        painter.setBrush(QColor(m_color.red(), m_color.green(), m_color.blue()));
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }

private:
    QColor m_color;
};

QSpinBox *spin(QWidget *parent, int maximum) {
    auto *box = new QSpinBox(parent);
    box->setRange(0, maximum);
    return box;
}

} // namespace

/*!
    \class QColorDialog
    \brief The QColorDialog class lets the user choose a color.

    The dialog has a grid of basic colors, a row of custom colors that the user can add to, a field of hues and
    saturations with a bar of values, and spin boxes and a text for the numbers. getColor() shows it and waits:

    \code
    const QColor color = QColorDialog::getColor(current, window, "Text Color");
    if (color.isValid())
        setTextColor(color);
    \endcode

    The custom colors are shared by all color dialogs of the application.

    \differences
    The dialog is always drawn by Nt; there is no native dialog and no eye dropper. The basic colors are a grid of
    grays and hues of Nt's own. There is no \c open() with a receiver. The texts are not translated.
    \enddifferences

    \sa QColor
*/

/*!
    \enum QColorDialog::ColorDialogOption

    Options for the look of the dialog. ColorDialogOptions is a typedef for \c{QFlags<ColorDialogOption>}.

    \value ShowAlphaChannel The user can choose the alpha channel too.
    \value NoButtons No OK and Cancel buttons, for a dialog that is used for its currentColorChanged() signal.
    \value DontUseNativeDialog Accepted for compatibility; the dialog is never native.
    \value NoEyeDropperButton Accepted for compatibility; there is no eye dropper.
*/

/*!
    Constructs a color dialog with the given \a parent. The current color is white.
*/
QColorDialog::QColorDialog(QWidget *parent) : QDialog(parent) { init(QColor(Qt::white)); }

/*!
    Constructs a color dialog with the given \a parent and the \a initial current color.
*/
QColorDialog::QColorDialog(const QColor &initial, QWidget *parent) : QDialog(parent) { init(initial); }

/*!
    Destroys the color dialog.
*/
QColorDialog::~QColorDialog() = default;

void QColorDialog::init(const QColor &initial) {
    setWindowTitle(QStringLiteral("Select Color"));
    auto *main = new QVBoxLayout(this);
    auto *columns = new QHBoxLayout;
    main->addLayout(columns);

    auto *left = new QVBoxLayout;
    columns->addLayout(left);
    left->addWidget(new QLabel(QStringLiteral("Basic colors"), this));
    auto *basic = new Swatches(this, basicColors().data(), kBasicRows, kBasicColumns);
    m_basic = basic;
    left->addWidget(basic);
    left->addWidget(new QLabel(QStringLiteral("Custom colors"), this));
    auto *custom = new Swatches(this, customColors().data(), 2, kCustomCount / 2);
    m_custom = custom;
    left->addWidget(custom);
    auto *add = new QPushButton(QStringLiteral("Add to Custom Colors"), this);
    add->setAutoDefault(false);
    left->addWidget(add);
    left->addStretch();
    basic->picked = [this](QColor c) { setColor(c, false); };
    custom->picked = [this](QColor c) { setColor(c, false); };
    QObject::connect(add, &QPushButton::clicked, this, [this, custom] {
        // Fill the selected custom cell, or the first one that is still white, or the first one.
        int at = custom->current();
        if (at < 0) {
            at = 0;
            for (int i = 0; i < kCustomCount; ++i) {
                if (customColors()[static_cast<std::size_t>(i)] == qRgb(255, 255, 255)) {
                    at = i;
                    break;
                }
            }
        }
        customColors()[static_cast<std::size_t>(at)] = m_color.rgb();
        m_custom->update();
    });

    auto *right = new QVBoxLayout;
    columns->addLayout(right);
    auto *pickers = new QHBoxLayout;
    right->addLayout(pickers);
    auto *field = new HueSaturationField(this);
    m_field = field;
    auto *valueBar = new ValueBar(this);
    m_valueBar = valueBar;
    pickers->addWidget(field);
    pickers->addWidget(valueBar);
    field->picked = [this](int h, int s) {
        m_hueKept = h;
        setColor(QColor::fromHsv(h, s, m_color.value(), m_color.alpha()), false);
    };
    valueBar->picked = [this](int v) {
        setColor(QColor::fromHsv(m_color.saturation() > 0 ? m_color.hue() : m_hueKept, m_color.saturation(), v,
                                 m_color.alpha()),
                 false);
    };

    auto *numbers = new QHBoxLayout;
    right->addLayout(numbers);
    auto *preview = new ColorPreview(this);
    m_preview = preview;
    numbers->addWidget(preview);
    auto *grid = new QGridLayout;
    numbers->addLayout(grid);
    m_hue = spin(this, 359);
    m_sat = spin(this, 255);
    m_val = spin(this, 255);
    m_red = spin(this, 255);
    m_green = spin(this, 255);
    m_blue = spin(this, 255);
    m_alpha = spin(this, 255);
    m_hue->setWrapping(true);
    const struct {
        const char *label;
        QSpinBox *box;
        int row;
        int column;
    } fields[] = {{"Hue:", m_hue, 0, 0}, {"Sat:", m_sat, 1, 0},     {"Val:", m_val, 2, 0},
                  {"Red:", m_red, 0, 2}, {"Green:", m_green, 1, 2}, {"Blue:", m_blue, 2, 2}};
    for (const auto &f : fields) {
        grid->addWidget(new QLabel(QString::fromUtf8(f.label), this), f.row, f.column);
        grid->addWidget(f.box, f.row, f.column + 1);
    }
    m_alphaLabel = new QLabel(QStringLiteral("Alpha channel:"), this);
    grid->addWidget(m_alphaLabel, 3, 0, 1, 2);
    grid->addWidget(m_alpha, 3, 2, 1, 2);
    grid->addWidget(new QLabel(QStringLiteral("HTML:"), this), 4, 0);
    m_html = new QLineEdit(this);
    grid->addWidget(m_html, 4, 1, 1, 3);
    for (QSpinBox *box : {m_hue, m_sat, m_val})
        QObject::connect(box, &QSpinBox::valueChanged, this, [this] { fromHsvSpins(); });
    for (QSpinBox *box : {m_red, m_green, m_blue, m_alpha})
        QObject::connect(box, &QSpinBox::valueChanged, this, [this] { fromRgbSpins(); });
    QObject::connect(m_html, &QLineEdit::textEdited, this, [this](const QString &text) {
        const QColor color(text.trimmed());
        if (color.isValid())
            setColor(QColor(color.red(), color.green(), color.blue(), m_color.alpha()), true);
    });

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    main->addWidget(m_buttons);
    QObject::connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    QObject::connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    setOptions(ColorDialogOptions());
    m_color = QColor(); // so that the first color counts as a change
    setColor(initial.isValid() ? initial : QColor(Qt::white), false);
}

/*!
    Sets the current color to \a color. Without ShowAlphaChannel the color is made opaque.

    \sa currentColor()
*/
void QColorDialog::setCurrentColor(const QColor &color) { setColor(color, false); }

/*!
    \fn QColor QColorDialog::currentColor() const

    Returns the color that the user is picking.

    \sa setCurrentColor(), selectedColor()
*/

/*!
    \fn QColor QColorDialog::selectedColor() const

    Returns the color that was chosen when the dialog was accepted, or an invalid color.

    \sa currentColor()
*/

/*!
    Sets \a option if \a on is \c true, and clears it otherwise.

    \sa testOption()
*/
void QColorDialog::setOption(ColorDialogOption option, bool on) {
    ColorDialogOptions options = m_options;
    options.setFlag(option, on);
    setOptions(options);
}

/*!
    \fn bool QColorDialog::testOption(ColorDialogOption option) const

    Returns whether \a option is set.
*/

/*!
    Sets the options of the dialog to \a options.

    \sa options()
*/
void QColorDialog::setOptions(ColorDialogOptions options) {
    m_options = options;
    const bool alpha = options.testFlag(ShowAlphaChannel);
    m_alpha->setVisible(alpha);
    m_alphaLabel->setVisible(alpha);
    m_buttons->setVisible(!options.testFlag(NoButtons));
    if (!alpha && m_color.isValid() && m_color.alpha() != 255)
        setColor(QColor(m_color.red(), m_color.green(), m_color.blue()), false);
}

/*!
    \fn QColorDialog::ColorDialogOptions QColorDialog::options() const

    Returns the options of the dialog.
*/

/*!
    \reimp

    An accepted dialog records the selected color and emits colorSelected().
*/
void QColorDialog::done(int result) {
    if (result == Accepted) {
        m_selected = m_color;
        emit colorSelected(m_selected);
    } else {
        m_selected = QColor();
    }
    QDialog::done(result);
}

void QColorDialog::setColor(const QColor &color, bool fromText) {
    QColor next = color;
    if (!m_options.testFlag(ShowAlphaChannel))
        next.setAlpha(255);
    if (next.saturation() > 0)
        m_hueKept = next.hue();
    const bool changed = next != m_color;
    m_color = next;
    syncControls(fromText);
    if (changed)
        emit currentColorChanged(m_color);
}

void QColorDialog::syncControls(bool fromText) {
    if (m_updating)
        return;
    m_updating = true;
    const int hue = m_color.saturation() > 0 ? m_color.hue() : m_hueKept;
    m_hue->setValue(hue);
    m_sat->setValue(m_color.saturation());
    m_val->setValue(m_color.value());
    m_red->setValue(m_color.red());
    m_green->setValue(m_color.green());
    m_blue->setValue(m_color.blue());
    m_alpha->setValue(m_color.alpha());
    if (!fromText)
        m_html->setText(m_color.name());
    static_cast<HueSaturationField *>(m_field)->setHueSaturation(hue, m_color.saturation());
    static_cast<ValueBar *>(m_valueBar)->setColor(hue, m_color.saturation(), m_color.value());
    static_cast<ColorPreview *>(m_preview)->setColor(m_color);
    m_updating = false;
}

void QColorDialog::fromHsvSpins() {
    if (m_updating)
        return;
    m_hueKept = m_hue->value();
    setColor(QColor::fromHsv(m_hue->value(), m_sat->value(), m_val->value(), m_color.alpha()), false);
}

void QColorDialog::fromRgbSpins() {
    if (m_updating)
        return;
    setColor(QColor(m_red->value(), m_green->value(), m_blue->value(), m_alpha->value()), false);
}

/*!
    Shows a color dialog with the given \a parent, window \a title and \a options, starting at the \a initial color,
    and waits for the user. Returns the chosen color, or an invalid color if the user canceled.
*/
QColor QColorDialog::getColor(const QColor &initial, QWidget *parent, const QString &title,
                              ColorDialogOptions options) {
    QColorDialog dialog(parent);
    if (!title.isEmpty())
        dialog.setWindowTitle(title);
    dialog.setOptions(options);
    dialog.setCurrentColor(initial);
    dialog.exec();
    return dialog.selectedColor();
}

/*!
    Returns the number of custom colors, 16.
*/
int QColorDialog::customCount() { return kCustomCount; }

/*!
    Returns the custom color at \a index, or an invalid color if \a index is out of range.

    \sa setCustomColor()
*/
QColor QColorDialog::customColor(int index) {
    if (index < 0 || index >= kCustomCount)
        return QColor();
    return QColor(customColors()[static_cast<std::size_t>(index)]);
}

/*!
    Sets the custom color at \a index to \a color.

    \sa customColor()
*/
void QColorDialog::setCustomColor(int index, QColor color) {
    if (index < 0 || index >= kCustomCount)
        return;
    customColors()[static_cast<std::size_t>(index)] = color.rgb();
}

/*!
    Returns the basic color at \a index, from 0 to 47, or an invalid color if \a index is out of range.

    \sa setStandardColor()
*/
QColor QColorDialog::standardColor(int index) {
    if (index < 0 || index >= kBasicRows * kBasicColumns)
        return QColor();
    return QColor(basicColors()[static_cast<std::size_t>(index)]);
}

/*!
    Sets the basic color at \a index to \a color.

    \sa standardColor()
*/
void QColorDialog::setStandardColor(int index, QColor color) {
    if (index < 0 || index >= kBasicRows * kBasicColumns)
        return;
    basicColors()[static_cast<std::size_t>(index)] = color.rgb();
}

#include "moc_qcolordialog.cpp" // NOLINT(bugprone-suspicious-include): generated by ntmoc
