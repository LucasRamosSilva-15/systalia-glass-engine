#include <QSignalSpy>
#include <QTest>
#include <QQuickWindow>
#include <QQuickItem>
#include <memory>

#include "gfx/v2/ValoGlassItemV2.hpp"
#include "gfx/v2/ValoGlassMaterialV2.hpp"
#include "AppRegistryService.hpp"
#include "NotificationServer.hpp"
#include "WindowStackModel.hpp"
#include "gfx/v2/SystaliaSurfacePropsV2.hpp"

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
    using Systalia::Gfx::V2::ValoGlassItemV2;

    std::unique_ptr<QQuickWindow> window = std::make_unique<QQuickWindow>();
    constexpr int ITERATIONS = 100;

    for (int i = 0; i < ITERATIONS; ++i) {
        auto *item = new ValoGlassItemV2(window->contentItem());
        item->setWidth(800.0);
        item->setHeight(500.0);
        item->setIor(1.12f + static_cast<float>(i) * 0.001f);
        item->setCornerRadii(QVector4D(20.0f, 20.0f, 20.0f, 20.0f));
        item->setCausticIntensity(0.85f);
        item->setBlurProfile(i % 3);

        QCOMPARE(item->cornerRadii(), QVector4D(20.0f, 20.0f, 20.0f, 20.0f));
        QCOMPARE(item->causticIntensity(), 0.85f);

        delete item;
    }

    QCOMPARE(window->contentItem()->childItems().count(), 0);
}

void TestResourceLeakAudit::materialShaderPipelineSharing() {
    using Systalia::Gfx::V2::ValoGlassMaterialV2;

    // Valida que múltiplas instâncias de ValoGlassMaterialV2 compartilham exatamente
    // o mesmo QSGMaterialType por modo diagnóstico, evitando recompilações em tempo de execução.
    ValoGlassMaterialV2 mat1(0);
    ValoGlassMaterialV2 mat2(0);
    ValoGlassMaterialV2 mat3(0);

    QVERIFY(mat1.type() != nullptr);
    QCOMPARE(mat1.type(), mat2.type());
    QCOMPARE(mat2.type(), mat3.type());
}

void TestResourceLeakAudit::materialBatchingComparison() {
    using Systalia::Gfx::V2::ValoGlassMaterialV2;

    ValoGlassMaterialV2 matA(0);
    ValoGlassMaterialV2 matB(0);

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
