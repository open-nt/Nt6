// MIT License - see LICENSE
#pragma once

#include <QtCore/qpointer.h>
#include <QtGui/qaction.h>
#include <QtWidgets/qwidget.h>

#include <vector>

class QWidgetAction : public QAction {
    Q_OBJECT
public:
    explicit QWidgetAction(QObject *parent);
    ~QWidgetAction() override;

    void setDefaultWidget(QWidget *widget);
    QWidget *defaultWidget() const { return m_default.data(); }
    QWidget *requestWidget(QWidget *parent);
    void releaseWidget(QWidget *widget);

protected:
    virtual QWidget *createWidget(QWidget *parent);
    virtual void deleteWidget(QWidget *widget);
    QList<QWidget *> createdWidgets() const;

private:
    QPointer<QWidget> m_default;
    bool m_defaultInUse = false;
    std::vector<QPointer<QWidget>> m_created;
};
