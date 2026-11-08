// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtGui/qfont.h>
#include <QtWidgets/qcombobox.h>

class QFontComboBox : public QComboBox {
    Q_OBJECT
    Q_PROPERTY(FontFilters fontFilters READ fontFilters WRITE setFontFilters)
    Q_PROPERTY(QFont currentFont READ currentFont WRITE setCurrentFont NOTIFY currentFontChanged)
public:
    enum FontFilter {
        AllFonts = 0,
        ScalableFonts = 0x1,
        NonScalableFonts = 0x2,
        MonospacedFonts = 0x4,
        ProportionalFonts = 0x8
    };
    Q_DECLARE_FLAGS(FontFilters, FontFilter)

    explicit QFontComboBox(QWidget *parent = nullptr);
    ~QFontComboBox() override;

    void setFontFilters(FontFilters filters) { m_filters = filters; }
    FontFilters fontFilters() const { return m_filters; }
    QFont currentFont() const;

public slots:
    void setCurrentFont(const QFont &font);

signals:
    /*!
        This signal is emitted when the current font changes to \a font, the font of the family that was chosen.
    */
    void currentFontChanged(const QFont &font) { emitSignal(&QFontComboBox::currentFontChanged, font); }

private:
    FontFilters m_filters;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFontComboBox::FontFilters)
