// MIT License - see LICENSE
#pragma once

#include <QtWidgets/qlayout.h>

#include <vector>

class QFormLayout : public QLayout {
    Q_OBJECT
public:
    enum FieldGrowthPolicy { FieldsStayAtSizeHint, ExpandingFieldsGrow, AllNonFixedFieldsGrow };
    enum RowWrapPolicy { DontWrapRows, WrapLongRows, WrapAllRows };
    enum ItemRole { LabelRole = 0, FieldRole = 1, SpanningRole = 2 };
    Q_ENUM(FieldGrowthPolicy)
    Q_ENUM(RowWrapPolicy)
    Q_ENUM(ItemRole)

    explicit QFormLayout(QWidget *parent = nullptr);
    ~QFormLayout() override;

    void addRow(QWidget *label, QWidget *field);
    void addRow(QWidget *label, QLayout *field);
    void addRow(const QString &labelText, QWidget *field);
    void addRow(const QString &labelText, QLayout *field);
    void addRow(QWidget *widget);
    void addRow(QLayout *layout);
    void insertRow(int row, QWidget *label, QWidget *field);
    void insertRow(int row, QWidget *label, QLayout *field);
    void insertRow(int row, const QString &labelText, QWidget *field);
    void insertRow(int row, const QString &labelText, QLayout *field);
    void insertRow(int row, QWidget *widget);
    void insertRow(int row, QLayout *layout);
    void removeRow(int row);

    int rowCount() const { return static_cast<int>(m_rows.size()); }
    void setWidget(int row, ItemRole role, QWidget *widget);
    void setLayout(int row, ItemRole role, QLayout *layout);
    QLayoutItem *itemAt(int row, ItemRole role) const;
    void getWidgetPosition(QWidget *widget, int *row, ItemRole *role) const;
    void getLayoutPosition(QLayout *layout, int *row, ItemRole *role) const;
    QWidget *labelForField(QWidget *field) const;
    QWidget *labelForField(QLayout *field) const;

    FieldGrowthPolicy fieldGrowthPolicy() const { return m_growth; }
    void setFieldGrowthPolicy(FieldGrowthPolicy policy);
    RowWrapPolicy rowWrapPolicy() const { return m_wrap; }
    void setRowWrapPolicy(RowWrapPolicy policy);
    Qt::Alignment labelAlignment() const { return m_labelAlignment; }
    void setLabelAlignment(Qt::Alignment alignment);
    Qt::Alignment formAlignment() const { return m_formAlignment; }
    void setFormAlignment(Qt::Alignment alignment);
    int horizontalSpacing() const;
    void setHorizontalSpacing(int spacing);
    int verticalSpacing() const;
    void setVerticalSpacing(int spacing);

    void addItem(QLayoutItem *item) override;
    QLayoutItem *itemAt(int index) const override;
    QLayoutItem *takeAt(int index) override;
    int count() const override;
    QSize sizeHint() const override;
    QSize minimumSize() const override;
    Qt::Orientations expandingDirections() const override;
    void setGeometry(const QRect &rect) override;

private:
    struct Row {
        QLayoutItem *label = nullptr;
        QLayoutItem *field = nullptr; // the spanning item of a row with only one
        bool labelOwned = false;
        bool fieldOwned = false;
        bool spanning = false;
    };
    struct Metrics;
    void setItem(int row, ItemRole role, QLayoutItem *item, bool owned);
    int insertEmptyRow(int row);
    QWidget *newLabel(const QString &text);
    Metrics measure(int width) const;
    QSize total(bool minimum) const;
    bool wraps(const Row &row, int width, int labelWidth) const;

    std::vector<Row> m_rows;
    FieldGrowthPolicy m_growth = AllNonFixedFieldsGrow;
    RowWrapPolicy m_wrap = DontWrapRows;
    Qt::Alignment m_labelAlignment = Qt::AlignLeft;
    Qt::Alignment m_formAlignment = Qt::AlignLeft | Qt::AlignTop;
    int m_horizontalSpacing = -1;
    int m_verticalSpacing = -1;
};
