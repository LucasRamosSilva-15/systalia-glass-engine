#include "SystaliaCompositor.hpp"
#include <QDebug>
#include <QFile>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <utility>

SystaliaCompositor::SystaliaCompositor(QObject *parent)
    : QWaylandCompositor(parent) {
    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &SystaliaCompositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::surfaceAboutToBeDestroyed,
            this, &SystaliaCompositor::onSurfaceAboutToBeDestroyed);

    m_surfaceExtension = new SystaliaSurfaceExtension(this);
    m_xdgShell = new QWaylandXdgShell(this);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &SystaliaCompositor::onToplevelCreated);
    connect(m_xdgShell, &QWaylandXdgShell::popupCreated,
            this, &SystaliaCompositor::onPopupCreated);

    m_windowStackModel = new WindowStackModel(this);
    connect(m_windowStackModel, &WindowStackModel::taskSwitcherVisibleChanged,
            this, &SystaliaCompositor::taskSwitcherVisibleChanged);
    connect(m_windowStackModel, &WindowStackModel::taskSwitcherIndexChanged,
            this, &SystaliaCompositor::taskSwitcherIndexChanged);
    connect(m_windowStackModel, &WindowStackModel::windowCountChanged,
            this, &SystaliaCompositor::windowCountChanged);
    connect(m_windowStackModel, &WindowStackModel::windowSelected,
            this, &SystaliaCompositor::windowSelected);

    // Instanciação e configuração forçada de Server-Side Decoration (zxdg_decoration_manager_v1)
    m_decorationManager = new QWaylandXdgDecorationManagerV1();
    m_decorationManager->setParent(this);
    m_decorationManager->setExtensionContainer(this);
    m_decorationManager->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);
}

SystaliaCompositor::~SystaliaCompositor() {
    qInfo().noquote() << "[SGE-WAYLAND] Servidor finalizado no socket:" << socketName();
}

void SystaliaCompositor::create() {
    if (socketName().isEmpty()) {
        setSocketName("wayland-systalia-0");
    }

    // Inicialização do assento de entrada padrão (teclado + mouse/pointer)
    if (!defaultSeat()) {
        new QWaylandSeat(this, QWaylandSeat::DefaultCapabilities);
    }

    if (!m_keyboardFocusConnection) {
        m_keyboardFocusConnection = connect(
            defaultSeat(), &QWaylandSeat::keyboardFocusChanged, this,
            [this](QWaylandSurface *surface, QWaylandSurface *) {
                if (!surface)
                    return;
                for (QWaylandXdgToplevel *toplevel : std::as_const(m_activeToplevels)) {
                    if (toplevel && toplevel->xdgSurface()
                        && toplevel->xdgSurface()->surface() == surface) {
                        m_windowStackModel->promoteWindow(toplevel);
                        return;
                    }
                }
            });
    }

    QWaylandCompositor::create();

    // Inicializa explicitamente a extensão XDG Shell se ainda não estiver ativa no registro
    if (m_xdgShell && !m_xdgShell->isInitialized()) {
        m_xdgShell->initialize();
    }

    // Inicializa a extensão de Decoração Server-Side no registro global
    if (m_decorationManager && !m_decorationManager->isInitialized()) {
        m_decorationManager->initialize();
    }

    if (isCreated()) {
        m_surfaceExtension->initialize();
        qInfo().noquote() << "[SGE-WAYLAND] Servidor ativo em:" << socketName();
        qInfo().noquote() << "[SGE-WAYLAND] XDG Shell registrado:" << (m_xdgShell && m_xdgShell->isInitialized());
        qInfo().noquote() << "[SGE-WAYLAND] XDG Decoration Manager registrado (SSD forçado):"
                          << (m_decorationManager && m_decorationManager->isInitialized());
        qInfo().noquote() << "[SGE-WAYLAND] Assento padrão inicializado com teclado e ponteiro.";
    } else {
        qWarning().noquote() << "[SGE-WAYLAND] Falha ao inicializar servidor no socket:" << socketName();
    }
}

bool SystaliaCompositor::closeToplevel(QObject *toplevelObject) {
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(toplevelObject);
    if (!toplevel || !m_activeToplevels.contains(toplevel) || !toplevel->xdgSurface()
        || toplevel->xdgSurface()->shell() != m_xdgShell) {
        return false;
    }

    toplevel->sendClose();
    return true;
}

bool SystaliaCompositor::minimizeToplevel(QObject *toplevelObject) {
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(toplevelObject);
    if (!toplevel || !m_activeToplevels.contains(toplevel) || !toplevel->xdgSurface()
        || toplevel->xdgSurface()->shell() != m_xdgShell) {
        return false;
    }
    if (m_minimizedToplevels.contains(toplevel))
        return true;

    m_minimizedToplevels.insert(toplevel);
    ++m_minimizedToplevelsRevision;
    emit minimizedToplevelsChanged();
    emit activeToplevelStateChanged();

    QWaylandSeat *seat = defaultSeat();
    QWaylandSurface *surface = toplevel->xdgSurface()->surface();
    if (!seat || !surface || seat->keyboardFocus() != surface)
        return true;

    for (int index = 0; index < m_windowStackModel->windowCount(); ++index) {
        auto *candidate = qobject_cast<QWaylandXdgToplevel *>(
            m_windowStackModel->windowAt(index));
        if (!candidate || candidate == toplevel || m_minimizedToplevels.contains(candidate))
            continue;
        if (activateToplevel(candidate))
            return true;
    }

    seat->setKeyboardFocus(nullptr);
    QList<QWaylandXdgToplevel::State> states = toplevel->states();
    states.removeAll(QWaylandXdgToplevel::ActivatedState);
    const QSize currentSize = toplevel->xdgSurface()->windowGeometry().size();
    if (currentSize.width() > 0 && currentSize.height() > 0)
        toplevel->sendConfigure(currentSize, states);
    return true;
}

bool SystaliaCompositor::isToplevelMinimized(QObject *toplevelObject) const {
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(toplevelObject);
    return toplevel && m_minimizedToplevels.contains(toplevel);
}

bool SystaliaCompositor::activateToplevel(QObject *toplevelObject) {
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(toplevelObject);
    if (!toplevel || !m_activeToplevels.contains(toplevel) || !toplevel->xdgSurface()
        || toplevel->xdgSurface()->shell() != m_xdgShell) {
        return false;
    }

    QWaylandSurface *surface = toplevel->xdgSurface()->surface();
    QWaylandSeat *seat = defaultSeat();
    if (!surface || !seat) {
        return false;
    }

    const bool wasMinimized = m_minimizedToplevels.remove(toplevel) > 0;
    if (wasMinimized) {
        ++m_minimizedToplevelsRevision;
        emit minimizedToplevelsChanged();
        emit activeToplevelStateChanged();
    }
    if (!seat->setKeyboardFocus(surface)) {
        if (wasMinimized) {
            m_minimizedToplevels.insert(toplevel);
            ++m_minimizedToplevelsRevision;
            emit minimizedToplevelsChanged();
            emit activeToplevelStateChanged();
        }
        return false;
    }
    m_windowStackModel->promoteWindow(toplevel);

    const auto configureSize = [](QWaylandXdgToplevel *window) {
        return window->xdgSurface()->windowGeometry().size();
    };

    for (QWaylandXdgToplevel *candidate : std::as_const(m_activeToplevels)) {
        if (!candidate || candidate == toplevel || !candidate->xdgSurface()) {
            continue;
        }
        const QSize currentSize = configureSize(candidate);
        if (currentSize.width() <= 0 || currentSize.height() <= 0) {
            continue;
        }
        QList<QWaylandXdgToplevel::State> states = candidate->states();
        states.removeAll(QWaylandXdgToplevel::ActivatedState);
        candidate->sendConfigure(currentSize, states);
    }

    QList<QWaylandXdgToplevel::State> states = toplevel->states();
    if (!states.contains(QWaylandXdgToplevel::ActivatedState)) {
        states.append(QWaylandXdgToplevel::ActivatedState);
    }
    const QSize currentSize = configureSize(toplevel);
    if (currentSize.width() > 0 && currentSize.height() > 0) {
        toplevel->sendConfigure(currentSize, states);
    }
    return true;
}

bool SystaliaCompositor::selectWindow(int index) {
    QObject *selected = m_windowStackModel->selectAndCommit(index);
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(selected);
    return toplevel && activateToplevel(toplevel);
}

void SystaliaCompositor::attachTaskSwitcherWindow(QQuickWindow *window) {
    if (m_taskSwitcherWindow == window)
        return;
    if (m_taskSwitcherWindow)
        m_taskSwitcherWindow->removeEventFilter(this);
    m_taskSwitcherWindow = window;
    if (m_taskSwitcherWindow)
        m_taskSwitcherWindow->installEventFilter(this);
}

bool SystaliaCompositor::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_taskSwitcherWindow.data() && event) {
        if (event->type() == QEvent::ShortcutOverride) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Tab
                && keyEvent->modifiers().testFlag(Qt::AltModifier)) {
                keyEvent->accept();
                return true;
            }
        }
        if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
            && handleTaskSwitcherKeyEvent(static_cast<QKeyEvent *>(event))) {
            event->accept();
            return true;
        }
    }
    return QWaylandCompositor::eventFilter(watched, event);
}

bool SystaliaCompositor::handleTaskSwitcherKeyEvent(QKeyEvent *event) {
    if (!event)
        return false;

    if (event->type() == QEvent::KeyPress) {
        if (event->key() == Qt::Key_Alt) {
            m_altPressWasForwarded = true;
            return false;
        }
        if (event->key() == Qt::Key_Tab) {
            if (event->modifiers().testFlag(Qt::AltModifier)) {
                const int direction = event->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1;
                return m_windowStackModel->beginTaskSwitch(direction);
            }
            return m_windowStackModel->taskSwitcherVisible();
        }
        return false;
    }

    if (event->type() != QEvent::KeyRelease)
        return false;

    if (event->key() == Qt::Key_Alt) {
        if (!m_windowStackModel->taskSwitcherVisible()) {
            m_altPressWasForwarded = false;
            return false;
        }

        QWaylandSeat *seat = defaultSeat();
        if (m_altPressWasForwarded && seat && seat->keyboard()) {
            const uint scanCode = seat->keyboard()->keyToScanCode(Qt::Key_Alt);
            if (scanCode != 0)
                seat->sendKeyReleaseEvent(scanCode);
        }
        m_altPressWasForwarded = false;

        QObject *selected = m_windowStackModel->commitTaskSwitch();
        if (selected)
            activateToplevel(selected);
        return true;
    }

    if (event->key() == Qt::Key_Tab && m_windowStackModel->taskSwitcherVisible())
        return true;
    return false;
}

bool SystaliaCompositor::toggleToplevelMaximized(QObject *toplevelObject,
                                                 const QSize &size) {
    auto *toplevel = qobject_cast<QWaylandXdgToplevel *>(toplevelObject);
    if (!toplevel || !toplevel->xdgSurface()
        || toplevel->xdgSurface()->shell() != m_xdgShell) {
        return false;
    }

    const QSize minimum = toplevel->minSize().expandedTo(QSize(240, 160));
    const QSize maximum = toplevel->maxSize();
    QSize configureSize = size.expandedTo(minimum);
    if (maximum.width() > 0)
        configureSize.setWidth(qMin(configureSize.width(), qMax(minimum.width(), maximum.width())));
    if (maximum.height() > 0)
        configureSize.setHeight(qMin(configureSize.height(), qMax(minimum.height(), maximum.height())));

    if (toplevel->maximized())
        toplevel->sendUnmaximized(configureSize);
    else
        toplevel->sendMaximized(configureSize);
    return true;
}

void SystaliaCompositor::requestToplevelResize(QWaylandXdgToplevel *toplevel,
                                               const QSize &newSize, bool isResizing) {
    if (!toplevel || !toplevel->xdgSurface()
        || toplevel->xdgSurface()->shell() != m_xdgShell) {
        return;
    }

    const QSize minimum = toplevel->minSize().expandedTo(QSize(240, 160));
    const QSize maximum = toplevel->maxSize();
    QSize size = newSize.expandedTo(minimum);
    if (maximum.width() > 0) {
        size.setWidth(qMin(size.width(), qMax(minimum.width(), maximum.width())));
    }
    if (maximum.height() > 0) {
        size.setHeight(qMin(size.height(), qMax(minimum.height(), maximum.height())));
    }

    QList<QWaylandXdgToplevel::State> states{QWaylandXdgToplevel::ActivatedState};
    if (isResizing) {
        states.prepend(QWaylandXdgToplevel::ResizingState);
    }
    toplevel->sendConfigure(size, states);
}

void SystaliaCompositor::onSurfaceCreated(QWaylandSurface *surface) {
    if (surface) {
        m_activeSurfaces.insert(surface);
        recalculateClientCount();
        qDebug().noquote() << "[SGE-WAYLAND] Nova superfície criada. Total de superfícies ativas:" << m_activeSurfaces.size();
    }
}

void SystaliaCompositor::onSurfaceAboutToBeDestroyed(QWaylandSurface *surface) {
    if (surface) {
        m_activeSurfaces.remove(surface);
        recalculateClientCount();
        qDebug().noquote() << "[SGE-WAYLAND] Superfície prestes a ser destruída. Restantes:" << m_activeSurfaces.size();
    }
}

void SystaliaCompositor::onToplevelCreated(QWaylandXdgToplevel *toplevel, QWaylandXdgSurface *xdgSurface) {
    if (toplevel) {
        m_activeToplevels.insert(toplevel);
        m_windowStackModel->registerWindow(toplevel);
        connect(toplevel, &QWaylandXdgToplevel::appIdChanged, this, [this]() {
            emit activeAppIdsChanged();
        });
        connect(toplevel, &QWaylandXdgToplevel::maximizedChanged, this, [this]() {
            emit activeToplevelStateChanged();
        });
        connect(toplevel, &QObject::destroyed, this, [this, toplevel]() {
            m_activeToplevels.remove(toplevel);
            if (m_minimizedToplevels.remove(toplevel) > 0) {
                ++m_minimizedToplevelsRevision;
                emit minimizedToplevelsChanged();
                emit activeToplevelStateChanged();
            }
            emit activeAppIdsChanged();
            emit activeToplevelStateChanged();
        });
        emit activeAppIdsChanged();
        emit activeToplevelStateChanged();

        qInfo().noquote() << "[SGE-WAYLAND] Novo cliente XDG Toplevel:"
                          << toplevel->appId()
                          << "| Título:" << toplevel->title()
                          << "| Decoração atual:" << (toplevel->decorationMode() == QWaylandXdgToplevel::ServerSideDecoration ? "ServerSide" : "ClientSide");

        connect(toplevel, &QWaylandXdgToplevel::decorationModeChanged, this, [toplevel]() {
            qInfo().noquote() << "[SGE-WAYLAND] Modo de decoração do cliente atualizado:"
                              << (toplevel->decorationMode() == QWaylandXdgToplevel::ServerSideDecoration ? "ServerSide" : "ClientSide");
        });

        if (xdgSurface && xdgSurface->shell() == m_xdgShell) {
            const uint serial = toplevel->sendConfigure(
                QSize(800, 500), QList<QWaylandXdgToplevel::State>{});
            qInfo().noquote() << "[SGE-WAYLAND] Configure inicial enviado; serial:"
                              << serial << "| tamanho: 800x500";
        }
    }

    emit toplevelCreated(toplevel);
    emit toplevelCreatedForQml(toplevel);
}

void SystaliaCompositor::onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface) {
    Q_UNUSED(xdgSurface);
    qDebug().noquote() << "[SGE-WAYLAND] Novo XDG Popup criado";
    emit popupCreated(popup);
}

QStringList SystaliaCompositor::activeAppIds() const {
    QStringList appIds;
    for (const QWaylandXdgToplevel *toplevel : m_activeToplevels) {
        if (!toplevel) {
            continue;
        }
        const QString appId = toplevel->appId().trimmed();
        if (!appId.isEmpty() && !appIds.contains(appId, Qt::CaseInsensitive)) {
            appIds.append(appId);
        }
    }
    appIds.sort(Qt::CaseInsensitive);
    return appIds;
}

bool SystaliaCompositor::hasMaximizedToplevel() const {
    for (QWaylandXdgToplevel *toplevel : m_activeToplevels) {
        if (toplevel && !m_minimizedToplevels.contains(toplevel) && toplevel->maximized()) {
            return true;
        }
    }
    return false;
}

void SystaliaCompositor::recalculateClientCount() {
    QSet<QWaylandClient *> uniqueClients;
    for (auto *c : clients()) {
        if (c) {
            uniqueClients.insert(c);
        }
    }
    for (auto *s : m_activeSurfaces) {
        if (s && s->client()) {
            uniqueClients.insert(s->client());
        }
    }

    const int count = uniqueClients.size();
    if (m_clientCount != count) {
        m_clientCount = count;
        emit clientCountChanged();
    }
}
