// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qstyle.h>

#include <memory>

class QCommonStyle : public QStyle {
    Q_OBJECT
public:
    QCommonStyle();
    ~QCommonStyle() override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    QRect subElementRect(SubElement element, const QStyleOption *option,
                         const QWidget *widget = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &contentsSize,
                           const QWidget *widget = nullptr) const override;
    QPalette standardPalette() const override;
    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const override;
    int layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2, Qt::Orientation orientation,
                      const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;

private:
    std::unique_ptr<QStyle> m_look; // draws what the style does not draw itself
};
