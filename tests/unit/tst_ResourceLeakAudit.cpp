#include <QSignalSpy>
#include <QTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <memory>

#include "SystaliaGlassItem.hpp"
#include "SystaliaGlassMaterial.hpp"
#include "AppRegistryService.hpp"
#include "NotificationServer.hpp"
#include "WindowStackModel.hpp"
#include "SystaliaSurfaceProps.hpp"

class TestResourceLeakAudit final : public QObject {
    Q_OBJECT

private slots:
    void glassItemLifecycleInLoop();
    void materialShaderPipelineSharing();
    void materialBatchingComparison();
    void appRegistryServiceLifecycleInLoop();
    void notificationServerLifecycleInLoop();
    void windowStackModelLifecycleInLoop();
};

void TestResourceLeakAudit::glassItemLifecycleInLoop() {
    std::unique_ptr<QQuickWindow> window = std::make_unique<QQuickWindow>();
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto *item = new SystaliaGlassItem(window->contentItem());
        item->setWidth(800.0);
        item->setHeight(500.0);
        item->setIor(1.20f + static_cast<float>(i) * 0.001f);
        item->setRadii(QVector4D(20.0f, 20.0f, 20.0f, 20.0f));
        item->setCausticIntensity(0.85f);
        item->setTime(static_cast<float>(i) * 0.016f);

        QCOMPARE(item->radii(), QVector4D(20.0f, 20.0f, 20.0f, 20.0f));
        QCOMPARE(item->causticIntensity(), 0.85f);

        item->releaseResources();
        delete item;
    }

    QCOMPARE(window->contentItem()->childItems().count(), 0);
}

void TestResourceLeakAudit::materialShaderPipelineSharing() {
    // Valida que múltiplas instâncias de SystaliaGlassMaterial compartilham exatamente
    // o mesmo QSGMaterialType, garantindo que o Qt SceneGraph / QRhi compartilhe o mesmo
    // pipeline state e shader program sem recompilações em tempo de execução.
    SystaliaGlassMaterial mat1;
    SystaliaGlassMaterial mat2;
    SystaliaGlassMaterial mat3;

    QVERIFY(mat1.type() != nullptr);
    QCOMPARE(mat1.type(), mat2.type());
    QCOMPARE(mat2.type(), mat3.type());
}

void TestResourceLeakAudit::materialBatchingComparison() {
    SystaliaGlassMaterial matA;
    SystaliaGlassMaterial matB;

    // Sem texturas vinculadas, materiais devem ser equivalentes para batching (compare == 0)
    QCOMPARE(matA.compare(&matB), 0);
    QCOMPARE(matB.compare(&matA), 0);
}

void TestResourceLeakAudit::appRegistryServiceLifecycleInLoop() {
    constexpr int ITERATIONS = 100;
    for (int i = 0; i < ITERATIONS; ++i) {
        auto *service = new AppRegistryService();
        QVERIFY(service != nullptr);
        delete service;
    }
}

void TestResourceLeakAudit::notificationServerLifecycleInLoop() {
    constexpr int ITERATIONS = 100;
    for (int i = 0; i < ITERATIONS; ++i) {
        // false: não tenta registrar no barramento de sessão real durante o teste
        auto *server = new NotificationServer(false);
        QCOMPARE(server->notificationCount(), 0);

        const uint id = server->Notify(QStringLiteral("SGE-Test"), 0, QStringLiteral("dialog-information"),
                                       QStringLiteral("Título"), QStringLiteral("Corpo"),
                                       QStringList{}, QVariantMap{}, 5000);
        QVERIFY(id > 0);
        QCOMPARE(server->notificationCount(), 1);

        server->CloseNotification(id);
        QCOMPARE(server->notificationCount(), 0);
        delete server;
    }
}

void TestResourceLeakAudit::windowStackModelLifecycleInLoop() {
    constexpr int ITERATIONS = 100;
    for (int i = 0; i < ITERATIONS; ++i) {
        auto *model = new WindowStackModel();
        QCOMPARE(model->windowCount(), 0);
        QVERIFY(!model->taskSwitcherVisible());

        auto *obj1 = new QObject(model);
        auto *obj2 = new QObject(model);
        model->registerWindow(obj1);
        model->registerWindow(obj2);
        model->promoteWindow(obj1);

        QCOMPARE(model->windowCount(), 2);
        QVERIFY(model->beginTaskSwitch(1));
        QVERIFY(model->taskSwitcherVisible());

        model->commitTaskSwitch();
        QVERIFY(!model->taskSwitcherVisible());

        delete model;
    }
}

QTEST_MAIN(TestResourceLeakAudit)
#include "tst_ResourceLeakAudit.moc"
