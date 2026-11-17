// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qhash.h>
#include <QtCore/qmetaobject.h>

class QWidget;

class QItemEditorCreatorBase {
public:
    virtual ~QItemEditorCreatorBase() = default;

    virtual QWidget *createWidget(QWidget *parent) const = 0;
    virtual QByteArray valuePropertyName() const = 0;
};

template <class T> class QItemEditorCreator : public QItemEditorCreatorBase {
public:
    explicit QItemEditorCreator(const QByteArray &valuePropertyName) : m_property(valuePropertyName) {}
    QWidget *createWidget(QWidget *parent) const override { return new T(parent); }
    QByteArray valuePropertyName() const override { return m_property; }

private:
    QByteArray m_property;
};

template <class T> class QStandardItemEditorCreator : public QItemEditorCreatorBase {
public:
    QStandardItemEditorCreator() : m_property(T::staticMetaObject.userProperty().name()) {}
    QWidget *createWidget(QWidget *parent) const override { return new T(parent); }
    QByteArray valuePropertyName() const override { return m_property; }

private:
    QByteArray m_property;
};

class QItemEditorFactory {
public:
    QItemEditorFactory();
    virtual ~QItemEditorFactory();
    QItemEditorFactory(const QItemEditorFactory &) = delete;
    QItemEditorFactory &operator=(const QItemEditorFactory &) = delete;

    virtual QWidget *createEditor(int userType, QWidget *parent) const;
    virtual QByteArray valuePropertyName(int userType) const;
    void registerEditor(int userType, QItemEditorCreatorBase *creator);

    static const QItemEditorFactory *defaultFactory();
    static void setDefaultFactory(QItemEditorFactory *factory);

private:
    QHash<int, QItemEditorCreatorBase *> m_creators; // owned; one creator may serve several types
};
