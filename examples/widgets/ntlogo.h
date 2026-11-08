// MIT License - see LICENSE
//
// The Nt logo as a widget, drawn from the outlines of assets/nt-logo.svg. Qt has no SVG support in the widgets module,
// and Nt has no path or image loading yet, so the outlines are in the source. Used by the widget examples.
#pragma once

#include <QImage>
#include <QPaintEvent>
#include <QPainter>
#include <QWidget>

#include <algorithm>
#include <cmath>

namespace NtExamples {

//! [logo]
// The outlines of the letters of the Nt logo (assets/nt-logo.svg), in its 256 by 256 coordinate system: x and y pairs.
const float kLetterN[] = {
    57.05f,  179.38f, 57.05f,  76.62f,  86.15f,  76.62f,  117.35f, 164.68f, 119.60f, 164.68f, 119.60f, 76.62f,
    136.25f, 76.62f,  136.25f, 179.38f, 107.75f, 179.38f, 75.80f,  91.17f,  73.70f,  91.17f,  73.70f,  179.38f,
};
const float kLetterT[] = {
    198.35f, 118.33f, 177.65f, 118.33f, 177.65f, 151.32f, 177.67f, 153.51f, 177.73f, 155.51f, 177.84f, 157.32f,
    177.99f, 158.93f, 178.18f, 160.36f, 178.41f, 161.59f, 178.68f, 162.62f, 179.00f, 163.47f, 179.40f, 164.17f,
    179.94f, 164.78f, 180.60f, 165.30f, 181.40f, 165.72f, 182.33f, 166.05f, 183.39f, 166.28f, 184.58f, 166.42f,
    185.90f, 166.47f, 198.20f, 166.03f, 198.95f, 179.07f, 196.51f, 179.53f, 194.22f, 179.93f, 192.09f, 180.26f,
    190.10f, 180.54f, 188.26f, 180.75f, 186.58f, 180.91f, 185.04f, 181.00f, 183.65f, 181.03f, 180.59f, 180.94f,
    177.78f, 180.66f, 175.22f, 180.21f, 172.91f, 179.57f, 170.85f, 178.74f, 169.04f, 177.74f, 167.49f, 176.55f,
    166.18f, 175.18f, 165.07f, 173.55f, 164.11f, 171.60f, 163.30f, 169.33f, 162.63f, 166.73f, 162.12f, 163.82f,
    161.75f, 160.57f, 161.52f, 157.01f, 161.45f, 153.12f, 161.45f, 118.33f, 151.85f, 118.33f, 151.85f, 104.38f,
    161.45f, 104.38f, 161.45f, 82.62f,  177.65f, 82.62f,  177.65f, 104.38f, 198.35f, 104.38f,
};
// The hexagon is drawn with a stroke 24 wide and round joins, which is the polygon grown by 12.
const float kHexagon[] = {128, 24, 218, 76, 218, 180, 128, 232, 38, 180, 38, 76};
constexpr float kHexagonGrow = 12.0f;

inline bool insidePolygon(const float *xy, int points, float x, float y) {
    bool inside = false;
    for (int i = 0, j = points - 1; i < points; j = i++) {
        const float xi = xy[2 * i], yi = xy[2 * i + 1], xj = xy[2 * j], yj = xy[2 * j + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi)
            inside = !inside;
    }
    return inside;
}

inline float distanceToPolygon(const float *xy, int points, float x, float y) {
    float best = 1e9f;
    for (int i = 0, j = points - 1; i < points; j = i++) {
        const float ax = xy[2 * j], ay = xy[2 * j + 1], bx = xy[2 * i], by = xy[2 * i + 1];
        const float dx = bx - ax, dy = by - ay;
        const float t = std::clamp(((x - ax) * dx + (y - ay) * dy) / (dx * dx + dy * dy), 0.0f, 1.0f);
        best = std::min(best, std::hypot(x - (ax + t * dx), y - (ay + t * dy)));
    }
    return best;
}

// Draws the logo into an image of `size` by `size` pixels, with 4 by 4 samples per pixel for smooth edges.
inline QImage renderLogo(int size) {
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(0u);
    const float scale = 256.0f / static_cast<float>(size);
    const int teal[3] = {0x13, 0x97, 0x7f};
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            int covered = 0, white = 0;
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const float x = (static_cast<float>(px) + (static_cast<float>(sx) + 0.5f) / 4.0f) * scale;
                    const float y = (static_cast<float>(py) + (static_cast<float>(sy) + 0.5f) / 4.0f) * scale;
                    if (insidePolygon(kLetterN, 12, x, y) || insidePolygon(kLetterT, 53, x, y)) {
                        ++covered;
                        ++white;
                    } else if (insidePolygon(kHexagon, 6, x, y) ||
                               distanceToPolygon(kHexagon, 6, x, y) <= kHexagonGrow) {
                        ++covered;
                    }
                }
            }
            if (covered == 0)
                continue;
            auto channel = [&](int tealValue) { return (white * 255 + (covered - white) * tealValue) / 16; };
            const int a = covered * 255 / 16;
            uchar *line = image.scanLine(py) + px * 4;
            const uint value = (static_cast<uint>(a) << 24) | (static_cast<uint>(channel(teal[0])) << 16) |
                               (static_cast<uint>(channel(teal[1])) << 8) | static_cast<uint>(channel(teal[2]));
            std::copy_n(reinterpret_cast<const uchar *>(&value), 4, line);
        }
    }
    return image;
}

// The Nt logo, drawn from its outlines.
class Logo : public QWidget {
public:
    explicit Logo(int size, QWidget *parent = nullptr) : QWidget(parent), m_size(size) { setFixedSize(size, size); }

protected:
    void paintEvent(QPaintEvent *) override {
        // Draw one pixel per device pixel, so that the logo stays sharp on a high-resolution screen.
        const qreal ratio = devicePixelRatio();
        if (m_image.isNull() || m_image.devicePixelRatio() != ratio) {
            m_image = renderLogo(qRound(m_size * ratio));
            m_image.setDevicePixelRatio(ratio);
        }
        QPainter painter(this);
        painter.drawImage(0, 0, m_image);
    }

private:
    int m_size;
    QImage m_image;
};
//! [logo]

} // namespace NtExamples
