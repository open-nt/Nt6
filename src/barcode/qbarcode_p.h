// MIT License - see LICENSE
//
// Internals of Nt Barcode: the shared data of QBarcode, Galois-field arithmetic and Reed-Solomon
// error correction for the two-dimensional symbologies, and the encoder of each symbology.
#pragma once

#include "qbarcode.h"

#include <QtCore/qlist.h>
#include <QtCore/qshareddata.h>

#include <vector>

// One piece of human-readable text under a linear symbol, in module units: x is the centre of the
// text relative to the left edge of the bars; small marks the digits that UPC prints outside the
// guard bars in a smaller size.
struct QBarcodeTextItem {
    double x = 0.0;
    QString text;
    bool small = false;
};

class QBarcodePrivate : public QSharedData {
public:
    QBarcode::Symbology symbology = QBarcode::QrCode;
    QBarcode::ErrorCorrectionLevel level = QBarcode::LevelM;
    QBarcode::Options options;
    QByteArray data;
    QString error;
    QString text;
    // One byte per module, dark = 1. A two-dimensional symbol stores width * height modules; a
    // linear one stores a single row of width bars.
    QByteArray modules;
    // For EAN and UPC: 1 for each column whose bar extends into the text band (the guard bars).
    QByteArray guards;
    QList<QBarcodeTextItem> textItems;
    int width = 0;
    int height = 0;
    int version = 0;
    int mask = -1;
    int bearer = 0; // ITF-14: thickness of the bearer bars above and below, in modules
    bool linear = false;
    bool textVisible = false;

    void fail(const QString &message);
    void setMatrix(int size, const std::vector<quint8> &dark);
    void setMatrix(int columns, int rows, const std::vector<quint8> &dark);
    void setBars(const QByteArray &bars, int barHeight);
};

namespace QtBarcodePrivate {

// GF(2^m) given by its primitive polynomial (with the x^m term).
class GaloisField {
public:
    GaloisField(int bits, int primitive);
    int size() const { return m_size; }
    int exp(int power) const { return m_exp[std::size_t(power % (m_size - 1))]; }
    int multiply(int a, int b) const;

private:
    int m_size;
    std::vector<int> m_exp;
    std::vector<int> m_log;
};

// The check words of data under a Reed-Solomon code whose generator has the roots
// a^base ... a^(base + count - 1).
std::vector<int> reedSolomon(const GaloisField &field, const std::vector<int> &data, int count, int base);

// FNC1 in the character lists given to the Code 128 and Data Matrix encoders.
constexpr int kFnc1 = -1;

// Parses GS1 element strings written as (AI)value...: appends FNC1, the AIs and values, and FNC1
// after each variable-length field that another follows, to chars; sets text to the
// human-readable form. Returns an empty string on success, or the error.
QString parseGs1(const QByteArray &data, std::vector<int> *chars, QString *text);

bool allDigits(const QByteArray &data);
int mod10CheckDigit(const QByteArray &digits);

void encodeQr(QBarcodePrivate *d);
void encodeDataMatrix(QBarcodePrivate *d);
void encodeAztec(QBarcodePrivate *d);
void encodeCode128(QBarcodePrivate *d);
void encodeGs1Code128(QBarcodePrivate *d);
void encodeEanUpc(QBarcodePrivate *d);
void encodeCode39(QBarcodePrivate *d);
void encodeInterleaved2Of5(QBarcodePrivate *d);

} // namespace QtBarcodePrivate
