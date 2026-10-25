#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"

namespace {

bool writeEntry(const QString &path, const QByteArray &contents) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    return file.write(contents) == contents.size();
}

} // namespace

class TestAppRegistry final : public QObject {
    Q_OBJECT

private slots:
    void parsesDesktopEntryMetadata();
    void rejectsHiddenDesktopEntry();
    void filtersModelForQuery_data();
    void filtersModelForQuery();
    void filterEmitsModelAndPropertyNotifications();
    void unchangedNormalizedFilterDoesNotEmitAgain();
};

void TestAppRegistry::parsesDesktopEntryMetadata() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("systalia-terminal.desktop"));
    QVERIFY(writeEntry(path,
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Ocean Terminal\n"
        "Exec=/usr/bin/terminal --title=%c %U\n"
        "Icon=utilities-terminal\n"
        "Categories=System;TerminalEmulator;\n"));

    const auto parsed = AppRegistryService::parseDesktopFile(path);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->name, QStringLiteral("Ocean Terminal"));
    QCOMPARE(parsed->exec, QStringLiteral("/usr/bin/terminal --title=%c %U"));
    QCOMPARE(parsed->command, QStringLiteral("/usr/bin/terminal"));
    QCOMPARE(parsed->arguments, QStringList{QStringLiteral("--title=Ocean Terminal")});
    QCOMPARE(parsed->icon, QStringLiteral("utilities-terminal"));
    QCOMPARE(parsed->categories,
             QStringList({QStringLiteral("System"), QStringLiteral("TerminalEmulator")}));
}

void TestAppRegistry::rejectsHiddenDesktopEntry() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("hidden.desktop"));
    QVERIFY(writeEntry(path,
        "[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nNoDisplay=true\n"));
    QVERIFY(!AppRegistryService::parseDesktopFile(path).has_value());
}

void TestAppRegistry::filtersModelForQuery_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<QString>("expectedName");
    QTest::newRow("case-insensitive-category")
        << QStringLiteral("NETWORK") << QStringLiteral("Blue Browser");
    QTest::newRow("name")
        << QStringLiteral("terminal") << QStringLiteral("Ocean Terminal");
    QTest::newRow("executable")
        << QStringLiteral("browser-bin") << QStringLiteral("Blue Browser");
}

void TestAppRegistry::filtersModelForQuery() {
    QFETCH(QString, query);
    QFETCH(QString, expectedName);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeEntry(directory.filePath(QStringLiteral("terminal.desktop")),
        "[Desktop Entry]\nType=Application\nName=Ocean Terminal\nExec=terminal-bin\nCategories=System;\n"));
    QVERIFY(writeEntry(directory.filePath(QStringLiteral("browser.desktop")),
        "[Desktop Entry]\nType=Application\nName=Blue Browser\nExec=browser-bin\nCategories=Network;WebBrowser;\n"));

    AppRegistryService service({directory.path()});
    QCOMPARE(service.rowCount(), 2);
    service.setFilterText(query);
    QCOMPARE(service.rowCount(), 1);
    QCOMPARE(service.data(service.index(0, 0), AppRegistryService::NameRole).toString(),
             expectedName);
}

void TestAppRegistry::filterEmitsModelAndPropertyNotifications() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(writeEntry(directory.filePath(QStringLiteral("terminal.desktop")),
        "[Desktop Entry]\nType=Application\nName=Ocean Terminal\nExec=terminal-bin\n"));
    QVERIFY(writeEntry(directory.filePath(QStringLiteral("browser.desktop")),
        "[Desktop Entry]\nType=Application\nName=Blue Browser\nExec=browser-bin\n"));
    AppRegistryService service({directory.path()});
    QSignalSpy modelReset(&service, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&service, &AppRegistryService::filterTextChanged);
    QSignalSpy countChanged(&service, &AppRegistryService::applicationCountChanged);
    QVERIFY(modelReset.isValid());
    QVERIFY(filterChanged.isValid());
    QVERIFY(countChanged.isValid());

    service.setFilterText(QStringLiteral("terminal"));
    QCOMPARE(service.rowCount(), 1);
    QCOMPARE(modelReset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);
    QCOMPARE(countChanged.count(), 1);
}

void TestAppRegistry::unchangedNormalizedFilterDoesNotEmitAgain() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    AppRegistryService service({directory.path()});
    QSignalSpy modelReset(&service, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&service, &AppRegistryService::filterTextChanged);
    QVERIFY(modelReset.isValid());
    QVERIFY(filterChanged.isValid());
    service.setFilterText(QStringLiteral("  network  "));
    QCOMPARE(service.filterText(), QStringLiteral("network"));
    QCOMPARE(modelReset.count(), 1);
    service.setFilterText(QStringLiteral("network"));
    QCOMPARE(modelReset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);
}

QTEST_GUILESS_MAIN(TestAppRegistry)
#include "tst_AppRegistry.moc"
