#include "WaylibSurfaceAdapter.hpp"
#include "WaylibOutputAdapter.hpp"
#include "WorkspaceManagerService.hpp"

#include <woutput.h>
#include <wseat.h>
#include <wsurface.h>
#include <wxdgshell.h>
#include <wxdgtoplevelsurface.h>

#include <wlr_all.h>

#include <QDebug>

WAYLIB_SERVER_USE_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

WaylibSurfaceListModel::WaylibSurfaceListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int WaylibSurfaceListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant WaylibSurfaceListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    const Entry &entry = m_entries.at(index.row());
    WXdgToplevelSurface *toplevel = entry.shellSurface.data();

    switch (role) {
    case SurfaceRole:
        return QVariant::fromValue(toplevel ? toplevel->surface() : nullptr);
    case ShellSurfaceRole:
        return QVariant::fromValue(toplevel);
    case TitleRole:
        return toplevel ? toplevel->title() : QString();
    case AppIdRole:
        return toplevel ? toplevel->appId() : QString();
    case ActivatedRole:
        return entry.activated || (toplevel && toplevel->isActivated());
    case MinimizedRole:
        return entry.minimized;
    case MaximizedRole:
        return entry.maximized;
    case SurfaceIdRole:
        return static_cast<quint64>(entry.surfaceId);
    case ZOrderRole:
        return entry.zOrder;
    case WorkspaceIndexRole:
        return entry.workspaceIndex;
    default:
        return {};
    }
}

QHash<int, QByteArray> WaylibSurfaceListModel::roleNames() const {
    return {
        {SurfaceRole, "surface"},
        {ShellSurfaceRole, "shellSurface"},
        {TitleRole, "title"},
        {AppIdRole, "appId"},
        {ActivatedRole, "activated"},
        {MinimizedRole, "minimized"},
        {MaximizedRole, "maximized"},
        {SurfaceIdRole, "surfaceId"},
        {ZOrderRole, "zOrder"},
        {WorkspaceIndexRole, "workspaceIndex"},
    };
}

int WaylibSurfaceListModel::indexOf(WXdgToplevelSurface *surface) const {
    if (!surface) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        if (m_entries.at(i).shellSurface.data() == surface) {
            return i;
        }
    }
    return -1;
}

WaylibSurfaceListModel::Entry *WaylibSurfaceListModel::entryAt(int row) {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }
    return &m_entries[row];
}

const WaylibSurfaceListModel::Entry *WaylibSurfaceListModel::entryAt(int row) const {
    if (row < 0 || row >= rowCount()) {
        return nullptr;
    }
    return &m_entries.at(row);
}

void WaylibSurfaceListModel::appendEntry(const Entry &entry) {
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

void WaylibSurfaceListModel::removeRowAt(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
}

void WaylibSurfaceListModel::notifyRowChanged(int row, const QList<int> &roles) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    const QModelIndex idx = index(row, 0);
    Q_EMIT dataChanged(idx, idx, roles);
}

void WaylibSurfaceListModel::clearAll() {
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

WaylibPopupListModel::WaylibPopupListModel(QObject *parent)
    : QAbstractListModel(parent) {}

int WaylibPopupListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_entries.size());
}

QVariant WaylibPopupListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }
    WXdgPopupSurface *popup = m_entries.at(index.row()).popupSurface.data();
    switch (role) {
    case SurfaceRole:
        return QVariant::fromValue(popup ? popup->surface() : nullptr);
    case PopupSurfaceRole:
        return QVariant::fromValue(popup);
    case ParentSurfaceRole:
        return QVariant::fromValue(popup ? popup->parentSurface() : nullptr);
    case PopupXRole:
        return popup ? popup->getPopupPosition().x() : 0.0;
    case PopupYRole:
        return popup ? popup->getPopupPosition().y() : 0.0;
    default:
        return {};
    }
}

QHash<int, QByteArray> WaylibPopupListModel::roleNames() const {
    return {
        {SurfaceRole, "surface"},
        {PopupSurfaceRole, "popupSurface"},
        {ParentSurfaceRole, "parentSurface"},
        {PopupXRole, "popupX"},
        {PopupYRole, "popupY"},
    };
}

int WaylibPopupListModel::indexOf(WXdgPopupSurface *popup) const {
    if (!popup) {
        return -1;
    }
    for (int i = 0; i < static_cast<int>(m_entries.size()); ++i) {
        if (m_entries.at(i).popupSurface.data() == popup) {
            return i;
        }
    }
    return -1;
}

void WaylibPopupListModel::appendEntry(const Entry &entry) {
    const int row = rowCount();
    beginInsertRows(QModelIndex(), row, row);
    m_entries.append(entry);
    endInsertRows();
}

void WaylibPopupListModel::removeRowAt(int row) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();
}

void WaylibPopupListModel::notifyRowChanged(int row, const QList<int> &roles) {
    if (row < 0 || row >= rowCount()) {
        return;
    }
    const QModelIndex idx = index(row, 0);
    Q_EMIT dataChanged(idx, idx, roles);
}

void WaylibPopupListModel::clearAll() {
    if (m_entries.isEmpty()) {
        return;
    }
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

WaylibSurfaceAdapter::WaylibSurfaceAdapter(QObject *parent)
    : QObject(parent),
      m_model(new WaylibSurfaceListModel(this)),
      m_popupModel(new WaylibPopupListModel(this)) {}

void WaylibSurfaceAdapter::attachXdgShell(WXdgShell *xdgShell) {
    if (!xdgShell) {
        return;
    }
    connect(xdgShell, &WXdgShell::toplevelSurfaceAdded,
            this, &WaylibSurfaceAdapter::onToplevelAdded);
    connect(xdgShell, &WXdgShell::toplevelSurfaceRemoved,
            this, &WaylibSurfaceAdapter::onToplevelRemoved);
    connect(xdgShell, &WXdgShell::popupSurfaceAdded,
            this, &WaylibSurfaceAdapter::onPopupAdded);
    connect(xdgShell, &WXdgShell::popupSurfaceRemoved,
            this, &WaylibSurfaceAdapter::onPopupRemoved);
}

void WaylibSurfaceAdapter::attachSeat(WSeat *seat) {
    m_seat = seat;
}

void WaylibSurfaceAdapter::attachOutputAdapter(WaylibOutputAdapter *outputAdapter) {
    if (m_outputAdapter == outputAdapter) {
        return;
    }
    if (m_outputAdapter) {
        m_outputAdapter->disconnect(this);
    }
    m_outputAdapter = outputAdapter;
    if (m_outputAdapter) {
        connect(m_outputAdapter, &WaylibOutputAdapter::outputReady,
                this, [this](WOutput *output) {
            if (!output) {
                return;
            }
            for (const auto &entry : m_model->entries()) {
                if (entry.shellSurface && entry.shellSurface->surface()) {
                    entry.shellSurface->surface()->enterOutput(output);
                }
            }
            for (const auto &popupEntry : m_popupModel->entries()) {
                if (popupEntry.popupSurface && popupEntry.popupSurface->surface()) {
                    popupEntry.popupSurface->surface()->enterOutput(output);
                }
            }
        });
    }
}

void WaylibSurfaceAdapter::reset() {
    const auto entries = m_model->entries();
    for (const auto &entry : entries) {
        if (entry.shellSurface) {
            entry.shellSurface->disconnect(this);
        }
    }
    const auto popups = m_popupModel->entries();
    for (const auto &popupEntry : popups) {
        if (popupEntry.popupSurface) {
            popupEntry.popupSurface->disconnect(this);
        }
    }
    const bool hadPopups = (m_popupModel->rowCount() > 0);
    const bool hadEntries = (m_model->rowCount() > 0);
    const bool hadActive = !m_activeSurface.isNull();
    m_activeSurface.clear();
    if (m_seat && m_seat->handle()) {
        m_seat->clearKeyboardFocusSurface();
    }
    if (hadPopups) {
        m_popupModel->clearAll();
        Q_EMIT popupsChanged();
    }
    if (hadEntries) {
        m_model->clearAll();
        Q_EMIT surfacesChanged();
    }
    if (hadActive) {
        Q_EMIT activeSurfaceChanged();
    }
}

QList<QObject *> WaylibSurfaceAdapter::surfaces() const {
    QList<QObject *> result;
    const auto &entries = m_model->entries();
    result.reserve(entries.size());
    for (const auto &entry : entries) {
        if (entry.shellSurface) {
            result.append(entry.shellSurface.data());
        }
    }
    return result;
}

QString WaylibSurfaceAdapter::activeTitle() const {
    return m_activeSurface ? m_activeSurface->title() : QString();
}

QString WaylibSurfaceAdapter::activeAppId() const {
    return m_activeSurface ? m_activeSurface->appId() : QString();
}

QStringList WaylibSurfaceAdapter::activeAppIds() const {
    QStringList ids;
    if (m_activeSurface) {
        const QString appId = m_activeSurface->appId();
        if (!appId.isEmpty()) {
            ids.append(appId);
        }
    }
    return ids;
}

void WaylibSurfaceAdapter::activateSurface(WXdgToplevelSurface *surface) {
    if (!surface) {
        WXdgToplevelSurface *previous = m_activeSurface.data();
        if (previous) {
            const int prevRow = m_model->indexOf(previous);
            if (auto *prevEntry = m_model->entryAt(prevRow)) {
                prevEntry->activated = false;
                m_model->notifyRowChanged(prevRow, {WaylibSurfaceListModel::ActivatedRole});
            }
            previous->setActivate(false);
        }
        const bool changed = !m_activeSurface.isNull();
        m_activeSurface.clear();
        syncSeatFocus(nullptr);
        if (changed) {
            Q_EMIT activeSurfaceChanged();
            Q_EMIT surfacesChanged();
        }
        return;
    }

    const int targetRow = m_model->indexOf(surface);
    if (targetRow < 0) {
        return;
    }

    auto *targetEntry = m_model->entryAt(targetRow);
    if (!targetEntry) {
        return;
    }

    if (targetEntry->minimized) {
        targetEntry->minimized = false;
        surface->setMinimize(false);
    }

    WXdgToplevelSurface *previous = m_activeSurface.data();
    const bool activeChanged = (previous != surface);
    if (previous && previous != surface) {
        const int prevRow = m_model->indexOf(previous);
        if (auto *prevEntry = m_model->entryAt(prevRow)) {
            prevEntry->activated = false;
            m_model->notifyRowChanged(prevRow, {WaylibSurfaceListModel::ActivatedRole});
        }
        previous->setActivate(false);
    }

    targetEntry->zOrder = ++m_nextZOrder;
    targetEntry->activated = true;
    m_activeSurface = surface;
    surface->setActivate(true);
    syncSeatFocus(surface);

    m_model->notifyRowChanged(targetRow, {
        WaylibSurfaceListModel::ActivatedRole,
        WaylibSurfaceListModel::MinimizedRole,
        WaylibSurfaceListModel::ZOrderRole
    });

    if (activeChanged) {
        Q_EMIT activeSurfaceChanged();
        Q_EMIT surfacesChanged();
    }
}

void WaylibSurfaceAdapter::raiseSurface(WXdgToplevelSurface *surface) {
    const int row = m_model->indexOf(surface);
    if (row < 0) {
        return;
    }
    if (auto *entry = m_model->entryAt(row)) {
        entry->zOrder = ++m_nextZOrder;
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::ZOrderRole});
    }
}

void WaylibSurfaceAdapter::requestClose(WXdgToplevelSurface *surface) {
    if (!surface || m_model->indexOf(surface) < 0) {
        return;
    }
    surface->close();
}

void WaylibSurfaceAdapter::toggleMaximize(WXdgToplevelSurface *surface) {
    const int row = m_model->indexOf(surface);
    if (row < 0) {
        return;
    }
    auto *entry = m_model->entryAt(row);
    if (!entry) {
        return;
    }
    entry->maximized = !entry->maximized;
    surface->setMaximize(entry->maximized);
    m_model->notifyRowChanged(row, {WaylibSurfaceListModel::MaximizedRole});
    activateSurface(surface);
}

void WaylibSurfaceAdapter::toggleMinimize(WXdgToplevelSurface *surface) {
    const int row = m_model->indexOf(surface);
    if (row < 0) {
        return;
    }
    auto *entry = m_model->entryAt(row);
    if (!entry) {
        return;
    }
    entry->minimized = !entry->minimized;
    surface->setMinimize(entry->minimized);

    if (entry->minimized && m_activeSurface.data() == surface) {
        entry->activated = false;
        surface->setActivate(false);
        m_model->notifyRowChanged(row, {
            WaylibSurfaceListModel::MinimizedRole,
            WaylibSurfaceListModel::ActivatedRole
        });
        WXdgToplevelSurface *next = topVisibleSurface(surface);
        activateSurface(next);
    } else if (!entry->minimized) {
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::MinimizedRole});
        activateSurface(surface);
    } else {
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::MinimizedRole});
    }
}

void WaylibSurfaceAdapter::focusOrRestore(WXdgToplevelSurface *surface) {
    if (!surface || m_model->indexOf(surface) < 0) {
        return;
    }
    activateSurface(surface);
}

void WaylibSurfaceAdapter::resizeSurface(WXdgToplevelSurface *surface, int width, int height) {
    if (!surface || m_model->indexOf(surface) < 0 || width <= 0 || height <= 0) {
        return;
    }
    surface->resize(QSize(width, height));
}

void WaylibSurfaceAdapter::closeActiveSurface() {
    if (m_activeSurface) {
        requestClose(m_activeSurface.data());
    }
}

void WaylibSurfaceAdapter::cycleFocus() {
    const auto &entries = m_model->entries();
    if (entries.isEmpty()) {
        return;
    }
    const int activeWs = WorkspaceManagerService::instance()
                             ? WorkspaceManagerService::instance()->currentWorkspace()
                             : 0;
    if (!m_activeSurface) {
        activateSurface(topVisibleSurface());
        return;
    }
    const int currentRow = m_model->indexOf(m_activeSurface.data());
    const int count = static_cast<int>(entries.size());
    for (int offset = 1; offset <= count; ++offset) {
        const int nextRow = (currentRow + offset) % count;
        const auto &candidate = entries.at(nextRow);
        if (candidate.shellSurface && !candidate.minimized && candidate.workspaceIndex == activeWs) {
            activateSurface(candidate.shellSurface.data());
            return;
        }
    }
    for (int offset = 1; offset <= count; ++offset) {
        const int nextRow = (currentRow + offset) % count;
        const auto &candidate = entries.at(nextRow);
        if (candidate.shellSurface && !candidate.minimized) {
            activateSurface(candidate.shellSurface.data());
            return;
        }
    }
}

int WaylibSurfaceListModelRowOrMinusOne(const WaylibSurfaceListModel *model,
                                        WXdgToplevelSurface *surface) {
    return model ? model->indexOf(surface) : -1;
}

int WaylibSurfaceAdapter::zOrderForSurface(WXdgToplevelSurface *surface) const {
    const int row = m_model->indexOf(surface);
    if (const auto *entry = m_model->entryAt(row)) {
        return entry->zOrder;
    }
    return -1;
}

int WaylibSurfaceAdapter::surfaceWorkspace(WXdgToplevelSurface *surface) const {
    const int row = m_model->indexOf(surface);
    if (const auto *entry = m_model->entryAt(row)) {
        return entry->workspaceIndex;
    }
    return 0;
}

void WaylibSurfaceAdapter::setSurfaceWorkspace(WXdgToplevelSurface *surface, int workspaceIndex) {
    const int row = m_model->indexOf(surface);
    if (row < 0) {
        return;
    }
    const int totalWs = WorkspaceManagerService::instance()
                            ? WorkspaceManagerService::instance()->totalWorkspaces()
                            : WorkspaceManagerService::kDefaultTotalWorkspaces;
    const int clamped = qBound(0, workspaceIndex, qMax(0, totalWs - 1));
    if (auto *entry = m_model->entryAt(row)) {
        if (entry->workspaceIndex == clamped) {
            return;
        }
        entry->workspaceIndex = clamped;
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::WorkspaceIndexRole});
    }
}

bool WaylibSurfaceAdapter::isMinimized(WXdgToplevelSurface *surface) const {
    const int row = m_model->indexOf(surface);
    if (const auto *entry = m_model->entryAt(row)) {
        return entry->minimized;
    }
    return false;
}

bool WaylibSurfaceAdapter::isMaximized(WXdgToplevelSurface *surface) const {
    const int row = m_model->indexOf(surface);
    if (const auto *entry = m_model->entryAt(row)) {
        return entry->maximized;
    }
    return false;
}

bool WaylibSurfaceAdapter::isSurfaceOpaque(WSurface *surface) const {
    if (!surface || !surface->handle()) {
        return false;
    }
    return pixman_region32_not_empty(&surface->handle()->opaque_region) != 0;
}

void WaylibSurfaceAdapter::onToplevelAdded(WXdgToplevelSurface *surface) {
    if (!surface || m_model->indexOf(surface) >= 0) {
        return;
    }

    WaylibSurfaceListModel::Entry entry;
    entry.surfaceId = ++m_nextSurfaceId;
    entry.shellSurface = surface;
    entry.zOrder = ++m_nextZOrder;
    if (auto *ws = WorkspaceManagerService::instance()) {
        entry.workspaceIndex = qBound(0, ws->currentWorkspace(), qMax(0, ws->totalWorkspaces() - 1));
    } else {
        entry.workspaceIndex = 0;
    }
    entry.minimized = surface->isMinimized();
    entry.maximized = surface->isMaximized();
    entry.activated = false;
    m_model->appendEntry(entry);

    QPointer<WXdgToplevelSurface> safeSurface(surface);

    connect(surface, &WToplevelSurface::titleChanged, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (row < 0) {
            return;
        }
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::TitleRole});
        Q_EMIT titleChanged(safeSurface.data(), safeSurface->title());
        if (m_activeSurface.data() == safeSurface.data()) {
            Q_EMIT activeSurfaceChanged();
        }
    });

    connect(surface, &WToplevelSurface::appIdChanged, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (row < 0) {
            return;
        }
        m_model->notifyRowChanged(row, {WaylibSurfaceListModel::AppIdRole});
        Q_EMIT appIdChanged(safeSurface.data(), safeSurface->appId());
        if (m_activeSurface.data() == safeSurface.data()) {
            Q_EMIT activeSurfaceChanged();
            Q_EMIT surfacesChanged();
        }
    });

    connect(surface, &WToplevelSurface::activateChanged, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            entryPtr->activated = safeSurface->isActivated() || (m_activeSurface.data() == safeSurface.data());
            m_model->notifyRowChanged(row, {WaylibSurfaceListModel::ActivatedRole});
        }
    });

    connect(surface, &WToplevelSurface::maximizeChanged, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            entryPtr->maximized = safeSurface->isMaximized();
            m_model->notifyRowChanged(row, {WaylibSurfaceListModel::MaximizedRole});
        }
    });

    connect(surface, &WToplevelSurface::minimizeChanged, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            entryPtr->minimized = safeSurface->isMinimized();
            m_model->notifyRowChanged(row, {WaylibSurfaceListModel::MinimizedRole});
        }
    });

    connect(surface, &WToplevelSurface::requestMaximize, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            if (!entryPtr->maximized) {
                toggleMaximize(safeSurface.data());
            }
        }
    });

    connect(surface, &WToplevelSurface::requestCancelMaximize, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            if (entryPtr->maximized) {
                toggleMaximize(safeSurface.data());
            }
        }
    });

    connect(surface, &WToplevelSurface::requestMinimize, this, [this, safeSurface]() {
        if (!safeSurface) {
            return;
        }
        const int row = m_model->indexOf(safeSurface.data());
        if (auto *entryPtr = m_model->entryAt(row)) {
            if (!entryPtr->minimized) {
                toggleMinimize(safeSurface.data());
            }
        }
    });

    connect(surface, &WToplevelSurface::requestMove, this, [this, safeSurface](WSeat *, quint32) {
        if (!safeSurface) {
            return;
        }
        activateSurface(safeSurface.data());
        Q_EMIT moveRequested(safeSurface.data());
    });

    connect(surface, &WToplevelSurface::requestResize, this, [this, safeSurface](WSeat *, Qt::Edges edges, quint32) {
        if (!safeSurface) {
            return;
        }
        activateSurface(safeSurface.data());
        Q_EMIT resizeRequested(safeSurface.data(), static_cast<int>(edges));
    });

    connect(surface, &WToplevelSurface::beforeDestroy, this, [this, surface]() {
        removeSurfaceInternal(surface, true);
    });

    connect(surface, &QObject::destroyed, this, [this, surface]() {
        removeSurfaceInternal(surface, false);
    });

    qInfo() << "[SGE-WAYLIB] XDG toplevel added:" << surface->title() << "appId=" << surface->appId();
    if (surface->surface() && m_outputAdapter && m_outputAdapter->primaryOutput()) {
        surface->surface()->enterOutput(m_outputAdapter->primaryOutput());
    }
    Q_EMIT surfaceAdded(surface);
    activateSurface(surface);
}

void WaylibSurfaceAdapter::onToplevelRemoved(WXdgToplevelSurface *surface) {
    removeSurfaceInternal(surface, true);
}

void WaylibSurfaceAdapter::onPopupAdded(WXdgPopupSurface *popup) {
    if (!popup || m_popupModel->indexOf(popup) >= 0) {
        return;
    }

    WaylibPopupListModel::Entry entry;
    entry.popupSurface = popup;
    m_popupModel->appendEntry(entry);
    if (popup->surface() && m_outputAdapter && m_outputAdapter->primaryOutput()) {
        popup->surface()->enterOutput(m_outputAdapter->primaryOutput());
    }

    QPointer<WXdgPopupSurface> safePopup(popup);
    connect(popup, &WXdgPopupSurface::reposition, this, [this, safePopup]() {
        if (!safePopup) {
            return;
        }
        const int row = m_popupModel->indexOf(safePopup.data());
        if (row >= 0) {
            m_popupModel->notifyRowChanged(row, {
                WaylibPopupListModel::PopupXRole,
                WaylibPopupListModel::PopupYRole
            });
        }
    });
    connect(popup, &WToplevelSurface::beforeDestroy, this, [this, popup]() {
        removePopupInternal(popup, true);
    });
    connect(popup, &QObject::destroyed, this, [this, popup]() {
        removePopupInternal(popup, false);
    });

    Q_EMIT popupAdded(popup);
    Q_EMIT popupsChanged();
}

void WaylibSurfaceAdapter::onPopupRemoved(WXdgPopupSurface *popup) {
    removePopupInternal(popup, true);
}

void WaylibSurfaceAdapter::removePopupInternal(WXdgPopupSurface *popup, bool emitRemoved) {
    const int row = m_popupModel->indexOf(popup);
    if (row < 0) {
        return;
    }
    if (emitRemoved && popup) {
        popup->disconnect(this);
    }
    m_popupModel->removeRowAt(row);
    if (emitRemoved) {
        Q_EMIT popupRemoved(popup);
    }
    Q_EMIT popupsChanged();
}

void WaylibSurfaceAdapter::removeSurfaceInternal(WXdgToplevelSurface *surface, bool emitRemoved) {
    const int row = m_model->indexOf(surface);
    if (row < 0) {
        return;
    }

    const QString title = emitRemoved && surface ? surface->title() : QString();
    if (emitRemoved && surface) {
        surface->disconnect(this);
    }

    const bool wasActive = (m_activeSurface.data() == surface);
    m_model->removeRowAt(row);

    if (wasActive) {
        m_activeSurface.clear();
        WXdgToplevelSurface *next = topVisibleSurface();
        if (next) {
            activateSurface(next);
        } else {
            syncSeatFocus(nullptr);
            Q_EMIT activeSurfaceChanged();
        }
    }

    if (emitRemoved) {
        qInfo() << "[SGE-WAYLIB] XDG toplevel removed:" << title;
        Q_EMIT surfaceRemoved(surface);
    }
    Q_EMIT surfacesChanged();
}

WXdgToplevelSurface *WaylibSurfaceAdapter::topVisibleSurfaceForWorkspace(
    int workspaceIndex,
    WXdgToplevelSurface *exclude) const {
    WXdgToplevelSurface *best = nullptr;
    int bestZ = -1;
    const auto &entries = m_model->entries();
    for (const auto &entry : entries) {
        WXdgToplevelSurface *candidate = entry.shellSurface.data();
        if (!candidate || candidate == exclude || entry.minimized || entry.workspaceIndex != workspaceIndex) {
            continue;
        }
        if (entry.zOrder > bestZ) {
            bestZ = entry.zOrder;
            best = candidate;
        }
    }
    return best;
}

WXdgToplevelSurface *WaylibSurfaceAdapter::topVisibleSurface(WXdgToplevelSurface *exclude) const {
    const int activeWs = WorkspaceManagerService::instance()
                             ? WorkspaceManagerService::instance()->currentWorkspace()
                             : 0;
    if (WXdgToplevelSurface *inWs = topVisibleSurfaceForWorkspace(activeWs, exclude)) {
        return inWs;
    }
    WXdgToplevelSurface *best = nullptr;
    int bestZ = -1;
    const auto &entries = m_model->entries();
    for (const auto &entry : entries) {
        WXdgToplevelSurface *candidate = entry.shellSurface.data();
        if (!candidate || candidate == exclude || entry.minimized) {
            continue;
        }
        if (entry.zOrder > bestZ) {
            bestZ = entry.zOrder;
            best = candidate;
        }
    }
    return best;
}

void WaylibSurfaceAdapter::syncSeatFocus(WXdgToplevelSurface *surface) {
    if (!m_seat || !m_seat->handle()) {
        return;
    }
    if (surface && surface->surface()) {
        m_seat->setKeyboardFocusSurface(surface->surface());
    } else {
        m_seat->clearKeyboardFocusSurface();
    }
}

} // namespace Systalia::Compositor::WaylibBackend
