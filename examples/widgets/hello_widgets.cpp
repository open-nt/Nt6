// MIT License - see LICENSE
//
// One of every widget, on the pages of a tab widget. Run it with a display, or without one:
// `hello_widgets -platform offscreen`. A number as an argument opens that page first: `hello_widgets 2`.
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

//! [form]
// A label and a widget side by side, with the labels of all rows of a page the same width.
static QHBoxLayout *row(const QString &text, QWidget *widget, QWidget *page) {
    auto *label = new QLabel(text, page);
    label->setFixedWidth(90);
    auto *layout = new QHBoxLayout;
    layout->addWidget(label);
    layout->addWidget(widget, 1);
    return layout;
}

// Buttons, check boxes and radio buttons.
static QWidget *buttonsPage(QLabel *status) {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *buttons = new QHBoxLayout;
    auto *button = new QPushButton("Click me", page);
    auto *toggle = new QPushButton("Toggle", page);
    toggle->setCheckable(true);
    auto *flat = new QPushButton("Flat", page);
    flat->setFlat(true);
    buttons->addWidget(button);
    buttons->addWidget(toggle);
    buttons->addWidget(flat);
    buttons->addStretch();
    layout->addLayout(buttons);

    auto *clicks = new QLabel("The button was clicked 0 times.", page);
    auto *shout = new QCheckBox("Shout", page);
    auto *enabled = new QCheckBox("Toggle button enabled", page);
    enabled->setChecked(true);
    layout->addWidget(clicks);
    layout->addWidget(shout);
    layout->addWidget(enabled);

    auto *group = new QGroupBox("Size", page);
    auto *groupLayout = new QHBoxLayout(group);
    auto *small = new QRadioButton("Small", group);
    auto *medium = new QRadioButton("Medium", group);
    auto *large = new QRadioButton("Large", group);
    medium->setChecked(true);
    groupLayout->addWidget(small);
    groupLayout->addWidget(medium);
    groupLayout->addWidget(large);
    groupLayout->addStretch();
    layout->addWidget(group);
    layout->addStretch();

    int count = 0;
    QObject::connect(button, &QPushButton::clicked, [=]() mutable {
        const QString text = QString("The button was clicked %1 times.").arg(++count);
        clicks->setText(shout->isChecked() ? text.toUpper() : text);
    });
    QObject::connect(enabled, &QCheckBox::toggled, toggle, &QPushButton::setEnabled);
    QObject::connect(toggle, &QPushButton::toggled,
                     [=](bool on) { status->setText(on ? "Toggled on." : "Toggled off."); });
    for (QRadioButton *choice : {small, medium, large})
        QObject::connect(choice, &QRadioButton::toggled, [=](bool on) {
            if (on)
                status->setText("Size: " + choice->text());
        });
    return page;
}

// Text and numbers to type, and a list to choose from.
static QWidget *inputPage(QLabel *status) {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);

    auto *name = new QLineEdit(page);
    name->setPlaceholderText("Your name");
    auto *password = new QLineEdit(page);
    password->setEchoMode(QLineEdit::Password);
    password->setText("secret");
    auto *age = new QSpinBox(page);
    age->setRange(0, 120);
    age->setValue(30);
    age->setSuffix(" years");
    auto *color = new QComboBox(page);
    color->addItems({"Teal", "Orange", "Green", "Purple", "Yellow", "Pink"});
    auto *greeting = new QLabel("Type a name.", page);

    layout->addLayout(row("Name", name, page));
    layout->addLayout(row("Password", password, page));
    layout->addLayout(row("Age", age, page));
    layout->addLayout(row("Color", color, page));
    layout->addSpacing(6);
    layout->addWidget(greeting);
    layout->addStretch();

    QObject::connect(name, &QLineEdit::textChanged, [=](const QString &text) {
        greeting->setText(text.isEmpty() ? "Type a name." : "Hello, " + text + "!");
    });
    QObject::connect(name, &QLineEdit::returnPressed, [=] { status->setText("Name: " + name->text()); });
    QObject::connect(age, &QSpinBox::valueChanged, [=](int value) { status->setText(QString("Age: %1").arg(value)); });
    QObject::connect(color, &QComboBox::currentIndexChanged,
                     [=](int) { status->setText("Color: " + color->currentText()); });
    return page;
}

// A slider, a vertical slider and a progress bar that show the same value.
static QWidget *valuesPage(QLabel *status) {
    auto *page = new QWidget;
    auto *layout = new QHBoxLayout(page);
    auto *left = new QVBoxLayout;

    auto *slider = new QSlider(Qt::Horizontal, page);
    slider->setRange(0, 100);
    slider->setValue(35);
    auto *bar = new QProgressBar(page);
    bar->setValue(35);
    auto *spin = new QSpinBox(page);
    spin->setRange(0, 100);
    spin->setValue(35);
    spin->setSuffix(" %");
    auto *hint = new QLabel("Drag a slider, click next to its handle, or use the arrow keys.", page);

    left->addLayout(row("Slider", slider, page));
    left->addLayout(row("Progress", bar, page));
    left->addLayout(row("Spin box", spin, page));
    left->addWidget(hint);
    left->addStretch();
    auto *tall = new QSlider(Qt::Vertical, page);
    tall->setRange(0, 100);
    tall->setValue(35);
    layout->addLayout(left, 1);
    layout->addWidget(tall);

    // Each control sets the others; setValue() does nothing when the value is the same, so there is no loop.
    for (QSlider *s : {slider, tall})
        QObject::connect(s, &QSlider::valueChanged, [=](int value) {
            slider->setValue(value);
            tall->setValue(value);
            bar->setValue(value);
            spin->setValue(value);
            status->setText(QString("Value: %1").arg(value));
        });
    QObject::connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);
    return page;
}

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Hello, widgets");

    auto *title = new QLabel("Nt Widgets", &window);
    QFont font = title->font();
    font.setPointSize(16);
    font.setBold(true);
    title->setFont(font);
    auto *status = new QLabel("Ready.", &window);

    auto *tabs = new QTabWidget(&window);
    tabs->addTab(buttonsPage(status), "Buttons");
    tabs->addTab(inputPage(status), "Input");
    tabs->addTab(valuesPage(status), "Values");
    for (const QString &argument : app.arguments()) {
        bool isNumber = false;
        const int index = argument.toInt(&isNumber);
        if (isNumber)
            tabs->setCurrentIndex(index);
    }

    auto *quit = new QPushButton("Quit", &window);
    QObject::connect(quit, &QPushButton::clicked, &app, &QApplication::quit);
    auto *bottom = new QHBoxLayout;
    bottom->addWidget(status, 1);
    bottom->addWidget(quit);

    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(title);
    layout->addWidget(tabs, 1);
    layout->addLayout(bottom);

    window.resize(480, 400);
    window.show();
    return app.exec();
}
//! [form]
