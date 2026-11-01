#include "WorkspaceManagerService.hpp"

#include <QSet>
#include <QVariant>
#include <algorithm>
#include <cmath>

namespace {
WorkspaceManagerService *s_instance = nullptr;
} // namespace

WorkspaceManagerService::WorkspaceManagerService(QObject *parent)
    : QObject(parent) {
    s_instance = this;

    m_transitionTimer.setSingleShot(true);
    m_transitionTimer.setInterval(kTransitionDurationMs);
    connect(&m_transitionTimer, &QTimer::timeout, this, [this]() {
        setTransitioning(false);
    });
}

WorkspaceManagerService::~WorkspaceManagerService() {
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

WorkspaceManagerService *WorkspaceManagerService::instance() noexcept {
    return s_instance;
}

void WorkspaceManagerService::setTotalWorkspaces(int count) {
    const int clampedCount = std::max(1, count);
    if (m_totalWorkspaces == clampedCount) {
        return;
    }

    m_totalWorkspaces = clampedCount;
    emit totalWorkspacesChanged(m_totalWorkspaces);

    if (m_currentWorkspace >= m_totalWorkspaces) {
        switchToWorkspace(m_totalWorkspaces - 1);
    }
}

void WorkspaceManagerService::setTransitioning(bool transitioning) {
    if (m_isTransitioning == transitioning) {
        return;
    }
    m_isTransitioning = transitioning;
    syncWindowCullingStates();
    emit isTransitioningChanged(m_isTransitioning);
}

void WorkspaceManagerService::setActiveWindow(QQuickItem *window) {
    if (m_activeWindow.data() == window) {
        return;
    }
    m_activeWindow = window;
    emit activeWindowChanged(m_activeWindow.data());
}

void WorkspaceManagerService::switchToWorkspace(int index) {
    if (index < 0 || index >= m_totalWorkspaces) {
        return;
    }
    if (m_currentWorkspace == index) {
        return;
    }

    m_previousWorkspace = m_currentWorkspace;
    m_currentWorkspace = index;
    setTransitioning(true);
    m_transitionTimer.start();
    syncWindowCullingStates();
    emit currentWorkspaceChanged(m_currentWorkspace);
}

void WorkspaceManagerService::nextWorkspace() {
    if (m_currentWorkspace + 1 < m_totalWorkspaces) {
        switchToWorkspace(m_currentWorkspace + 1);
    }
}

void WorkspaceManagerService::previousWorkspace() {
    if (m_currentWorkspace - 1 >= 0) {
        switchToWorkspace(m_currentWorkspace - 1);
    }
}

void WorkspaceManagerService::moveActiveWindowToWorkspace(int targetIndex) {
    if (targetIndex < 0 || targetIndex >= m_totalWorkspaces) {
        return;
    }

    if (m_activeWindow) {
        setWindowWorkspace(m_activeWindow.data(), targetIndex);
    }
    emit activeWindowMoveRequested(targetIndex);
}

int WorkspaceManagerService::windowWorkspace(QQuickItem *window) const {
    if (!window) {
        return -1;
    }
    return m_windowWorkspaces.value(window, 0);
}

void WorkspaceManagerService::setWindowWorkspace(QQuickItem *window, int workspaceIndex) {
    if (!window || workspaceIndex < 0 || workspaceIndex >= m_totalWorkspaces) {
        return;
    }

    const bool alreadyTracked = m_windowWorkspaces.contains(window);
    const int previousWorkspace = m_windowWorkspaces.value(window, -1);

    if (!alreadyTracked) {
        connect(window, &QObject::destroyed, this, [this, window]() {
            if (m_windowWorkspaces.remove(window) > 0) {
                bumpWorkspaceWindowsRevision();
            }
        });
    }

    m_windowWorkspaces.insert(window, workspaceIndex);
    if (window->metaObject()->indexOfProperty("workspaceIndex") >= 0) {
        window->setProperty("workspaceIndex", workspaceIndex);
    }

    if (QObject *toplevel = window->property("toplevel").value<QObject *>()) {
        setObjectWorkspace(toplevel, workspaceIndex);
    }

    if (!alreadyTracked || previousWorkspace != workspaceIndex) {
        syncWindowCullingStates();
        emit windowWorkspaceChanged(window, workspaceIndex);
        bumpWorkspaceWindowsRevision();
    }
}

bool WorkspaceManagerService::hasWindowsInWorkspace(int workspaceIndex) const {
    return windowCountInWorkspace(workspaceIndex) > 0;
}

bool WorkspaceManagerService::isWorkspaceWithinFrustum(int workspaceIndex, qreal viewportOffset,
                                                       qreal screenWidth) const noexcept {
    if (workspaceIndex < 0 || workspaceIndex >= m_totalWorkspaces) {
        return false;
    }
    if (screenWidth <= 0.0) {
        return workspaceIndex == m_currentWorkspace;
    }
    const qreal xViewport = static_cast<qreal>(workspaceIndex) * screenWidth + viewportOffset;
    return std::abs(xViewport) < screenWidth;
}

bool WorkspaceManagerService::isWindowVisibleInViewport(QQuickItem *window, qreal viewportOffset,
                                                        qreal screenWidth) const {
    if (!window) {
        return false;
    }
    const int ws = windowWorkspace(window);
    if (!m_isTransitioning) {
        return ws == m_currentWorkspace;
    }
    return isWorkspaceWithinFrustum(ws, viewportOffset, screenWidth);
}

int WorkspaceManagerService::applyViewportCulling(qreal viewportOffset, qreal screenWidth) {
    int activeCount = 0;

    auto setItemOccludedRecursive = [](QQuickItem *root, bool occluded) {
        if (!root) {
            return;
        }
        if (root->metaObject()->indexOfProperty("occluded") >= 0) {
            root->setProperty("occluded", occluded);
        }
        const auto children = root->childItems();
        for (QQuickItem *child : children) {
            if (child && child->metaObject()->indexOfProperty("occluded") >= 0) {
                child->setProperty("occluded", occluded);
            }
        }
    };

    for (auto it = m_windowWorkspaces.cbegin(); it != m_windowWorkspaces.cend(); ++it) {
        QQuickItem *item = it.key();
        if (!item) {
            continue;
        }
        const int ws = it.value();
        const bool visibleInFrustum = isWorkspaceWithinFrustum(ws, viewportOffset, screenWidth)
                                      && (m_isTransitioning || ws == m_currentWorkspace);
        item->setVisible(visibleInFrustum);
        setItemOccludedRecursive(item, !visibleInFrustum);
        if (visibleInFrustum) {
            ++activeCount;
        }
    }

    return activeCount;
}

int WorkspaceManagerService::activeVisibleWindowCount() const {
    int activeCount = 0;
    for (auto it = m_windowWorkspaces.cbegin(); it != m_windowWorkspaces.cend(); ++it) {
        QQuickItem *item = it.key();
        if (!item) {
            continue;
        }
        if (it.value() == m_currentWorkspace && item->isVisible()) {
            ++activeCount;
        }
    }
    return activeCount;
}

void WorkspaceManagerService::syncWindowCullingStates() {
    constexpr qreal kDefaultScreenWidth = 1920.0;
    if (m_isTransitioning) {
        const int minWs = std::min(m_previousWorkspace, m_currentWorkspace);
        const int maxWs = std::max(m_previousWorkspace, m_currentWorkspace);
        for (auto it = m_windowWorkspaces.cbegin(); it != m_windowWorkspaces.cend(); ++it) {
            QQuickItem *item = it.key();
            if (!item) {
                continue;
            }
            const int ws = it.value();
            const bool visible = (ws >= minWs && ws <= maxWs);
            item->setVisible(visible);
            if (item->metaObject()->indexOfProperty("occluded") >= 0) {
                item->setProperty("occluded", !visible);
            }
        }
    } else {
        const qreal defaultOffset = -static_cast<qreal>(m_currentWorkspace) * kDefaultScreenWidth;
        applyViewportCulling(defaultOffset, kDefaultScreenWidth);
    }
}

int WorkspaceManagerService::windowCountInWorkspace(int workspaceIndex) const {
    if (workspaceIndex < 0 || workspaceIndex >= m_totalWorkspaces) {
        return 0;
    }

    int count = 0;
    QSet<QObject *> countedObjects;

    for (auto it = m_windowWorkspaces.cbegin(); it != m_windowWorkspaces.cend(); ++it) {
        QQuickItem *item = it.key();
        if (!item) {
            continue;
        }
        countedObjects.insert(item);
        if (QObject *toplevel = item->property("toplevel").value<QObject *>()) {
            countedObjects.insert(toplevel);
        }
        if (it.value() == workspaceIndex) {
            ++count;
        }
    }

    for (auto it = m_objectWorkspaces.cbegin(); it != m_objectWorkspaces.cend(); ++it) {
        QObject *obj = it.key();
        if (!obj || countedObjects.contains(obj)) {
            continue;
        }
        if (it.value() == workspaceIndex) {
            ++count;
        }
    }

    return count;
}

int WorkspaceManagerService::objectWorkspace(QObject *object) const {
    if (!object) {
        return -1;
    }
    return m_objectWorkspaces.value(object, 0);
}

void WorkspaceManagerService::setObjectWorkspace(QObject *object, int workspaceIndex) {
    if (!object || workspaceIndex < 0 || workspaceIndex >= m_totalWorkspaces) {
        return;
    }

    const bool alreadyTracked = m_objectWorkspaces.contains(object);
    const int previousWorkspace = m_objectWorkspaces.value(object, -1);

    if (!alreadyTracked) {
        connect(object, &QObject::destroyed, this, [this, object]() {
            if (m_objectWorkspaces.remove(object) > 0) {
                bumpWorkspaceWindowsRevision();
            }
        });
    }

    m_objectWorkspaces.insert(object, workspaceIndex);
    if (!alreadyTracked || previousWorkspace != workspaceIndex) {
        emit objectWorkspaceChanged(object, workspaceIndex);
        bumpWorkspaceWindowsRevision();
    }
}

void WorkspaceManagerService::unregisterObject(QObject *object) {
    if (!object) {
        return;
    }
    bool changed = false;
    if (m_objectWorkspaces.remove(object) > 0) {
        changed = true;
    }
    for (auto it = m_windowWorkspaces.begin(); it != m_windowWorkspaces.end(); ) {
        if (static_cast<QObject *>(it.key()) == object) {
            it = m_windowWorkspaces.erase(it);
            changed = true;
        } else {
            ++it;
        }
    }
    if (changed) {
        bumpWorkspaceWindowsRevision();
    }
}

void WorkspaceManagerService::bumpWorkspaceWindowsRevision() {
    ++m_workspaceWindowsRevision;
    emit workspaceWindowsChanged();
}

