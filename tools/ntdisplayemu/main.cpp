// MIT License - see LICENSE
//
// ntdisplayemu: a bench of small displays, switches and LEDs, for developing Nt GPIO programs
// without the hardware. It plays SSD1322 OLED panels on SPI, HD44780 character LCDs on a PCF8574
// I2C backpack or on GPIO lines, and every GPIO line a program opens: inputs become switches to
// click, outputs LEDs that light up. Start it, then start the program with NT_GPIO_EMULATOR set to
// its name, and QGpioLine, QSpiDevice and QI2cDevice talk to the bench instead of /dev:
//
//   ntdisplayemu
//   NT_GPIO_EMULATOR=ntdisplayemu ./myapp
//   NT_GPIO_EMULATOR=ntdisplayemu ./myguiapp -platform ssd1322:dc=gpiochip0:25:reset=gpiochip0:24
//
// Without options the bench has one 256x64 SSD1322 on spidev0.0 (D/C gpiochip0:25, RES
// gpiochip0:24) and one 16x2 HD44780 on i2c-1 at 0x27. Options replace that:
//
//   --ssd1322 spi=spidev1.0,dc=gpiochip0:5,reset=gpiochip0:6,size=256x64,offset=28
//   --hd44780 i2c=i2c-1,address=0x3f,size=20x4
//   --hd44780 rs=gpiochip0:7,e=gpiochip0:8,d4=gpiochip0:9,d5=gpiochip0:10,d6=gpiochip0:11,d7=gpiochip0:12,size=16x2
//   --switch gpiochip0:17=PTT --led gpiochip0:5=Power --name bench2
//   --button gpiochip0:20=Up --button gpiochip0:21=Select,high
//
// The color of each OLED panel (white, green, yellow or red) is chosen in the window, as the panel
// fitted to a board would be; neither the command line nor the program sets it.
#include "bench.h"

#include <QtCore/qcommandlineparser.h>
#include <QtCore/qhash.h>
#include <QtCore/qtimer.h>
#include <QtGui/qfont.h>
#include <QtGui/qpainter.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qcombobox.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qgroupbox.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>
#include <QtWidgets/qwidget.h>

#include <cstdio>
#include <cstdlib>
#include <deque>

namespace {

// Parses "key=value,key=value"; values may contain colons and equals signs.
QHash<QString, QString> parseSpec(const QString &spec) {
    QHash<QString, QString> out;
    for (const QString &part : spec.split(QChar(','))) {
        const qsizetype eq = part.indexOf(QChar('='));
        if (eq > 0)
            out.insert(part.left(eq).trimmed().toLower(), part.mid(eq + 1).trimmed());
    }
    return out;
}

bool parseSize(const QString &text, QSize &size) {
    const QStringList wh = text.split(QChar('x'));
    bool okW = false;
    bool okH = false;
    if (wh.size() != 2)
        return false;
    const int w = wh.at(0).toInt(&okW);
    const int h = wh.at(1).toInt(&okH);
    if (!okW || !okH || w <= 0 || h <= 0)
        return false;
    size = QSize(w, h);
    return true;
}

[[noreturn]] void usageError(const QString &message) {
    std::fprintf(stderr, "ntdisplayemu: %s\n", message.toUtf8().constData());
    std::exit(2);
}

// The panel of an SSD1322, each pixel a square of light with a dark gap around it.
class OledView : public QWidget {
public:
    OledView(const Bench *bench, std::size_t index, QSize size, QWidget *parent = nullptr)
        : QWidget(parent), m_bench(bench), m_index(index), m_size(size) {
        m_scale = size.width() <= 128 ? 5 : 3;
        setFixedSize(size.width() * m_scale + 24, size.height() * m_scale + 24);
    }

    void setTint(const QColor &tint) {
        m_tint = tint;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        const std::vector<QImage> images = m_bench->oledImages();
        QImage frame(m_size.width() * m_scale, m_size.height() * m_scale, QImage::Format_RGB32);
        frame.fill(QColor(8, 8, 10));
        if (m_index < images.size()) {
            const QImage &gray = images[m_index];
            for (int y = 0; y < gray.height(); ++y) {
                const uchar *in = gray.constScanLine(y);
                for (int x = 0; x < gray.width(); ++x) {
                    if (!in[x])
                        continue;
                    const double k = in[x] / 255.0;
                    const quint32 color = qRgb(static_cast<int>(m_tint.red() * k), static_cast<int>(m_tint.green() * k),
                                               static_cast<int>(m_tint.blue() * k));
                    for (int dy = 0; dy < m_scale - 1; ++dy) {
                        auto *out = reinterpret_cast<quint32 *>(frame.scanLine(y * m_scale + dy)) + x * m_scale;
                        for (int dx = 0; dx < m_scale - 1; ++dx)
                            out[dx] = color;
                    }
                }
            }
        }
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), QColor(30, 30, 34));
        p.drawImage(QPoint(12, 12), frame);
    }

private:
    const Bench *m_bench;
    std::size_t m_index;
    QSize m_size;
    QColor m_tint{235, 240, 255};
    int m_scale = 3;
};

// A character LCD: cells of 5x8 dots on a backlit glass, custom characters drawn dot by dot and the
// others with a font.
class LcdView : public QWidget {
public:
    static constexpr int Dot = 4;
    static constexpr int CellW = 6 * Dot;
    static constexpr int CellH = 9 * Dot;

    LcdView(const Bench *bench, std::size_t index, int columns, int rows, QWidget *parent = nullptr)
        : QWidget(parent), m_bench(bench), m_index(index) {
        setFixedSize(columns * CellW + 32, rows * CellH + 32);
        auto *blink = new QTimer(this);
        QObject::connect(blink, &QTimer::timeout, this, [this] {
            m_blinkPhase = !m_blinkPhase;
            update();
        });
        blink->start(500);
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.fillRect(QRect(0, 0, width(), height()), QColor(20, 40, 20));
        m_bench->read([&](const auto &, const auto &characters, const auto &) {
            if (m_index >= characters.size())
                return;
            const Hd44780Model &m = *characters[m_index].model;
            const bool lit = m.backlight();
            const QColor glass = lit ? QColor(150, 200, 60) : QColor(70, 90, 40);
            const QColor dotOff = lit ? QColor(138, 186, 54) : QColor(64, 82, 36);
            const QColor dotOn = QColor(20, 30, 20);
            p.fillRect(QRect(8, 8, width() - 16, height() - 16), glass);
            QFont font(QStringLiteral("monospace"));
            font.setPixelSize(CellH - 2 * Dot);
            p.setFont(font);
            const QPoint cursor = m.cursor();
            for (int r = 0; r < m.rows(); ++r) {
                for (int c = 0; c < m.columns(); ++c) {
                    const QRect cell(16 + c * CellW, 16 + r * CellH, 5 * Dot, 8 * Dot);
                    p.fillRect(cell, dotOff);
                    if (!m.displayOn())
                        continue;
                    const quint8 code = m.codeAt(c, r);
                    if (code < 16) {
                        const QByteArray rows = m.glyph(code);
                        for (int y = 0; y < 8; ++y) {
                            for (int x = 0; x < 5; ++x) {
                                if (static_cast<quint8>(rows.at(y)) & (0x10 >> x))
                                    p.fillRect(QRect(cell.x() + x * Dot, cell.y() + y * Dot, Dot - 1, Dot - 1), dotOn);
                            }
                        }
                    } else if (code != ' ') {
                        p.setPen(dotOn);
                        p.drawText(QRectF(cell), Qt::AlignCenter, QString(Hd44780Model::toUnicode(code)));
                    }
                    if (cursor == QPoint(c, r)) {
                        if (m.cursorVisible())
                            p.fillRect(QRect(cell.x(), cell.y() + 7 * Dot, 5 * Dot, Dot - 1), dotOn);
                        if (m.cursorBlinking() && m_blinkPhase)
                            p.fillRect(cell, dotOn);
                    }
                }
            }
        });
    }

private:
    const Bench *m_bench;
    std::size_t m_index;
    bool m_blinkPhase = false;
};

// A round LED.
class LedView : public QWidget {
public:
    explicit LedView(QWidget *parent = nullptr) : QWidget(parent) { setFixedSize(18, 18); }
    void setOn(bool on) {
        if (on != m_on) {
            m_on = on;
            update();
        }
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QColor(40, 40, 40));
        p.setBrush(m_on ? QColor(255, 60, 40) : QColor(90, 30, 25));
        p.drawEllipse(QRect(2, 2, 14, 14));
    }

private:
    bool m_on = false;
};

// The GPIO lines: a row per line, added as programs open them.
class LinePanel : public QGroupBox {
public:
    explicit LinePanel(Bench *bench, QWidget *parent = nullptr)
        : QGroupBox(QStringLiteral("GPIO"), parent), m_bench(bench) {
        m_grid = new QGridLayout(this);
        m_empty = new QLabel(QStringLiteral("Lines appear here as programs open them."), this);
        m_grid->addWidget(m_empty, 0, 0, 1, 3);
        refresh();
    }

    void refresh() {
        const std::vector<Bench::Line> lines = m_bench->lines();
        for (const Bench::Line &line : lines) {
            Row *row = find(line.name);
            if (!row)
                row = add(line);
            if (row->led)
                row->led->setOn(line.level);
            if (row->button && !row->momentary) {
                row->button->setChecked(line.level);
                row->button->setText(line.level ? QStringLiteral("High") : QStringLiteral("Low"));
            }
        }
        m_empty->setVisible(lines.empty());
    }

private:
    struct Row {
        QString name;
        bool output = false;
        LedView *led = nullptr;
        QPushButton *button = nullptr;
        bool momentary = false;
    };

    Row *find(const QString &name) {
        for (Row &row : m_rows) {
            if (row.name == name)
                return &row;
        }
        return nullptr;
    }

    Row *add(const Bench::Line &line) {
        const int r = static_cast<int>(m_rows.size()) + 1;
        Row row;
        row.name = line.name;
        row.output = line.output;
        m_grid->addWidget(new QLabel(line.label.isEmpty() ? line.name : line.label, this), r, 0);
        m_grid->addWidget(new QLabel(line.output      ? QStringLiteral("output")
                                     : line.momentary ? QStringLiteral("button")
                                                      : QStringLiteral("switch"),
                                     this),
                          r, 1);
        if (line.output) {
            row.led = new LedView(this);
            m_grid->addWidget(row.led, r, 2);
        } else {
            const QString name = line.name;
            if (line.momentary) {
                // A push button: pressed while the mouse holds it down.
                row.button = new QPushButton(QStringLiteral("Push"), this);
                row.momentary = true;
                const bool pressed = line.pressedLevel;
                QObject::connect(row.button, &QPushButton::pressed, this,
                                 [this, name, pressed] { m_bench->setInput(name, pressed); });
                QObject::connect(row.button, &QPushButton::released, this,
                                 [this, name, pressed] { m_bench->setInput(name, !pressed); });
            } else {
                row.button = new QPushButton(QStringLiteral("Low"), this);
                row.button->setCheckable(true);
                QObject::connect(row.button, &QPushButton::toggled, this,
                                 [this, name](bool checked) { m_bench->setInput(name, checked); });
            }
            m_grid->addWidget(row.button, r, 2);
        }
        m_rows.push_back(row);
        return &m_rows.back();
    }

    Bench *m_bench;
    QGridLayout *m_grid;
    QLabel *m_empty;
    std::deque<Row> m_rows;
};

// The colors SSD1322 panels are made in, white first as the default.
struct PanelColor {
    const char *name;
    QColor color;
};
const PanelColor panelColors[] = {
    {"White", QColor(235, 240, 255)},
    {"Green", QColor(80, 255, 140)},
    {"Yellow", QColor(255, 200, 40)},
    {"Red", QColor(255, 70, 50)},
};

} // namespace

int main(int argc, char **argv) {
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ntdisplayemu"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Emulates SSD1322 and HD44780 displays, switches and LEDs for Nt GPIO "
                       "programs started with NT_GPIO_EMULATOR=<name>."));
    parser.addHelpOption();
    const QCommandLineOption nameOption(QStringList{QStringLiteral("name")},
                                        QStringLiteral("The server name or socket path (default ntdisplayemu)."),
                                        QStringLiteral("name"), QStringLiteral("ntdisplayemu"));
    const QCommandLineOption oledOption(QStringList{QStringLiteral("ssd1322")},
                                        QStringLiteral("Adds an SSD1322: spi=,dc=,reset=,size=WxH,offset=."),
                                        QStringLiteral("spec"));
    const QCommandLineOption lcdOption(
        QStringList{QStringLiteral("hd44780")},
        QStringLiteral("Adds an HD44780: i2c=,address=,size=CxR or rs=,e=,d4=..d7=,size=CxR."), QStringLiteral("spec"));
    const QCommandLineOption switchOption(QStringList{QStringLiteral("switch")},
                                          QStringLiteral("Declares an input line, NAME or NAME=label."),
                                          QStringLiteral("line"));
    const QCommandLineOption buttonOption(QStringList{QStringLiteral("button")},
                                          QStringLiteral("Declares a push button, NAME or NAME=label; ',high' "
                                                         "makes it pull the line high instead of low."),
                                          QStringLiteral("line"));
    const QCommandLineOption ledOption(QStringList{QStringLiteral("led")},
                                       QStringLiteral("Declares an output line, NAME or NAME=label."),
                                       QStringLiteral("line"));
    parser.addOptions({nameOption, oledOption, lcdOption, switchOption, buttonOption, ledOption});
    parser.process(app);

    Bench bench;
    QStringList oleds = parser.values(oledOption);
    QStringList lcds = parser.values(lcdOption);
    if (oleds.isEmpty() && lcds.isEmpty()) {
        oleds.append(QStringLiteral("spi=spidev0.0,dc=gpiochip0:25,reset=gpiochip0:24"));
        lcds.append(QStringLiteral("i2c=i2c-1,address=0x27,size=16x2"));
    }
    std::vector<QSize> oledSizes;
    for (const QString &spec : oleds) {
        const QHash<QString, QString> v = parseSpec(spec);
        Ssd1322Model::Wiring wiring;
        wiring.spi = v.value(QStringLiteral("spi"), wiring.spi);
        wiring.dc = v.value(QStringLiteral("dc"), QStringLiteral("gpiochip0:25"));
        wiring.reset = v.value(QStringLiteral("reset"));
        if (v.contains(QStringLiteral("size")) && !parseSize(v.value(QStringLiteral("size")), wiring.size))
            usageError(QStringLiteral("bad size in --ssd1322 %1").arg(spec));
        if (v.contains(QStringLiteral("offset")))
            wiring.columnOffset = v.value(QStringLiteral("offset")).toInt(nullptr, 0);
        if (wiring.size.width() > 480 || wiring.size.height() > 128 ||
            wiring.columnOffset * 4 + wiring.size.width() > 480)
            usageError(QStringLiteral("the panel of --ssd1322 %1 does not fit the controller").arg(spec));
        bench.addSsd1322(wiring);
        oledSizes.push_back(wiring.size);
    }
    std::vector<QSize> lcdSizes;
    for (const QString &spec : lcds) {
        const QHash<QString, QString> v = parseSpec(spec);
        QSize size(16, 2);
        if (v.contains(QStringLiteral("size")) && !parseSize(v.value(QStringLiteral("size")), size))
            usageError(QStringLiteral("bad size in --hd44780 %1").arg(spec));
        if (size.height() > 4 || size.width() > 40 || size.width() * size.height() > 80)
            usageError(QStringLiteral("an HD44780 drives at most 4 rows, 40 columns and 80 characters"));
        if (v.contains(QStringLiteral("rs"))) {
            std::array<QString, 6> names;
            const char *keys[] = {"rs", "e", "d4", "d5", "d6", "d7"};
            for (std::size_t i = 0; i < names.size(); ++i) {
                names[i] = v.value(QString::fromLatin1(keys[i]));
                if (names[i].isEmpty())
                    usageError(QStringLiteral("--hd44780 on GPIO needs rs, e and d4 to d7"));
            }
            bench.addHd44780Gpio(names, size.width(), size.height());
        } else {
            bench.addHd44780I2c(v.value(QStringLiteral("i2c"), QStringLiteral("i2c-1")),
                                v.value(QStringLiteral("address"), QStringLiteral("0x27")).toInt(nullptr, 0),
                                size.width(), size.height());
        }
        lcdSizes.push_back(size);
    }
    const auto splitLabel = [](const QString &spec, QString &name, QString &label) {
        const qsizetype eq = spec.indexOf(QChar('='));
        name = eq < 0 ? spec : spec.left(eq);
        label = eq < 0 ? QString() : spec.mid(eq + 1);
    };
    for (const QString &spec : parser.values(switchOption)) {
        QString name, label;
        splitLabel(spec, name, label);
        bench.addSwitch(name, label);
    }
    for (QString spec : parser.values(buttonOption)) {
        const bool high = spec.endsWith(QStringLiteral(",high"));
        if (high || spec.endsWith(QStringLiteral(",low")))
            spec = spec.left(spec.lastIndexOf(QChar(',')));
        QString name, label;
        splitLabel(spec, name, label);
        bench.addButton(name, label, high);
    }
    for (const QString &spec : parser.values(ledOption)) {
        QString name, label;
        splitLabel(spec, name, label);
        bench.addLed(name, label);
    }

    const QString serverName = parser.value(nameOption);
    QString error;
    if (!bench.listen(serverName, error)) {
        std::fprintf(stderr, "ntdisplayemu: cannot listen on %s: %s\n", serverName.toUtf8().constData(),
                     error.toUtf8().constData());
        return 1;
    }

    QWidget window;
    window.setWindowTitle(QStringLiteral("ntdisplayemu"));
    auto *layout = new QVBoxLayout(&window);
    std::vector<QWidget *> views;
    for (std::size_t i = 0; i < oledSizes.size(); ++i) {
        auto *view = new OledView(&bench, i, oledSizes[i], &window);
        auto *row = new QHBoxLayout;
        row->addWidget(view);
        auto *color = new QComboBox(&window);
        for (const PanelColor &c : panelColors)
            color->addItem(QString::fromLatin1(c.name));
        QObject::connect(color, &QComboBox::currentIndexChanged, view, [view](int index) {
            if (index >= 0 && index < static_cast<int>(std::size(panelColors)))
                view->setTint(panelColors[index].color);
        });
        row->addWidget(color);
        row->addStretch();
        layout->addLayout(row);
        views.push_back(view);
    }
    for (std::size_t i = 0; i < lcdSizes.size(); ++i) {
        auto *view = new LcdView(&bench, i, lcdSizes[i].width(), lcdSizes[i].height(), &window);
        layout->addWidget(view);
        views.push_back(view);
    }
    auto *lines = new LinePanel(&bench, &window);
    layout->addWidget(lines);
    auto *statusLabel = new QLabel(&window);
    layout->addWidget(statusLabel);
    const auto showStatus = [&bench, statusLabel, serverName] {
        statusLabel->setText(QStringLiteral("Listening on %1, %2 connection(s). Run programs with NT_GPIO_EMULATOR=%3")
                                 .arg(bench.serverPath())
                                 .arg(bench.connectionCount())
                                 .arg(serverName));
    };
    showStatus();

    // Repaint at most 60 times a second however fast the programs write.
    QTimer repaint;
    repaint.setSingleShot(true);
    repaint.setInterval(16);
    QObject::connect(&repaint, &QTimer::timeout, &window, [&] {
        for (QWidget *view : views)
            view->update();
        lines->refresh();
        showStatus();
    });
    bench.changed = [&repaint] {
        if (!repaint.isActive())
            repaint.start();
    };

    std::printf("ntdisplayemu: listening on %s; run programs with NT_GPIO_EMULATOR=%s\n",
                bench.serverPath().toUtf8().constData(), serverName.toUtf8().constData());
    std::fflush(stdout);
    window.show();
    const int status = app.exec();
    bench.changed = nullptr; // the timer goes before the bench
    return status;
}
