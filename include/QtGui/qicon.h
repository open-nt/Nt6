// MIT License - see LICENSE
#pragma once

#include <QtCore/qlist.h>
#include <QtCore/qnamespace.h>
#include <QtCore/qrect.h>
#include <QtCore/qsize.h>
#include <QtCore/qstring.h>
#include <QtCore/qstringlist.h>
#include <QtGui/qpixmap.h>

#include <memory>

class QPainter;

class QVariant;

class QIcon {
public:
    operator QVariant() const;
    enum Mode { Normal, Disabled, Active, Selected };
    enum State { On, Off };

    QIcon() noexcept;
    QIcon(const QPixmap &pixmap); // NOLINT(google-explicit-constructor): implicit, as in Qt
    explicit QIcon(const QString &fileName);
    QIcon(const QIcon &other);
    QIcon(QIcon &&other) noexcept;
    ~QIcon();
    QIcon &operator=(const QIcon &other);
    QIcon &operator=(QIcon &&other) noexcept;
    void swap(QIcon &other) noexcept { m_d.swap(other.m_d); }

    bool isNull() const;
    QString name() const;
    qint64 cacheKey() const;

    QPixmap pixmap(const QSize &size, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QPixmap pixmap(const QSize &size, qreal devicePixelRatio, QIcon::Mode mode = Normal,
                   QIcon::State state = Off) const;
    QPixmap pixmap(int w, int h, QIcon::Mode mode = Normal, QIcon::State state = Off) const {
        return pixmap(QSize(w, h), mode, state);
    }
    QPixmap pixmap(int extent, QIcon::Mode mode = Normal, QIcon::State state = Off) const {
        return pixmap(QSize(extent, extent), mode, state);
    }
    QSize actualSize(const QSize &size, QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    QList<QSize> availableSizes(QIcon::Mode mode = Normal, QIcon::State state = Off) const;

    void paint(QPainter *painter, const QRect &rect, Qt::Alignment alignment = Qt::AlignCenter,
               QIcon::Mode mode = Normal, QIcon::State state = Off) const;
    void paint(QPainter *painter, int x, int y, int w, int h, Qt::Alignment alignment = Qt::AlignCenter,
               QIcon::Mode mode = Normal, QIcon::State state = Off) const {
        paint(painter, QRect(x, y, w, h), alignment, mode, state);
    }

    void addPixmap(const QPixmap &pixmap, QIcon::Mode mode = Normal, QIcon::State state = Off);
    void addFile(const QString &fileName, const QSize &size = QSize(), QIcon::Mode mode = Normal,
                 QIcon::State state = Off);

    static QIcon fromTheme(const QString &name);
    static QIcon fromTheme(const QString &name, const QIcon &fallback);
    static bool hasThemeIcon(const QString &name);
    static QString themeName();
    static void setThemeName(const QString &name);
    static QString fallbackThemeName();
    static void setFallbackThemeName(const QString &name);
    static QStringList themeSearchPaths();
    static void setThemeSearchPaths(const QStringList &paths);
    static QStringList fallbackSearchPaths();
    static void setFallbackSearchPaths(const QStringList &paths);

private:
    struct Data;
    void detach();
    std::shared_ptr<Data> m_d;
};

