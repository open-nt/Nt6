// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtCore/qstring.h>
#include <QtWidgets/qstyle.h>

class QProxyStyle : public QStyle {
    Q_OBJECT
public:
    explicit QProxyStyle(QStyle *style = nullptr);
    explicit QProxyStyle(const QString &key);
    ~QProxyStyle() override;

    QStyle *baseStyle() const;
    void setBaseStyle(QStyle *style);

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
    QRect itemTextRect(const QFontMetrics &metrics, const QRect &rectangle, int alignment, bool enabled,
                       const QString &text) const override;
    QRect itemPixmapRect(const QRect &rectangle, int alignment, const QPixmap &pixmap) const override;
    void drawItemText(QPainter *painter, const QRect &rectangle, int alignment, const QPalette &palette, bool enabled,
                      const QString &text, QPalette::ColorRole textRole = QPalette::NoRole) const override;
    void drawItemPixmap(QPainter *painter, const QRect &rectangle, int alignment, const QPixmap &pixmap) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override;
    SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option, const QPoint &position,
                                     const QWidget *widget = nullptr) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                         const QWidget *widget = nullptr) const override;
    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption *option = nullptr,
                       const QWidget *widget = nullptr) const override;
    QPixmap standardPixmap(StandardPixmap standardPixmap, const QStyleOption *option = nullptr,
                           const QWidget *widget = nullptr) const override;
    QPixmap generatedIconPixmap(QIcon::Mode iconMode, const QPixmap &pixmap, const QStyleOption *option) const override;
    int layoutSpacing(QSizePolicy::ControlType control1, QSizePolicy::ControlType control2, Qt::Orientation orientation,
                      const QStyleOption *option = nullptr, const QWidget *widget = nullptr) const override;
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;
    void polish(QApplication *application) override;
    void unpolish(QApplication *application) override;
    void polish(QPalette &palette) override;

private:
    QPointer<QStyle> m_base;
};
