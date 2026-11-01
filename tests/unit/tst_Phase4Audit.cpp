#include <QQuickItem>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <cmath>
#include <memory>

#include "compositor/OutputManagerService.hpp"
#include "gfx/SystaliaGlassItem.hpp"
#include "system/LiquidTilingService.hpp"
#include "system/WorkspaceManagerService.hpp"

class tst_Phase4Audit : public QObject {
    Q_OBJECT

private slots:
    void workspaceOcclusionCullingAndVramPreservation();
    void tileSplitterStressAndG2FusedRadiiSafety();
    void multiMonitorHotUnplugMigration();
};

void tst_Phase4Audit::workspaceOcclusionCullingAndVramPreservation() {
    WorkspaceManagerService wsManager;
    wsManager.setTotalWorkspaces(4);
    wsManager.switchToWorkspace(0);

    // Simula 4 workspaces com contentores de janela e chassis ópticos SystaliaGlassItem
    QQuickItem rootCanvas;
    std::vector<std::unique_ptr<QQuickItem>> containers;
    std::vector<SystaliaGlassItem *> glassItems;

    auto dummyBackdrop = std::make_unique<QQuickItem>(&rootCanvas);
    auto dummyClient = std::make_unique<QQuickItem>(&rootCanvas);

    for (int ws = 0; ws < 4; ++ws) {
        auto container = std::make_unique<QQuickItem>(&rootCanvas);
        container->setWidth(800.0);
        container->setHeight(520.0);

        auto *glass = new SystaliaGlassItem(container.get());
        glass->setWidth(800.0);
        glass->setHeight(520.0);
        glass->setBackdropSource(dummyBackdrop.get());
        glass->setClientTextureSource(dummyClient.get());

        wsManager.setWindowWorkspace(container.get(), ws);
        glassItems.push_back(glass);
        containers.push_back(std::move(container));
    }

    // 1. Em repouso no Workspace 0 (scrollOffset = 0, screenWidth = 1920):
    // Apenas o Workspace 0 está dentro do frustum visível (|xViewport| < 1920).
    wsManager.setTransitioning(false);
    const int visibleAtZero = wsManager.applyViewportCulling(0.0, 1920.0);
    QCOMPARE(visibleAtZero, 1);
    QCOMPARE(wsManager.activeVisibleWindowCount(), 1);

    for (int ws = 0; ws < 4; ++ws) {
        glassItems[static_cast<size_t>(ws)]->setOccluded(!containers[static_cast<size_t>(ws)]->isVisible());
    }

    QVERIFY(containers[0]->isVisible());
    QVERIFY(!glassItems[0]->isOccluded());
    QVERIFY(!glassItems[0]->shouldSuppressSceneGraphUpdate());
    QVERIFY(glassItems[0]->hasPreservedVramTextures());

    for (int ws = 1; ws < 4; ++ws) {
        const auto idx = static_cast<size_t>(ws);
        QVERIFY(!containers[idx]->isVisible());
        QVERIFY(glassItems[idx]->isOccluded());
        QVERIFY(glassItems[idx]->shouldSuppressSceneGraphUpdate());
        // As fontes de textura continuam preservadas em VRAM mesmo com oclusão ativa
        QVERIFY(glassItems[idx]->hasPreservedVramTextures());

        const quint64 beforeSuppressed = glassItems[idx]->suppressedUpdateCount();
        glassItems[idx]->setIor(1.52f);
        glassItems[idx]->setCausticIntensity(0.85f);
        QVERIFY(glassItems[idx]->suppressedUpdateCount() > beforeSuppressed);
    }

    // 2. Durante transição hidrodinâmica entre Workspace 0 e Workspace 1 (viewportOffset = -960):
    // Ambos os Workspaces 0 (xViewport = -960) e 1 (xViewport = +960) cruzam o frustum visível;
    // Workspaces 2 e 3 permanecem ocultos.
    wsManager.setTransitioning(true);
    const int visibleMidTransition = wsManager.applyViewportCulling(-960.0, 1920.0);
    QCOMPARE(visibleMidTransition, 2);
    QVERIFY(containers[0]->isVisible());
    QVERIFY(containers[1]->isVisible());
    QVERIFY(!containers[2]->isVisible());
    QVERIFY(!containers[3]->isVisible());

    // 3. Ao concluir transição para o Workspace 2 (viewportOffset = -3840):
    wsManager.switchToWorkspace(2);
    wsManager.setTransitioning(false);
    const int visibleAtTwo = wsManager.applyViewportCulling(-2.0 * 1920.0, 1920.0);
    QCOMPARE(visibleAtTwo, 1);
    QVERIFY(!containers[0]->isVisible());
    QVERIFY(!containers[1]->isVisible());
    QVERIFY(containers[2]->isVisible());
    QVERIFY(!containers[3]->isVisible());
    QVERIFY(glassItems[2]->hasPreservedVramTextures());
}

void tst_Phase4Audit::tileSplitterStressAndG2FusedRadiiSafety() {
    LiquidTilingService tiling;
    tiling.setTargetRefreshRateHz(60);
    tiling.resetConfigureAuditCounters();

    const QRectF available(12.0, 56.0, 1896.0, 900.0);

    // Simula 100 atualizações rápidas consecutivas de splitRatio entre 0.1 e 0.9
    for (int i = 0; i < 100; ++i) {
        const qreal rawRatio = 0.1 + (0.8 * static_cast<qreal>(i) / 99.0);
        tiling.setSplitRatio(rawRatio);

        // Verifica limites estritos [0.20, 0.80]
        QVERIFY(tiling.splitRatio() >= 0.20 - 1e-6);
        QVERIFY(tiling.splitRatio() <= 0.80 + 1e-6);

        // Verifica conservação de largura: Left + Right + Gap == AvailableWidth
        const QRectF leftRect = tiling.getSnapGeometry(
            LiquidTilingService::SnapLeft, available, tiling.splitRatio());
        const QRectF rightRect = tiling.getSnapGeometry(
            LiquidTilingService::SnapRight, available, tiling.splitRatio());
        const qreal totalWidth = leftRect.width() + rightRect.width() + tiling.splitterGap();
        QVERIFY(std::abs(totalWidth - available.width()) <= 1e-4);

        // Simula pedido interativo de redimensionamento durante arraste
        tiling.shouldDispatchInteractiveResize(true);
    }

    // O throttling V-Blank deve ter filtrado a grande maioria das 100 chamadas síncronas
    QVERIFY(tiling.throttledConfigureCount() > 80);
    QVERIFY(tiling.dispatchedConfigureCount() >= 1);

    // Ao soltar o divisor (interactive = false), o despacho final é sempre imediato
    QVERIFY(tiling.shouldDispatchInteractiveResize(false));

    // Validação algébrica dos raios fundidos G² em todas as zonas de acoplamento
    const LiquidTilingService::SnapZone zones[] = {
        LiquidTilingService::SnapNone,
        LiquidTilingService::SnapLeft,
        LiquidTilingService::SnapRight,
        LiquidTilingService::SnapTop,
        LiquidTilingService::SnapTopLeft,
        LiquidTilingService::SnapTopRight,
        LiquidTilingService::SnapBottomLeft,
        LiquidTilingService::SnapBottomRight
    };

    for (const auto zone : zones) {
        const QVector4D radii = tiling.calculateFusedRadii(static_cast<int>(zone), 22.0f);
        QVERIFY(tiling.isValidFusedRadii(radii));

        // Testa pontos críticos nos 4 cantos, centro e origem para garantir ausência de NaN/Inf
        const QPointF testPoints[] = {
            QPointF(0.0, 0.0),
            QPointF(400.0, 300.0),
            QPointF(-400.0, 300.0),
            QPointF(400.0, -300.0),
            QPointF(-400.0, -300.0),
            QPointF(199.9999, 149.9999)
        };
        for (const QPointF &pt : testPoints) {
            const float sdf = tiling.evaluateSafeG2Distance(
                pt, QSizeF(400.0, 300.0), radii);
            QVERIFY(std::isfinite(sdf));
            QVERIFY(!std::isnan(sdf));
        }
    }
}

void tst_Phase4Audit::multiMonitorHotUnplugMigration() {
    QTemporaryDir runtimeDir;
    QVERIFY(runtimeDir.isValid());
    qputenv("XDG_RUNTIME_DIR", runtimeDir.path().toUtf8());

    QWaylandCompositor compositor;
    compositor.setSocketName(QByteArrayLiteral("wayland-sge-phase4-audit"));
    compositor.create();

    OutputManagerService outputMgr;
    outputMgr.attachCompositor(&compositor);
    outputMgr.clearScreens();

    const int primaryIdx = outputMgr.addVirtualScreen(
        QStringLiteral("DP-1"), QRect(0, 0, 1920, 1080), 1.0, 60000);
    const int secondaryIdx = outputMgr.addVirtualScreen(
        QStringLiteral("DP-2"), QRect(1920, 0, 2560, 1440), 1.5, 144000);

    QCOMPARE(primaryIdx, 0);
    QCOMPARE(secondaryIdx, 1);
    QCOMPARE(outputMgr.screenCount(), 2);

    // Cria janela e superfície Wayland posicionadas no monitor secundário (DP-2)
    QQuickItem secondaryWindow;
    secondaryWindow.setX(2300.0);
    secondaryWindow.setY(220.0);
    secondaryWindow.setWidth(900.0);
    secondaryWindow.setHeight(600.0);

    QWaylandSurface clientSurface;

    const int routedScreen = outputMgr.updateWindowOutputRouting(
        &secondaryWindow, &clientSurface,
        QRectF(secondaryWindow.x(), secondaryWindow.y(),
               secondaryWindow.width(), secondaryWindow.height()));
    QCOMPARE(routedScreen, 1);
    QCOMPARE(outputMgr.windowScreenIndex(&secondaryWindow), 1);

    outputMgr.setActiveCursorScreen(1);
    QCOMPARE(outputMgr.activeCursorScreen(), 1);

    QSignalSpy migratedSpy(&outputMgr, &OutputManagerService::windowMigratedOnHotUnplug);
    QSignalSpy transitionSpy(&outputMgr, &OutputManagerService::windowScreenTransitioned);

    // Simula desconexão abrupta (Hot-Unplug) do monitor secundário DP-2
    const bool removed = outputMgr.removeScreen(1);
    QVERIFY(removed);
    QCOMPARE(outputMgr.screenCount(), 1);
    QCOMPARE(outputMgr.activeCursorScreen(), 0);

    // A janela órfã deve ter sido migrada para o ecrã primário (índice 0)
    QCOMPARE(outputMgr.windowScreenIndex(&secondaryWindow), 0);
    QCOMPARE(migratedSpy.count(), 1);
    QCOMPARE(transitionSpy.count(), 1);

    // As coordenadas da janela devem estar contidas dentro dos limites do monitor primário [0, 1920]x[0, 1080]
    const QRect primaryBounds = outputMgr.screenGeometryAt(0);
    QVERIFY(secondaryWindow.x() >= primaryBounds.x());
    QVERIFY(secondaryWindow.x() + secondaryWindow.width() <= primaryBounds.x() + primaryBounds.width());
    QVERIFY(secondaryWindow.y() >= primaryBounds.y());
    QVERIFY(secondaryWindow.y() + secondaryWindow.height() <= primaryBounds.y() + primaryBounds.height());
}

QTEST_MAIN(tst_Phase4Audit)
#include "tst_Phase4Audit.moc"
