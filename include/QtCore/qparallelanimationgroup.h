// MIT License - see LICENSE
#pragma once

#include <QtCore/qanimationgroup.h>

class QParallelAnimationGroup : public QAnimationGroup {
    Q_OBJECT
public:
    explicit QParallelAnimationGroup(QObject *parent = nullptr);
    ~QParallelAnimationGroup() override;

    int duration() const override;

protected:
    bool event(QEvent *event) override;

    void updateCurrentTime(int currentTime) override;
    void updateState(QAbstractAnimation::State newState, QAbstractAnimation::State oldState) override;
    void updateDirection(QAbstractAnimation::Direction direction) override;
    /*! \internal */
    void animationStopped(QAbstractAnimation *animation) override;

private:
    int m_lastLoop = 0;
};
