// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qhash.h>
#include <QtGui/qfont.h>
#include <QtGui/qfontdatabase.h>
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
    QFontDatabase::WritingSystem writingSystem() const { return m_writingSystem; }
    void setWritingSystem(QFontDatabase::WritingSystem script) { m_writingSystem = script; }
    QFont displayFont(const QString &family) const;
    void setDisplayFont(const QString &family, const QFont &font);
    QString sampleTextForSystem(QFontDatabase::WritingSystem writingSystem) const;
    void setSampleTextForSystem(QFontDatabase::WritingSystem writingSystem, const QString &sampleText);
    QString sampleTextForFont(const QString &fontFamily) const;
    void setSampleTextForFont(const QString &fontFamily, const QString &sampleText);

public slots:
    void setCurrentFont(const QFont &font);

signals:
    /*!
        This signal is emitted when the current font changes to \a font, the font of the family that was chosen.
    */
    void currentFontChanged(const QFont &font) { emitSignal(&QFontComboBox::currentFontChanged, font); }

private:
    FontFilters m_filters;
    QFontDatabase::WritingSystem m_writingSystem = QFontDatabase::Any;
    QHash<QString, QFont> m_displayFonts;
    QHash<int, QString> m_systemSamples;
    QHash<QString, QString> m_fontSamples;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QFontComboBox::FontFilters)
