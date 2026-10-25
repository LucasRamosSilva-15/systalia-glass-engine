#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"

class TestAppRegistry final : public QObject {
    Q_OBJECT

private slots:
    void parsesDesktopEntryAndFieldCodes();
    void filtersModelAndEmitsNotifications();
};

void TestAppRegistry::parsesDesktopEntryAndFieldCodes() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QString desktopFile = directory.filePath(QStringLiteral("systalia-terminal.desktop"));
    QFile file(desktopFile);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray desktopContents =
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Ocean Terminal\n"
        "Exec=/usr/bin/terminal --title=%c %U\n"
        "Icon=utilities-terminal\n"
        "Categories=System;TerminalEmulator;\n";
    QCOMPARE(file.write(desktopContents), desktopContents.size());
    file.close();

    const auto parsed = AppRegistryService::parseDesktopFile(desktopFile);
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->name, QStringLiteral("Ocean Terminal"));
    QCOMPARE(parsed->exec, QStringLiteral("/usr/bin/terminal --title=%c %U"));
    QCOMPARE(parsed->command, QStringLiteral("/usr/bin/terminal"));
    QCOMPARE(parsed->arguments, QStringList{QStringLiteral("--title=Ocean Terminal")});
    QCOMPARE(parsed->icon, QStringLiteral("utilities-terminal"));
    QCOMPARE(parsed->categories,
             QStringList({QStringLiteral("System"), QStringLiteral("TerminalEmulator")}));

    const QString hiddenFile = directory.filePath(QStringLiteral("hidden.desktop"));
    QFile hidden(hiddenFile);
    QVERIFY(hidden.open(QIODevice::WriteOnly | QIODevice::Text));
    const QByteArray hiddenContents =
        "[Desktop Entry]\nType=Application\nName=Hidden\nExec=hidden\nNoDisplay=true\n";
    QCOMPARE(hidden.write(hiddenContents), hiddenContents.size());
    hidden.close();
    QVERIFY(!AppRegistryService::parseDesktopFile(hiddenFile).has_value());
}

void TestAppRegistry::filtersModelAndEmitsNotifications() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    const QStringList desktopFiles{
        directory.filePath(QStringLiteral("terminal.desktop")),
        directory.filePath(QStringLiteral("browser.desktop"))
    };
    const QList<QByteArray> desktopContents{
        "[Desktop Entry]\nType=Application\nName=Ocean Terminal\nExec=terminal\nCategories=System;\n",
        "[Desktop Entry]\nType=Application\nName=Blue Browser\nExec=browser\nCategories=Network;WebBrowser;\n"
    };
    for (qsizetype index = 0; index < desktopFiles.size(); ++index) {
        QFile file(desktopFiles.at(index));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QCOMPARE(file.write(desktopContents.at(index)), desktopContents.at(index).size());
    }

    AppRegistryService service({directory.path()});
    QSignalSpy modelReset(&service, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&service, &AppRegistryService::filterTextChanged);
    QVERIFY(modelReset.isValid());
    QVERIFY(filterChanged.isValid());
    QCOMPARE(service.rowCount(), 2);
    QCOMPARE(service.applicationCount(), 2);

    service.setFilterText(QStringLiteral("NETWORK"));
    QCOMPARE(service.filterText(), QStringLiteral("NETWORK"));
    QCOMPARE(service.rowCount(), 1);
    QCOMPARE(service.data(service.index(0, 0), AppRegistryService::NameRole).toString(),
             QStringLiteral("Blue Browser"));
    QCOMPARE(modelReset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);

    service.setFilterText(QString());
    QCOMPARE(service.rowCount(), 2);
    QCOMPARE(modelReset.count(), 2);
}

QTEST_GUILESS_MAIN(TestAppRegistry)
#include "tst_AppRegistry.moc"
