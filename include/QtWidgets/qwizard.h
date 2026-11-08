// MIT License - see LICENSE
#pragma once

#include <QtCore/qflags.h>
#include <QtCore/qmap.h>
#include <QtCore/qpointer.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qabstractbutton.h>
#include <QtWidgets/qdialog.h>

#include <vector>

class QHBoxLayout;
class QLabel;
class QStackedLayout;
class QWizardPage;

class QWizard : public QDialog {
    Q_OBJECT
    Q_PROPERTY(WizardStyle wizardStyle READ wizardStyle WRITE setWizardStyle)
    Q_PROPERTY(int startId READ startId WRITE setStartId)
    Q_PROPERTY(int currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged)
public:
    enum WizardButton {
        BackButton,
        NextButton,
        CommitButton,
        FinishButton,
        CancelButton,
        HelpButton,
        CustomButton1,
        CustomButton2,
        CustomButton3,
        Stretch,
        NoButton = -1,
        NStandardButtons = 6,
        NButtons = 9
    };
    Q_ENUM(WizardButton)

    enum WizardStyle { ClassicStyle, ModernStyle, MacStyle, AeroStyle, NStyles };
    Q_ENUM(WizardStyle)

    enum WizardOption {
        IndependentPages = 0x00000001,
        IgnoreSubTitles = 0x00000002,
        ExtendedWatermarkPixmap = 0x00000004,
        NoDefaultButton = 0x00000008,
        NoBackButtonOnStartPage = 0x00000010,
        NoBackButtonOnLastPage = 0x00000020,
        DisabledBackButtonOnLastPage = 0x00000040,
        HaveNextButtonOnLastPage = 0x00000080,
        HaveFinishButtonOnEarlyPages = 0x00000100,
        NoCancelButton = 0x00000200,
        CancelButtonOnLeft = 0x00000400,
        HaveHelpButton = 0x00000800,
        HelpButtonOnRight = 0x00001000,
        HaveCustomButton1 = 0x00002000,
        HaveCustomButton2 = 0x00004000,
        HaveCustomButton3 = 0x00008000,
        NoCancelButtonOnLastPage = 0x00010000
    };
    Q_DECLARE_FLAGS(WizardOptions, WizardOption)

    explicit QWizard(QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    ~QWizard() override;

    int addPage(QWizardPage *page);
    void setPage(int id, QWizardPage *page);
    void removePage(int id);
    QWizardPage *page(int id) const;
    bool hasVisitedPage(int id) const;
    QList<int> visitedIds() const;
    QList<int> pageIds() const;
    void setStartId(int id);
    int startId() const;
    QWizardPage *currentPage() const;
    int currentId() const { return m_current; }

    virtual bool validateCurrentPage();
    virtual int nextId() const;

    void setField(const QString &name, const QVariant &value);
    QVariant field(const QString &name) const;

    void setWizardStyle(WizardStyle style) { m_style = style; }
    WizardStyle wizardStyle() const { return m_style; }
    void setOption(WizardOption option, bool on = true);
    bool testOption(WizardOption option) const { return m_options.testFlag(option); }
    void setOptions(WizardOptions options);
    WizardOptions options() const { return m_options; }

    void setButtonText(WizardButton which, const QString &text);
    QString buttonText(WizardButton which) const;
    void setButton(WizardButton which, QAbstractButton *button);
    QAbstractButton *button(WizardButton which) const;
    void setSideWidget(QWidget *widget);
    QWidget *sideWidget() const { return m_side.data(); }

    void setVisible(bool visible) override;

public slots:
    void back();
    void next();
    void setCurrentId(int id);
    void restart();

signals:
    /*!
        This signal is emitted when the current page changes, with the \a id of the new one.
    */
    void currentIdChanged(int id) { emitSignal(&QWizard::currentIdChanged, id); }
    /*!
        This signal is emitted when the user clicks the Help button.
    */
    void helpRequested() { emitSignal(&QWizard::helpRequested); }
    /*!
        This signal is emitted when the user clicks a custom button; \a which is CustomButton1, CustomButton2 or
        CustomButton3.
    */
    void customButtonClicked(int which) { emitSignal(&QWizard::customButtonClicked, which); }
    /*!
        This signal is emitted when a page is added, with its \a id.
    */
    void pageAdded(int id) { emitSignal(&QWizard::pageAdded, id); }
    /*!
        This signal is emitted when a page is removed, with its \a id.
    */
    void pageRemoved(int id) { emitSignal(&QWizard::pageRemoved, id); }

protected:
    void done(int result) override;
    virtual void initializePage(int id);
    virtual void cleanupPage(int id);

private:
    friend class QWizardPage;
    void switchTo(int id, bool forward);
    void updateButtons();
    void updateHeader();
    void layoutButtons();
    QString defaultButtonText(WizardButton which) const;

    QLabel *m_title = nullptr;
    QLabel *m_subTitle = nullptr;
    QWidget *m_header = nullptr;
    QStackedLayout *m_stack = nullptr;
    QHBoxLayout *m_buttonRow = nullptr;
    QHBoxLayout *m_body = nullptr;
    QPointer<QWidget> m_side;
    QAbstractButton *m_buttons[NButtons] = {};
    QString m_buttonTexts[NButtons];
    QMap<int, QWizardPage *> m_pages;
    std::vector<int> m_history;
    int m_current = -1;
    int m_start = -1;
    WizardStyle m_style = ClassicStyle;
    WizardOptions m_options;
    bool m_restarted = false;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QWizard::WizardOptions)

class QWizardPage : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QString title READ title WRITE setTitle)
    Q_PROPERTY(QString subTitle READ subTitle WRITE setSubTitle)
public:
    explicit QWizardPage(QWidget *parent = nullptr);
    ~QWizardPage() override;

    void setTitle(const QString &title);
    QString title() const { return m_title; }
    void setSubTitle(const QString &subTitle);
    QString subTitle() const { return m_subTitle; }
    void setFinalPage(bool finalPage);
    bool isFinalPage() const;
    void setCommitPage(bool commitPage);
    bool isCommitPage() const { return m_commit; }
    void setButtonText(QWizard::WizardButton which, const QString &text);
    QString buttonText(QWizard::WizardButton which) const;

    virtual void initializePage();
    virtual void cleanupPage();
    virtual bool validatePage();
    virtual bool isComplete() const;
    virtual int nextId() const;

signals:
    /*!
        This signal is emitted when what isComplete() returns may have changed. A wizard updates its buttons then.
    */
    void completeChanged() { emitSignal(&QWizardPage::completeChanged); }

protected:
    void setField(const QString &name, const QVariant &value);
    QVariant field(const QString &name) const;
    void registerField(const QString &name, QWidget *widget, const char *property = nullptr,
                       const char *changedSignal = nullptr);
    QWizard *wizard() const { return m_wizard; }

private:
    friend class QWizard;
    struct Field {
        QString name;
        QPointer<QWidget> widget;
        QByteArray property;
        QVariant initial;
        bool mandatory;
    };

    const Field *findField(const QString &name) const;

    QString m_title;
    QString m_subTitle;
    QMap<int, QString> m_buttonTexts;
    std::vector<Field> m_fields;
    QWizard *m_wizard = nullptr;
    bool m_final = false;
    bool m_commit = false;
};
