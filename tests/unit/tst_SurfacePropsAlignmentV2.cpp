#include <QTest>
#include <cstddef>
#include <array>

#include "ValoStd140Matrix.hpp"
#include "SystaliaSurfacePropsV2.hpp"

using namespace Systalia::Gfx::V2;

class TestSurfacePropsAlignmentV2 final : public QObject {
    Q_OBJECT

private slots:
    void std140MatrixBlockPreserved();
    void v2UniformBlockSizeIs160Bytes();
    void v2UniformBlockAlignmentIs16Bytes();
    void v2MemberOffsets_data();
    void v2MemberOffsets();
    void v2SurfaceDefaultsAreStable();
};

void TestSurfacePropsAlignmentV2::std140MatrixBlockPreserved() {
    // Garantir que o bloco compartilhado Matrix4x4_std140 (64 bytes, align 16) está preservado
    QCOMPARE(sizeof(Matrix4x4_std140), std::size_t(64));
    QCOMPARE(alignof(Matrix4x4_std140), std::size_t(16));
}

void TestSurfacePropsAlignmentV2::v2UniformBlockSizeIs160Bytes() {
    QCOMPARE(sizeof(SystaliaSurfacePropsV2), std::size_t(160));
    QCOMPARE(sizeof(Matrix4x4_std140), std::size_t(64));
    QCOMPARE(sizeof(SystaliaSurfacePropsV2) % 16, std::size_t(0));
}

void TestSurfacePropsAlignmentV2::v2UniformBlockAlignmentIs16Bytes() {
    QCOMPARE(alignof(SystaliaSurfacePropsV2), std::size_t(16));
    QCOMPARE(alignof(Matrix4x4_std140), std::size_t(16));
}

void TestSurfacePropsAlignmentV2::v2MemberOffsets_data() {
    QTest::addColumn<QString>("member");
    QTest::addColumn<quint64>("expectedOffset");
    QTest::addColumn<int>("memberIndex");

    QTest::newRow("matrix") << QStringLiteral("matrix") << quint64(0) << 0;
    QTest::newRow("radii") << QStringLiteral("radii") << quint64(64) << 1;
    QTest::newRow("windowSize") << QStringLiteral("windowSize") << quint64(80) << 2;
    QTest::newRow("screenResolution") << QStringLiteral("screenResolution") << quint64(88) << 3;
    QTest::newRow("slant") << QStringLiteral("slant") << quint64(96) << 4;
    QTest::newRow("ior") << QStringLiteral("ior") << quint64(100) << 5;
    QTest::newRow("chromaticDispersion") << QStringLiteral("chromaticDispersion") << quint64(104) << 6;
    QTest::newRow("causticIntensity") << QStringLiteral("causticIntensity") << quint64(108) << 7;
    QTest::newRow("surfaceOpacity") << QStringLiteral("surfaceOpacity") << quint64(112) << 8;
    QTest::newRow("time") << QStringLiteral("time") << quint64(116) << 9;
    QTest::newRow("hasClientTexture") << QStringLiteral("hasClientTexture") << quint64(120) << 10;
    QTest::newRow("inheritedOpacity") << QStringLiteral("inheritedOpacity") << quint64(124) << 11;
    // Campos V2
    QTest::newRow("specularStrokeWidth") << QStringLiteral("specularStrokeWidth") << quint64(128) << 12;
    QTest::newRow("geometryMode") << QStringLiteral("geometryMode") << quint64(132) << 13;
    QTest::newRow("shadowIntensity") << QStringLiteral("shadowIntensity") << quint64(136) << 14;
    QTest::newRow("shadowOffsetY") << QStringLiteral("shadowOffsetY") << quint64(140) << 15;
    QTest::newRow("volumetricTint") << QStringLiteral("volumetricTint") << quint64(144) << 16;
}

void TestSurfacePropsAlignmentV2::v2MemberOffsets() {
    QFETCH(QString, member);
    QFETCH(quint64, expectedOffset);
    QFETCH(int, memberIndex);

    constexpr std::array<quint64, 17> offsets{
        offsetof(SystaliaSurfacePropsV2, matrix),
        offsetof(SystaliaSurfacePropsV2, radii),
        offsetof(SystaliaSurfacePropsV2, windowSize),
        offsetof(SystaliaSurfacePropsV2, screenResolution),
        offsetof(SystaliaSurfacePropsV2, slant),
        offsetof(SystaliaSurfacePropsV2, ior),
        offsetof(SystaliaSurfacePropsV2, chromaticDispersion),
        offsetof(SystaliaSurfacePropsV2, causticIntensity),
        offsetof(SystaliaSurfacePropsV2, surfaceOpacity),
        offsetof(SystaliaSurfacePropsV2, time),
        offsetof(SystaliaSurfacePropsV2, hasClientTexture),
        offsetof(SystaliaSurfacePropsV2, inheritedOpacity),
        offsetof(SystaliaSurfacePropsV2, specularStrokeWidth),
        offsetof(SystaliaSurfacePropsV2, geometryMode),
        offsetof(SystaliaSurfacePropsV2, shadowIntensity),
        offsetof(SystaliaSurfacePropsV2, shadowOffsetY),
        offsetof(SystaliaSurfacePropsV2, volumetricTint)
    };

    QVERIFY2(offsets.at(static_cast<std::size_t>(memberIndex)) == expectedOffset,
             qPrintable(QStringLiteral("Incorrect std140 offset for %1 in V2").arg(member)));
}

void TestSurfacePropsAlignmentV2::v2SurfaceDefaultsAreStable() {
    const SystaliaSurfacePropsV2 props;
    QCOMPARE(props.slant, 0.0f);
    QCOMPARE(props.ior, 1.25f);
    QCOMPARE(props.chromaticDispersion, 0.040f);
    QCOMPARE(props.causticIntensity, 0.85f);
    QCOMPARE(props.surfaceOpacity, 0.34f);
    QCOMPARE(props.time, 0.0f);
    QCOMPARE(props.hasClientTexture, 0.0f);
    QCOMPARE(props.inheritedOpacity, 1.0f);
    QCOMPARE(props.specularStrokeWidth, 1.25f);
    QCOMPARE(props.geometryMode, 0.0f);
    QCOMPARE(props.shadowIntensity, 0.35f);
    QCOMPARE(props.shadowOffsetY, 12.0f);
    QCOMPARE(props.volumetricTint, QVector4D(0.03f, 0.18f, 0.32f, 0.39f));
}

QTEST_GUILESS_MAIN(TestSurfacePropsAlignmentV2)
#include "tst_SurfacePropsAlignmentV2.moc"

