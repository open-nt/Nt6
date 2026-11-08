// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayoutitem.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace QtPrivate {

struct Cell {
    QLayoutItem *item;
    int stretch;
    int min;
    int hint;
    int max;
    bool expansive;
    bool empty; // spacers and empty layouts: no spacing is put next to them
    int size = 0;
    bool done = false;

    // An item with a stretch factor starts from its minimum size and gets its size from the stretch.
    int smartHint() const { return stretch > 0 ? min : hint; }
};

inline int fixedRound(double v) { return static_cast<int>(std::lround(v)); }

// Shares `space` between the cells with `spacing` between the non-empty ones, as Qt's qGeomCalc() does. The sizes
// come out in Cell::size. If no cell can take the space that is left, it is returned as extra spacing: put it before the
// first cell and after every cell that has spacing.
inline int calculate(std::vector<Cell> &cells, int space, int spacing) {
    int leftover = 0;
    int hintSum = 0, minSum = 0, stretchSum = 0, spacerCount = 0, pendingSpacing = -1;
    bool wannaGrow = false;
    for (Cell &c : cells) {
        c.done = false;
        hintSum += c.smartHint();
        minSum += c.min;
        stretchSum += c.stretch;
        if (!c.empty) {
            if (pendingSpacing >= 0)
                ++spacerCount;
            pendingSpacing = spacing;
        }
        wannaGrow = wannaGrow || c.expansive || c.stretch > 0;
    }
    const int count = static_cast<int>(cells.size());

    if (space < minSum + spacerCount * spacing) {
        // Less than the minimum sizes: the spacing shrinks too, and the larger cells give up space first.
        int minimum = minSum + spacerCount * spacing;
        if (spacing >= 0) {
            spacing = minimum > 0 ? spacing * space / minimum : 0;
            minimum = spacerCount * spacing;
        }
        std::vector<int> minSizes;
        minSizes.reserve(cells.size());
        for (const Cell &c : cells)
            minSizes.push_back(c.min);
        std::sort(minSizes.begin(), minSizes.end());
        const int spaceLeft = space - minimum;
        int sum = 0, idx = 0, used = 0, current = 0;
        while (idx < count && used < spaceLeft) {
            current = minSizes[static_cast<std::size_t>(idx)];
            used = sum + current * (count - idx);
            sum += current;
            ++idx;
        }
        --idx;
        const int deficit = used - spaceLeft;
        const int items = count - idx;
        // Cells smaller than `current` keep their minimum; the rest share what is left, evenly.
        if (items == 0 || idx < 0) {
            for (Cell &c : cells)
                c.size = c.min;
        } else {
            int surplus = deficit % items;
            for (Cell &c : cells) {
                if (c.min < current) {
                    c.size = c.min;
                } else {
                    c.size = current - deficit / items;
                    if (surplus > 0) {
                        --c.size;
                        --surplus;
                    }
                }
            }
        }
    } else if (space < hintSum + spacerCount * spacing) {
        // Between the minimum and the hint: every cell gives up the same amount, within its minimum.
        int n = count;
        int spaceLeft = space - spacerCount * spacing;
        for (Cell &c : cells) {
            if (!c.done && c.min >= c.smartHint()) {
                c.size = c.smartHint();
                c.done = true;
                spaceLeft -= c.smartHint();
                --n;
            }
        }
        int overdraft = -spaceLeft;
        for (const Cell &c : cells)
            if (!c.done)
                overdraft += c.smartHint();
        bool finished = n == 0;
        while (!finished) {
            finished = true;
            double carry = 0.0;
            for (Cell &c : cells) {
                if (c.done)
                    continue;
                carry += static_cast<double>(overdraft) / n;
                const int w = fixedRound(carry);
                c.size = c.smartHint() - w;
                carry -= w;
                if (c.size < c.min) {
                    c.done = true;
                    c.size = c.min;
                    finished = false;
                    overdraft -= c.smartHint() - c.min;
                    --n;
                    break;
                }
            }
        }
    } else {
        // More than the hints. Cells that cannot grow, and those that do not want to when another does, keep
        // their hint; the others share the rest, by stretch or equally.
        int n = count;
        int spaceLeft = space - spacerCount * spacing;
        for (Cell &c : cells) {
            if (!c.done && (c.max <= c.smartHint() || (wannaGrow && c.stretch <= 0 && !c.expansive))) {
                c.size = c.smartHint();
                c.done = true;
                spaceLeft -= c.smartHint();
                stretchSum -= c.stretch;
                --n;
            }
        }
        // Try an even sharing and see how far off it is: when more space is missing than is spare, give the small cells
        // what they need and try again; otherwise cap the large ones.
        int surplus, deficit;
        do {
            surplus = deficit = 0;
            double carry = 0.0;
            for (Cell &c : cells) {
                if (c.done)
                    continue;
                if (stretchSum <= 0)
                    carry += static_cast<double>(spaceLeft) / n;
                else
                    carry += static_cast<double>(spaceLeft) * c.stretch / stretchSum;
                const int w = fixedRound(carry);
                c.size = w;
                carry -= w;
                if (w < c.smartHint())
                    deficit += c.smartHint() - w;
                else if (w > c.max)
                    surplus += w - c.max;
            }
            if (deficit > 0 && surplus <= deficit) {
                for (Cell &c : cells) {
                    if (!c.done && c.size < c.smartHint()) {
                        c.size = c.smartHint();
                        c.done = true;
                        spaceLeft -= c.smartHint();
                        stretchSum -= c.stretch;
                        --n;
                    }
                }
            }
            if (surplus > 0 && surplus >= deficit) {
                for (Cell &c : cells) {
                    if (!c.done && c.size > c.max) {
                        c.size = c.max;
                        c.done = true;
                        spaceLeft -= c.max;
                        stretchSum -= c.stretch;
                        --n;
                    }
                }
            }
        } while (n > 0 && surplus != deficit);
        if (n == 0)
            leftover = spaceLeft;
    }
    return leftover / (spacerCount + 2);
}


} // namespace QtPrivate
