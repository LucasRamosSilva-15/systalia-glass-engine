#include <QTest>
#include <QQuickWindow>
#include <QDateTime>
#include <QTimeZone>
#include <cmath>
#include <string_view>
#include "SystemTelemetryService.hpp"

class TestTelemetry final : public QObject {
    Q_OBJECT

private slots:
    void parsesCpuCounters_data();
    void parsesCpuCounters();
    void computesCpuLoad_data();
    void computesCpuLoad();
    void rejectsInvalidCpuSamples();
    void parsesMemoryCounters_data();
    void parsesMemoryCounters();
    void convertsMemoryUnits();
    void rejectsIncompleteMemorySamples();
    void readsLiveMemoryFromProc();
    void formatsSystemClock();
    void countsWindowFrameSwaps();
};

void TestTelemetry::parsesCpuCounters_data() {
    QTest::addColumn<QByteArray>("sample");
    QTest::addColumn<quint64>("expectedTotal");
    QTest::addColumn<quint64>("expectedIdle");
    QTest::newRow("aggregate") << QByteArray("cpu 100 20 30 400 50 5 10 2 8 1\n")
                                << quint64(617) << quint64(450);
    QTest::newRow("single-core-line") << QByteArray("cpu 10 2 3 40 5 1 1 0\n")
                                      << quint64(62) << quint64(45);
    QTest::newRow("minimal-fields") << QByteArray("cpu 1 2 3 4\n")
                                     << quint64(10) << quint64(4);
}

void TestTelemetry::parsesCpuCounters() {
    QFETCH(QByteArray, sample);
    QFETCH(quint64, expectedTotal);
    QFETCH(quint64, expectedIdle);
    SystemTelemetryService::CpuCounters counters;
    QVERIFY(SystemTelemetryService::parseCpuStat(
        std::string_view(sample.constData(), static_cast<std::size_t>(sample.size())), counters));
    QCOMPARE(counters.total, expectedTotal);
    QCOMPARE(counters.idle, expectedIdle);
}

void TestTelemetry::computesCpuLoad_data() {
    QTest::addColumn<quint64>("previousTotal");
    QTest::addColumn<quint64>("previousIdle");
    QTest::addColumn<quint64>("currentTotal");
    QTest::addColumn<quint64>("currentIdle");
    QTest::addColumn<float>("expectedUsage");
    QTest::newRow("mixed-load") << quint64(617) << quint64(450) << quint64(655)
                                 << quint64(465) << 60.526315f;
    QTest::newRow("sixty-percent") << quint64(1000) << quint64(700) << quint64(1100)
                                   << quint64(740) << 60.0f;
    QTest::newRow("idle") << quint64(100) << quint64(80) << quint64(200)
                           << quint64(180) << 0.0f;
}

void TestTelemetry::computesCpuLoad() {
    QFETCH(quint64, previousTotal);
    QFETCH(quint64, previousIdle);
    QFETCH(quint64, currentTotal);
    QFETCH(quint64, currentIdle);
    QFETCH(float, expectedUsage);
    const SystemTelemetryService::CpuCounters previous{previousTotal, previousIdle};
    const SystemTelemetryService::CpuCounters current{currentTotal, currentIdle};
    const float usage = SystemTelemetryService::calculateCpuUsage(previous, current);
    QVERIFY(std::abs(usage - expectedUsage) < 0.001f);
}

void TestTelemetry::rejectsInvalidCpuSamples() {
    SystemTelemetryService::CpuCounters counters;
    QVERIFY(!SystemTelemetryService::parseCpuStat("cpu0 1 2 3 4\n", counters));
    QVERIFY(!SystemTelemetryService::parseCpuStat("cpu 1 2\n", counters));
    const SystemTelemetryService::CpuCounters previous{100, 80};
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(previous, previous), 0.0f);
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(
                 SystemTelemetryService::CpuCounters{110, 90}, previous), 0.0f);
}

void TestTelemetry::parsesMemoryCounters_data() {
    QTest::addColumn<QByteArray>("sample");
    QTest::addColumn<quint64>("expectedTotal");
    QTest::addColumn<quint64>("expectedAvailable");
    QTest::newRow("proc-order") << QByteArray(
        "MemTotal:       16384000 kB\nMemFree: 2048000 kB\nMemAvailable: 8192000 kB\n")
        << quint64(16384000) << quint64(8192000);
    QTest::newRow("reordered") << QByteArray(
        "MemAvailable: 4096 kB\nBuffers: 500 kB\nMemTotal: 8192 kB\n")
        << quint64(8192) << quint64(4096);
}

void TestTelemetry::parsesMemoryCounters() {
    QFETCH(QByteArray, sample);
    QFETCH(quint64, expectedTotal);
    QFETCH(quint64, expectedAvailable);
    SystemTelemetryService::MemoryCounters memory;
    QVERIFY(SystemTelemetryService::parseMemInfo(
        std::string_view(sample.constData(), static_cast<std::size_t>(sample.size())), memory));
    QCOMPARE(memory.totalKiB, expectedTotal);
    QCOMPARE(memory.availableKiB, expectedAvailable);
}

void TestTelemetry::convertsMemoryUnits() {
    QCOMPARE(SystemTelemetryService::kibibytesToGigabytes(16384000), 15.625f);
    QCOMPARE(SystemTelemetryService::kibibytesToGigabytes(8192000), 7.8125f);
}

void TestTelemetry::rejectsIncompleteMemorySamples() {
    SystemTelemetryService::MemoryCounters memory;
    QVERIFY(!SystemTelemetryService::parseMemInfo("MemFree: 0 kB\n", memory));
    QVERIFY(!SystemTelemetryService::parseMemInfo("MemTotal: 0 kB\nMemAvailable: 0 kB\n", memory));
}

void TestTelemetry::readsLiveMemoryFromProc() {
    SystemTelemetryService telemetry;

    QVERIFY(telemetry.ramTotalGb() > 0.0f);
    QVERIFY(telemetry.ramUsageGb() >= 0.0f);
    QVERIFY(telemetry.ramUsageGb() <= telemetry.ramTotalGb());
}

void TestTelemetry::formatsSystemClock() {
    const QDateTime fixedTime(QDate(2025, 2, 3), QTime(4, 5, 6), QTimeZone::UTC);
    QCOMPARE(SystemTelemetryService::formatSystemTime(fixedTime),
             QStringLiteral("MON 03 FEB > 04:05:06"));
}

void TestTelemetry::countsWindowFrameSwaps() {
    QQuickWindow window;
    SystemTelemetryService telemetry;
    telemetry.attachWindow(&window);

    for (int frame = 0; frame < 48; ++frame)
        QVERIFY(QMetaObject::invokeMethod(&window, "frameSwapped", Qt::DirectConnection));

    QTRY_VERIFY_WITH_TIMEOUT(telemetry.fps() >= 40 && telemetry.fps() <= 60, 1500);
}

QTEST_MAIN(TestTelemetry)
#include "tst_Telemetry.moc"
