// MIT License - see LICENSE
//
// The Nt logo, and one of every widget on the pages of a tab widget. The first page, Nt, is a showcase of all the
// widgets that Nt has, each one live. Run it with a display, or without one: `hello_widgets -platform offscreen`.
// A number as an argument opens that page first: `hello_widgets 3`.
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

#include "ntlogo.h"

//! [form]
// A label and a widget side by side, with the labels of all rows of a page the same width.
static QHBoxLayout *row(const QString &text, QWidget *widget, QWidget *page, int labelWidth = 90) {
    auto *label = new QLabel(text, page);
    label->setFixedWidth(labelWidth);
    auto *layout = new QHBoxLayout;
    layout->addWidget(label);
    layout->addWidget(widget, 1);
    return layout;
}

// The widgets of Nt, one row each: the name of the class and a live example. Everything reports to the status line.
static QWidget *showcasePage(QLabel *status) {
    auto *page = new QWidget;
    auto *layout = new QVBoxLayout(page);
    layout->setSpacing(5);
    const int names = 104;

    auto *label = new QLabel("Plain text, nothing to edit.", page);
    layout->addLayout(row("QLabel", label, page, names));

    auto *buttons = new QWidget(page);
    auto *buttonsLayout = new QHBoxLayout(buttons);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    auto *push = new QPushButton("Click me", buttons);
    auto *toggle = new QPushButton("Toggle", buttons);
    toggle->setCheckable(true);
    auto *flat = new QPushButton("Flat", buttons);
    flat->setFlat(true);
    buttonsLayout->addWidget(push);
    buttonsLayout->addWidget(toggle);
    buttonsLayout->addWidget(flat);
    buttonsLayout->addStretch();
    layout->addLayout(row("QPushButton", buttons, page, names));
    int clicks = 0;
    QObject::connect(push, &QPushButton::clicked,
                     [=]() mutable { status->setText(QString("Clicked %1 times.").arg(++clicks)); });
    QObject::connect(toggle, &QPushButton::toggled,
                     [=](bool on) { status->setText(on ? "Toggled on." : "Toggled off."); });

    auto *check = new QCheckBox("Check me", page);
    layout->addLayout(row("QCheckBox", check, page, names));
    QObject::connect(check, &QCheckBox::toggled, [=](bool on) { status->setText(on ? "Checked." : "Unchecked."); });

    // Radio buttons with the same parent are exclusive, so they get a widget of their own.
    auto *radios = new QWidget(page);
    auto *radiosLayout = new QHBoxLayout(radios);
    radiosLayout->setContentsMargins(0, 0, 0, 0);
    auto *yes = new QRadioButton("Yes", radios);
    auto *no = new QRadioButton("No", radios);
    yes->setChecked(true);
    radiosLayout->addWidget(yes);
    radiosLayout->addWidget(no);
    radiosLayout->addStretch();
    layout->addLayout(row("QRadioButton", radios, page, names));
    for (QRadioButton *choice : {yes, no})
        QObject::connect(choice, &QRadioButton::toggled, [=](bool on) {
            if (on)
                status->setText("Chose " + choice->text() + ".");
        });

    auto *edit = new QLineEdit(page);
    edit->setPlaceholderText("Type here");
    layout->addLayout(row("QLineEdit", edit, page, names));
    QObject::connect(edit, &QLineEdit::textChanged, [=](const QString &text) { status->setText("Text: " + text); });

    auto *spin = new QSpinBox(page);
    spin->setRange(0, 100);
    spin->setValue(35);
    spin->setSuffix(" %");
    layout->addLayout(row("QSpinBox", spin, page, names));

    auto *combo = new QComboBox(page);
    combo->addItems({"Teal", "Orange", "Green", "Purple"});
    layout->addLayout(row("QComboBox", combo, page, names));
    QObject::connect(combo, &QComboBox::currentIndexChanged,
                     [=](int) { status->setText("Chose " + combo->currentText() + "."); });

    auto *slider = new QSlider(Qt::Horizontal, page);
    slider->setRange(0, 100);
    slider->setValue(35);
    layout->addLayout(row("QSlider", slider, page, names));

    auto *bar = new QProgressBar(page);
    bar->setValue(35);
    layout->addLayout(row("QProgressBar", bar, page, names));
    // The spin box, the slider and the progress bar show one value; setValue() does nothing for the same value.
    QObject::connect(slider, &QSlider::valueChanged, spin, &QSpinBox::setValue);
    QObject::connect(slider, &QSlider::valueChanged, bar, &QProgressBar::setValue);
    QObject::connect(spin, &QSpinBox::valueChanged, slider, &QSlider::setValue);

    auto *group = new QGroupBox("Checkable", page);
    group->setCheckable(true);
    auto *groupLayout = new QHBoxLayout(group);
    groupLayout->addWidget(new QLabel("Unchecking disables what is inside.", group));
    layout->addLayout(row("QGroupBox", group, page, names));

    auto *tabs = new QTabWidget(page);
    for (const char *name : {"Tabs", "Pages"}) {
        auto *inner = new QWidget;
        auto *innerLayout = new QVBoxLayout(inner);
        innerLayout->addWidget(new QLabel(QString("The page \"%1\".").arg(name), inner));
        tabs->addTab(inner, name);
    }
    tabs->setFixedHeight(80);
    layout->addLayout(row("QTabWidget", tabs, page, names));
    layout->addStretch();
    return page;
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

    // The header: the logo, the name and what the example is.
    auto *title = new QLabel("Nt Widgets", &window);
    QFont font = title->font();
    font.setPointSize(16);
    font.setBold(true);
    title->setFont(font);
    auto *subtitle = new QLabel("One of every widget, live.", &window);
    auto *words = new QVBoxLayout;
    words->setSpacing(2);
    words->addStretch();
    words->addWidget(title);
    words->addWidget(subtitle);
    words->addStretch();
    auto *header = new QHBoxLayout;
    header->setSpacing(12);
    header->addWidget(new NtExamples::Logo(56, &window));
    header->addLayout(words, 1);
    auto *status = new QLabel("Ready.", &window);

    auto *tabs = new QTabWidget(&window);
    tabs->addTab(showcasePage(status), "Nt");
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
    layout->addLayout(header);
    layout->addWidget(tabs, 1);
    layout->addLayout(bottom);

    window.resize(500, 600);
    window.show();
    return app.exec();
}
//! [form]
