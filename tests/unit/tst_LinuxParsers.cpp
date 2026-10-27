#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"
#include "SystemTelemetryService.hpp"

namespace {

std::optional<AppRegistryService::ApplicationEntry> parseExecFixture(const QString &exec) {
    QTemporaryDir directory;
    if (!directory.isValid())
        return std::nullopt;

    QFile file(directory.filePath(QStringLiteral("exec-fixture.desktop")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return std::nullopt;
    const QByteArray contents = QStringLiteral(
        "[Desktop Entry]\nType=Application\nName=Parser Fixture\nExec=%1\n")
        .arg(exec).toUtf8();
    if (file.write(contents) != contents.size())
        return std::nullopt;
    file.close();
    return AppRegistryService::parseDesktopFile(file.fileName());
}

} // namespace

class TestLinuxParsers final : public QObject {
    Q_OBJECT

private slots:
    void parsesAggregateCpuLine_data();
    void parsesAggregateCpuLine();
    void rejectsMalformedCpuLine_data();
    void rejectsMalformedCpuLine();
    void computesCpuUsage_data();
    void computesCpuUsage();
    void handlesCpuCounterReset_data();
    void handlesCpuCounterReset();
    void parsesValidMemoryInfo_data();
    void parsesValidMemoryInfo();
    void rejectsInvalidMemoryInfo_data();
    void rejectsInvalidMemoryInfo();
    void removesExecFileFieldCode_data();
    void removesExecFileFieldCode();
    void expandsExecNameAndPercentCodes();
};

void TestLinuxParsers::parsesAggregateCpuLine_data() {
    QTest::addColumn<QByteArray>("line");
    QTest::addColumn<quint64>("expectedTotal");
    QTest::addColumn<quint64>("expectedIdle");
    QTest::newRow("single-core-snapshot")
        << QByteArray("cpu 100 0 50 850 0 0 0 0\n") << quint64(1000) << quint64(850);
    QTest::newRow("four-core-aggregate")
        << QByteArray("cpu 400 40 200 3360 0 0 0 0\n") << quint64(4000) << quint64(3360);
    QTest::newRow("eight-core-aggregate")
        << QByteArray("cpu 800 80 400 6720 0 0 0 0\n") << quint64(8000) << quint64(6720);
}

void TestLinuxParsers::parsesAggregateCpuLine() {
    QFETCH(QByteArray, line);
    QFETCH(quint64, expectedTotal);
    QFETCH(quint64, expectedIdle);
    SystemTelemetryService::CpuCounters counters;
    QVERIFY(SystemTelemetryService::parseCpuStat(line.toStdString(), counters));
    QCOMPARE(counters.total, expectedTotal);
    QCOMPARE(counters.idle, expectedIdle);
}

void TestLinuxParsers::rejectsMalformedCpuLine_data() {
    QTest::addColumn<QByteArray>("line");
    QTest::newRow("too-few-fields") << QByteArray("cpu 10 20\n");
    QTest::newRow("per-core-label") << QByteArray("cpu0 10 20 30 40\n");
    QTest::newRow("invalid-token") << QByteArray("cpu 10 20 x 40\n");
    QTest::newRow("integer-overflow")
        << QByteArray("cpu 18446744073709551615 1 1 1\n");
}

void TestLinuxParsers::rejectsMalformedCpuLine() {
    QFETCH(QByteArray, line);
    SystemTelemetryService::CpuCounters counters;
    QVERIFY(!SystemTelemetryService::parseCpuStat(line.toStdString(), counters));
}

void TestLinuxParsers::computesCpuUsage_data() {
    QTest::addColumn<quint64>("previousTotal");
    QTest::addColumn<quint64>("previousIdle");
    QTest::addColumn<quint64>("currentTotal");
    QTest::addColumn<quint64>("currentIdle");
    QTest::addColumn<float>("expectedPercent");
    QTest::newRow("single-core-load") << quint64(1000) << quint64(850)
        << quint64(1150) << quint64(925) << 50.0f;
    QTest::newRow("four-core-load") << quint64(4000) << quint64(3360)
        << quint64(4400) << quint64(3600) << 40.0f;
    QTest::newRow("eight-core-load") << quint64(8000) << quint64(6720)
        << quint64(8600) << quint64(7200) << 20.0f;
}

void TestLinuxParsers::computesCpuUsage() {
    QFETCH(quint64, previousTotal);
    QFETCH(quint64, previousIdle);
    QFETCH(quint64, currentTotal);
    QFETCH(quint64, currentIdle);
    QFETCH(float, expectedPercent);
    const SystemTelemetryService::CpuCounters previous{previousTotal, previousIdle};
    const SystemTelemetryService::CpuCounters current{currentTotal, currentIdle};
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(previous, current), expectedPercent);
}

void TestLinuxParsers::handlesCpuCounterReset_data() {
    QTest::addColumn<quint64>("previousTotal");
    QTest::addColumn<quint64>("previousIdle");
    QTest::addColumn<quint64>("currentTotal");
    QTest::addColumn<quint64>("currentIdle");
    QTest::newRow("counter-rollback") << quint64(500) << quint64(300) << quint64(400) << quint64(250);
    QTest::newRow("no-progress") << quint64(500) << quint64(300) << quint64(500) << quint64(300);
    QTest::newRow("idle-delta-exceeds-total") << quint64(500) << quint64(300) << quint64(510) << quint64(320);
}

void TestLinuxParsers::handlesCpuCounterReset() {
    QFETCH(quint64, previousTotal);
    QFETCH(quint64, previousIdle);
    QFETCH(quint64, currentTotal);
    QFETCH(quint64, currentIdle);
    const SystemTelemetryService::CpuCounters previous{previousTotal, previousIdle};
    const SystemTelemetryService::CpuCounters current{currentTotal, currentIdle};
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(previous, current), 0.0f);
}

void TestLinuxParsers::parsesValidMemoryInfo_data() {
    QTest::addColumn<QByteArray>("contents");
    QTest::addColumn<quint64>("expectedTotal");
    QTest::addColumn<quint64>("expectedAvailable");
    QTest::newRow("out-of-order-fields")
        << QByteArray("Cached: 100 kB\nMemAvailable: 700 kB\nMemFree: 500 kB\nMemTotal: 1000 kB\n")
        << quint64(1000) << quint64(700);
    QTest::newRow("free-fallback")
        << QByteArray("MemFree: 300 kB\nMemTotal: 1200 kB\n")
        << quint64(1200) << quint64(300);
    QTest::newRow("available-clamped-to-total")
        << QByteArray("MemAvailable: 2000 kB\nMemTotal: 1000 kB\n")
        << quint64(1000) << quint64(1000);
}

void TestLinuxParsers::parsesValidMemoryInfo() {
    QFETCH(QByteArray, contents);
    QFETCH(quint64, expectedTotal);
    QFETCH(quint64, expectedAvailable);
    SystemTelemetryService::MemoryCounters counters;
    QVERIFY(SystemTelemetryService::parseMemInfo(contents.toStdString(), counters));
    QCOMPARE(counters.totalKiB, expectedTotal);
    QCOMPARE(counters.availableKiB, expectedAvailable);
}

void TestLinuxParsers::rejectsInvalidMemoryInfo_data() {
    QTest::addColumn<QByteArray>("contents");
    QTest::newRow("truncated-available") << QByteArray("MemTotal: 1000 kB\nMemAvailable:");
    QTest::newRow("missing-total") << QByteArray("MemAvailable: 500 kB\n");
    QTest::newRow("zero-total") << QByteArray("MemTotal: 0 kB\nMemAvailable: 0 kB\n");
    QTest::newRow("malformed-number")
        << QByteArray("MemTotal: one kB\nMemAvailable: 500 kB\n");
}

void TestLinuxParsers::rejectsInvalidMemoryInfo() {
    QFETCH(QByteArray, contents);
    SystemTelemetryService::MemoryCounters counters;
    QVERIFY(!SystemTelemetryService::parseMemInfo(contents.toStdString(), counters));
}

void TestLinuxParsers::removesExecFileFieldCode_data() {
    QTest::addColumn<QString>("fieldCode");
    QTest::newRow("file") << QStringLiteral("%f");
    QTest::newRow("files") << QStringLiteral("%F");
    QTest::newRow("url") << QStringLiteral("%u");
    QTest::newRow("urls") << QStringLiteral("%U");
}

void TestLinuxParsers::removesExecFileFieldCode() {
    QFETCH(QString, fieldCode);
    const auto parsed = parseExecFixture(QStringLiteral("/usr/bin/parser %1").arg(fieldCode));
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->command, QStringLiteral("/usr/bin/parser"));
    QVERIFY(parsed->arguments.isEmpty());
}

void TestLinuxParsers::expandsExecNameAndPercentCodes() {
    const auto parsed = parseExecFixture(
        QStringLiteral("/usr/bin/parser --name %c --literal %%"));
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->arguments,
             QStringList({QStringLiteral("--name"), QStringLiteral("Parser Fixture"),
                          QStringLiteral("--literal"), QStringLiteral("%") }));
}

QTEST_GUILESS_MAIN(TestLinuxParsers)
#include "tst_LinuxParsers.moc"
