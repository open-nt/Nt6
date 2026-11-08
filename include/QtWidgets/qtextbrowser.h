// MIT License - see LICENSE
#pragma once

#include <QtCore/qstringlist.h>
#include <QtCore/qurl.h>
#include <QtCore/qvariant.h>
#include <QtWidgets/qtextedit.h>

#include <vector>

class QTextBrowser : public QTextEdit {
    Q_OBJECT
    Q_PROPERTY(QUrl source READ source WRITE setSource)
    Q_PROPERTY(QStringList searchPaths READ searchPaths WRITE setSearchPaths)
    Q_PROPERTY(bool openExternalLinks READ openExternalLinks WRITE setOpenExternalLinks)
    Q_PROPERTY(bool openLinks READ openLinks WRITE setOpenLinks)
public:
    explicit QTextBrowser(QWidget *parent = nullptr);
    ~QTextBrowser() override;

    QUrl source() const;
    QStringList searchPaths() const { return m_searchPaths; }
    void setSearchPaths(const QStringList &paths) { m_searchPaths = paths; }
    virtual QVariant loadResource(int type, const QUrl &name);
    bool isBackwardAvailable() const { return m_position > 0; }
    bool isForwardAvailable() const { return m_position + 1 < static_cast<int>(m_history.size()); }
    void clearHistory();
    QString historyTitle(int i) const;
    QUrl historyUrl(int i) const;
    int backwardHistoryCount() const { return std::max(0, m_position); }
    int forwardHistoryCount() const;
    bool openExternalLinks() const { return m_openExternal; }
    void setOpenExternalLinks(bool open) { m_openExternal = open; }
    bool openLinks() const { return m_openLinks; }
    void setOpenLinks(bool open) { m_openLinks = open; }

public slots:
    virtual void setSource(const QUrl &name);
    virtual void backward();
    virtual void forward();
    virtual void home();
    virtual void reload();

signals:
    /*!
        This signal is emitted when going backward becomes possible or not; \a available says which.
    */
    void backwardAvailable(bool available) { emitSignal(&QTextBrowser::backwardAvailable, available); }
    /*!
        This signal is emitted when going forward becomes possible or not; \a available says which.
    */
    void forwardAvailable(bool available) { emitSignal(&QTextBrowser::forwardAvailable, available); }
    /*!
        This signal is emitted when the history changes.
    */
    void historyChanged() { emitSignal(&QTextBrowser::historyChanged); }
    /*!
        This signal is emitted when the source changes to \a src.
    */
    void sourceChanged(const QUrl &src) { emitSignal(&QTextBrowser::sourceChanged, src); }

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void show(const QUrl &url);
    void notifyHistory(bool back, bool fwd);

    std::vector<QUrl> m_history;
    int m_position = -1;
    QStringList m_searchPaths;
    bool m_openExternal = false;
    bool m_openLinks = true;
};
