// MIT License - see LICENSE
#pragma once

#include <QtGui/qcolor.h>
#include <QtWidgets/qcommonstyle.h>

// The Material 3 Expressive look. Nt only: Qt Widgets has no such style.
class QMaterialStyle : public QCommonStyle {
    Q_OBJECT
public:
    // The colour roles of Material 3.
    enum class Role {
        Primary,
        OnPrimary,
        PrimaryContainer,
        OnPrimaryContainer,
        Secondary,
        OnSecondary,
        SecondaryContainer,
        OnSecondaryContainer,
        Tertiary,
        OnTertiary,
        TertiaryContainer,
        OnTertiaryContainer,
        Error,
        OnError,
        ErrorContainer,
        OnErrorContainer,
        Surface,
        OnSurface,
        SurfaceVariant,
        OnSurfaceVariant,
        SurfaceContainerLowest,
        SurfaceContainerLow,
        SurfaceContainer,
        SurfaceContainerHigh,
        SurfaceContainerHighest,
        Outline,
        OutlineVariant,
        InverseSurface,
        InverseOnSurface,
        InversePrimary
    };
    Q_ENUM(Role)

    QMaterialStyle();
    explicit QMaterialStyle(const QColor &seedColor);
    ~QMaterialStyle() override;

    QColor seedColor() const { return m_seed; }
    void setSeedColor(const QColor &color);
    Qt::ColorScheme colorScheme() const { return m_scheme; }
    void setColorScheme(Qt::ColorScheme scheme);
    bool isDark() const;
    QColor color(Role role) const;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                           const QWidget *widget = nullptr) const override;
    QPalette standardPalette() const override;
    using QCommonStyle::polish;
    using QCommonStyle::unpolish;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void polish(QApplication *application) override;
    void unpolish(QApplication *application) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class ButtonKind { Filled, Tonal, Outlined, Text, Elevated };
    static ButtonKind buttonKind(const QStyleOption *option, const QWidget *widget);
    void drawButtonContainer(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawButtonLabel(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawSwitch(const QStyleOption *option, const QRect &rect, QPainter *painter) const;
    void drawSlider(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawProgressBar(const QStyleOption *option, QPainter *painter) const;
    void drawTab(const QStyleOption *option, QPainter *painter, const QWidget *widget) const;
    void drawComboBox(const QStyleOption *option, QPainter *painter) const;
    void drawMenuItem(const QStyleOption *option, QPainter *painter) const;
    QColor buttonLabelColor(const QStyleOption *option, const QWidget *widget) const;
    bool keyboardFocus(const QStyleOption *option) const;
    double stateLayer(const QStyleOption *option) const;

    QColor m_seed;
    Qt::ColorScheme m_scheme = Qt::ColorScheme::Unknown; // Unknown follows the system
    bool m_keyboard = false;                             // the focus was last moved with the keyboard
};
