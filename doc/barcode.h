// MIT License - see LICENSE
//
// Overview page about Nt Barcode for the NDoc documentation. This file holds only a documentation
// comment and is not part of the library. The images in doc/images/barcode are written by
// tests/test_barcode.cpp (run it with NT_UPDATE_DOC_IMAGES=1), which also checks they are current.

/*!
    \page nt-barcode.html
    \title Using Nt Barcode
    \brief How to make QR Codes, Data Matrix, Aztec and linear barcodes and draw them as images, SVG or with QPainter.

    Nt Barcode encodes data as a two-dimensional QR Code, Data Matrix or Aztec Code, or as a linear
    Code 128, GS1-128, Code 39, Interleaved 2 of 5, ITF-14, EAN or UPC symbol. Qt has no barcode
    module; QBarcode is an Nt addition in the style of the Qt value classes. The module needs
    Nt Core and Nt GUI:

    \code
    find_package(Nt6 REQUIRED)
    add_executable(labels main.cpp)
    target_link_libraries(labels PRIVATE Nt::Barcode)
    \endcode

    \section1 Making a Symbol

    The constructor encodes the data. Check isNull() if the data may not fit, or may not suit the
    symbology:

    \code
    QBarcode code(QStringLiteral("WIFI:T:WPA;S:kiosk;P:secret;;"), QBarcode::QrCode, QBarcode::LevelQ);
    if (code.isNull())
        qWarning() << code.errorString();
    \endcode

    Choose the symbology by what reads it:

    \table
    \header
        \li Symbology
        \li Used for
        \li Data
    \row
        \li \l{QBarcode::QrCode}{QrCode}
        \li Phones and camera scanners: URLs, Wi-Fi join codes, pairing data.
        \li Text in any language (a QString is encoded as UTF-8) or binary, up to 2953 bytes.
    \row
        \li \l{QBarcode::DataMatrix}{DataMatrix}
        \li Part marking, electronics, healthcare packs, MIL-STD-130 IUID labels.
        \li Text or binary: up to 3116 digits, 2335 letters or 1555 bytes.
    \row
        \li \l{QBarcode::Gs1DataMatrix}{Gs1DataMatrix}
        \li GS1 healthcare and industrial marks: GTIN, batch, expiry and serial number.
        \li Application Identifiers: \c{(01)09501101530003(17)250101(10)AB12}.
    \row
        \li \l{QBarcode::Aztec}{Aztec}
        \li Boarding passes and rail tickets on phones; needs no quiet zone.
        \li Text or binary, up to about 1900 bytes at LevelL.
    \row
        \li \l{QBarcode::Code128}{Code128}
        \li Serial numbers, asset tags, order numbers.
        \li ASCII; runs of digits are packed two to a symbol character.
    \row
        \li \l{QBarcode::Gs1Code128}{Gs1Code128}
        \li Pallet and carton labels in the supply chain.
        \li Application Identifiers, as for GS1 DataMatrix.
    \row
        \li \l{QBarcode::Code39}{Code39}
        \li Older logistics, automotive and defence labels (LOGMARS).
        \li \c{0-9 A-Z}, space and \c{-.$/+%}.
    \row
        \li \l{QBarcode::Interleaved2Of5}{Interleaved2Of5}, \l{QBarcode::Itf14}{Itf14}
        \li Outer cartons; ITF-14 holds a GTIN-14 inside bearer bars.
        \li Digits.
    \row
        \li \l{QBarcode::Ean13}{Ean13}, \l{QBarcode::Ean8}{Ean8}, \l{QBarcode::UpcA}{UpcA}, \l{QBarcode::UpcE}{UpcE}
        \li Retail product numbers at the till.
        \li Digits; QBarcode adds the check digit, or checks it if you give it.
    \endtable

    A higher \l{QBarcode::ErrorCorrectionLevel}{error correction level} lets a QR Code or Aztec Code
    survive more damage, or a logo printed over the middle, at the cost of a larger symbol.
    QBarcode::checkDigit() computes an EAN, UPC, GTIN or SSCC check digit on its own.

    \section1 Gallery

    One symbol of each kind, as toSvg() draws it with setTextVisible(true). Each image is the output
    of the test suite, which fails if an image no longer matches what QBarcode makes, and each was
    read back by a barcode scanner library.

    \section2 QR Code

    \image barcode/qrcode.svg A QR Code

    \c{QBarcode("https://open-nt.org")}: A version 2 QR Code at level M, 25 x 25 modules.

    \section2 Data Matrix

    \image barcode/datamatrix.svg A Data Matrix symbol

    \c{QBarcode("NT6-0005 Acorn", QBarcode::DataMatrix)}: 16 x 16 modules. The solid L of the
    finder and the alternating clock track frame the data.

    \section2 Rectangular Data Matrix

    \image barcode/datamatrix-rectangular.svg A rectangular Data Matrix symbol

    \c{QBarcode("NT6-0005", QBarcode::DataMatrix, QBarcode::LevelM,
    QBarcode::RectangularSymbol)}: 8 x 18 modules, for a narrow label.

    \section2 GS1 DataMatrix

    \image barcode/gs1datamatrix.svg A GS1 DataMatrix symbol

    \c{QBarcode("(01)09501101530003(17)250101(10)ABC123", QBarcode::Gs1DataMatrix)}: A GTIN, an
    expiry date and a batch number, after the FNC1 that marks GS1 data.

    \section2 Aztec Code

    \image barcode/aztec.svg An Aztec Code

    \c{QBarcode("Nt 6 Acorn ticket 0005", QBarcode::Aztec)}: A compact symbol, with its bullseye
    in the middle and no quiet zone.

    \section2 Code 128

    \image barcode/code128.svg A Code 128 symbol

    \c{QBarcode("NT-6.0.5", QBarcode::Code128)}: Code set B, with the text under the bars.

    \section2 GS1-128

    \image barcode/gs1code128.svg A GS1-128 symbol

    \c{QBarcode("(01)09501101530003(10)ABC123", QBarcode::Gs1Code128)}: The Application
    Identifiers print in parentheses; the bars hold FNC1 instead.

    \section2 Code 39

    \image barcode/code39.svg A Code 39 symbol

    \c{QBarcode("NT-605", QBarcode::Code39)}: The start and stop characters are added.

    \section2 Interleaved 2 of 5

    \image barcode/interleaved2of5.svg An Interleaved 2 of 5 symbol

    \c{QBarcode("12345678", QBarcode::Interleaved2Of5)}: Digits in pairs, the first in the bars
    and the second in the spaces.

    \section2 ITF-14

    \image barcode/itf14.svg An ITF-14 symbol

    \c{QBarcode("1540014128876", QBarcode::Itf14)}: The check digit 3 is added, and bearer bars
    run above and below across the quiet zones.

    \section2 EAN-13

    \image barcode/ean13.svg An EAN-13 symbol

    \c{QBarcode("400638133393", QBarcode::Ean13)}: The check digit 1 is added; the first digit
    prints in the quiet zone.

    \section2 EAN-8

    \image barcode/ean8.svg An EAN-8 symbol

    \c{QBarcode("9638507", QBarcode::Ean8)}: The check digit 4 is added.

    \section2 UPC-A

    \image barcode/upca.svg A UPC-A symbol

    \c{QBarcode("03600029145", QBarcode::UpcA)}: The check digit 2 is added; the first and last
    digits print small, outside the guard bars.

    \section2 UPC-E

    \image barcode/upce.svg A UPC-E symbol

    \c{QBarcode("0425261", QBarcode::UpcE)}: Stands for UPC-A 04252600006, whose check digit 4
    it carries.

    \section1 Data Matrix Encoding

    Data Matrix packs data in several ways, and QBarcode mixes them so that the data takes the
    fewest codewords, and so the smallest symbol:

    \list
    \li ASCII takes a character, or a pair of digits, per codeword.
    \li C40 and Text take three upper case or lower case letters, digits or spaces per two
        codewords, which suits serial numbers and words.
    \li X12 does the same for the EDI characters \c{0-9 A-Z * >}, space and carriage return.
    \li Base 256 takes a byte per codeword, for binary data such as compressed or encrypted
        payloads.
    \endlist

    Pass \l{QBarcode::RectangularSymbol}{RectangularSymbol} to get one of the six rectangular sizes,
    8 x 18 to 16 x 48 modules, for a narrow label or the edge of a circuit board. Data too long
    for them gets a square symbol.

    \section1 Drawing

    A symbol is a grid of modules. toImage() scales each module to a whole number of pixels, which
    keeps the edges sharp for a screen or a printer:

    \code
    QImage image = code.toImage(6);                 // 6 pixels per module, 4-module quiet zone
    QImage overlay = code.toImage(4, 2, Qt::white, Qt::transparent);
    \endcode

    toSvg() writes a scalable document, one rectangle per run of dark modules, for print
    layouts and web pages. paint() draws into any QPainter, for example in a widget's
    paintEvent(), filling the target rectangle:

    \code
    void TicketView::paintEvent(QPaintEvent *)
    {
        QPainter painter(this);
        painter.fillRect(rect(), Qt::white);
        const int side = qMin(width(), height()) - 32;
        m_code.paint(&painter, QRect(16, 16, side, side));
    }
    \endcode

    Call setTextVisible() to print the human-readable text under a linear symbol. EAN and UPC
    then extend their guard bars and set the digits in the usual groups:

    \code
    QBarcode product("400638133393", QBarcode::Ean13);
    product.setTextVisible(true);
    product.toImage(3).save(QStringLiteral("product.png"));   // digits 4 006381 333931
    \endcode

    Leave the quiet zone light. Scanners need it to find the symbol: 4 modules around a QR Code,
    1 around Data Matrix, none around Aztec, and 9 to 11 beside a linear symbol, which is what
    quietZone() returns and what toImage() and toSvg() add by default.
*/
