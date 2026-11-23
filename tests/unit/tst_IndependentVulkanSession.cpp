#include <QtTest>

#include "DrmVulkanMatcher.hpp"
#include "InputDeviceManager.hpp"
#include "VulkanQuickRenderer.hpp"
#include "WaylandFrameCoordinator.hpp"

#include <QColor>
#include <QImage>
#include <QQuickWindow>
#include <QSGRendererInterface>

using namespace Systalia::Platform::Drm;

namespace {

class MockFrameSink final : public IWaylandOutputFrameSink {
public:
    bool automaticCallbacks = true;
    int startedCount = 0;
    int sentCount = 0;

    void setAutomaticCallbacks(bool automatic) override { automaticCallbacks = automatic; }
    void notifyFrameStarted() override { ++startedCount; }
    void notifySendFrameCallbacks() override { ++sentCount; }
};

} // namespace

class tst_IndependentVulkanSession final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase() { QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan); }
    void experimentalFlagOnlyActivatesInBareMetalKms();
    void neutralQpaInitializesVulkanAndRendersQmlWithoutHostCompositor();
    void drmVulkanDeviceMatcherIdentifiesPrimaryAndRenderNodes();
    void waylandFrameCoordinatorPreventsDuplicatesAndPrematureCallbacks();
};

void tst_IndependentVulkanSession::experimentalFlagOnlyActivatesInBareMetalKms() {
    // 1. Bare-metal sem flag mantém eglfs_kms
    const auto legacyBare = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArray(), QByteArray(), QByteArray(), QString(), false);
    QCOMPARE(legacyBare.mode, InputDeviceManager::SessionBareMetalKms);
    QVERIFY(!legacyBare.useExperimentalDrmBackend);
    QCOMPARE(legacyBare.qtQpaPlatform, QByteArrayLiteral("eglfs"));
    QCOMPARE(legacyBare.qtQpaEglfsIntegration, QByteArrayLiteral("eglfs_kms"));

    // 2. Bare-metal com flag seleciona QPA neutro sge_vulkan_offscreen sem eglfs_kms
    const auto expBare = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArray(), QByteArray(), QByteArray(), QString(), true);
    QCOMPARE(expBare.mode, InputDeviceManager::SessionBareMetalKms);
    QVERIFY(expBare.useExperimentalDrmBackend);
    QCOMPARE(expBare.qtQpaPlatform, QByteArrayLiteral("sge_vulkan_offscreen"));
    QVERIFY(expBare.qtQpaEglfsIntegration.isEmpty());
    QVERIFY(expBare.eglfsKmsConfigPath.isEmpty());

    // 3. Nested e Headless ignoram a flag experimental
    const auto nested = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArrayLiteral("wayland-1"), QByteArray(), QByteArray(), QString(), true);
    QCOMPARE(nested.mode, InputDeviceManager::SessionNested);
    QVERIFY(!nested.useExperimentalDrmBackend);

    const auto headless = InputDeviceManager::evaluatePlatformEnvironment(
        QByteArray(), QByteArray(), QByteArrayLiteral("offscreen"), QString(), true);
    QCOMPARE(headless.mode, InputDeviceManager::SessionHeadless);
    QVERIFY(!headless.useExperimentalDrmBackend);
}

void tst_IndependentVulkanSession::neutralQpaInitializesVulkanAndRendersQmlWithoutHostCompositor() {
    VulkanQuickRenderer renderer;
    QVERIFY2(renderer.initialize(QSize(64, 64)), qPrintable(renderer.lastError()));
    QVERIFY(!renderer.capabilities().deviceName.isEmpty());

    const QByteArray qml =
        "import QtQuick 2.15\n"
        "Rectangle {\n"
        "  width: 64; height: 64; color: '#220044'\n"
        "  Rectangle { x: 16; y: 16; width: 32; height: 32; color: '#ffaa00' }\n"
        "}\n";
    QVERIFY2(renderer.loadQmlSource(qml), qPrintable(renderer.lastError()));
    QVERIFY(renderer.renderFrame());

    const QImage img = renderer.readbackImage();
    QVERIFY(!img.isNull());
    const QColor corner = img.pixelColor(4, 4);
    const QColor center = img.pixelColor(32, 32);
    QVERIFY(qAbs(corner.red() - 0x22) <= 4 && corner.green() <= 4 && qAbs(corner.blue() - 0x44) <= 4);
    QVERIFY(center.red() >= 250 && qAbs(center.green() - 0xaa) <= 4 && center.blue() <= 4);
    renderer.shutdown();
}

void tst_IndependentVulkanSession::drmVulkanDeviceMatcherIdentifiesPrimaryAndRenderNodes() {
    DrmVulkanDeviceIds ids;
    ids.hasPrimary = true;
    ids.primaryMajor = 226;
    ids.primaryMinor = 1;
    ids.hasRender = true;
    ids.renderMajor = 226;
    ids.renderMinor = 129;

    QVERIFY(matchesDrmDevice(ids, 226, 1));
    QVERIFY(!matchesDrmDevice(ids, 226, 0));

    // Mesmo quando apenas renderNode está disponível (card1=226:1 -> render=226:129)
    ids.hasPrimary = false;
    QVERIFY(matchesDrmDevice(ids, 226, 1));
    QVERIFY(!matchesDrmDevice(ids, 226, 2));
}

void tst_IndependentVulkanSession::waylandFrameCoordinatorPreventsDuplicatesAndPrematureCallbacks() {
    MockFrameSink sink;
    WaylandFrameCoordinator coord(sink);
    QVERIFY(!sink.automaticCallbacks);

    // 1. Ciclo normal: 1 frameStarted -> 1 sendFrameCallbacks
    QVERIFY(coord.beginFrame());
    QVERIFY(!coord.beginFrame()); // bloqueia duplicata enquanto em voo
    QVERIFY(coord.completeFrame(true));
    QCOMPARE(sink.startedCount, 1);
    QCOMPARE(sink.sentCount, 1);
    QCOMPARE(coord.completedFrames(), 1ULL);

    // 2. Falha no commit KMS não envia wl_surface.frame
    QVERIFY(coord.beginFrame());
    QVERIFY(!coord.completeFrame(false));
    QCOMPARE(sink.sentCount, 1);
    QCOMPARE(coord.droppedFrames(), 1ULL);

    // 3. Suspensão bloqueia início e invalida frame em voo
    QVERIFY(coord.beginFrame());
    coord.setSuspended(true);
    QVERIFY(!coord.beginFrame());
    QVERIFY(!coord.completeFrame(true));
    QCOMPARE(sink.sentCount, 1);

    coord.setSuspended(false);
    QVERIFY(coord.beginFrame());
    QVERIFY(coord.completeFrame(true));
    QCOMPARE(sink.sentCount, 2);
}

QTEST_MAIN(tst_IndependentVulkanSession)
#include "tst_IndependentVulkanSession.moc"
