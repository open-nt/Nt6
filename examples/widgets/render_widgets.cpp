// MIT License - see LICENSE
//
// Renders every widget of Nt Widgets in its states, in the light color scheme, to PNG files: the pictures in the
// reference documentation. It needs no display, since it paints with QWidget::render() on the offscreen platform.
//
//   render_widgets <directory>      writes <directory>/widgets/<class>.png
//
// The CMake target `widget_images` runs it into doc/images. The PNG encoder at the top is all there is to it: Nt has
// no image file support yet.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QStyleHints>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <sys/stat.h>

namespace {

// ---- PNG: a small encoder, with deflate using the fixed Huffman codes and a search for repeated bytes ----

uint32_t crc32(const uint8_t *data, size_t size, uint32_t crc = 0) {
    static uint32_t table[256];
    static bool ready = false;
    if (!ready) {
        for (uint32_t n = 0; n < 256; ++n) {
            uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = (c & 1) ? 0xedb88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (size_t i = 0; i < size; ++i)
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return ~crc;
}

class BitWriter {
public:
    void bits(uint32_t value, int count) { // least significant bit first
        for (int i = 0; i < count; ++i)
            put((value >> i) & 1);
    }
    void code(uint32_t value, int count) { // Huffman codes go most significant bit first
        for (int i = count - 1; i >= 0; --i)
            put((value >> i) & 1);
    }
    std::vector<uint8_t> finish() {
        if (m_used)
            m_bytes.push_back(m_current);
        return m_bytes;
    }

private:
    void put(uint32_t bit) {
        m_current = static_cast<uint8_t>(m_current | (bit << m_used));
        if (++m_used == 8) {
            m_bytes.push_back(m_current);
            m_current = 0;
            m_used = 0;
        }
    }
    std::vector<uint8_t> m_bytes;
    uint8_t m_current = 0;
    int m_used = 0;
};

void literalOrLength(BitWriter &w, int symbol) {
    if (symbol < 144)
        w.code(0x30u + static_cast<uint32_t>(symbol), 8);
    else if (symbol < 256)
        w.code(0x190u + static_cast<uint32_t>(symbol - 144), 9);
    else if (symbol < 280)
        w.code(static_cast<uint32_t>(symbol - 256), 7);
    else
        w.code(0xc0u + static_cast<uint32_t>(symbol - 280), 8);
}

std::vector<uint8_t> deflate(const std::vector<uint8_t> &in) {
    static const int lengthBase[] = {3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
                                     31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
    static const int lengthExtra[] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                      2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int distBase[] = {1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
                                   33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
                                   1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int distExtra[] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    BitWriter w;
    w.bits(1, 1); // the only block
    w.bits(1, 2); // fixed Huffman codes
    const size_t n = in.size();
    const size_t window = 32768;
    std::vector<int32_t> head(1 << 15, -1), prev(n, -1);
    auto hash = [&](size_t i) { return ((in[i] << 10) ^ (in[i + 1] << 5) ^ in[i + 2]) & 0x7fff; };
    size_t i = 0;
    while (i < n) {
        size_t bestLength = 0, bestDistance = 0;
        if (i + 3 <= n) {
            const int h = hash(i);
            int32_t candidate = head[static_cast<size_t>(h)];
            for (int tries = 0; candidate >= 0 && tries < 64 && i - static_cast<size_t>(candidate) <= window; ++tries) {
                size_t length = 0;
                const size_t limit = std::min<size_t>(258, n - i);
                while (length < limit && in[static_cast<size_t>(candidate) + length] == in[i + length])
                    ++length;
                if (length > bestLength) {
                    bestLength = length;
                    bestDistance = i - static_cast<size_t>(candidate);
                    if (length == limit)
                        break;
                }
                candidate = prev[static_cast<size_t>(candidate)];
            }
        }
        const size_t advance = bestLength >= 3 ? bestLength : 1;
        if (bestLength >= 3) {
            int code = 28;
            while (lengthBase[code] > static_cast<int>(bestLength))
                --code;
            literalOrLength(w, 257 + code);
            w.bits(static_cast<uint32_t>(static_cast<int>(bestLength) - lengthBase[code]), lengthExtra[code]);
            int dcode = 29;
            while (distBase[dcode] > static_cast<int>(bestDistance))
                --dcode;
            w.code(static_cast<uint32_t>(dcode), 5);
            w.bits(static_cast<uint32_t>(static_cast<int>(bestDistance) - distBase[dcode]), distExtra[dcode]);
        } else {
            literalOrLength(w, in[i]);
        }
        for (size_t k = 0; k < advance; ++k, ++i) {
            if (i + 3 <= n) {
                const size_t h = static_cast<size_t>(hash(i));
                prev[i] = head[h];
                head[h] = static_cast<int32_t>(i);
            }
        }
    }
    literalOrLength(w, 256);
    return w.finish();
}

void chunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &data) {
    auto be32 = [&](uint32_t v) {
        for (int shift = 24; shift >= 0; shift -= 8)
            out.push_back(static_cast<uint8_t>(v >> shift));
    };
    be32(static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> body(type, type + 4);
    body.insert(body.end(), data.begin(), data.end());
    out.insert(out.end(), body.begin(), body.end());
    be32(crc32(body.data(), body.size()));
}

// Writes `image` as an 8-bit RGB PNG; the alpha channel is dropped.
bool writePng(const QImage &image, const std::string &path) {
    const int w = image.width(), h = image.height();
    const size_t stride = static_cast<size_t>(w) * 3;
    std::vector<uint8_t> raw;
    std::vector<uint8_t> previous(stride, 0), row(stride);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const QRgb p = image.pixel(x, y);
            row[static_cast<size_t>(x) * 3] = static_cast<uint8_t>(qRed(p));
            row[static_cast<size_t>(x) * 3 + 1] = static_cast<uint8_t>(qGreen(p));
            row[static_cast<size_t>(x) * 3 + 2] = static_cast<uint8_t>(qBlue(p));
        }
        // Choose among no filter, Sub and Up the one whose bytes are the smallest in sum, as a signed value.
        std::vector<uint8_t> candidates[3];
        candidates[0] = row;
        candidates[1].resize(stride);
        candidates[2].resize(stride);
        for (size_t i = 0; i < stride; ++i) {
            candidates[1][i] = static_cast<uint8_t>(row[i] - (i >= 3 ? row[i - 3] : 0));
            candidates[2][i] = static_cast<uint8_t>(row[i] - previous[i]);
        }
        int best = 0;
        long bestSum = -1;
        for (int f = 0; f < 3; ++f) {
            long sum = 0;
            for (uint8_t b : candidates[f])
                sum += b < 128 ? b : 256 - b;
            if (bestSum < 0 || sum < bestSum) {
                best = f;
                bestSum = sum;
            }
        }
        raw.push_back(static_cast<uint8_t>(best));
        raw.insert(raw.end(), candidates[best].begin(), candidates[best].end());
        previous = row;
    }
    // zlib: header, deflate, Adler-32
    std::vector<uint8_t> z{0x78, 0x01};
    const std::vector<uint8_t> compressed = deflate(raw);
    z.insert(z.end(), compressed.begin(), compressed.end());
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    const uint32_t adler = (b << 16) | a;
    for (int shift = 24; shift >= 0; shift -= 8)
        z.push_back(static_cast<uint8_t>(adler >> shift));

    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<uint8_t> header;
    for (uint32_t v : {static_cast<uint32_t>(w), static_cast<uint32_t>(h)})
        for (int shift = 24; shift >= 0; shift -= 8)
            header.push_back(static_cast<uint8_t>(v >> shift));
    header.insert(header.end(), {8, 2, 0, 0, 0}); // 8 bits, RGB, deflate, filters, no interlace
    chunk(png, "IHDR", header);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", {});
    std::FILE *file = std::fopen(path.c_str(), "wb");
    if (!file)
        return false;
    const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
    std::fclose(file);
    return ok;
}

// ---- The pictures ----

constexpr int kScale = 2; // pixels of the picture per unit of the widgets

// Puts `inner` in a widget of its own, so that radio buttons made like this do not share a group.
QWidget *alone(QWidget *inner, QWidget *parent) {
    auto *box = new QWidget(parent);
    auto *layout = new QHBoxLayout(box);
    layout->setContentsMargins(0, 0, 0, 0);
    inner->setParent(box);
    layout->addWidget(inner);
    return box;
}

// Shows `host`, gives the keyboard focus to `focus` (or to nothing), and writes its picture.
bool picture(QWidget &host, QWidget *focus, const std::string &directory, const char *name) {
    host.show();
    QCoreApplication::processEvents();
    if (QWidget *current = host.focusWidget())
        current->clearFocus();
    if (focus)
        focus->setFocus();
    QCoreApplication::processEvents();
    QImage image(host.size() * kScale, QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(kScale);
    image.fill(0u);
    host.render(&image);
    const std::string path = directory + "/widgets/" + name + ".png";
    if (!writePng(image, path)) {
        std::fprintf(stderr, "render_widgets: cannot write %s\n", path.c_str());
        return false;
    }
    std::printf("%s  %dx%d\n", path.c_str(), image.width(), image.height());
    return true;
}

// A row or a column of widgets in a window of its own.
QWidget *host(int width, int height, QBoxLayout::Direction direction = QBoxLayout::TopToBottom) {
    auto *window = new QWidget;
    window->resize(width, height);
    auto *layout = new QBoxLayout(direction, window);
    layout->setSpacing(6);
    return window;
}

bool labels(const std::string &dir) {
    QWidget &w = *host(240, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto *plain = new QLabel("A label shows a text.", &w);
    auto *bold = new QLabel("The same, in bold and larger.", &w);
    QFont font = bold->font();
    font.setBold(true);
    font.setPointSize(font.pointSize() + 3);
    bold->setFont(font);
    auto *wrapped = new QLabel("With word wrap on, a long text is broken into lines to fit the width.", &w);
    wrapped->setWordWrap(true);
    wrapped->setFixedHeight(2 * wrapped->fontMetrics().height() + 2); // a label has no height for its width yet
    auto *right = new QLabel("Aligned to the right", &w);
    right->setAlignment(Qt::AlignRight);
    auto *disabled = new QLabel("A disabled label", &w);
    disabled->setEnabled(false);
    for (QLabel *l : {plain, bold, wrapped, right, disabled})
        layout->addWidget(l);
    w.resize(240, w.sizeHint().height());
    return picture(w, nullptr, dir, "qlabel");
}

bool pushButtons(const std::string &dir) {
    QWidget &w = *host(380, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto row = [&](std::initializer_list<QPushButton *> buttons) {
        auto *line = new QHBoxLayout;
        for (QPushButton *b : buttons)
            line->addWidget(b);
        line->addStretch();
        layout->addLayout(line);
    };
    auto *normal = new QPushButton("Normal", &w);
    auto *focused = new QPushButton("Focused", &w);
    auto *pressed = new QPushButton("Pressed", &w);
    pressed->setDown(true);
    auto *checked = new QPushButton("Checked", &w);
    checked->setCheckable(true);
    checked->setChecked(true);
    auto *flat = new QPushButton("Flat", &w);
    flat->setFlat(true);
    auto *disabled = new QPushButton("Disabled", &w);
    disabled->setEnabled(false);
    row({normal, focused, pressed});
    row({checked, flat, disabled});
    w.resize(380, w.sizeHint().height());
    return picture(w, focused, dir, "qpushbutton");
}

bool checkBoxes(const std::string &dir) {
    QWidget &w = *host(220, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto *off = new QCheckBox("Unchecked", &w);
    auto *on = new QCheckBox("Checked", &w);
    on->setChecked(true);
    auto *focused = new QCheckBox("Focused", &w);
    auto *disabled = new QCheckBox("Disabled", &w);
    disabled->setEnabled(false);
    auto *disabledOn = new QCheckBox("Disabled and checked", &w);
    disabledOn->setChecked(true);
    disabledOn->setEnabled(false);
    for (QCheckBox *b : {off, on, focused, disabled, disabledOn})
        layout->addWidget(b);
    w.resize(220, w.sizeHint().height());
    return picture(w, focused, dir, "qcheckbox");
}

bool radioButtons(const std::string &dir) {
    QWidget &w = *host(220, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    // Each in a widget of its own, so that they are not exclusive to each other.
    auto *off = new QRadioButton("Not chosen", &w);
    auto *on = new QRadioButton("Chosen", &w);
    on->setChecked(true);
    auto *focused = new QRadioButton("Focused", &w);
    auto *disabled = new QRadioButton("Disabled", &w);
    disabled->setEnabled(false);
    auto *disabledOn = new QRadioButton("Disabled and chosen", &w);
    disabledOn->setChecked(true);
    disabledOn->setEnabled(false);
    for (QRadioButton *b : {off, on, focused, disabled, disabledOn})
        layout->addWidget(alone(b, &w));
    w.resize(220, w.sizeHint().height());
    return picture(w, focused, dir, "qradiobutton");
}

bool lineEdits(const std::string &dir) {
    QWidget &w = *host(240, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto *placeholder = new QLineEdit(&w);
    placeholder->setPlaceholderText("Placeholder text");
    auto *typed = new QLineEdit("Text with the cursor", &w);
    typed->setCursorPosition(9);
    auto *selected = new QLineEdit("Some selected text", &w);
    selected->setSelection(5, 8);
    auto *password = new QLineEdit("secret", &w);
    password->setEchoMode(QLineEdit::Password);
    auto *readOnly = new QLineEdit("Read-only text", &w);
    readOnly->setReadOnly(true);
    auto *disabled = new QLineEdit("Disabled", &w);
    disabled->setEnabled(false);
    for (QLineEdit *e : {placeholder, typed, selected, password, readOnly, disabled})
        layout->addWidget(e);
    w.resize(240, w.sizeHint().height());
    return picture(w, typed, dir, "qlineedit");
}

bool sliders(const std::string &dir) {
    QWidget &w = *host(280, 120, QBoxLayout::LeftToRight);
    auto *left = new QVBoxLayout;
    auto *low = new QSlider(Qt::Horizontal, &w);
    low->setValue(0);
    auto *middle = new QSlider(Qt::Horizontal, &w);
    middle->setValue(35);
    auto *high = new QSlider(Qt::Horizontal, &w);
    high->setValue(99);
    auto *focused = new QSlider(Qt::Horizontal, &w);
    focused->setValue(60);
    auto *disabled = new QSlider(Qt::Horizontal, &w);
    disabled->setValue(35);
    disabled->setEnabled(false);
    for (QSlider *s : {low, middle, high, focused, disabled})
        left->addWidget(s);
    left->addStretch();
    auto *vertical = new QSlider(Qt::Vertical, &w);
    vertical->setValue(60);
    static_cast<QBoxLayout *>(w.layout())->addLayout(left, 1);
    w.layout()->addWidget(vertical);
    return picture(w, focused, dir, "qslider");
}

bool progressBars(const std::string &dir) {
    QWidget &w = *host(240, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto *none = new QProgressBar(&w);
    auto *some = new QProgressBar(&w);
    some->setValue(35);
    auto *full = new QProgressBar(&w);
    full->setValue(100);
    auto *format = new QProgressBar(&w);
    format->setRange(0, 200);
    format->setValue(50);
    format->setFormat("%v of %m");
    auto *silent = new QProgressBar(&w);
    silent->setValue(70);
    silent->setTextVisible(false);
    auto *disabled = new QProgressBar(&w);
    disabled->setValue(35);
    disabled->setEnabled(false);
    for (QProgressBar *b : {none, some, full, format, silent, disabled})
        layout->addWidget(b);
    w.resize(240, w.sizeHint().height());
    return picture(w, nullptr, dir, "qprogressbar");
}

bool spinBoxes(const std::string &dir) {
    QWidget &w = *host(200, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto *plain = new QSpinBox(&w);
    plain->setValue(35);
    auto *focused = new QSpinBox(&w);
    focused->setRange(0, 100);
    focused->setValue(72);
    focused->setSuffix(" %");
    auto *prefixed = new QSpinBox(&w);
    prefixed->setRange(-100, 100);
    prefixed->setValue(-5);
    prefixed->setPrefix("Offset ");
    auto *atEnd = new QSpinBox(&w);
    atEnd->setValue(99);
    auto *disabled = new QSpinBox(&w);
    disabled->setValue(12);
    disabled->setEnabled(false);
    for (QSpinBox *s : {plain, focused, prefixed, atEnd, disabled})
        layout->addWidget(s);
    w.resize(200, w.sizeHint().height());
    return picture(w, focused, dir, "qspinbox");
}

bool comboBoxes(const std::string &dir) {
    QWidget &w = *host(220, 200);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    const QStringList items{"Teal", "Orange", "Green", "Purple"};
    auto *closed = new QComboBox(&w);
    closed->addItems(items);
    auto *disabled = new QComboBox(&w);
    disabled->addItems(items);
    disabled->setEnabled(false);
    auto *open = new QComboBox(&w);
    open->addItems(items);
    open->setCurrentIndex(1);
    layout->addWidget(closed);
    layout->addWidget(disabled);
    layout->addWidget(open);
    layout->addStretch();
    w.show();
    QCoreApplication::processEvents();
    open->showPopup();
    return picture(w, nullptr, dir, "qcombobox");
}

bool groupBoxes(const std::string &dir) {
    QWidget &w = *host(260, 10);
    auto *layout = static_cast<QBoxLayout *>(w.layout());
    auto make = [&](const char *title, bool checkable, bool checked) {
        auto *group = new QGroupBox(title, &w);
        auto *inner = new QHBoxLayout(group);
        inner->addWidget(new QCheckBox("An option", group));
        inner->addWidget(new QPushButton("A button", group));
        // The group box enables and disables the widgets that are in it, so they come first.
        group->setCheckable(checkable);
        group->setChecked(checked);
        layout->addWidget(group);
    };
    make("A group box", false, true);
    make("Checkable and checked", true, true);
    make("Checkable and unchecked", true, false);
    w.resize(260, w.sizeHint().height());
    return picture(w, nullptr, dir, "qgroupbox");
}

bool tabWidgets(const std::string &dir) {
    QWidget &w = *host(300, 150);
    auto *tabs = new QTabWidget(&w);
    for (const char *name : {"General", "Details", "Disabled"}) {
        auto *page = new QWidget;
        auto *layout = new QVBoxLayout(page);
        layout->addWidget(new QLabel(QString("The page \"%1\".").arg(name), page));
        layout->addWidget(new QPushButton("A button", page));
        layout->addStretch();
        tabs->addTab(page, name);
    }
    tabs->setTabEnabled(2, false);
    w.layout()->addWidget(tabs);
    return picture(w, nullptr, dir, "qtabwidget");
}

} // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: render_widgets <directory>\n");
        return 2;
    }
    // The pictures do not depend on the machine: no display, and sizes in units of one pixel.
    ::setenv("QT_QPA_PLATFORM", "offscreen", 1);
    ::setenv("QT_SCALE_FACTOR", "1", 1);
    QApplication app(argc, argv);
    app.styleHints()->setColorScheme(Qt::ColorScheme::Light);

    const std::string directory = argv[1];
    ::mkdir(directory.c_str(), 0755);
    ::mkdir((directory + "/widgets").c_str(), 0755);
    bool ok = true;
    for (bool (*render)(const std::string &) : {labels, pushButtons, checkBoxes, radioButtons, lineEdits, sliders,
                                                progressBars, spinBoxes, comboBoxes, groupBoxes, tabWidgets})
        ok = render(directory) && ok;
    return ok ? 0 : 1;
}
