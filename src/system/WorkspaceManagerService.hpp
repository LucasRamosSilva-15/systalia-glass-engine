#pragma once

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QQuickItem>
#include <QTimer>

class WorkspaceManagerService final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int currentWorkspace READ currentWorkspace NOTIFY currentWorkspaceChanged)
    Q_PROPERTY(int totalWorkspaces READ totalWorkspaces WRITE setTotalWorkspaces
               NOTIFY totalWorkspacesChanged)
    Q_PROPERTY(bool isTransitioning READ isTransitioning WRITE setTransitioning
               NOTIFY isTransitioningChanged)
    Q_PROPERTY(QQuickItem *activeWindow READ activeWindow WRITE setActiveWindow
               NOTIFY activeWindowChanged)
    Q_PROPERTY(int workspaceWindowsRevision READ workspaceWindowsRevision
               NOTIFY workspaceWindowsChanged)

public:
    static constexpr int kDefaultTotalWorkspaces = 4;
    static constexpr int kTransitionDurationMs = 320;

    explicit WorkspaceManagerService(QObject *parent = nullptr);
    ~WorkspaceManagerService() override;

    static WorkspaceManagerService *instance() noexcept;

    int currentWorkspace() const noexcept { return m_currentWorkspace; }
    int totalWorkspaces() const noexcept { return m_totalWorkspaces; }
    bool isTransitioning() const noexcept { return m_isTransitioning; }
    QQuickItem *activeWindow() const noexcept { return m_activeWindow.data(); }
    int workspaceWindowsRevision() const noexcept { return m_workspaceWindowsRevision; }

    Q_INVOKABLE void setTotalWorkspaces(int count);
    Q_INVOKABLE void setTransitioning(bool transitioning);
    Q_INVOKABLE void setActiveWindow(QQuickItem *window);

    Q_INVOKABLE void switchToWorkspace(int index);
    Q_INVOKABLE void nextWorkspace();
    Q_INVOKABLE void previousWorkspace();
    Q_INVOKABLE void moveActiveWindowToWorkspace(int targetIndex);

    Q_INVOKABLE int windowWorkspace(QQuickItem *window) const;
    Q_INVOKABLE void setWindowWorkspace(QQuickItem *window, int workspaceIndex);

    Q_INVOKABLE bool hasWindowsInWorkspace(int workspaceIndex) const;
    Q_INVOKABLE int windowCountInWorkspace(int workspaceIndex) const;

    int objectWorkspace(QObject *object) const;
    void setObjectWorkspace(QObject *object, int workspaceIndex);
    void unregisterObject(QObject *object);

signals:
    void currentWorkspaceChanged(int currentWorkspace);
    void totalWorkspacesChanged(int totalWorkspaces);
    void isTransitioningChanged(bool isTransitioning);
    void activeWindowChanged(QQuickItem *activeWindow);
    void windowWorkspaceChanged(QQuickItem *window, int workspaceIndex);
    void objectWorkspaceChanged(QObject *object, int workspaceIndex);
    void activeWindowMoveRequested(int targetIndex);
    void workspaceWindowsChanged();

private:
    void bumpWorkspaceWindowsRevision();

    int m_currentWorkspace = 0;
    int m_totalWorkspaces = kDefaultTotalWorkspaces;
    bool m_isTransitioning = false;
    int m_workspaceWindowsRevision = 0;
    QPointer<QQuickItem> m_activeWindow;
    QHash<QQuickItem *, int> m_windowWorkspaces;
    QHash<QObject *, int> m_objectWorkspaces;
    QTimer m_transitionTimer;
};

