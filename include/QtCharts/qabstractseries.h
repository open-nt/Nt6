// MIT License - see LICENSE
#pragma once

#include <QtCharts/qchartglobal.h>
#include <QtCore/qlist.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QAbstractAxis;
class QChart;

class QAbstractSeries : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    Q_PROPERTY(bool visible READ isVisible WRITE setVisible NOTIFY visibleChanged)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity NOTIFY opacityChanged)
    Q_PROPERTY(SeriesType type READ type)
    Q_PROPERTY(bool useOpenGL READ useOpenGL WRITE setUseOpenGL NOTIFY useOpenGLChanged)
public:
    enum SeriesType {
        SeriesTypeLine,
        SeriesTypeArea,
        SeriesTypeBar,
        SeriesTypeStackedBar,
        SeriesTypePercentBar,
        SeriesTypePie,
        SeriesTypeScatter,
        SeriesTypeSpline,
        SeriesTypeHorizontalBar,
        SeriesTypeHorizontalStackedBar,
        SeriesTypeHorizontalPercentBar,
        SeriesTypeBoxPlot,
        SeriesTypeCandlestick
    };
    Q_ENUM(SeriesType)

    ~QAbstractSeries() override;

    virtual SeriesType type() const = 0;

    void setName(const QString &name);
    QString name() const;
    void setVisible(bool visible = true);
    bool isVisible() const;
    qreal opacity() const;
    void setOpacity(qreal opacity);
    void setUseOpenGL(bool enable = true);
    bool useOpenGL() const;

    QChart *chart() const;

    bool attachAxis(QAbstractAxis *axis);
    bool detachAxis(QAbstractAxis *axis);
    QList<QAbstractAxis *> attachedAxes();

    void show();
    void hide();

signals:
    /*!
        This signal is emitted when the name of the series changes.
    */
    void nameChanged() { emitSignal(&QAbstractSeries::nameChanged); }
    /*!
        This signal is emitted when the series is shown or hidden.
    */
    void visibleChanged() { emitSignal(&QAbstractSeries::visibleChanged); }
    /*!
        This signal is emitted when the opacity of the series changes.
    */
    void opacityChanged() { emitSignal(&QAbstractSeries::opacityChanged); }
    /*!
        This signal is emitted when useOpenGL() changes.
    */
    void useOpenGLChanged() { emitSignal(&QAbstractSeries::useOpenGLChanged); }

protected:
    explicit QAbstractSeries(QObject *parent = nullptr);

    void markChanged();

private:
    friend class QtChartsPrivate::ChartEngine;

    QChart *m_chart = nullptr;
    QList<QAbstractAxis *> m_axes;
    QString m_name;
    bool m_visible = true;
    bool m_useOpenGL = false;
    qreal m_opacity = 1.0;
};
