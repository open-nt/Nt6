// MIT License - see LICENSE
#pragma once

#include <QtCore/qobject.h>
#include <QtCore/qpointer.h>
#include <QtCore/qrect.h>
#include <QtCore/qstringlist.h>
#include <QtWidgets/qwidget.h>

class QAbstractItemView;
class QKeyEvent;
class QLineEdit;
class QListWidget;
class QAbstractItemModel;
class QStringListModel;

class QCompleter : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString completionPrefix READ completionPrefix WRITE setCompletionPrefix)
    Q_PROPERTY(CompletionMode completionMode READ completionMode WRITE setCompletionMode)
    Q_PROPERTY(Qt::MatchFlags filterMode READ filterMode WRITE setFilterMode)
    Q_PROPERTY(int maxVisibleItems READ maxVisibleItems WRITE setMaxVisibleItems)
    Q_PROPERTY(Qt::CaseSensitivity caseSensitivity READ caseSensitivity WRITE setCaseSensitivity)
    Q_PROPERTY(bool wrapAround READ wrapAround WRITE setWrapAround)
    Q_PROPERTY(int completionColumn READ completionColumn WRITE setCompletionColumn)
    Q_PROPERTY(int completionRole READ completionRole WRITE setCompletionRole)
public:
    enum CompletionMode { PopupCompletion, UnfilteredPopupCompletion, InlineCompletion };
    Q_ENUM(CompletionMode)
    enum ModelSorting { UnsortedModel = 0, CaseSensitivelySortedModel, CaseInsensitivelySortedModel };
    Q_ENUM(ModelSorting)

    explicit QCompleter(QObject *parent = nullptr);
    explicit QCompleter(const QStringList &completions, QObject *parent = nullptr);
    explicit QCompleter(QAbstractItemModel *model, QObject *parent = nullptr);
    ~QCompleter() override;

    void setWidget(QWidget *widget);
    QWidget *widget() const { return m_widget.data(); }
    void setCompletionMode(CompletionMode mode) { m_mode = mode; }
    CompletionMode completionMode() const { return m_mode; }
    void setFilterMode(Qt::MatchFlags filterMode);
    Qt::MatchFlags filterMode() const { return m_filter; }
    void setCaseSensitivity(Qt::CaseSensitivity caseSensitivity);
    Qt::CaseSensitivity caseSensitivity() const { return m_case; }
    void setModelSorting(ModelSorting sorting) { m_sorting = sorting; }
    ModelSorting modelSorting() const { return m_sorting; }
    void setMaxVisibleItems(int maxItems);
    int maxVisibleItems() const { return m_maxVisible; }
    void setWrapAround(bool wrap) { m_wrap = wrap; }
    bool wrapAround() const { return m_wrap; }

    QString completionPrefix() const { return m_prefix; }
    int completionCount() const { return static_cast<int>(m_matches.size()); }
    bool setCurrentRow(int row);
    int currentRow() const { return m_row; }
    QString currentCompletion() const;
    QAbstractItemView *popup() const;
    QAbstractItemModel *model() const;
    void setModel(QAbstractItemModel *model);
    int completionColumn() const { return m_column; }
    void setCompletionColumn(int column);
    int completionRole() const { return m_role; }
    void setCompletionRole(int role);
    QAbstractItemModel *completionModel() const;
    QModelIndex currentIndex() const;
    virtual QString pathFromIndex(const QModelIndex &index) const;
    virtual QStringList splitPath(const QString &path) const;

public slots:
    void setCompletionPrefix(const QString &prefix);
    void complete(const QRect &rect = QRect());

signals:
    /*!
        This signal is emitted when the user chooses the completion \a text, from the popup or with Return.
    */
    void activated(const QString &text) { emitSignal(&QCompleter::activated, text); }
    /*!
        This signal is emitted when the user moves to the completion \a text in the popup.
    */
    void highlighted(const QString &text) { emitSignal(&QCompleter::highlighted, text); }

private:
    friend class QLineEdit;
    void filter();
    bool matches(const QString &candidate) const;
    void showPopup(const QRect &rect);
    void hidePopup();
    void choose(int row);
    // What a line edit asks of its completer.
    bool handleKey(QKeyEvent *event);
    // `deleting`: the user deleted text, which inline completion must not put back.
    void textEdited(const QString &edited, bool deleting);

    void readModel(); // takes the completions from the model
    QStringList m_completions;
    QPointer<QAbstractItemModel> m_model;
    bool m_ownModel = false;
    QList<QMetaObject::Connection> m_modelConnections;
    mutable QStringListModel *m_completionModel = nullptr;
    int m_column = 0;
    int m_role = Qt::EditRole;
    QStringList m_matches;
    QList<int> m_matchRows; // the row of the model of each match, for its icon
    QString m_prefix;
    QPointer<QWidget> m_widget;
    QListWidget *m_popup = nullptr;
    CompletionMode m_mode = PopupCompletion;
    Qt::MatchFlags m_filter = Qt::MatchStartsWith;
    Qt::CaseSensitivity m_case = Qt::CaseSensitive;
    ModelSorting m_sorting = UnsortedModel;
    int m_maxVisible = 7;
    int m_row = -1;
    bool m_wrap = true;
};
