// MIT License - see LICENSE
//
// A small form: a label, a check box and buttons in layouts. Run it with a display, or without one:
// `hello_widgets -platform offscreen`.
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

//! [form]
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QWidget window;
    window.setWindowTitle("Hello, widgets");

    auto *title = new QLabel("Nt Widgets");
    QFont font = title->font();
    font.setPointSize(16);
    font.setBold(true);
    title->setFont(font);

    auto *counter = new QLabel("The button was clicked 0 times.");
    auto *shout = new QCheckBox("Shout");
    auto *button = new QPushButton("Click me");
    auto *quit = new QPushButton("Quit");

    int clicks = 0;
    QObject::connect(button, &QPushButton::clicked, [&] {
        ++clicks;
        QString text = QString("The button was clicked %1 times.").arg(clicks);
        counter->setText(shout->isChecked() ? text.toUpper() : text);
    });
    QObject::connect(quit, &QPushButton::clicked, &app, &QApplication::quit);

    auto *buttons = new QHBoxLayout;
    buttons->addStretch();
    buttons->addWidget(button);
    buttons->addWidget(quit);

    auto *layout = new QVBoxLayout(&window);
    layout->addWidget(title);
    layout->addWidget(counter);
    layout->addWidget(shout);
    layout->addStretch();
    layout->addLayout(buttons);

    window.resize(360, 200);
    window.show();
    return app.exec();
}
//! [form]
