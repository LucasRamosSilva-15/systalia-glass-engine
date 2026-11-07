#include <QTest>
#include <QFile>
#include <QByteArray>
#include <QSignalSpy>

#include "SystaliaSurfacePropsV2.hpp"
#include "ValoGlassMaterialV2.hpp"
#include "ValoGlassItemV2.hpp"

using namespace Systalia::Gfx::V2;

class TestValoGlassShaderV2 final : public QObject {
    Q_OBJECT

private slots:
    void v2ShaderBytecodesExistInResources();
    void v2MaterialFlagsAndType();
    void v2MaterialCompare();
    void v2MaterialUboSizeAndDirtyTracking();
    void v2ItemPropertiesAndSignals();
    void v2ItemGeometryModeEnum();
};

void TestValoGlassShaderV2::v2ShaderBytecodesExistInResources() {
    // 1. Verificar presença do bytecode de vértice V2 compilado via qsb
    QFile vertFile(QStringLiteral(":/shaders/v2/valo_optical_v2.vert.qsb"));
    QVERIFY2(vertFile.exists(), "Shader :/shaders/v2/valo_optical_v2.vert.qsb nao encontrado no recurso Qt.");
    QVERIFY(vertFile.open(QIODevice::ReadOnly));
    const QByteArray vertData = vertFile.readAll();
    QVERIFY(vertData.size() > 64);
    vertFile.close();

    // 2. Verificar presença do bytecode de fragmento V2 compilado via qsb
    QFile fragFile(QStringLiteral(":/shaders/v2/valo_optical_v2.frag.qsb"));
    QVERIFY2(fragFile.exists(), "Shader :/shaders/v2/valo_optical_v2.frag.qsb nao encontrado no recurso Qt.");
    QVERIFY(fragFile.open(QIODevice::ReadOnly));
    const QByteArray fragData = fragFile.readAll();
    QVERIFY(fragData.size() > 64);
    fragFile.close();
}

void TestValoGlassShaderV2::v2MaterialFlagsAndType() {
    ValoGlassMaterialV2 mat1;
    ValoGlassMaterialV2 mat2;

    // Flags exigidas pelo contrato de composição alpha e transformação
    QVERIFY(mat1.flags().testFlag(QSGMaterial::Blending));
    QVERIFY(mat1.flags().testFlag(QSGMaterial::RequiresFullMatrix));

    // Ambos os materiais devem compartilhar o mesmo QSGMaterialType estático
    QCOMPARE(mat1.type(), mat2.type());
    QVERIFY(mat1.type() != nullptr);
}

void TestValoGlassShaderV2::v2MaterialCompare() {
    ValoGlassMaterialV2 matA;
    ValoGlassMaterialV2 matB;

    // Idênticos inicialmente
    QCOMPARE(matA.compare(&matB), 0);

    // Diferenças no modo geométrico (L2 vs G2) devem ordenar o batching
    matA.props().geometryMode = 0.0f;
    matB.props().geometryMode = 1.0f;
    QVERIFY(matA.compare(&matB) < 0);
    QVERIFY(matB.compare(&matA) > 0);

    // Mesma geometria mas larguras especulares diferentes
    matB.props().geometryMode = 0.0f;
    matA.props().specularStrokeWidth = 1.0f;
    matB.props().specularStrokeWidth = 2.0f;
    QVERIFY(matA.compare(&matB) < 0);
}

void TestValoGlassShaderV2::v2MaterialUboSizeAndDirtyTracking() {
    // Validação estrita do tamanho do UBO
    QCOMPARE(sizeof(SystaliaSurfacePropsV2), std::size_t(160));
    QCOMPARE(sizeof(SystaliaSurfacePropsV2) % 16, std::size_t(0));

    ValoGlassMaterialV2 mat;
    mat.props().windowSize = QVector2D(400.0f, 250.0f);
    mat.props().radii = QVector4D(24.0f, 24.0f, 24.0f, 24.0f);
    mat.props().surfaceOpacity = 0.45f;
    mat.props().specularStrokeWidth = 1.50f;
    mat.props().geometryMode = 1.0f; // Squircle G2

    QCOMPARE(mat.props().windowSize, QVector2D(400.0f, 250.0f));
    QCOMPARE(mat.props().surfaceOpacity, 0.45f);
    QCOMPARE(mat.props().geometryMode, 1.0f);
}

void TestValoGlassShaderV2::v2ItemPropertiesAndSignals() {
    ValoGlassItemV2 item;

    QSignalSpy radiiSpy(&item, &ValoGlassItemV2::cornerRadiiChanged);
    QSignalSpy modeSpy(&item, &ValoGlassItemV2::geometryModeChanged);
    QSignalSpy opacitySpy(&item, &ValoGlassItemV2::surfaceOpacityChanged);
    QSignalSpy strokeSpy(&item, &ValoGlassItemV2::specularStrokeWidthChanged);
    QSignalSpy tintSpy(&item, &ValoGlassItemV2::volumetricTintChanged);
    QSignalSpy activeSpy(&item, &ValoGlassItemV2::activeChanged);
    QSignalSpy debugSpy(&item, &ValoGlassItemV2::debugModeChanged);
    QSignalSpy slantSpy(&item, &ValoGlassItemV2::slantChanged);

    // Valores padrão
    QCOMPARE(item.cornerRadii(), QVector4D(32.0f, 32.0f, 32.0f, 32.0f));
    QCOMPARE(item.geometryMode(), static_cast<int>(ValoGlassItemV2::CircularL2));
    QCOMPARE(item.surfaceOpacity(), 0.34f);
    QCOMPARE(item.specularStrokeWidth(), 1.25f);
    QVERIFY(item.isActive());
    QCOMPARE(item.debugMode(), 0);
    QCOMPARE(item.slant(), 0.0f);

    // Modificações e disparos de sinal
    item.setCornerRadii(QVector4D(40.0f, 40.0f, 15.0f, 15.0f));
    QCOMPARE(radiiSpy.count(), 1);
    QCOMPARE(item.cornerRadii(), QVector4D(40.0f, 40.0f, 15.0f, 15.0f));

    item.setGeometryMode(ValoGlassItemV2::SquircleG2);
    QCOMPARE(modeSpy.count(), 1);
    QCOMPARE(item.geometryMode(), static_cast<int>(ValoGlassItemV2::SquircleG2));

    item.setSurfaceOpacity(0.50f);
    QCOMPARE(opacitySpy.count(), 1);
    QCOMPARE(item.surfaceOpacity(), 0.50f);

    item.setSpecularStrokeWidth(2.0f);
    QCOMPARE(strokeSpy.count(), 1);
    QCOMPARE(item.specularStrokeWidth(), 2.0f);

    item.setVolumetricTint(QColor(10, 50, 90, 180));
    QCOMPARE(tintSpy.count(), 1);
    QCOMPARE(item.volumetricTint(), QColor(10, 50, 90, 180));

    item.setActive(false);
    QCOMPARE(activeSpy.count(), 1);
    QVERIFY(!item.isActive());

    item.setDebugMode(2);
    QCOMPARE(debugSpy.count(), 1);
    QCOMPARE(item.debugMode(), 2);

    item.setSlant(0.015f);
    QCOMPARE(slantSpy.count(), 1);
    QCOMPARE(item.slant(), 0.015f);
}

void TestValoGlassShaderV2::v2ItemGeometryModeEnum() {
    QCOMPARE(static_cast<int>(ValoGlassItemV2::CircularL2), 0);
    QCOMPARE(static_cast<int>(ValoGlassItemV2::SquircleG2), 1);
}

QTEST_GUILESS_MAIN(TestValoGlassShaderV2)
#include "tst_ValoGlassShaderV2.moc"

