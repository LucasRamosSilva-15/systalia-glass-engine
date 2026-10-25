#include <QTest>
#include <QFile>
#include "SystaliaSurfaceProps.hpp"

class TestSystaliaEngine : public QObject {
    Q_OBJECT

private slots:
    void testStd140AlignmentAndSize();
    void testStd140Offsets();
    void testShaderBytecodeAssets();
};

void TestSystaliaEngine::testStd140AlignmentAndSize() {
    QCOMPARE(sizeof(SystaliaSurfaceProps), size_t(128));
    QCOMPARE(sizeof(SystaliaSurfaceProps) % 16, size_t(0));
    QCOMPARE(sizeof(Matrix4x4_std140), size_t(64));
    QCOMPARE(alignof(SystaliaSurfaceProps), size_t(16));
    QCOMPARE(alignof(Matrix4x4_std140), size_t(16));
}

void TestSystaliaEngine::testStd140Offsets() {
    QCOMPARE(offsetof(SystaliaSurfaceProps, matrix), size_t(0));
    QCOMPARE(offsetof(SystaliaSurfaceProps, radii), size_t(64));
    QCOMPARE(offsetof(SystaliaSurfaceProps, windowSize), size_t(80));
    QCOMPARE(offsetof(SystaliaSurfaceProps, screenResolution), size_t(88));
    QCOMPARE(offsetof(SystaliaSurfaceProps, slant), size_t(96));
    QCOMPARE(offsetof(SystaliaSurfaceProps, ior), size_t(100));
    QCOMPARE(offsetof(SystaliaSurfaceProps, chromaticDispersion), size_t(104));
    QCOMPARE(offsetof(SystaliaSurfaceProps, causticIntensity), size_t(108));
    QCOMPARE(offsetof(SystaliaSurfaceProps, opacity), size_t(112));
    QCOMPARE(offsetof(SystaliaSurfaceProps, time), size_t(116));
    QCOMPARE(offsetof(SystaliaSurfaceProps, hasClientTexture), size_t(120));
    QCOMPARE(offsetof(SystaliaSurfaceProps, titleBarHeight), size_t(124));
}

void TestSystaliaEngine::testShaderBytecodeAssets() {
    QVERIFY2(QFile::exists(":/shaders/valo_optical.vert.qsb"),
             "Bytecode SPIR-V vert.qsb não encontrado em :/shaders/");
    QVERIFY2(QFile::exists(":/shaders/valo_optical.frag.qsb"),
             "Bytecode SPIR-V frag.qsb não encontrado em :/shaders/");
}

QTEST_MAIN(TestSystaliaEngine)
#include "tst_SystaliaEngine.moc"
