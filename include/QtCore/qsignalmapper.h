// MIT License - see LICENSE
#pragma once

#include <QtCore/qhash.h>
#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QSignalMapper : public QObject {
    Q_OBJECT
public:
    explicit QSignalMapper(QObject *parent = nullptr);
    ~QSignalMapper() override;

    void setMapping(QObject *sender, int id);
    void setMapping(QObject *sender, const QString &text);
    void setMapping(QObject *sender, QObject *object);
    void removeMappings(QObject *sender);

    QObject *mapping(int id) const;
    QObject *mapping(const QString &text) const;
    QObject *mapping(QObject *object) const;

signals:
    /*!
        This signal is emitted when map() is called by a sender that has an
        integer mapping, with the identifier \a i of that mapping.
    */
    void mappedInt(int i);
    /*!
        This signal is emitted when map() is called by a sender that has a
        string mapping, with the \a text of that mapping.
    */
    void mappedString(const QString &text);
    /*!
        This signal is emitted when map() is called by a sender that has an
        object mapping, with the \a object of that mapping.
    */
    void mappedObject(QObject *object);

public slots:
    void map();
    void map(QObject *sender);

private:
    void watch(QObject *sender);

    QHash<QObject *, int> m_ints;
    QHash<QObject *, QString> m_strings;
    QHash<QObject *, QObject *> m_objects;
};
