// MIT License - see LICENSE
#pragma once

#include <QtCore/qabstractanimation.h>

class QPauseAnimation : public QAbstractAnimation {
    Q_OBJECT
    Q_PROPERTY(int duration READ duration WRITE setDuration)
public:
    explicit QPauseAnimation(QObject *parent = nullptr);
    explicit QPauseAnimation(int msecs, QObject *parent = nullptr);
    ~QPauseAnimation() override;

    int duration() const override;
    void setDuration(int msecs);

protected:
    bool event(QEvent *event) override;
    void updateCurrentTime(int currentTime) override;

private:
    int m_duration = 250;
};
