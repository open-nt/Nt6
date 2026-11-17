// MIT License - see LICENSE
#pragma once

#include <QtCore/qmargins.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>

class QBrush;
class QColor;
class QPainter;
class QPalette;
class QPixmap;

void qDrawShadeLine(QPainter *p, int x1, int y1, int x2, int y2, const QPalette &pal, bool sunken = true,
                    int lineWidth = 1, int midLineWidth = 0);
void qDrawShadeLine(QPainter *p, const QPoint &p1, const QPoint &p2, const QPalette &pal, bool sunken = true,
                    int lineWidth = 1, int midLineWidth = 0);
void qDrawShadeRect(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken = false,
                    int lineWidth = 1, int midLineWidth = 0, const QBrush *fill = nullptr);
void qDrawShadeRect(QPainter *p, const QRect &r, const QPalette &pal, bool sunken = false, int lineWidth = 1,
                    int midLineWidth = 0, const QBrush *fill = nullptr);
void qDrawShadePanel(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken = false,
                     int lineWidth = 1, const QBrush *fill = nullptr);
void qDrawShadePanel(QPainter *p, const QRect &r, const QPalette &pal, bool sunken = false, int lineWidth = 1,
                     const QBrush *fill = nullptr);
void qDrawWinButton(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken = false,
                    const QBrush *fill = nullptr);
void qDrawWinButton(QPainter *p, const QRect &r, const QPalette &pal, bool sunken = false,
                    const QBrush *fill = nullptr);
void qDrawWinPanel(QPainter *p, int x, int y, int w, int h, const QPalette &pal, bool sunken = false,
                   const QBrush *fill = nullptr);
void qDrawWinPanel(QPainter *p, const QRect &r, const QPalette &pal, bool sunken = false,
                   const QBrush *fill = nullptr);
void qDrawPlainRect(QPainter *p, int x, int y, int w, int h, const QColor &c, int lineWidth = 1,
                    const QBrush *fill = nullptr);
void qDrawPlainRect(QPainter *p, const QRect &r, const QColor &c, int lineWidth = 1, const QBrush *fill = nullptr);

struct QTileRules {
    /*! Constructs rules with \a horizontalRule across and \a verticalRule down. */
    constexpr QTileRules(Qt::TileRule horizontalRule, Qt::TileRule verticalRule) noexcept
        : horizontal(horizontalRule), vertical(verticalRule) {}
    /*! Constructs rules with \a rule both ways. */
    constexpr QTileRules(Qt::TileRule rule = Qt::StretchTile) noexcept : horizontal(rule), vertical(rule) {}
    /*! How the parts are filled across. */
    Qt::TileRule horizontal;
    /*! How the parts are filled down. */
    Qt::TileRule vertical;
};

void qDrawBorderPixmap(QPainter *painter, const QRect &targetRect, const QMargins &targetMargins,
                       const QPixmap &pixmap, const QRect &sourceRect, const QMargins &sourceMargins,
                       const QTileRules &rules = QTileRules());
void qDrawBorderPixmap(QPainter *painter, const QRect &target, const QMargins &margins, const QPixmap &pixmap);
