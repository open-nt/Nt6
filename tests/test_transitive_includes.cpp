// MIT License - see LICENSE

// Checks that headers make available the classes Qt 6 code relies on transitively. The order of
// the includes below matters: each check sits directly after the only header it may rely on.

#include <QString>

// QString reaches qpair.h (through QList), as in Qt 6.
using StringFlagPair = QPair<QString, bool>;

#include <QJsonArray>

#include <cstdio>

// QJsonArray reaches a complete QJsonDocument through qjsonvalue.h, as in Qt 6.
static QByteArray compactArray(const QJsonArray &array) {
    return QJsonDocument(array).toJson(QJsonDocument::Compact);
}

int main() {
    const StringFlagPair pair(QStringLiteral("eth0"), true);
    QJsonArray array;
    array.append(pair.first);
    array.append(pair.second);
    const QByteArray json = compactArray(array);
    if (json != QByteArray("[\"eth0\",true]")) {
        std::fprintf(stderr, "FAIL: unexpected JSON %s\n", json.constData());
        return 1;
    }
    std::printf("PASS\n");
    return 0;
}
