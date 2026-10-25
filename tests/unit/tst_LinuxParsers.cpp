#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"
#include "SystemTelemetryService.hpp"

class TestLinuxParsers final : public QObject {
    Q_OBJECT

private slots:
    void cpuCounters_data();
    void cpuCounters();
    void memoryInfo_data();
    void memoryInfo();
    void execFieldCodesAreRemoved();
};

void TestLinuxParsers::cpuCounters_data() {
    QTest::addColumn<QByteArray>("previousLine");
    QTest::addColumn<QByteArray>("currentLine");
    QTest::addColumn<float>("expectedPercent");

    QTest::newRow("single-core-snapshot")
        << QByteArray("cpu 100 0 50 850 0 0 0 0\n")
        << QByteArray("cpu 150 0 75 925 0 0 0 0\n") << 50.0f;
    QTest::newRow("four-core-aggregate")
        << QByteArray("cpu 400 40 200 3360 0 0 0 0\n")
        << QByteArray("cpu 520 40 240 3600 0 0 0 0\n") << 40.0f;
    QTest::newRow("eight-core-aggregate")
        << QByteArray("cpu 800 80 400 6720 0 0 0 0\n")
        << QByteArray("cpu 880 80 440 7200 0 0 0 0\n") << 20.0f;
}

void TestLinuxParsers::cpuCounters() {
    QFETCH(QByteArray, previousLine);
    QFETCH(QByteArray, currentLine);
    QFETCH(float, expectedPercent);

    SystemTelemetryService::CpuCounters previous;
    SystemTelemetryService::CpuCounters current;
    QVERIFY(SystemTelemetryService::parseCpuStat(previousLine.toStdString(), previous));
    QVERIFY(SystemTelemetryService::parseCpuStat(currentLine.toStdString(), current));
    QCOMPARE(SystemTelemetryService::calculateCpuUsage(previous, current), expectedPercent);

    SystemTelemetryService::CpuCounters invalid;
    QVERIFY(!SystemTelemetryService::parseCpuStat("cpu 10 20", invalid));
    QVERIFY(!SystemTelemetryService::parseCpuStat("cpu0 10 20 30 40", invalid));
}

void TestLinuxParsers::memoryInfo_data() {
    QTest::addColumn<QByteArray>("contents");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<quint64>("totalKiB");
    QTest::addColumn<quint64>("availableKiB");

    QTest::newRow("out-of-order")
        << QByteArray("Cached: 100 kB\nMemAvailable: 700 kB\nMemFree: 500 kB\nMemTotal: 1000 kB\n")
        << true << quint64(1000) << quint64(700);
    QTest::newRow("fallback-to-free")
        << QByteArray("MemFree: 300 kB\nMemTotal: 1200 kB\n")
        << true << quint64(1200) << quint64(300);
    QTest::newRow("clamped-available")
        << QByteArray("MemAvailable: 2000 kB\nMemTotal: 1000 kB\n")
        << true << quint64(1000) << quint64(1000);
    QTest::newRow("truncated")
        << QByteArray("MemTotal: 1000 kB\nMemAvailable:")
        << false << quint64(0) << quint64(0);
    QTest::newRow("missing-total")
        << QByteArray("MemAvailable: 500 kB\n")
        << false << quint64(0) << quint64(0);
}

void TestLinuxParsers::memoryInfo() {
    QFETCH(QByteArray, contents);
    QFETCH(bool, valid);
    QFETCH(quint64, totalKiB);
    QFETCH(quint64, availableKiB);

    SystemTelemetryService::MemoryCounters counters;
    QCOMPARE(SystemTelemetryService::parseMemInfo(contents.toStdString(), counters), valid);
    if (valid) {
        QCOMPARE(counters.totalKiB, totalKiB);
        QCOMPARE(counters.availableKiB, availableKiB);
    }
}

void TestLinuxParsers::execFieldCodesAreRemoved() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("exec-fields.desktop"));
    QFile desktopFile(path);
    QVERIFY(desktopFile.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray content =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Parser Fixture\n"
        "Exec=/usr/bin/parser --file %f --files %F --url %u --urls %U --name %c --percent %%\n";
    QCOMPARE(desktopFile.write(content), content.size());
    desktopFile.close();

    const auto parsed = AppRegistryService::parseDesktopFile(path);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->command, QStringLiteral("/usr/bin/parser"));
    QCOMPARE(parsed->arguments,
             QStringList({QStringLiteral("--file"), QStringLiteral("--files"),
                          QStringLiteral("--url"), QStringLiteral("--urls"),
                          QStringLiteral("--name"), QStringLiteral("Parser Fixture"),
                          QStringLiteral("--percent"), QStringLiteral("%") }));
}

QTEST_GUILESS_MAIN(TestLinuxParsers)
#include "tst_LinuxParsers.moc"
