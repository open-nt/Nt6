// MIT License - see LICENSE
#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QLabel label(QStringLiteral("Nt application with its own activity, service and permissions"));
    label.show();
    return app.exec();
}
