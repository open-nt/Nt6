// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qflags.h>
#include <QtCore/qrect.h>
#include <QtCore/qshareddata.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtGui/qcolor.h>
#include <QtGui/qimage.h>

class QPainter;
class QBarcodePrivate;

class QBarcode {
public:
    enum Symbology {
        QrCode,
        Code128,
        Ean13,
        Ean8,
        UpcA,
        UpcE,
        Gs1Code128,
        Code39,
        Interleaved2Of5,
        Itf14,
        DataMatrix,
        Aztec,
        Gs1DataMatrix
    };
    enum ErrorCorrectionLevel { LevelL, LevelM, LevelQ, LevelH };
    enum Option { NoOption = 0x0, RectangularSymbol = 0x1 };
    Q_DECLARE_FLAGS(Options, Option)

    QBarcode();
    explicit QBarcode(const QByteArray &data, Symbology symbology = QrCode, ErrorCorrectionLevel level = LevelM,
                      Options options = NoOption);
    explicit QBarcode(const QString &text, Symbology symbology = QrCode, ErrorCorrectionLevel level = LevelM,
                      Options options = NoOption);
    explicit QBarcode(const char *text, Symbology symbology = QrCode, ErrorCorrectionLevel level = LevelM,
                      Options options = NoOption);
    QBarcode(const QBarcode &other);
    QBarcode(QBarcode &&other) noexcept;
    ~QBarcode();
    QBarcode &operator=(const QBarcode &other);
    QBarcode &operator=(QBarcode &&other) noexcept;
    void swap(QBarcode &other) noexcept { d.swap(other.d); }

    bool isNull() const;
    QString errorString() const;

    QByteArray data() const;
    QString text() const;
    Symbology symbology() const;
    ErrorCorrectionLevel errorCorrectionLevel() const;
    Options options() const;
    int version() const;
    int maskPattern() const;
    bool isLinear() const;

    bool isTextVisible() const;
    void setTextVisible(bool visible);

    QSize size() const;
    bool isDark(int x, int y) const;
    int quietZone() const;
    QSize imageSize(int moduleSize = 4, int quietZone = -1) const;

    QImage toImage(int moduleSize = 4, int quietZone = -1, const QColor &dark = QColor(0, 0, 0),
                   const QColor &light = QColor(255, 255, 255)) const;
    QByteArray toSvg(int moduleSize = 4, int quietZone = -1, const QColor &dark = QColor(0, 0, 0),
                     const QColor &light = QColor(255, 255, 255)) const;
    void paint(QPainter *painter, const QRect &target, const QColor &dark = QColor(0, 0, 0)) const;

    static int checkDigit(const QByteArray &digits);

private:
    QSharedDataPointer<QBarcodePrivate> d;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QBarcode::Options)
