#include "OutputManagerService.hpp"

#include <QGuiApplication>
#include <QQuickItem>
#include <QVariantMap>
#include <QtMath>
#include <QtWaylandCompositor/QWaylandOutputMode>
#include <QtWaylandCompositor/QWaylandXdgOutputV1>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
OutputManagerService *s_instance = nullptr;
constexpr qreal kMinScaleFactor = 0.5;
constexpr qreal kMaxScaleFactor = 4.0;
} // namespace

OutputManagerService::OutputManagerService(QObject *parent)
    : QObject(parent) {
    s_instance = this;
    syncWithHostScreens();

    if (auto *guiApp = qobject_cast<QGuiApplication *>(QCoreApplication::instance())) {
        connect(guiApp, &QGuiApplication::screenAdded, this, [this](QScreen *) {
            syncWithHostScreens();
        });
        connect(guiApp, &QGuiApplication::screenRemoved, this, [this](QScreen *) {
            syncWithHostScreens();
        });
    }
}

OutputManagerService::~OutputManagerService() {
    if (m_compositor && m_xdgOutputManager) {
        for (auto &entry : m_outputs) {
            delete entry.xdgOutput.data();
            delete entry.waylandOutput.data();
        }
    }
    m_outputs.clear();
    m_outputXdgMap.clear();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

OutputManagerService *OutputManagerService::instance() noexcept {
    return s_instance;
}

void OutputManagerService::attachCompositor(QWaylandCompositor *compositor) {
    if (m_compositor == compositor) {
        return;
    }

    m_compositor = compositor;
    if (!m_compositor) {
        return;
    }

    if (!m_xdgOutputManager) {
        m_xdgOutputManager = new QWaylandXdgOutputManagerV1();
        m_xdgOutputManager->setParent(m_compositor);
        m_xdgOutputManager->setExtensionContainer(m_compositor);
    }

    connect(m_compositor, &QWaylandCompositor::outputAdded, this, [this](QWaylandOutput *out) {
        ensureXdgOutputFor(out);
    });

    if (m_compositor->isCreated()) {
        initializeWaylandExtensions();
    } else {
        connect(m_compositor, &QWaylandCompositor::createdChanged, this, [this]() {
            if (m_compositor && m_compositor->isCreated()) {
                initializeWaylandExtensions();
            }
        });
    }
}

QVariantList OutputManagerService::screenGeometries() const {
    QVariantList result;
    result.reserve(m_outputs.size());
    for (const auto &entry : m_outputs) {
        result.append(entry.geometry);
    }
    return result;
}

QVariantList OutputManagerService::screenOutputs() const {
    QVariantList result;
    result.reserve(m_outputs.size());
    for (int i = 0; i < m_outputs.size(); ++i) {
        const auto &entry = m_outputs.at(i);
        const QVector2D phys = computePhysicalResolution(entry.geometry.size(), entry.scale);
        QVariantMap map;
        map.insert(QStringLiteral("index"), i);
        map.insert(QStringLiteral("name"), entry.name);
        map.insert(QStringLiteral("geometry"), entry.geometry);
        map.insert(QStringLiteral("x"), entry.geometry.x());
        map.insert(QStringLiteral("y"), entry.geometry.y());
        map.insert(QStringLiteral("width"), entry.geometry.width());
        map.insert(QStringLiteral("height"), entry.geometry.height());
        map.insert(QStringLiteral("scale"), entry.scale);
        map.insert(QStringLiteral("scale120ths"), toFractionalScale120ths(entry.scale));
        map.insert(QStringLiteral("physicalWidth"), static_cast<int>( std::lround(phys.x())));
        map.insert(QStringLiteral("physicalHeight"), static_cast<int>(std::lround(phys.y())));
        result.append(map);
    }
    return result;
}

qreal OutputManagerService::primaryScale() const noexcept {
    if (m_outputs.isEmpty()) {
        return 1.0;
    }
    return m_outputs.first().scale;
}

QRect OutputManagerService::virtualCanvasBounds() const noexcept {
    if (m_outputs.isEmpty()) {
        return QRect(0, 0, 1920, 1080);
    }

    QRect bounds = m_outputs.first().geometry;
    for (int i = 1; i < m_outputs.size(); ++i) {
        bounds = bounds.united(m_outputs.at(i).geometry);
    }
    return bounds;
}

void OutputManagerService::setPrimaryScale(qreal scale) {
    if (m_outputs.isEmpty()) {
        addVirtualScreen(QStringLiteral("SGE-Primary-0"), QRect(0, 0, 1920, 1080), scale);
        return;
    }
    setScreenScale(0, scale);
}

void OutputManagerService::setActiveCursorScreen(int screenIndex) {
    if (m_outputs.isEmpty()) {
        return;
    }
    const int clamped = std::clamp(screenIndex, 0, static_cast<int>(m_outputs.size()) - 1);
    if (m_activeCursorScreen == clamped) {
        return;
    }
    m_activeCursorScreen = clamped;
    emit activeCursorScreenChanged(m_activeCursorScreen);
}

void OutputManagerService::setReplicateTopBarOnAllScreens(bool replicate) {
    if (m_replicateTopBarOnAllScreens == replicate) {
        return;
    }
    m_replicateTopBarOnAllScreens = replicate;
    emit replicateTopBarOnAllScreensChanged(m_replicateTopBarOnAllScreens);
}

void OutputManagerService::clearScreens() {
    for (auto &entry : m_outputs) {
        if (entry.waylandOutput) {
            m_outputXdgMap.remove(entry.waylandOutput);
        }
        delete entry.xdgOutput.data();
        delete entry.waylandOutput.data();
    }
    m_outputs.clear();
    m_activeCursorScreen = 0;
    emit screensChanged();
    emit primaryScaleChanged(1.0);
}

int OutputManagerService::addVirtualScreen(const QString &name, const QRect &geometry,
                                           qreal scale, int refreshRateMilliHz) {
    if (!geometry.isValid() || geometry.width() <= 0 || geometry.height() <= 0) {
        return -1;
    }

    const qreal clampedScale = std::clamp(scale, kMinScaleFactor, kMaxScaleFactor);
    ManagedOutputInfo info;
    info.name = name.isEmpty()
                ? QStringLiteral("SGE-Output-%1").arg(m_outputs.size())
                : name;
    info.geometry = geometry;
    info.scale = clampedScale;
    info.refreshRateMilliHz = std::max(1000, refreshRateMilliHz);

    if (m_compositor && m_compositor->isCreated()) {
        createWaylandOutputForEntry(info);
    }

    const int index = static_cast<int>(m_outputs.size());
    m_outputs.append(info);

    emit screensChanged();
    if (index == 0) {
        emit primaryScaleChanged(info.scale);
    }
    return index;
}

bool OutputManagerService::removeScreen(int index) {
    if (index < 0 || index >= m_outputs.size() || m_outputs.size() <= 1) {
        return false;
    }

    ManagedOutputInfo removedEntry = m_outputs.takeAt(index);
    const QRect primaryGeo = m_outputs.first().geometry;
    const qreal primaryScaleVal = m_outputs.first().scale;
    QWaylandOutput *primaryWlOut = m_outputs.first().waylandOutput;

    for (auto it = m_windowScreenMap.begin(); it != m_windowScreenMap.end(); ++it) {
        QObject *winObj = it.key();
        const int prevIdx = it.value();

        if (prevIdx == index) {
            it.value() = 0;

            if (QWaylandSurface *surf = m_windowSurfaceMap.value(winObj).data()) {
                if (removedEntry.waylandOutput) {
                    removedEntry.waylandOutput->surfaceLeave(surf);
                }
                if (primaryWlOut) {
                    primaryWlOut->surfaceEnter(surf);
                }
            }

            QRectF migratedRect;
            if (auto *item = qobject_cast<QQuickItem *>(winObj)) {
                const QRectF currentRect(item->x(), item->y(), item->width(), item->height());
                migratedRect = clampWindowToPrimaryScreen(currentRect);
                item->setX(migratedRect.x());
                item->setY(migratedRect.y());
                if (item->width() > 0.0 && migratedRect.width() > 0.0) {
                    item->setWidth(migratedRect.width());
                }
                if (item->height() > 0.0 && migratedRect.height() > 0.0) {
                    item->setHeight(migratedRect.height());
                }
            } else {
                migratedRect = QRectF(primaryGeo);
            }

            emit windowMigratedOnHotUnplug(winObj, index, 0, migratedRect);
            emit windowScreenTransitioned(winObj, 0, index, primaryScaleVal);
        } else if (prevIdx > index) {
            it.value() = prevIdx - 1;
        }
    }

    if (removedEntry.waylandOutput) {
        m_outputXdgMap.remove(removedEntry.waylandOutput);
    }
    delete removedEntry.xdgOutput.data();
    delete removedEntry.waylandOutput.data();

    if (m_activeCursorScreen >= m_outputs.size()) {
        m_activeCursorScreen = 0;
        emit activeCursorScreenChanged(m_activeCursorScreen);
    } else if (m_activeCursorScreen == index) {
        m_activeCursorScreen = 0;
        emit activeCursorScreenChanged(m_activeCursorScreen);
    } else if (m_activeCursorScreen > index) {
        --m_activeCursorScreen;
        emit activeCursorScreenChanged(m_activeCursorScreen);
    }

    emit screensChanged();
    if (index == 0) {
        emit primaryScaleChanged(primaryScale());
    }
    return true;
}

bool OutputManagerService::removeScreenByName(const QString &name) {
    for (int i = 0; i < m_outputs.size(); ++i) {
        if (m_outputs.at(i).name == name) {
            return removeScreen(i);
        }
    }
    return false;
}

QRectF OutputManagerService::clampWindowToPrimaryScreen(const QRectF &windowRect) const {
    const QRect primary = m_outputs.isEmpty() ? QRect(0, 0, 1920, 1080) : m_outputs.first().geometry;
    const qreal w = std::clamp(windowRect.width() > 0.0 ? windowRect.width() : 640.0,
                               120.0, static_cast<qreal>(primary.width()));
    const qreal h = std::clamp(windowRect.height() > 0.0 ? windowRect.height() : 420.0,
                               90.0, static_cast<qreal>(primary.height()));
    const qreal maxX = std::max<qreal>(primary.x(), primary.x() + primary.width() - w);
    const qreal maxY = std::max<qreal>(primary.y(), primary.y() + primary.height() - h);
    const qreal clampedX = std::clamp(windowRect.x(), static_cast<qreal>(primary.x()), maxX);
    const qreal clampedY = std::clamp(windowRect.y(), static_cast<qreal>(primary.y()), maxY);
    return QRectF(clampedX, clampedY, w, h);
}

void OutputManagerService::setScreenScale(int index, qreal scale) {
    if (index < 0 || index >= m_outputs.size()) {
        return;
    }

    const qreal clamped = std::clamp(scale, kMinScaleFactor, kMaxScaleFactor);
    auto &entry = m_outputs[index];
    if (qFuzzyCompare(entry.scale, clamped)) {
        return;
    }

    entry.scale = clamped;
    if (entry.waylandOutput) {
        entry.waylandOutput->setScaleFactor(std::max(1, static_cast<int>(std::ceil(clamped))));
    }
    if (entry.xdgOutput) {
        entry.xdgOutput->setLogicalSize(entry.geometry.size());
    }

    emit screensChanged();
    if (index == 0) {
        emit primaryScaleChanged(entry.scale);
    }
}

void OutputManagerService::syncWithHostScreens() {
    const QList<QScreen *> hostScreens = QGuiApplication::screens();
    if (hostScreens.isEmpty()) {
        if (m_outputs.isEmpty()) {
            addVirtualScreen(QStringLiteral("SGE-Virtual-0"), QRect(0, 0, 1920, 1080), 1.0);
        }
        return;
    }

    for (auto &entry : m_outputs) {
        if (entry.waylandOutput) {
            m_outputXdgMap.remove(entry.waylandOutput);
        }
        delete entry.xdgOutput.data();
        delete entry.waylandOutput.data();
    }
    m_outputs.clear();

    for (int i = 0; i < hostScreens.size(); ++i) {
        QScreen *scr = hostScreens.at(i);
        if (!scr) {
            continue;
        }
        const QRect geo = scr->geometry().isValid()
                          ? scr->geometry()
                          : QRect(i * 1920, 0, 1920, 1080);
        const qreal dpr = std::clamp(scr->devicePixelRatio(), kMinScaleFactor, kMaxScaleFactor);
        const int refresh = std::max(1000, static_cast<int>(std::lround(scr->refreshRate() * 1000.0)));

        ManagedOutputInfo info;
        info.name = scr->name().isEmpty()
                    ? QStringLiteral("SGE-Screen-%1").arg(i)
                    : scr->name();
        info.geometry = geo;
        info.scale = dpr;
        info.refreshRateMilliHz = refresh;
        info.hostScreen = scr;

        if (m_compositor && m_compositor->isCreated()) {
            createWaylandOutputForEntry(info);
        }
        m_outputs.append(info);
    }

    if (m_activeCursorScreen >= m_outputs.size()) {
        m_activeCursorScreen = 0;
    }

    emit screensChanged();
    emit primaryScaleChanged(primaryScale());
}

int OutputManagerService::windowCenterToScreenIndex(const QRectF &windowRect) const {
    if (m_outputs.isEmpty()) {
        return -1;
    }
    if (!windowRect.isValid()) {
        return 0;
    }

    const QPointF center = windowRect.center();
    const int byCenter = pointToScreenIndex(center);
    if (byCenter >= 0 && QRectF(m_outputs.at(byCenter).geometry).contains(center)) {
        return byCenter;
    }

    // Caso o centro esteja fora das bordas, prioriza o ecrã com maior área de sobreposição
    int bestOverlapIndex = -1;
    qreal maxOverlapArea = 0.0;
    for (int i = 0; i < m_outputs.size(); ++i) {
        const QRectF intersection = QRectF(m_outputs.at(i).geometry).intersected(windowRect);
        const qreal area = intersection.width() * intersection.height();
        if (area > maxOverlapArea) {
            maxOverlapArea = area;
            bestOverlapIndex = i;
        }
    }
    if (bestOverlapIndex >= 0) {
        return bestOverlapIndex;
    }

    return std::max(0, byCenter);
}

int OutputManagerService::pointToScreenIndex(const QPointF &globalPoint) const {
    if (m_outputs.isEmpty()) {
        return -1;
    }

    for (int i = 0; i < m_outputs.size(); ++i) {
        const QRect &rect = m_outputs.at(i).geometry;
        if (globalPoint.x() >= rect.x()
                && globalPoint.x() < (rect.x() + rect.width())
                && globalPoint.y() >= rect.y()
                && globalPoint.y() < (rect.y() + rect.height())) {
            return i;
        }
    }

    // Fallback para o monitor mais próximo por distância ao centro
    int closestIndex = 0;
    qreal minDistanceSq = std::numeric_limits<qreal>::max();
    for (int i = 0; i < m_outputs.size(); ++i) {
        const QPointF screenCenter = QRectF(m_outputs.at(i).geometry).center();
        const qreal dx = globalPoint.x() - screenCenter.x();
        const qreal dy = globalPoint.y() - screenCenter.y();
        const qreal distSq = dx * dx + dy * dy;
        if (distSq < minDistanceSq) {
            minDistanceSq = distSq;
            closestIndex = i;
        }
    }
    return closestIndex;
}

QRect OutputManagerService::screenGeometryAt(int index) const {
    if (index < 0 || index >= m_outputs.size()) {
        return QRect();
    }
    return m_outputs.at(index).geometry;
}

qreal OutputManagerService::screenScaleAt(int index) const {
    if (index < 0 || index >= m_outputs.size()) {
        return 1.0;
    }
    return m_outputs.at(index).scale;
}

int OutputManagerService::screenScale120thsAt(int index) const {
    return toFractionalScale120ths(screenScaleAt(index));
}

QVector2D OutputManagerService::physicalResolutionAt(int index) const {
    if (index < 0 || index >= m_outputs.size()) {
        return QVector2D(1920.0f, 1080.0f);
    }
    const auto &entry = m_outputs.at(index);
    return computePhysicalResolution(entry.geometry.size(), entry.scale);
}

QVector2D OutputManagerService::projectBackdropUV(const QPointF &globalPos,
                                                  int screenIndex) const {
    if (screenIndex < 0 || screenIndex >= m_outputs.size()) {
        screenIndex = pointToScreenIndex(globalPos);
    }
    if (screenIndex < 0 || screenIndex >= m_outputs.size()) {
        return QVector2D(0.0f, 0.0f);
    }

    const QRect &geo = m_outputs.at(screenIndex).geometry;
    const float u = std::clamp(
        static_cast<float>((globalPos.x() - geo.x()) / std::max(1, geo.width())),
        0.0f, 1.0f);
    const float v = std::clamp(
        static_cast<float>((globalPos.y() - geo.y()) / std::max(1, geo.height())),
        0.0f, 1.0f);
    return QVector2D(u, v);
}

int OutputManagerService::updateWindowOutputRouting(QObject *windowObj,
                                                    QWaylandSurface *surface,
                                                    const QRectF &windowRect) {
    const int targetIndex = windowCenterToScreenIndex(windowRect);
    if (targetIndex < 0) {
        return 0;
    }

    if (!windowObj) {
        return targetIndex;
    }

    const auto existing = m_windowScreenMap.constFind(windowObj);
    const bool alreadyTracked = (existing != m_windowScreenMap.cend());
    const int previousIndex = alreadyTracked ? existing.value() : -1;

    if (!alreadyTracked) {
        connect(windowObj, &QObject::destroyed, this,
                &OutputManagerService::unregisterWindowObject, Qt::UniqueConnection);
    }

    m_windowScreenMap.insert(windowObj, targetIndex);
    if (surface) {
        m_windowSurfaceMap.insert(windowObj, surface);
    }

    if (previousIndex != targetIndex) {
        if (surface) {
            if (previousIndex >= 0 && previousIndex < m_outputs.size()
                    && m_outputs.at(previousIndex).waylandOutput) {
                m_outputs.at(previousIndex).waylandOutput->surfaceLeave(surface);
            }
            if (m_outputs.at(targetIndex).waylandOutput) {
                m_outputs.at(targetIndex).waylandOutput->surfaceEnter(surface);
            }
        }
        emit windowScreenTransitioned(windowObj, targetIndex, previousIndex,
                                      screenScaleAt(targetIndex));
    }

    return targetIndex;
}

int OutputManagerService::windowScreenIndex(QObject *windowObj) const {
    if (!windowObj) {
        return 0;
    }
    const auto it = m_windowScreenMap.constFind(windowObj);
    return (it != m_windowScreenMap.cend()) ? it.value() : 0;
}

void OutputManagerService::updateCursorPosition(const QPointF &globalPos) {
    const int idx = pointToScreenIndex(globalPos);
    if (idx >= 0) {
        setActiveCursorScreen(idx);
    }
}

int OutputManagerService::toFractionalScale120ths(qreal scale) noexcept {
    const qreal clamped = std::clamp(scale, kMinScaleFactor, kMaxScaleFactor);
    return std::max(1, static_cast<int>(std::lround(clamped * 120.0)));
}

QVector2D OutputManagerService::computePhysicalResolution(const QSize &logicalSize,
                                                          qreal scale) noexcept {
    const qreal clamped = std::clamp(scale, kMinScaleFactor, kMaxScaleFactor);
    const float physW = static_cast<float>(std::max(1, logicalSize.width()) * clamped);
    const float physH = static_cast<float>(std::max(1, logicalSize.height()) * clamped);
    return QVector2D(physW, physH);
}

void OutputManagerService::ensureXdgOutputFor(QWaylandOutput *output) {
    if (!output || !m_xdgOutputManager) {
        return;
    }
    if (m_outputXdgMap.contains(output) && m_outputXdgMap.value(output)) {
        return;
    }
    if (!m_xdgOutputManager->isInitialized()) {
        m_xdgOutputManager->initialize();
    }

    QString name = output->model();
    if (name.isEmpty()) {
        name = QStringLiteral("SGE-Output-%1").arg(m_outputXdgMap.size());
    }
    QString desc = output->manufacturer();
    if (desc.isEmpty()) {
        desc = QStringLiteral("Systalia Glass Engine Display");
    }

    QPoint pos = output->position();
    QSize size = output->geometry().size();
    if (!size.isValid() || size.width() <= 0 || size.height() <= 0) {
        if (!m_outputs.isEmpty()) {
            size = m_outputs.first().geometry.size();
        } else {
            size = QSize(1920, 1080);
        }
    }

    auto *xdgOut = new QWaylandXdgOutputV1(output, m_xdgOutputManager);
    xdgOut->setName(name);
    xdgOut->setDescription(desc);
    xdgOut->setLogicalPosition(pos);
    xdgOut->setLogicalSize(size);

    m_outputXdgMap.insert(output, xdgOut);

    connect(output, &QWaylandOutput::positionChanged, xdgOut, [output, xdgOut]() {
        xdgOut->setLogicalPosition(output->position());
    });
    connect(output, &QWaylandOutput::geometryChanged, xdgOut, [output, xdgOut]() {
        if (output->geometry().size().isValid()) {
            xdgOut->setLogicalSize(output->geometry().size());
        }
    });
    connect(output, &QObject::destroyed, this, [this, output]() {
        m_outputXdgMap.remove(output);
    });
}

void OutputManagerService::createWaylandOutputForEntry(ManagedOutputInfo &entry) {
    if (!m_compositor || !m_compositor->isCreated()) {
        return;
    }

    if (!entry.waylandOutput) {
        auto *wlOut = new QWaylandOutput(m_compositor, nullptr);
        wlOut->setManufacturer(QStringLiteral("Systalia"));
        wlOut->setModel(entry.name);
        wlOut->setPosition(entry.geometry.topLeft());
        const QVector2D phys = computePhysicalResolution(entry.geometry.size(), entry.scale);
        const QSize modeSize(static_cast<int>(std::lround(phys.x())),
                             static_cast<int>(std::lround(phys.y())));
        const QWaylandOutputMode mode(modeSize, entry.refreshRateMilliHz);
        wlOut->addMode(mode, true);
        wlOut->setCurrentMode(mode);
        wlOut->setScaleFactor(std::max(1, static_cast<int>(std::ceil(entry.scale))));
        entry.waylandOutput = wlOut;
    }

    ensureXdgOutputFor(entry.waylandOutput);
    if (!entry.xdgOutput) {
        entry.xdgOutput = m_outputXdgMap.value(entry.waylandOutput);
    }
}

void OutputManagerService::initializeWaylandExtensions() {
    if (!m_compositor || !m_compositor->isCreated()) {
        return;
    }

    if (m_xdgOutputManager && !m_xdgOutputManager->isInitialized()) {
        m_xdgOutputManager->initialize();
    }

    for (QWaylandOutput *out : m_compositor->outputs()) {
        ensureXdgOutputFor(out);
    }

    for (auto &entry : m_outputs) {
        createWaylandOutputForEntry(entry);
    }
}

void OutputManagerService::unregisterWindowObject(QObject *obj) {
    if (!obj) {
        return;
    }
    m_windowScreenMap.remove(obj);
    m_windowSurfaceMap.remove(obj);
}

