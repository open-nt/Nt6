// MIT License - see LICENSE
#pragma once

#include <QtCharts/qabstractaxis.h>
#include <QtCore/qstringlist.h>

class QBarCategoryAxis : public QAbstractAxis {
    Q_OBJECT
    Q_PROPERTY(QStringList categories READ categories WRITE setCategories NOTIFY categoriesChanged)
    Q_PROPERTY(QString min READ min WRITE setMin NOTIFY minChanged)
    Q_PROPERTY(QString max READ max WRITE setMax NOTIFY maxChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
public:
    explicit QBarCategoryAxis(QObject *parent = nullptr);
    ~QBarCategoryAxis() override;

    AxisType type() const override;

    void append(const QStringList &categories);
    void append(const QString &category);
    void remove(const QString &category);
    void insert(int index, const QString &category);
    void replace(const QString &oldCategory, const QString &newCategory);
    void clear();
    void setCategories(const QStringList &categories);
    QStringList categories();
    int count() const;
    QString at(int index) const;

    void setMin(const QString &minCategory);
    QString min() const;
    void setMax(const QString &maxCategory);
    QString max() const;
    void setRange(const QString &minCategory, const QString &maxCategory);

signals:
    /*!
        This signal is emitted when categories are added, removed or replaced.
    */
    void categoriesChanged() { emitSignal(&QBarCategoryAxis::categoriesChanged); }
    /*!
        This signal is emitted when the first category shown changes to \a min.
    */
    void minChanged(const QString &min) { emitSignal(&QBarCategoryAxis::minChanged, min); }
    /*!
        This signal is emitted when the last category shown changes to \a max.
    */
    void maxChanged(const QString &max) { emitSignal(&QBarCategoryAxis::maxChanged, max); }
    /*!
        This signal is emitted when the categories shown change to the range from \a min to \a max.
    */
    void rangeChanged(const QString &min, const QString &max) { emitSignal(&QBarCategoryAxis::rangeChanged, min, max); }
    /*!
        This signal is emitted when the number of categories changes.
    */
    void countChanged() { emitSignal(&QBarCategoryAxis::countChanged); }

protected:
    void setRangeFromVariant(const QVariant &min, const QVariant &max) override;

private:
    friend class QtChartsPrivate::ChartEngine;
    void categoriesUpdated(int oldCount);
    void setIndexRange(qreal min, qreal max);

    QStringList m_categories;
    // The range in category indices: category i covers i - 0.5 to i + 0.5.
    qreal m_min = -0.5;
    qreal m_max = -0.5;
};
