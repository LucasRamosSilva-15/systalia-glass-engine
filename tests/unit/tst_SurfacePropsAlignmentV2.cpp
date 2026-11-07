#include <QTest>
#include <cstddef>
#include <array>

#include "SystaliaSurfaceProps.hpp"
#include "SystaliaSurfacePropsV2.hpp"

using namespace Systalia::Gfx::V2;

class TestSurfacePropsAlignmentV2 final : public QObject {
    Q_OBJECT

private slots:
    void v1LegacyUniformBlockPreserved();
    void v2UniformBlockSizeIs160Bytes();
    void v2UniformBlockAlignmentIs16Bytes();
    void v2MemberOffsets_data();
    void v2MemberOffsets();
    void v2SurfaceDefaultsAreStable();
};

void TestSurfacePropsAlignmentV2::v1LegacyUniformBlockPreserved() {
    // Garantir que a ABI legada de 128 bytes permaneceu 100% inalterada
    QCOMPARE(sizeof(SystaliaSurfaceProps), std::size_t(128));
    QCOMPARE(alignof(SystaliaSurfaceProps), std::size_t(16));
    QCOMPARE(offsetof(SystaliaSurfaceProps, matrix), std::size_t(0));
    QCOMPARE(offsetof(SystaliaSurfaceProps, radii), std::size_t(64));
    QCOMPARE(offsetof(SystaliaSurfaceProps, windowSize), std::size_t(80));
    QCOMPARE(offsetof(SystaliaSurfaceProps, screenResolution), std::size_t(88));
    QCOMPARE(offsetof(SystaliaSurfaceProps, slant), std::size_t(96));
    QCOMPARE(offsetof(SystaliaSurfaceProps, ior), std::size_t(100));
    QCOMPARE(offsetof(SystaliaSurfaceProps, chromaticDispersion), std::size_t(104));
    QCOMPARE(offsetof(SystaliaSurfaceProps, causticIntensity), std::size_t(108));
    QCOMPARE(offsetof(SystaliaSurfaceProps, opacity), std::size_t(112));
    QCOMPARE(offsetof(SystaliaSurfaceProps, time), std::size_t(116));
    QCOMPARE(offsetof(SystaliaSurfaceProps, hasClientTexture), std::size_t(120));
    QCOMPARE(offsetof(SystaliaSurfaceProps, titleBarHeight), std::size_t(124));
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
    QTest::newRow("titleBarHeight") << QStringLiteral("titleBarHeight") << quint64(124) << 11;
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
        offsetof(SystaliaSurfacePropsV2, titleBarHeight),
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
    QCOMPARE(props.titleBarHeight, 36.0f);
    QCOMPARE(props.specularStrokeWidth, 1.25f);
    QCOMPARE(props.geometryMode, 0.0f);
    QCOMPARE(props.shadowIntensity, 0.35f);
    QCOMPARE(props.shadowOffsetY, 12.0f);
    QCOMPARE(props.volumetricTint, QVector4D(0.03f, 0.18f, 0.32f, 0.39f));
}

QTEST_GUILESS_MAIN(TestSurfacePropsAlignmentV2)
#include "tst_SurfacePropsAlignmentV2.moc"

