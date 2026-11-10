// MIT License - see LICENSE
#pragma once

#include <QtCore/qbytearray.h>
#include <QtCore/qpointer.h>
#include <QtCore/qvariantanimation.h>

class QPropertyAnimation : public QVariantAnimation {
    Q_OBJECT
    Q_PROPERTY(QByteArray propertyName READ propertyName WRITE setPropertyName)
    Q_PROPERTY(QObject *targetObject READ targetObject WRITE setTargetObject)
public:
    explicit QPropertyAnimation(QObject *parent = nullptr);
    QPropertyAnimation(QObject *target, const QByteArray &propertyName, QObject *parent = nullptr);
    ~QPropertyAnimation() override;

    QObject *targetObject() const;
    void setTargetObject(QObject *target);

    QByteArray propertyName() const;
    void setPropertyName(const QByteArray &propertyName);

protected:
    bool event(QEvent *event) override;
    void updateCurrentValue(const QVariant &value) override;
    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;

private:
    QPointer<QObject> m_target;
    QByteArray m_propertyName;
};
