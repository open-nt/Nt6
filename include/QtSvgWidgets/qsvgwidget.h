// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qstring.h>
#include <QtSvg/qtsvgglobal.h>
#include <QtWidgets/qwidget.h>

class QSvgRenderer;

class QSvgWidget : public QWidget {
    Q_OBJECT
public:
    explicit QSvgWidget(QWidget *parent = nullptr);
    explicit QSvgWidget(const QString &file, QWidget *parent = nullptr);
    ~QSvgWidget() override;

    QSvgRenderer *renderer() const;
    QSize sizeHint() const override;

    QtSvg::Options options() const;
    void setOptions(QtSvg::Options options);

public slots:
    void load(const QString &file);
    void load(const QByteArray &contents);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QSvgRenderer *m_renderer;
};
