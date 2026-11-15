// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qstring.h>

class QWaylandKeymap : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString layout READ layout WRITE setLayout NOTIFY layoutChanged)
    Q_PROPERTY(QString variant READ variant WRITE setVariant NOTIFY variantChanged)
    Q_PROPERTY(QString options READ options WRITE setOptions NOTIFY optionsChanged)
    Q_PROPERTY(QString rules READ rules WRITE setRules NOTIFY rulesChanged)
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
public:
    QWaylandKeymap(const QString &layout = QString(), const QString &variant = QString(),
                   const QString &options = QString(), const QString &model = QString(),
                   const QString &rules = QString(), QObject *parent = nullptr);

    /*!
        Returns the XKB layout, such as \c "us".
    */
    QString layout() const { return m_layout; }
    void setLayout(const QString &layout);
    /*!
        Returns the XKB layout variant.
    */
    QString variant() const { return m_variant; }
    void setVariant(const QString &variant);
    /*!
        Returns the XKB options.
    */
    QString options() const { return m_options; }
    void setOptions(const QString &options);
    /*!
        Returns the XKB rules file name.
    */
    QString rules() const { return m_rules; }
    void setRules(const QString &rules);
    /*!
        Returns the XKB keyboard model.
    */
    QString model() const { return m_model; }
    void setModel(const QString &model);

signals:
    /*!
        This signal is emitted when the layout changes.
    */
    void layoutChanged() { emitSignal(&QWaylandKeymap::layoutChanged); }
    /*!
        This signal is emitted when the variant changes.
    */
    void variantChanged() { emitSignal(&QWaylandKeymap::variantChanged); }
    /*!
        This signal is emitted when the options change.
    */
    void optionsChanged() { emitSignal(&QWaylandKeymap::optionsChanged); }
    /*!
        This signal is emitted when the rules change.
    */
    void rulesChanged() { emitSignal(&QWaylandKeymap::rulesChanged); }
    /*!
        This signal is emitted when the model changes.
    */
    void modelChanged() { emitSignal(&QWaylandKeymap::modelChanged); }

private:
    QString m_layout;
    QString m_variant;
    QString m_options;
    QString m_rules;
    QString m_model;
};
