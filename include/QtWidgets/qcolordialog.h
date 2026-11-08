// MIT License - see LICENSE
#pragma once

#include <QtGui/qcolor.h>
#include <QtWidgets/qdialog.h>

class QDialogButtonBox;
class QLineEdit;
class QSpinBox;
class QLabel;

class QColorDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor NOTIFY currentColorChanged)
public:
    enum ColorDialogOption {
        ShowAlphaChannel = 0x00000001,
        NoButtons = 0x00000002,
        DontUseNativeDialog = 0x00000004,
        NoEyeDropperButton = 0x00000008
    };
    Q_DECLARE_FLAGS(ColorDialogOptions, ColorDialogOption)

    explicit QColorDialog(QWidget *parent = nullptr);
    explicit QColorDialog(const QColor &initial, QWidget *parent = nullptr);
    ~QColorDialog() override;

    void setCurrentColor(const QColor &color);
    QColor currentColor() const { return m_color; }
    QColor selectedColor() const { return m_selected; }
    void setOption(ColorDialogOption option, bool on = true);
    bool testOption(ColorDialogOption option) const { return m_options.testFlag(option); }
    void setOptions(ColorDialogOptions options);
    ColorDialogOptions options() const { return m_options; }

    static QColor getColor(const QColor &initial = Qt::white, QWidget *parent = nullptr,
                           const QString &title = QString(), ColorDialogOptions options = ColorDialogOptions());
    static int customCount();
    static QColor customColor(int index);
    static void setCustomColor(int index, QColor color);
    static QColor standardColor(int index);
    static void setStandardColor(int index, QColor color);

signals:
    /*!
        This signal is emitted when the current color changes to \a color, as the user picks it.
    */
    void currentColorChanged(const QColor &color) { emitSignal(&QColorDialog::currentColorChanged, color); }
    /*!
        This signal is emitted when the dialog is accepted, with the chosen \a color.
    */
    void colorSelected(const QColor &color) { emitSignal(&QColorDialog::colorSelected, color); }

protected:
    void done(int result) override;

private:
    void init(const QColor &initial);
    void setColor(const QColor &color, bool fromText);
    void syncControls(bool fromText);
    void fromHsvSpins();
    void fromRgbSpins();

    QWidget *m_basic = nullptr;
    QWidget *m_custom = nullptr;
    QWidget *m_field = nullptr;
    QWidget *m_valueBar = nullptr;
    QWidget *m_preview = nullptr;
    QSpinBox *m_hue = nullptr;
    QSpinBox *m_sat = nullptr;
    QSpinBox *m_val = nullptr;
    QSpinBox *m_red = nullptr;
    QSpinBox *m_green = nullptr;
    QSpinBox *m_blue = nullptr;
    QSpinBox *m_alpha = nullptr;
    QLabel *m_alphaLabel = nullptr;
    QLineEdit *m_html = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QColor m_color;
    QColor m_selected;
    ColorDialogOptions m_options;
    int m_hueKept = 0; // the hue of a gray, which has none of its own
    bool m_updating = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QColorDialog::ColorDialogOptions)
