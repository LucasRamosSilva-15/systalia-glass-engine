#pragma once

#include <QAbstractListModel>
#include <QPointer>
#include <QVector>

class QObject;

class WindowStackModel final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int windowCount READ windowCount NOTIFY windowCountChanged)
    Q_PROPERTY(bool taskSwitcherVisible READ taskSwitcherVisible NOTIFY taskSwitcherVisibleChanged)
    Q_PROPERTY(int taskSwitcherIndex READ taskSwitcherIndex NOTIFY taskSwitcherIndexChanged)
    Q_PROPERTY(QObject *currentWindow READ currentWindow NOTIFY stackChanged)

public:
    enum Role {
        ToplevelRole = Qt::UserRole + 1,
        SurfaceRole,
        TitleRole,
        AppIdRole,
        IconNameRole
    };
    Q_ENUM(Role)

    explicit WindowStackModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int windowCount() const { return m_windows.size(); }
    bool taskSwitcherVisible() const { return m_taskSwitcherVisible; }
    int taskSwitcherIndex() const { return m_taskSwitcherIndex; }
    QObject *currentWindow() const;
    QObject *windowAt(int index) const;
    QObject *selectedWindow() const;
    int indexOf(QObject *window) const;

    bool registerWindow(QObject *window);
    bool unregisterWindow(QObject *window);
    bool promoteWindow(QObject *window);
    bool beginTaskSwitch(int direction);
    QObject *commitTaskSwitch();
    QObject *selectAndCommit(int index);
    void cancelTaskSwitch();

signals:
    void windowCountChanged();
    void taskSwitcherVisibleChanged();
    void taskSwitcherIndexChanged();
    void stackChanged();
    void windowSelected(QObject *window);

private:
    struct Entry {
        quint64 id = 0;
        QPointer<QObject> window;
    };

    void setTaskSwitcherVisible(bool visible);
    void setTaskSwitcherIndex(int index);
    void unregisterWindowById(quint64 id);
    void notifyWindowPropertiesChanged(QObject *window);

    QVector<Entry> m_windows;
    quint64 m_nextId = 1;
    int m_taskSwitcherIndex = 0;
    bool m_taskSwitcherVisible = false;
};
