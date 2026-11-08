// MIT License - see LICENSE
//
// A tour of Nt Widgets: layouts, buttons, check boxes, labels, custom widgets that paint with the palette
// and react to the mouse, wheel and keyboard, a dark theme, and enabling and disabling groups of widgets.
// Run it with a display, or without one: `gallery -platform offscreen`.
#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStyleHints>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

namespace {

//! [logo]
// The outlines of the letters of the Nt logo (assets/nt-logo.svg), in its 256 by 256 coordinate system: x and y pairs.
const float kLetterN[] = {
    57.05f,  179.38f, 57.05f,  76.62f,  86.15f,  76.62f,  117.35f, 164.68f, 119.60f, 164.68f, 119.60f, 76.62f,
    136.25f, 76.62f,  136.25f, 179.38f, 107.75f, 179.38f, 75.80f,  91.17f,  73.70f,  91.17f,  73.70f,  179.38f,
};
const float kLetterT[] = {
    198.35f, 118.33f, 177.65f, 118.33f, 177.65f, 151.32f, 177.67f, 153.51f, 177.73f, 155.51f, 177.84f, 157.32f,
    177.99f, 158.93f, 178.18f, 160.36f, 178.41f, 161.59f, 178.68f, 162.62f, 179.00f, 163.47f, 179.40f, 164.17f,
    179.94f, 164.78f, 180.60f, 165.30f, 181.40f, 165.72f, 182.33f, 166.05f, 183.39f, 166.28f, 184.58f, 166.42f,
    185.90f, 166.47f, 198.20f, 166.03f, 198.95f, 179.07f, 196.51f, 179.53f, 194.22f, 179.93f, 192.09f, 180.26f,
    190.10f, 180.54f, 188.26f, 180.75f, 186.58f, 180.91f, 185.04f, 181.00f, 183.65f, 181.03f, 180.59f, 180.94f,
    177.78f, 180.66f, 175.22f, 180.21f, 172.91f, 179.57f, 170.85f, 178.74f, 169.04f, 177.74f, 167.49f, 176.55f,
    166.18f, 175.18f, 165.07f, 173.55f, 164.11f, 171.60f, 163.30f, 169.33f, 162.63f, 166.73f, 162.12f, 163.82f,
    161.75f, 160.57f, 161.52f, 157.01f, 161.45f, 153.12f, 161.45f, 118.33f, 151.85f, 118.33f, 151.85f, 104.38f,
    161.45f, 104.38f, 161.45f, 82.62f,  177.65f, 82.62f,  177.65f, 104.38f, 198.35f, 104.38f,
};
// The hexagon is drawn with a stroke 24 wide and round joins, which is the polygon grown by 12.
const float kHexagon[] = {128, 24, 218, 76, 218, 180, 128, 232, 38, 180, 38, 76};
constexpr float kHexagonGrow = 12.0f;

bool insidePolygon(const float *xy, int points, float x, float y) {
    bool inside = false;
    for (int i = 0, j = points - 1; i < points; j = i++) {
        const float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            inside = !inside;
    }
    return inside;
}

float distanceToPolygon(const float *xy, int points, float x, float y) {
    float best = 1e9f;
    for (int i = 0, j = points - 1; i < points; j = i++) {
        const float ax = xy[2 * j], ay = xy[2 * j + 1], bx = xy[2 * i], by = xy[2 * i + 1];
        const float dx = bx - ax, dy = by - ay;
        const float t = std::clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
        best = std::min(best, std::hypot(x - (ax + t * dx), y - (ay + t * dy)));
    }
    return best;
}

// Draws the logo into an image of `size` by `size` pixels, with 4 by 4 samples per pixel for smooth edges.
QImage renderLogo(int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(0u);
    const float scale = 256.0f / static_cast<float>(size);
    const int teal[3] = {0x13, 0x97, 0x7f};
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            int covered = 0, white = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float x = (static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / 4.0f) * scale;
                    const float y = (static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / 4.0f) * scale;
                    if (insidePolygon(kLetterN, 12, x, y) || insidePolygon(kLetterT, 53, x, y)) {
                        ++covered;
                        ++white;
                    } else if (insidePolygon(kHexagon, 6, x, y) ||
                               distanceToPolygon(kHexagon, 6, x, y) <= kHexagonGrow) {
                        ++covered;
                    }
                }
            }
            if (covered == 0)
                continue;
            auto channel = [&](int tealValue) { return (white * 255 + (covered - white) * tealValue) / 16; };
            const int a = covered * 255 / 16;
            uchar *line = image.scanLine(py) + px * 4;
            const uint value = (static_cast<uint>(a) << 24) | (static_cast<uint>(channel(teal[0])) << 16) |
                               (static_cast<uint>(channel(teal[1])) << 8) | static_cast<uint>(channel(teal[2]));
            std::copy_n(reinterpret_cast<const uchar *>(&value), 4, line);
        }
    }
    return image;
}

// The Nt logo, drawn from its outlines.
class Logo : public QWidget {
public:
    explicit Logo(int size, QWidget *parent = nullptr) : QWidget(parent), m_size(size) { setFixedSize(size, size); }

protected:
    void paintEvent(QPaintEvent *) override {
        // Draw one pixel per device pixel, so that the logo stays sharp on a high-resolution screen.
        const qreal ratio = devicePixelRatio();
        if (m_image.isNull() || m_image.devicePixelRatio() != ratio) {
            m_image = renderLogo(qRound(m_size * ratio));
            m_image.setDevicePixelRatio(ratio);
        }
        QPainter painter(this);
        painter.drawImage(0, 0, m_image);
    }

private:
    int m_size;
    QImage m_image;
};
//! [logo]

//! [meter]
// A bar that shows a value from 0 to 100. Click or drag it, turn the mouse wheel over it, or use the arrow
// keys while it has the focus.
class Meter : public QWidget {
public:
    explicit Meter(QWidget *parent = nullptr) : QWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    int value() const { return m_value; }
    void setValue(int value) {
        value = std::clamp(value, 0, 100);
        if (value == m_value)
            return;
        m_value = value;
        update();
        if (onChanged)
            onChanged(value);
    }
    void setAccent(const QColor &color) {
        m_accent = color;
        update();
    }

    QSize sizeHint() const override { return QSize(220, fontMetrics().height() + 12); }

    std::function<void(int)> onChanged;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        const QRect box = rect().adjusted(0, 0, -1, -1);
        painter.fillRect(rect(), palette().color(group, QPalette::Base));
        const int filled = (box.width() - 1) * m_value / 100;
        painter.fillRect(1, 1, filled, box.height() - 1,
                         isEnabled() ? m_accent : palette().color(group, QPalette::Mid));
        painter.setPen(hasFocus() ? palette().color(QPalette::Highlight)
                                  : palette().color(group, QPalette::Mid).darker(130));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(box);
        painter.setPen(palette().color(group, QPalette::Text));
        painter.drawText(rect(), Qt::AlignCenter, QString("%1 %").arg(m_value));
    }
    void mousePressEvent(QMouseEvent *event) override { setFromX(event->pos().x()); }
    void mouseMoveEvent(QMouseEvent *event) override { setFromX(event->pos().x()); }
    void wheelEvent(QWheelEvent *event) override {
        setValue(m_value + event->angleDelta().y() / 120 * 5);
        event->accept();
    }
    void keyPressEvent(QKeyEvent *event) override {
        switch (event->key()) {
        case Qt::Key_Left:
        case Qt::Key_Down:
            setValue(m_value - 5);
            break;
        case Qt::Key_Right:
        case Qt::Key_Up:
            setValue(m_value + 5);
            break;
        case Qt::Key_Home:
            setValue(0);
            break;
        case Qt::Key_End:
            setValue(100);
            break;
        default:
            QWidget::keyPressEvent(event);
        }
    }

private:
    void setFromX(int x) { setValue(qRound(100.0 * x / std::max(1, width() - 1))); }

    int m_value = 35;
    QColor m_accent{"#3d8ec9"};
};
//! [meter]

// A small square of color that can be chosen.
class Swatch : public QWidget {
public:
    Swatch(const QColor &color, QWidget *parent) : QWidget(parent), m_color(color) {
        setFixedSize(26, 26);
        setFocusPolicy(Qt::StrongFocus);
    }

    void setSelected(bool selected) {
        m_selected = selected;
        update();
    }
    QColor color() const { return m_color; }
    std::function<void(Swatch *)> onChosen;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const QColor face = underMouse() && isEnabled() ? m_color.lighter(115) : m_color;
        painter.fillRect(rect(), isEnabled() ? face : face.darker(160));
        painter.setPen(QPen(QBrush(m_selected ? palette().color(QPalette::WindowText) : palette().color(QPalette::Mid)),
                            m_selected ? 2 : 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(m_selected ? rect().adjusted(1, 1, -2, -2) : rect().adjusted(0, 0, -1, -1));
        if (hasFocus()) {
            painter.setPen(palette().color(QPalette::Highlight));
            painter.drawRect(rect().adjusted(3, 3, -4, -4));
        }
    }
    void mousePressEvent(QMouseEvent *) override { choose(); }
    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_Space || event->key() == Qt::Key_Return)
            choose();
        else
            event->ignore();
    }

private:
    void choose() {
        if (onChosen)
            onChosen(this);
    }

    QColor m_color;
    bool m_selected = false;
};

// A surface that follows the mouse: it draws cross hairs at the pointer and reports its position.
class Pad : public QWidget {
public:
    explicit Pad(QWidget *parent = nullptr) : QWidget(parent) {
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setAutoFillBackground(true);
    }

    QSize sizeHint() const override { return QSize(220, 90); }
    std::function<void(const QString &)> onStatus;

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        const QPalette::ColorGroup group = isEnabled() ? QPalette::Active : QPalette::Disabled;
        painter.fillRect(rect(), palette().color(group, QPalette::Base));
        painter.setPen(palette().color(group, QPalette::Mid).lighter(115));
        for (int x = 0; x < width(); x += 20)
            painter.drawLine(x, 0, x, height());
        for (int y = 0; y < height(); y += 20)
            painter.drawLine(0, y, width(), y);
        if (m_inside && isEnabled()) {
            painter.setPen(palette().color(QPalette::Highlight));
            painter.drawLine(m_mouse.x(), 0, m_mouse.x(), height());
            painter.drawLine(0, m_mouse.y(), width(), m_mouse.y());
            painter.setBrush(m_down ? palette().color(QPalette::Highlight) : palette().color(QPalette::Base));
            painter.drawEllipse(m_mouse.x() - 6, m_mouse.y() - 6, 12, 12);
        }
        painter.setPen(palette().color(group, QPalette::Mid).darker(130));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(rect().adjusted(0, 0, -1, -1));
    }
    void enterEvent(QEnterEvent *event) override {
        m_inside = true;
        m_mouse = event->pos();
        update();
    }
    void leaveEvent(QEvent *) override {
        m_inside = false;
        update();
        report("The mouse left the pad.");
    }
    void mouseMoveEvent(QMouseEvent *event) override {
        m_mouse = event->pos();
        update();
        report(QString("Pad: %1, %2%3").arg(m_mouse.x()).arg(m_mouse.y()).arg(m_down ? " (button down)" : ""));
    }
    void mousePressEvent(QMouseEvent *event) override {
        m_down = true;
        m_mouse = event->pos();
        update();
        report(QString("Pad: pressed at %1, %2").arg(m_mouse.x()).arg(m_mouse.y()));
    }
    void mouseReleaseEvent(QMouseEvent *) override {
        m_down = false;
        update();
    }

private:
    void report(const QString &text) {
        if (onStatus)
            onStatus(text);
    }

    QPoint m_mouse;
    bool m_inside = false;
    bool m_down = false;
};

QLabel *heading(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
}

} // namespace

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Nt Widgets gallery");
    auto *root = new QVBoxLayout(&window);
    root->setSpacing(8);

    QFont titleFont = window.font();
    titleFont.setPointSize(17);
    titleFont.setBold(true);
    auto *title = new QLabel("Nt Widgets", &window);
    title->setFont(titleFont);
    auto *subtitle = new QLabel("Layouts, buttons, check boxes and custom widgets.", &window);
    auto *header = new QHBoxLayout;
    header->setSpacing(12);
    auto *words = new QVBoxLayout;
    words->setSpacing(2);
    words->addStretch();
    words->addWidget(title);
    words->addWidget(subtitle);
    words->addStretch();
    header->addWidget(new Logo(72, &window));
    header->addLayout(words, 1);
    root->addLayout(header);

    // Appearance
    root->addWidget(heading("Appearance", &window));
    auto *appearance = new QHBoxLayout;
    auto *dark = new QCheckBox("Dark theme", &window);
    auto *large = new QCheckBox("Large text", &window);
    auto *enabled = new QCheckBox("Controls enabled", &window);
    enabled->setChecked(true);
    appearance->addWidget(dark);
    appearance->addWidget(large);
    appearance->addWidget(enabled);
    appearance->addStretch();
    root->addLayout(appearance);

    // The controls that the last check box switches
    auto *controls = new QWidget(&window);
    auto *controlsLayout = new QVBoxLayout(controls);
    controlsLayout->setContentsMargins(0, 0, 0, 0);
    controlsLayout->setSpacing(8);
    root->addWidget(controls);

    controlsLayout->addWidget(heading("Meter", controls));
    auto *meterRow = new QHBoxLayout;
    auto *minus = new QPushButton("-", controls);
    auto *plus = new QPushButton("+", controls);
    minus->setFixedWidth(34);
    plus->setFixedWidth(34);
    auto *meter = new Meter(controls);
    meterRow->addWidget(minus);
    meterRow->addWidget(meter);
    meterRow->addWidget(plus);
    controlsLayout->addLayout(meterRow);
    auto *meterHint = new QLabel("Click or drag the bar, turn the wheel over it, or use the arrow keys.", controls);
    QFont small = meterHint->font();
    small.setPointSize(small.pointSize() - 1);
    meterHint->setFont(small);
    controlsLayout->addWidget(meterHint);

    controlsLayout->addWidget(heading("Accent color", controls));
    auto *swatches = new QHBoxLayout;
    swatches->setSpacing(6);
    std::vector<Swatch *> all;
    for (const char *name : {"#3d8ec9", "#e4572e", "#29a35a", "#c77dff", "#f2a900", "#e63980"}) {
        auto *swatch = new Swatch(QColor(name), controls);
        all.push_back(swatch);
        swatches->addWidget(swatch);
    }
    swatches->addStretch();
    controlsLayout->addLayout(swatches);
    all.front()->setSelected(true);

    controlsLayout->addWidget(heading("Mouse pad", controls));
    auto *pad = new Pad(controls);
    controlsLayout->addWidget(pad, 1);

    // The status line and the buttons
    auto *status = new QLabel("Ready.", &window);
    root->addWidget(status);
    auto *buttons = new QHBoxLayout;
    auto *reset = new QPushButton("Reset", &window);
    auto *toggle = new QPushButton("Flat button", &window);
    toggle->setCheckable(true);
    toggle->setFlat(true);
    auto *quit = new QPushButton("Quit", &window);
    buttons->addWidget(toggle);
    buttons->addStretch();
    buttons->addWidget(reset);
    buttons->addWidget(quit);
    root->addLayout(buttons);

    // Behavior
    meter->onChanged = [&](int value) { status->setText(QString("Meter: %1 %").arg(value)); };
    QObject::connect(minus, &QPushButton::clicked, [&] { meter->setValue(meter->value() - 5); });
    QObject::connect(plus, &QPushButton::clicked, [&] { meter->setValue(meter->value() + 5); });
    for (Swatch *swatch : all) {
        swatch->onChosen = [&, swatch](Swatch *chosen) {
            for (Swatch *other : all)
                other->setSelected(other == chosen);
            meter->setAccent(swatch->color());
            status->setText(QString("Accent color: %1").arg(swatch->color().name()));
        };
    }
    pad->onStatus = [&](const QString &text) { status->setText(text); };
    // The check box starts with the color scheme of the desktop and follows it when it changes.
    dark->setChecked(app.styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    QObject::connect(app.styleHints(), &QStyleHints::colorSchemeChanged,
                     [dark](Qt::ColorScheme scheme) { dark->setChecked(scheme == Qt::ColorScheme::Dark); });
    QObject::connect(dark, &QCheckBox::toggled, [&](bool on) {
        app.styleHints()->setColorScheme(on ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
        status->setText(on ? "Dark theme on." : "Dark theme off.");
    });
    QObject::connect(large, &QCheckBox::toggled, [&](bool on) {
        QFont font = window.font();
        font.setPointSize(on ? 14 : 10);
        window.setFont(font);
        title->setFont(titleFont);
        status->setText(on ? "Large text on." : "Large text off.");
    });
    QObject::connect(enabled, &QCheckBox::toggled, [&](bool on) {
        controls->setEnabled(on);
        status->setText(on ? "Controls enabled." : "Controls disabled.");
    });
    QObject::connect(toggle, &QPushButton::toggled, [&](bool on) {
        toggle->setText(on ? "Flat button (checked)" : "Flat button");
        status->setText(on ? "The flat button is checked." : "The flat button is unchecked.");
    });
    QObject::connect(reset, &QPushButton::clicked, [&] {
        app.styleHints()->unsetColorScheme();
        large->setChecked(false);
        enabled->setChecked(true);
        toggle->setChecked(false);
        meter->setValue(35);
        all.front()->onChosen(all.front());
        status->setText("Everything is back to how it started.");
    });
    QObject::connect(quit, &QPushButton::clicked, &app, &QApplication::quit);

    window.resize(480, 520);
    window.show();
    return app.exec();
}
