#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"

class TestAppModelFiltering final : public QObject {
    Q_OBJECT

private slots:
    void filtersTenDesktopEntriesAndNotifiesViews();
};

void TestAppModelFiltering::filtersTenDesktopEntriesAndNotifiesViews() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    for (int index = 0; index < 10; ++index) {
        const QString path = directory.filePath(QStringLiteral("app-%1.desktop").arg(index));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        const bool isTerminal = index == 2 || index == 7;
        const QByteArray contents = QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=%1\nExec=%2\nCategories=%3;\n")
            .arg(isTerminal ? QStringLiteral("Glass Terminal %1").arg(index)
                            : QStringLiteral("Ocean App %1").arg(index),
                 QStringLiteral("app-%1").arg(index),
                 isTerminal ? QStringLiteral("TerminalEmulator") : QStringLiteral("Utility"))
            .toUtf8();
        QCOMPARE(file.write(contents), contents.size());
    }

    AppRegistryService model({directory.path()});
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&model, &AppRegistryService::filterTextChanged);
    QSignalSpy countChanged(&model, &AppRegistryService::applicationCountChanged);
    QVERIFY(reset.isValid());
    QVERIFY(filterChanged.isValid());
    QVERIFY(countChanged.isValid());
    QCOMPARE(model.rowCount(), 10);
    QCOMPARE(model.applicationCount(), 10);

    model.setFilterText(QStringLiteral("term"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), AppRegistryService::NameRole).toString(),
             QStringLiteral("Glass Terminal 2"));
    QCOMPARE(model.data(model.index(1, 0), AppRegistryService::NameRole).toString(),
             QStringLiteral("Glass Terminal 7"));
    QCOMPARE(reset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);
    QCOMPARE(countChanged.count(), 1);

    model.setFilterText(QStringLiteral("APP-5"));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), AppRegistryService::ExecRole).toString(),
             QStringLiteral("app-5"));
    QCOMPARE(reset.count(), 2);
    QCOMPARE(filterChanged.count(), 2);

    model.setFilterText(QStringLiteral("   APP-5   "));
    QCOMPARE(model.filterText(), QStringLiteral("APP-5"));
    QCOMPARE(reset.count(), 2);
    QCOMPARE(filterChanged.count(), 2);

    model.setFilterText(QString());
    QCOMPARE(model.rowCount(), 10);
    QCOMPARE(reset.count(), 3);
    QCOMPARE(filterChanged.count(), 3);
}

QTEST_GUILESS_MAIN(TestAppModelFiltering)
#include "tst_AppModelFiltering.moc"
