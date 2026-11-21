#include <QTest>
#include <QFile>
#include "graphics/SystaliaSurfacePropsV2.hpp"

using namespace Systalia::Gfx::V2;

class TestSystaliaEngine : public QObject {
    Q_OBJECT

private slots:
    void testStd140AlignmentAndSize();
    void testStd140Offsets();
    void testShaderBytecodeAssets();
};

void TestSystaliaEngine::testStd140AlignmentAndSize() {
    QCOMPARE(sizeof(SystaliaSurfacePropsV2), size_t(160));
    QCOMPARE(sizeof(SystaliaSurfacePropsV2) % 16, size_t(0));
    QCOMPARE(sizeof(Matrix4x4_std140), size_t(64));
    QCOMPARE(alignof(SystaliaSurfacePropsV2), size_t(16));
    QCOMPARE(alignof(Matrix4x4_std140), size_t(16));
}

void TestSystaliaEngine::testStd140Offsets() {
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, matrix), size_t(0));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, radii), size_t(64));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, windowSize), size_t(80));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, screenResolution), size_t(88));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, slant), size_t(96));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, ior), size_t(100));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, chromaticDispersion), size_t(104));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, causticIntensity), size_t(108));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, surfaceOpacity), size_t(112));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, time), size_t(116));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, hasClientTexture), size_t(120));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, titleBarHeight), size_t(124));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, specularStrokeWidth), size_t(128));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, geometryMode), size_t(132));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, shadowIntensity), size_t(136));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, shadowOffsetY), size_t(140));
    QCOMPARE(offsetof(SystaliaSurfacePropsV2, volumetricTint), size_t(144));
}

void TestSystaliaEngine::testShaderBytecodeAssets() {
    QVERIFY2(QFile::exists(":/shaders/v2/valo_optical_v2.vert.qsb"),
             "Bytecode SPIR-V valo_optical_v2.vert.qsb não encontrado em :/shaders/v2/");
    QVERIFY2(QFile::exists(":/shaders/v2/valo_optical_v2.frag.qsb"),
             "Bytecode SPIR-V valo_optical_v2.frag.qsb não encontrado em :/shaders/v2/");
    QVERIFY2(QFile::exists(":/shaders/v2/valo_shadow_v2.vert.qsb"),
             "Bytecode SPIR-V valo_shadow_v2.vert.qsb não encontrado em :/shaders/v2/");
    QVERIFY2(QFile::exists(":/shaders/v2/valo_shadow_v2.frag.qsb"),
             "Bytecode SPIR-V valo_shadow_v2.frag.qsb não encontrado em :/shaders/v2/");
    QVERIFY2(QFile::exists(":/shaders/v2/valo_kawase_down.frag.qsb"),
             "Bytecode SPIR-V valo_kawase_down.frag.qsb não encontrado em :/shaders/v2/");
    QVERIFY2(QFile::exists(":/shaders/v2/valo_kawase_up.frag.qsb"),
             "Bytecode SPIR-V valo_kawase_up.frag.qsb não encontrado em :/shaders/v2/");
}

QTEST_MAIN(TestSystaliaEngine)
#include "tst_SystaliaEngine.moc"
