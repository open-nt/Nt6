// MIT License - see LICENSE
#pragma once

#include <QtGui/qfont.h>
#include <QtWidgets/qdialog.h>

class QDialogButtonBox;
class QLineEdit;
class QListWidget;
class QSpinBox;

class QFontDialog : public QDialog {
    Q_OBJECT
    Q_PROPERTY(QFont currentFont READ currentFont WRITE setCurrentFont NOTIFY currentFontChanged)
public:
    enum FontDialogOption {
        NoButtons = 0x00000001,
        DontUseNativeDialog = 0x00000002,
        ScalableFonts = 0x00000004,
        NonScalableFonts = 0x00000008,
        MonospacedFonts = 0x00000010,
        ProportionalFonts = 0x00000020
    };
    Q_DECLARE_FLAGS(FontDialogOptions, FontDialogOption)

    explicit QFontDialog(QWidget *parent = nullptr);
    explicit QFontDialog(const QFont &initial, QWidget *parent = nullptr);
    ~QFontDialog() override;

    void setCurrentFont(const QFont &font);
    QFont currentFont() const { return m_font; }
    QFont selectedFont() const { return m_selected; }
    void setOption(FontDialogOption option, bool on = true);
    bool testOption(FontDialogOption option) const { return m_options.testFlag(option); }
    void setOptions(FontDialogOptions options);
    FontDialogOptions options() const { return m_options; }

    static QFont getFont(bool *ok, const QFont &initial, QWidget *parent = nullptr, const QString &title = QString(),
                         FontDialogOptions options = FontDialogOptions());
    static QFont getFont(bool *ok, QWidget *parent = nullptr);

signals:
    /*!
        This signal is emitted when the current font changes to \a font, as the user picks it.
    */
    void currentFontChanged(const QFont &font) { emitSignal(&QFontDialog::currentFontChanged, font); }
    /*!
        This signal is emitted when the dialog is accepted, with the chosen \a font.
    */
    void fontSelected(const QFont &font) { emitSignal(&QFontDialog::fontSelected, font); }

protected:
    void done(int result) override;

private:
    void init(const QFont &initial);
    void fromControls();
    void syncControls();

    QListWidget *m_families = nullptr;
    QListWidget *m_styles = nullptr;
    QListWidget *m_sizes = nullptr;
    QSpinBox *m_size = nullptr;
    QLineEdit *m_sample = nullptr;
    QDialogButtonBox *m_buttons = nullptr;
    QFont m_font;
    QFont m_selected;
    FontDialogOptions m_options;
    bool m_updating = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFontDialog::FontDialogOptions)
