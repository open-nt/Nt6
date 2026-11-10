// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractanimation.h>
#include <QtCore/qlist.h>

class QAnimationGroup : public QAbstractAnimation {
    Q_OBJECT
public:
    explicit QAnimationGroup(QObject *parent = nullptr);
    ~QAnimationGroup() override;

    QAbstractAnimation *animationAt(int index) const;
    int animationCount() const;
    int indexOfAnimation(QAbstractAnimation *animation) const;
    void addAnimation(QAbstractAnimation *animation);
    void insertAnimation(int index, QAbstractAnimation *animation);
    void removeAnimation(QAbstractAnimation *animation);
    QAbstractAnimation *takeAnimation(int index);
    void clear();

protected:
    bool event(QEvent *event) override;

    /*! \internal Called when the list of animations has changed at \a index. */
    virtual void animationsChanged(int index, bool inserted);
    /*! \internal Called when \a animation, which is in the group, has stopped. */
    virtual void animationStopped(QAbstractAnimation *animation);

private:
    friend class QAbstractAnimation;
    QList<QAbstractAnimation *> m_animations;
};
