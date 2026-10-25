#include <QTest>
#include <cstddef>
#include <array>

#include "SystaliaSurfaceProps.hpp"

static_assert(sizeof(SystaliaSurfaceProps) == 128);
static_assert(offsetof(SystaliaSurfaceProps, matrix) == 0);
static_assert(offsetof(SystaliaSurfaceProps, radii) == 64);
static_assert(offsetof(SystaliaSurfaceProps, windowSize) == 80);
static_assert(offsetof(SystaliaSurfaceProps, screenResolution) == 88);
static_assert(offsetof(SystaliaSurfaceProps, slant) == 96);
static_assert(offsetof(SystaliaSurfaceProps, ior) == 100);
static_assert(offsetof(SystaliaSurfaceProps, chromaticDispersion) == 104);
static_assert(offsetof(SystaliaSurfaceProps, causticIntensity) == 108);
static_assert(offsetof(SystaliaSurfaceProps, opacity) == 112);
static_assert(offsetof(SystaliaSurfaceProps, time) == 116);
static_assert(offsetof(SystaliaSurfaceProps, hasClientTexture) == 120);
static_assert(offsetof(SystaliaSurfaceProps, titleBarHeight) == 124);

class TestMemoryAlignment final : public QObject {
    Q_OBJECT

private slots:
    void uniformBlockSizeIs128Bytes();
    void uniformBlockAlignmentIs16Bytes();
    void memberOffsets_data();
    void memberOffsets();
    void surfaceDefaultsAreStable();
};

void TestMemoryAlignment::uniformBlockSizeIs128Bytes() {
    QCOMPARE(sizeof(SystaliaSurfaceProps), std::size_t(128));
    QCOMPARE(sizeof(Matrix4x4_std140), std::size_t(64));
}

void TestMemoryAlignment::uniformBlockAlignmentIs16Bytes() {
    QCOMPARE(alignof(SystaliaSurfaceProps), std::size_t(16));
    QCOMPARE(alignof(Matrix4x4_std140), std::size_t(16));
}

void TestMemoryAlignment::memberOffsets_data() {
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
    QTest::newRow("opacity") << QStringLiteral("opacity") << quint64(112) << 8;
    QTest::newRow("time") << QStringLiteral("time") << quint64(116) << 9;
    QTest::newRow("hasClientTexture") << QStringLiteral("hasClientTexture") << quint64(120) << 10;
    QTest::newRow("titleBarHeight") << QStringLiteral("titleBarHeight") << quint64(124) << 11;
}

void TestMemoryAlignment::memberOffsets() {
    QFETCH(QString, member);
    QFETCH(quint64, expectedOffset);
    QFETCH(int, memberIndex);
    constexpr std::array<quint64, 12> offsets{
        offsetof(SystaliaSurfaceProps, matrix),
        offsetof(SystaliaSurfaceProps, radii),
        offsetof(SystaliaSurfaceProps, windowSize),
        offsetof(SystaliaSurfaceProps, screenResolution),
        offsetof(SystaliaSurfaceProps, slant),
        offsetof(SystaliaSurfaceProps, ior),
        offsetof(SystaliaSurfaceProps, chromaticDispersion),
        offsetof(SystaliaSurfaceProps, causticIntensity),
        offsetof(SystaliaSurfaceProps, opacity),
        offsetof(SystaliaSurfaceProps, time),
        offsetof(SystaliaSurfaceProps, hasClientTexture),
        offsetof(SystaliaSurfaceProps, titleBarHeight)
    };
    QVERIFY2(offsets.at(static_cast<std::size_t>(memberIndex)) == expectedOffset,
             qPrintable(QStringLiteral("Incorrect std140 offset for %1").arg(member)));
}

void TestMemoryAlignment::surfaceDefaultsAreStable() {
    const SystaliaSurfaceProps props;
    QCOMPARE(props.slant, 0.0f);
    QCOMPARE(props.ior, 1.25f);
    QCOMPARE(props.chromaticDispersion, 0.040f);
    QCOMPARE(props.causticIntensity, 0.85f);
    QCOMPARE(props.opacity, 0.85f);
    QCOMPARE(props.time, 0.0f);
    QCOMPARE(props.hasClientTexture, 0.0f);
    QCOMPARE(props.titleBarHeight, 36.0f);
}

QTEST_GUILESS_MAIN(TestMemoryAlignment)
#include "tst_MemoryAlignment.moc"
