// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>
#include <QtWidgets/qdialog.h>

#include <set>
#include <utility>
#include <vector>

class QCheckBox;
class QLabel;
class QPushButton;

class QErrorMessage : public QDialog {
    Q_OBJECT
public:
    explicit QErrorMessage(QWidget *parent = nullptr);
    ~QErrorMessage() override;

public slots:
    void showMessage(const QString &message);
    void showMessage(const QString &message, const QString &type);

protected:
    void done(int result) override;

private:
    bool showNext();

    QLabel *m_label = nullptr;
    QCheckBox *m_again = nullptr;
    QPushButton *m_ok = nullptr;
    std::vector<std::pair<QString, QString>> m_pending; // message and type
    std::set<QString> m_doNotShow;                      // messages, or types prefixed with a tab
    QString m_current;
    QString m_currentType;
};
