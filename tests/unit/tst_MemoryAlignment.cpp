#include <QTest>
#include <cstddef>

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
    void exactStd140Layout();
};

void TestMemoryAlignment::exactStd140Layout() {
    const SystaliaSurfaceProps props;
    QCOMPARE(sizeof(props), std::size_t(128));
    QCOMPARE(alignof(SystaliaSurfaceProps), std::size_t(16));
    QCOMPARE(reinterpret_cast<const char *>(&props.radii)
                 - reinterpret_cast<const char *>(&props),
             std::ptrdiff_t(64));
    QCOMPARE(props.titleBarHeight, 36.0f);
}

QTEST_GUILESS_MAIN(TestMemoryAlignment)
#include "tst_MemoryAlignment.moc"
