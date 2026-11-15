// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractitemmodel.h>
#include <QtCore/qbytearray.h>
#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>

#include <vector>

class QAbstractItemDelegate;
class QWidget;

class QDataWidgetMapper : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentIndex READ currentIndex WRITE setCurrentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(Qt::Orientation orientation READ orientation WRITE setOrientation)
    Q_PROPERTY(SubmitPolicy submitPolicy READ submitPolicy WRITE setSubmitPolicy)
public:
    enum SubmitPolicy { AutoSubmit, ManualSubmit };
    Q_ENUM(SubmitPolicy)

    explicit QDataWidgetMapper(QObject *parent = nullptr);
    ~QDataWidgetMapper() override;

    void setModel(QAbstractItemModel *model);
    QAbstractItemModel *model() const;
    void setItemDelegate(QAbstractItemDelegate *delegate);
    QAbstractItemDelegate *itemDelegate() const;
    void setRootIndex(const QModelIndex &index);
    QModelIndex rootIndex() const;
    void setOrientation(Qt::Orientation orientation);
    Qt::Orientation orientation() const { return m_orientation; }
    void setSubmitPolicy(SubmitPolicy policy);
    SubmitPolicy submitPolicy() const { return m_policy; }

    void addMapping(QWidget *widget, int section);
    void addMapping(QWidget *widget, int section, const QByteArray &propertyName);
    void removeMapping(QWidget *widget);
    int mappedSection(QWidget *widget) const;
    QByteArray mappedPropertyName(QWidget *widget) const;
    QWidget *mappedWidgetAt(int section) const;
    void clearMapping();

    int currentIndex() const;

public slots:
    void revert();
    bool submit();
    void toFirst();
    void toLast();
    void toNext();
    void toPrevious();
    virtual void setCurrentIndex(int index);
    void setCurrentModelIndex(const QModelIndex &index);

signals:
    /*!
        This signal is emitted when the current row, or column, changes to \a index.
    */
    void currentIndexChanged(int index) { emitSignal(&QDataWidgetMapper::currentIndexChanged, index); }

private:
    struct Mapping {
        QPointer<QWidget> widget;
        int section = 0;
        QByteArray property;
    };
    QModelIndex indexOf(const Mapping &mapping) const;
    int count() const;
    void populate(const Mapping &mapping);
    void populateAll();
    bool commit(const Mapping &mapping);
    const Mapping *find(const QWidget *widget) const;
    void watch(QWidget *widget, bool on);
    void connectDelegate();

    QPointer<QAbstractItemModel> m_model;
    QPointer<QAbstractItemDelegate> m_delegate;
    QPersistentModelIndex m_root;
    QPersistentModelIndex m_current; // in section 0 of the current row or column
    std::vector<Mapping> m_mappings;
    std::vector<QMetaObject::Connection> m_modelConnections;
    std::vector<QMetaObject::Connection> m_delegateConnections;
    Qt::Orientation m_orientation = Qt::Horizontal;
    SubmitPolicy m_policy = AutoSubmit;
};
