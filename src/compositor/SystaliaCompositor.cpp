#include "SystaliaCompositor.hpp"
#include <QDebug>
#include <QFile>

SystaliaCompositor::SystaliaCompositor(QObject *parent)
    : QWaylandCompositor(parent) {
    connect(this, &QWaylandCompositor::surfaceCreated,
            this, &SystaliaCompositor::onSurfaceCreated);
    connect(this, &QWaylandCompositor::surfaceAboutToBeDestroyed,
            this, &SystaliaCompositor::onSurfaceAboutToBeDestroyed);

    m_xdgShell = new QWaylandXdgShell(this);
    connect(m_xdgShell, &QWaylandXdgShell::toplevelCreated,
            this, &SystaliaCompositor::onToplevelCreated);
    connect(m_xdgShell, &QWaylandXdgShell::popupCreated,
            this, &SystaliaCompositor::onPopupCreated);
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

    if (isCreated()) {
        qInfo().noquote() << "[SGE-WAYLAND] Servidor ativo em:" << socketName();
        qInfo().noquote() << "[SGE-WAYLAND] XDG Shell registrado:" << (m_xdgShell && m_xdgShell->isInitialized());
        qInfo().noquote() << "[SGE-WAYLAND] Assento padrão inicializado com teclado e ponteiro.";
    } else {
        qWarning().noquote() << "[SGE-WAYLAND] Falha ao inicializar servidor no socket:" << socketName();
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
    Q_UNUSED(xdgSurface);
    qInfo().noquote() << "[SGE-WAYLAND] Novo cliente XDG Toplevel:"
                      << (toplevel ? toplevel->appId() : QStringLiteral("desconhecido"))
                      << "| Título:" << (toplevel ? toplevel->title() : QStringLiteral("sem título"));
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
