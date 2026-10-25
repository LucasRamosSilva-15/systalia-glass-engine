#include <QTest>
#include <QQuickWindow>
#include <QTimeZone>
#include <cmath>
#include <string_view>
#include "SystemTelemetryService.hpp"

class TestTelemetry final : public QObject {
    Q_OBJECT

private slots:
    void parsesCpuCountersAndComputesLoad();
    void parsesMemoryCounters();
    void readsLiveMemoryFromProc();
    void formatsSystemClock();
    void countsWindowFrameSwaps();
};

void TestTelemetry::parsesCpuCountersAndComputesLoad() {
    SystemTelemetryService::CpuCounters previous;
    SystemTelemetryService::CpuCounters current;

    QVERIFY(SystemTelemetryService::parseCpuStat(
        "cpu 100 20 30 400 50 5 10 2 8 1\n", previous));
    QVERIFY(SystemTelemetryService::parseCpuStat(
        "cpu 110 25 35 410 55 6 11 3 9 1\n", current));
    QCOMPARE(previous.total, quint64(617));
    QCOMPARE(previous.idle, quint64(450));
    QCOMPARE(current.total, quint64(655));
    QCOMPARE(current.idle, quint64(465));

    const float usage = SystemTelemetryService::calculateCpuUsage(previous, current);
    QVERIFY(std::abs(usage - 60.526315f) < 0.001f);
    QVERIFY(!SystemTelemetryService::parseCpuStat("cpu0 1 2 3 4\n", current));
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(current, previous), 0.0f);
}

void TestTelemetry::parsesMemoryCounters() {
    constexpr std::string_view sample =
        "MemTotal:       16384000 kB\n"
        "MemFree:         2048000 kB\n"
        "MemAvailable:    8192000 kB\n"
        "Buffers:          256000 kB\n";
    SystemTelemetryService::MemoryCounters memory;

    QVERIFY(SystemTelemetryService::parseMemInfo(sample, memory));
    QCOMPARE(memory.totalKiB, quint64(16384000));
    QCOMPARE(memory.availableKiB, quint64(8192000));
    QCOMPARE(SystemTelemetryService::kibibytesToGigabytes(memory.totalKiB), 15.625f);
    QCOMPARE(SystemTelemetryService::kibibytesToGigabytes(
                 memory.totalKiB - memory.availableKiB), 7.8125f);
    QVERIFY(!SystemTelemetryService::parseMemInfo("MemFree: 0 kB\n", memory));
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
