// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

#include <memory>

class QCharacterDisplayPrivate;

class QCharacterDisplay : public QObject {
    Q_OBJECT
    Q_PROPERTY(Bus bus READ bus WRITE setBus)
    Q_PROPERTY(int columns READ columns)
    Q_PROPERTY(int rows READ rows)
    Q_PROPERTY(bool backlight READ hasBacklight WRITE setBacklight)
    Q_PROPERTY(DisplayError error READ error RESET clearError NOTIFY errorOccurred)
public:
    enum Bus { I2cPcf8574, Gpio4Bit };
    Q_ENUM(Bus)
    enum DisplayError { NoError, DeviceError, NotOpenError, UnsupportedOperationError };
    Q_ENUM(DisplayError)

    explicit QCharacterDisplay(QObject *parent = nullptr);
    ~QCharacterDisplay() override;

    void setBus(Bus bus);
    Bus bus() const;
    void setSize(int columns, int rows);
    int columns() const;
    int rows() const;
    void setI2cDevice(const QString &bus, int address = 0x27);
    void setGpioLines(const QString &registerSelect, const QString &enable, const QString &d4, const QString &d5,
                      const QString &d6, const QString &d7);
    void setBacklightLine(const QString &line);

    bool open();
    void close();
    bool isOpen() const;

    bool clear();
    bool home();
    bool setCursorPosition(int column, int row);
    bool print(const QString &text);
    bool setText(const QString &text);
    bool writeData(const QByteArray &codes);
    bool setCustomCharacter(int slot, const QByteArray &pattern);

    bool setDisplayEnabled(bool enabled);
    bool setCursorVisible(bool visible);
    bool setCursorBlinking(bool blinking);
    bool setBacklight(bool on);
    bool hasBacklight() const;

    static QByteArray toDisplayCodes(const QString &text);

    DisplayError error() const;
    QString errorString() const;
    void clearError();

signals:
    /*!
        This signal is emitted when the error \a displayError occurs. It is not emitted when the
        error is cleared.

        \sa error(), errorString()
    */
    void errorOccurred(QCharacterDisplay::DisplayError displayError) {
        emitSignal(&QCharacterDisplay::errorOccurred, displayError);
    }

private:
    friend class QCharacterDisplayPrivate;
    std::unique_ptr<QCharacterDisplayPrivate> d;
};
