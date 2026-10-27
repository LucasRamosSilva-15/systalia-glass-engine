#include "WindowStackModel.hpp"

#include <QWaylandSurface>
#include <QtWaylandCompositor/QWaylandXdgSurface>
#include <QtWaylandCompositor/QWaylandXdgToplevel>

WindowStackModel::WindowStackModel(QObject *parent)
    : QAbstractListModel(parent) {
}

int WindowStackModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_windows.size();
}

QVariant WindowStackModel::data(const QModelIndex &modelIndex, int role) const {
    if (!modelIndex.isValid() || modelIndex.row() < 0 || modelIndex.row() >= m_windows.size())
        return {};

    QObject *window = m_windows.at(modelIndex.row()).window.data();
    if (!window)
        return {};

    switch (role) {
    case Qt::DisplayRole:
    case TitleRole: {
        const QString title = window->property("title").toString().trimmed();
        return title.isEmpty() ? window->property("appId").toString() : title;
    }
    case ToplevelRole:
        return QVariant::fromValue(window);
    case SurfaceRole: {
        if (auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(window)) {
            QWaylandXdgSurface *xdgSurface = toplevel->xdgSurface();
            return QVariant::fromValue(xdgSurface ? xdgSurface->surface() : nullptr);
        }
        return window->property("surface");
    }
    case AppIdRole:
        return window->property("appId").toString();
    case IconNameRole: {
        const QString appId = window->property("appId").toString().trimmed();
        return appId.isEmpty() ? QStringLiteral("application-x-executable") : appId;
    }
    default:
        return {};
    }
}

QHash<int, QByteArray> WindowStackModel::roleNames() const {
    return {
        {ToplevelRole, QByteArrayLiteral("toplevel")},
        {SurfaceRole, QByteArrayLiteral("surface")},
        {TitleRole, QByteArrayLiteral("title")},
        {AppIdRole, QByteArrayLiteral("appId")},
        {IconNameRole, QByteArrayLiteral("iconName")}
    };
}

QObject *WindowStackModel::currentWindow() const {
    return windowAt(0);
}

QObject *WindowStackModel::windowAt(int index) const {
    if (index < 0 || index >= m_windows.size())
        return nullptr;
    return m_windows.at(index).window.data();
}

QObject *WindowStackModel::selectedWindow() const {
    if (m_taskSwitcherVisible)
        return windowAt(m_taskSwitcherIndex);
    return currentWindow();
}

int WindowStackModel::indexOf(QObject *window) const {
    if (!window)
        return -1;
    for (int index = 0; index < m_windows.size(); ++index) {
        if (m_windows.at(index).window.data() == window)
            return index;
    }
    return -1;
}

bool WindowStackModel::registerWindow(QObject *window) {
    if (!window || indexOf(window) >= 0)
        return false;

    QObject *selectedBeforeInsert = selectedWindow();
    const quint64 id = m_nextId++;
    beginInsertRows(QModelIndex(), 0, 0);
    m_windows.prepend(Entry{id, window});
    endInsertRows();

    connect(window, &QObject::destroyed, this, [this, id] {
        unregisterWindowById(id);
    });
    if (auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(window)) {
        connect(toplevel, &QWaylandXdgToplevel::titleChanged,
                this, [this, window] { notifyWindowPropertiesChanged(window); });
        connect(toplevel, &QWaylandXdgToplevel::appIdChanged,
                this, [this, window] { notifyWindowPropertiesChanged(window); });
    }

    if (m_taskSwitcherVisible && selectedBeforeInsert)
        setTaskSwitcherIndex(indexOf(selectedBeforeInsert));
    else
        setTaskSwitcherIndex(0);
    emit windowCountChanged();
    emit stackChanged();
    return true;
}

bool WindowStackModel::unregisterWindow(QObject *window) {
    const int row = indexOf(window);
    if (row < 0)
        return false;
    unregisterWindowById(m_windows.at(row).id);
    return true;
}

bool WindowStackModel::promoteWindow(QObject *window) {
    const int sourceRow = indexOf(window);
    if (sourceRow < 0 || sourceRow == 0)
        return false;

    QObject *selectedBeforeMove = selectedWindow();
    if (!beginMoveRows(QModelIndex(), sourceRow, sourceRow, QModelIndex(), 0))
        return false;
    m_windows.move(sourceRow, 0);
    endMoveRows();
    if (m_taskSwitcherVisible && selectedBeforeMove)
        setTaskSwitcherIndex(indexOf(selectedBeforeMove));
    emit stackChanged();
    return true;
}

bool WindowStackModel::beginTaskSwitch(int direction) {
    if (m_windows.size() < 2 || direction == 0)
        return false;

    setTaskSwitcherVisible(true);
    const int count = m_windows.size();
    int index = m_taskSwitcherIndex;
    index = direction > 0 ? (index + 1) % count : (index + count - 1) % count;
    setTaskSwitcherIndex(index);
    return true;
}

QObject *WindowStackModel::commitTaskSwitch() {
    return selectAndCommit(m_taskSwitcherIndex);
}

QObject *WindowStackModel::selectAndCommit(int index) {
    QObject *window = windowAt(index);
    if (!window)
        return nullptr;

    setTaskSwitcherVisible(false);
    promoteWindow(window);
    setTaskSwitcherIndex(0);
    emit windowSelected(window);
    return window;
}

void WindowStackModel::cancelTaskSwitch() {
    setTaskSwitcherVisible(false);
    setTaskSwitcherIndex(0);
}

void WindowStackModel::setTaskSwitcherVisible(bool visible) {
    if (m_taskSwitcherVisible == visible)
        return;
    m_taskSwitcherVisible = visible;
    emit taskSwitcherVisibleChanged();
}

void WindowStackModel::setTaskSwitcherIndex(int index) {
    if (m_windows.isEmpty())
        index = 0;
    else
        index = qBound(0, index, m_windows.size() - 1);
    if (m_taskSwitcherIndex == index)
        return;
    m_taskSwitcherIndex = index;
    emit taskSwitcherIndexChanged();
}

void WindowStackModel::unregisterWindowById(quint64 id) {
    int row = -1;
    for (int index = 0; index < m_windows.size(); ++index) {
        if (m_windows.at(index).id == id) {
            row = index;
            break;
        }
    }
    if (row < 0)
        return;

    const int previousIndex = m_taskSwitcherIndex;
    beginRemoveRows(QModelIndex(), row, row);
    m_windows.removeAt(row);
    endRemoveRows();
    if (m_windows.isEmpty()) {
        setTaskSwitcherVisible(false);
        setTaskSwitcherIndex(0);
    } else if (m_windows.size() < 2) {
        setTaskSwitcherVisible(false);
        setTaskSwitcherIndex(0);
    } else if (m_taskSwitcherVisible) {
        const int adjustedIndex = row < previousIndex ? previousIndex - 1
                                : qMin(previousIndex, m_windows.size() - 1);
        setTaskSwitcherIndex(adjustedIndex);
    } else {
        setTaskSwitcherIndex(0);
    }
    emit windowCountChanged();
    emit stackChanged();
}

void WindowStackModel::notifyWindowPropertiesChanged(QObject *window) {
    const int row = indexOf(window);
    if (row < 0)
        return;
    const QModelIndex changedIndex = index(row, 0);
    emit dataChanged(changedIndex, changedIndex,
                     {Qt::DisplayRole, TitleRole, AppIdRole, IconNameRole});
}
