#include "SystaliaCompositor.hpp"
#include <QDebug>
#include <QFile>
#include <QKeyEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandViewporter>
#include <utility>

SystaliaCompositor::SystaliaCompositor(QObject *parent)
    : QWaylandCompositor(parent) {
    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &SystaliaCompositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::surfaceAboutToBeDestroyed,
            this, &SystaliaCompositor::onSurfaceAboutToBeDestroyed);

    m_surfaceExtension = new SystaliaSurfaceExtension(this);
    m_xdgShell = new QWaylandXdgShell();
    m_xdgShell->setParent(this);
    m_xdgShell->setExtensionContainer(this);
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

    m_workspaceManager = WorkspaceManagerService::instance();
    if (!m_workspaceManager) {
        m_workspaceManager = new WorkspaceManagerService(this);
    }
    connect(m_workspaceManager, &WorkspaceManagerService::currentWorkspaceChanged,
            this, [this](int) {
                syncFocusForCurrentWorkspace();
                emit activeToplevelStateChanged();
            });
    connect(m_workspaceManager, &WorkspaceManagerService::objectWorkspaceChanged,
            this, [this](QObject *obj, int ws) {
                emit toplevelWorkspaceChanged(obj, ws);
                emit activeToplevelStateChanged();
            });
    connect(m_workspaceManager, &WorkspaceManagerService::activeWindowMoveRequested,
            this, [this](int targetIndex) {
                QWaylandXdgToplevel *targetToplevel = nullptr;
                if (QWaylandSeat *seat = defaultSeat()) {
                    targetToplevel = toplevelForSurface(seat->keyboardFocus());
                }
                if (!targetToplevel && m_windowStackModel) {
                    const int currentWs = m_workspaceManager->currentWorkspace();
                    for (int i = 0; i < m_windowStackModel->windowCount(); ++i) {
                        auto *candidate = qobject_cast<QWaylandXdgToplevel *>(
                            m_windowStackModel->windowAt(i));
                        if (candidate && !m_minimizedToplevels.contains(candidate)
                            && m_workspaceManager->objectWorkspace(candidate) == currentWs) {
                            targetToplevel = candidate;
                            break;
                        }
                    }
                }
                if (targetToplevel) {
                    m_workspaceManager->setObjectWorkspace(targetToplevel, targetIndex);
                    if (targetIndex != m_workspaceManager->currentWorkspace()) {
                        syncFocusForCurrentWorkspace();
                    }
                }
            });

    // Instanciação e configuração forçada de Server-Side Decoration (zxdg_decoration_manager_v1)
    m_decorationManager = new QWaylandXdgDecorationManagerV1();
    m_decorationManager->setParent(this);
    m_decorationManager->setExtensionContainer(this);
    m_decorationManager->setPreferredMode(QWaylandXdgToplevel::ServerSideDecoration);

    // Protocolo de Viewporter (wp_viewporter) para clientes modernos (Java AWT / JetBrains, Chromium, etc.)
    m_viewporter = new QWaylandViewporter();
    m_viewporter->setParent(this);
    m_viewporter->setExtensionContainer(this);

    m_outputManager = OutputManagerService::instance();
    if (m_outputManager) {
        m_outputManager->attachCompositor(this);
    }
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

    QWaylandCompositor::create();

    // QWaylandCompositor creates/installs its default seat during create().
    // Connect only after that point; defaultSeat() can still be null beforehand.
    if (!m_keyboardFocusConnection) {
        if (QWaylandSeat *seat = defaultSeat()) {
            m_keyboardFocusConnection = connect(
                seat, &QWaylandSeat::keyboardFocusChanged, this,
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
        } else {
            qWarning().noquote() << "[SGE-WAYLAND] Assento padrão indisponível; histórico de foco desativado.";
        }
    }

    // Inicializa explicitamente a extensão XDG Shell se ainda não estiver ativa no registro
    if (m_xdgShell && !m_xdgShell->isInitialized()) {
        m_xdgShell->initialize();
    }

    // Inicializa a extensão de Decoração Server-Side no registro global
    if (m_decorationManager && !m_decorationManager->isInitialized()) {
        m_decorationManager->initialize();
    }

    // Inicializa a extensão Viewporter (wp_viewporter) no registro global
    if (m_viewporter && !m_viewporter->isInitialized()) {
        m_viewporter->initialize();
    }

    if (isCreated()) {
        m_surfaceExtension->initialize();
        qInfo().noquote() << "[SGE-WAYLAND] Servidor ativo em:" << socketName();
        qInfo().noquote() << "[SGE-WAYLAND] XDG Shell registrado:" << (m_xdgShell && m_xdgShell->isInitialized());
        qInfo().noquote() << "[SGE-WAYLAND] XDG Decoration Manager registrado (SSD forçado):"
                          << (m_decorationManager && m_decorationManager->isInitialized());
        qInfo().noquote() << "[SGE-WAYLAND] Viewporter (wp_viewporter) registrado:"
                          << (m_viewporter && m_viewporter->isInitialized());
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

    const int currentWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;
    for (int index = 0; index < m_windowStackModel->windowCount(); ++index) {
        auto *candidate = qobject_cast<QWaylandXdgToplevel *>(
            m_windowStackModel->windowAt(index));
        if (!candidate || candidate == toplevel || m_minimizedToplevels.contains(candidate))
            continue;
        if (m_workspaceManager && m_workspaceManager->objectWorkspace(candidate) != currentWs)
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
        if (!window || !window->xdgSurface())
            return QSize(0, 0);
        QSize s = window->xdgSurface()->windowGeometry().size();
        if (s.width() <= 0 || s.height() <= 0) {
            if (window->xdgSurface()->surface())
                s = window->xdgSurface()->surface()->destinationSize();
        }
        return s;
    };

    for (QWaylandXdgToplevel *candidate : std::as_const(m_activeToplevels)) {
        if (!candidate || candidate == toplevel || !candidate->xdgSurface()) {
            continue;
        }
        const QSize currentSize = configureSize(candidate);
        QList<QWaylandXdgToplevel::State> states = candidate->states();
        states.removeAll(QWaylandXdgToplevel::ActivatedState);
        candidate->sendConfigure(currentSize, states);
    }

    QList<QWaylandXdgToplevel::State> states = toplevel->states();
    if (!states.contains(QWaylandXdgToplevel::ActivatedState)) {
        states.append(QWaylandXdgToplevel::ActivatedState);
    }
    const QSize currentSize = configureSize(toplevel);
    toplevel->sendConfigure(currentSize, states);
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
                && (keyEvent->modifiers().testFlag(Qt::AltModifier)
                    || keyEvent->modifiers().testFlag(Qt::MetaModifier))) {
                keyEvent->accept();
                return true;
            }
            if (keyEvent->modifiers().testFlag(Qt::MetaModifier)) {
                keyEvent->accept();
                return true;
            }
        }
        if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
            && handleWorkspaceKeyEvent(static_cast<QKeyEvent *>(event))) {
            event->accept();
            return true;
        }
        if ((event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease)
            && handleTaskSwitcherKeyEvent(static_cast<QKeyEvent *>(event))) {
            event->accept();
            return true;
        }

        // Encaminhamento direto de teclado para a superfície de cliente Wayland com foco no workspace ativo
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::KeyRelease) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            QWaylandSeat *seat = defaultSeat();
            QWaylandSurface *kbFocus = seat ? seat->keyboardFocus() : nullptr;
            QQuickItem *focusItem = m_taskSwitcherWindow ? m_taskSwitcherWindow->activeFocusItem() : nullptr;
            const bool shellInputHasFocus = focusItem
                && (focusItem->inherits("QQuickTextInput") || focusItem->inherits("QQuickTextField"));

            const int currentWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;
            if (kbFocus && m_workspaceManager) {
                QWaylandXdgToplevel *focusedTl = toplevelForSurface(kbFocus);
                if (focusedTl && m_workspaceManager->objectWorkspace(focusedTl) != currentWs) {
                    kbFocus = nullptr;
                }
            }

            if (seat && !kbFocus && m_windowStackModel) {
                syncFocusForCurrentWorkspace();
                kbFocus = seat->keyboardFocus();
            }

            if (seat && kbFocus) {
                if (!shellInputHasFocus) {
                    seat->sendFullKeyEvent(keyEvent);
                    event->accept();
                    return true;
                }
            }
        }
    }
    return QWaylandCompositor::eventFilter(watched, event);
}

bool SystaliaCompositor::handleWorkspaceKeyEvent(QKeyEvent *event) {
    if (!event || !m_workspaceManager) {
        return false;
    }

    const Qt::KeyboardModifiers mods = event->modifiers();
    if (!mods.testFlag(Qt::MetaModifier)) {
        return false;
    }

    // Super + Ctrl + Seta Esquerda / Direita
    if (mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::AltModifier)) {
        if (event->key() == Qt::Key_Left) {
            if (event->type() == QEvent::KeyPress) {
                m_workspaceManager->previousWorkspace();
            }
            return true;
        }
        if (event->key() == Qt::Key_Right) {
            if (event->type() == QEvent::KeyPress) {
                m_workspaceManager->nextWorkspace();
            }
            return true;
        }
    }

    // Super + 1..4 ou Super + Shift + 1..4
    if (!mods.testFlag(Qt::ControlModifier) && !mods.testFlag(Qt::AltModifier)) {
        int targetWorkspace = -1;
        const quint32 scanCode = event->nativeScanCode();
        switch (event->key()) {
        case Qt::Key_1:
        case Qt::Key_Exclam:
            targetWorkspace = 0;
            break;
        case Qt::Key_2:
        case Qt::Key_At:
        case Qt::Key_QuoteDbl:
            targetWorkspace = 1;
            break;
        case Qt::Key_3:
        case Qt::Key_NumberSign:
            targetWorkspace = 2;
            break;
        case Qt::Key_4:
        case Qt::Key_Dollar:
            targetWorkspace = 3;
            break;
        default:
            if (scanCode >= 10 && scanCode <= 13) {
                targetWorkspace = static_cast<int>(scanCode - 10);
            }
            break;
        }

        if (targetWorkspace >= 0 && targetWorkspace < m_workspaceManager->totalWorkspaces()) {
            if (event->type() == QEvent::KeyPress) {
                if (mods.testFlag(Qt::ShiftModifier)) {
                    m_workspaceManager->moveActiveWindowToWorkspace(targetWorkspace);
                } else {
                    m_workspaceManager->switchToWorkspace(targetWorkspace);
                }
            }
            return true;
        }
    }

    return false;
}

bool SystaliaCompositor::handleTaskSwitcherKeyEvent(QKeyEvent *event) {
    if (!event)
        return false;

    if (event->type() == QEvent::KeyPress) {
        if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
            m_altPressWasForwarded = true;
            return false;
        }
        if (event->key() == Qt::Key_Tab) {
            if (event->modifiers().testFlag(Qt::AltModifier)
                || event->modifiers().testFlag(Qt::MetaModifier)) {
                const int direction = event->modifiers().testFlag(Qt::ShiftModifier) ? -1 : 1;
                return m_windowStackModel->beginTaskSwitch(direction);
            }
            return m_windowStackModel->taskSwitcherVisible();
        }
        return false;
    }

    if (event->type() != QEvent::KeyRelease)
        return false;

    if (event->key() == Qt::Key_Alt || event->key() == Qt::Key_Meta) {
        QWaylandSeat *seat = defaultSeat();
        if (m_altPressWasForwarded && seat && seat->keyboard()) {
            const uint scanCode = seat->keyboard()->keyToScanCode(event->key());
            if (scanCode != 0)
                seat->sendKeyReleaseEvent(scanCode);
        }
        m_altPressWasForwarded = false;

        if (!m_windowStackModel->taskSwitcherVisible()) {
            return false;
        }

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

int SystaliaCompositor::toplevelWorkspace(QObject *toplevelObject) const {
    if (!m_workspaceManager || !toplevelObject) {
        return 0;
    }
    return m_workspaceManager->objectWorkspace(toplevelObject);
}

void SystaliaCompositor::setToplevelWorkspace(QObject *toplevelObject, int workspaceIndex) {
    if (!m_workspaceManager || !toplevelObject) {
        return;
    }
    m_workspaceManager->setObjectWorkspace(toplevelObject, workspaceIndex);
}

QWaylandXdgToplevel *SystaliaCompositor::toplevelForSurface(QWaylandSurface *surface) const {
    if (!surface) {
        return nullptr;
    }
    for (QWaylandXdgToplevel *toplevel : std::as_const(m_activeToplevels)) {
        if (toplevel && toplevel->xdgSurface() && toplevel->xdgSurface()->surface() == surface) {
            return toplevel;
        }
    }
    return nullptr;
}

void SystaliaCompositor::syncFocusForCurrentWorkspace() {
    if (!m_windowStackModel) {
        return;
    }
    const int currentWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;

    for (int index = 0; index < m_windowStackModel->windowCount(); ++index) {
        auto *candidate = qobject_cast<QWaylandXdgToplevel *>(
            m_windowStackModel->windowAt(index));
        if (!candidate || m_minimizedToplevels.contains(candidate)) {
            continue;
        }
        if (m_workspaceManager && m_workspaceManager->objectWorkspace(candidate) != currentWs) {
            continue;
        }
        if (activateToplevel(candidate)) {
            return;
        }
    }

    if (QWaylandSeat *seat = defaultSeat()) {
        seat->setKeyboardFocus(nullptr);
    }
    for (QWaylandXdgToplevel *candidate : std::as_const(m_activeToplevels)) {
        if (!candidate || !candidate->xdgSurface()) {
            continue;
        }
        const QSize currentSize = candidate->xdgSurface()->windowGeometry().size();
        if (currentSize.width() > 0 && currentSize.height() > 0) {
            QList<QWaylandXdgToplevel::State> states = candidate->states();
            states.removeAll(QWaylandXdgToplevel::ActivatedState);
            candidate->sendConfigure(currentSize, states);
        }
    }
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
        if (m_workspaceManager) {
            m_workspaceManager->setObjectWorkspace(toplevel, m_workspaceManager->currentWorkspace());
        }
        connect(toplevel, &QWaylandXdgToplevel::appIdChanged, this, [this]() {
            emit activeAppIdsChanged();
        });
        connect(toplevel, &QWaylandXdgToplevel::maximizedChanged, this, [this]() {
            emit activeToplevelStateChanged();
        });
        connect(toplevel, &QObject::destroyed, this, [this, toplevel]() {
            m_activeToplevels.remove(toplevel);
            if (m_workspaceManager) {
                m_workspaceManager->unregisterObject(toplevel);
            }
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
    const int currentWs = m_workspaceManager ? m_workspaceManager->currentWorkspace() : 0;
    for (QWaylandXdgToplevel *toplevel : m_activeToplevels) {
        if (toplevel && !m_minimizedToplevels.contains(toplevel) && toplevel->maximized()) {
            if (!m_workspaceManager || m_workspaceManager->objectWorkspace(toplevel) == currentWs) {
                return true;
            }
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
