#include "SystaliaCompositor.hpp"
#include <QDebug>
#include <QFile>

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
    Q_UNUSED(xdgSurface);
    if (toplevel) {
        qInfo().noquote() << "[SGE-WAYLAND] Novo cliente XDG Toplevel:"
                          << toplevel->appId()
                          << "| Título:" << toplevel->title()
                          << "| Decoração atual:" << (toplevel->decorationMode() == QWaylandXdgToplevel::ServerSideDecoration ? "ServerSide" : "ClientSide");

        connect(toplevel, &QWaylandXdgToplevel::decorationModeChanged, this, [toplevel]() {
            qInfo().noquote() << "[SGE-WAYLAND] Modo de decoração do cliente atualizado:"
                              << (toplevel->decorationMode() == QWaylandXdgToplevel::ServerSideDecoration ? "ServerSide" : "ClientSide");
        });
    }

    emit toplevelCreated(toplevel);
}

void SystaliaCompositor::onPopupCreated(QWaylandXdgPopup *popup, QWaylandXdgSurface *xdgSurface) {
    Q_UNUSED(xdgSurface);
    qDebug().noquote() << "[SGE-WAYLAND] Novo XDG Popup criado";
    emit popupCreated(popup);
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
