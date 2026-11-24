// MIT License - see LICENSE
#include "counter.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    auto *layout = new QVBoxLayout(&window);
    auto *label = new QLabel(QStringLiteral("0"));
    auto *button = new QPushButton(QStringLiteral("Count"));
    layout->addWidget(label);
    layout->addWidget(button);
    Counter counter;
    QObject::connect(button, &QPushButton::clicked, &counter, &Counter::increment);
    QObject::connect(&counter, &Counter::changed, [label](int value) { label->setText(QString::number(value)); });
    window.show();
    return app.exec();
}
