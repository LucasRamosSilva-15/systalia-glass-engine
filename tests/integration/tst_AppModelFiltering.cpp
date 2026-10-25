#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "AppRegistryService.hpp"

namespace {

bool createTenEntries(const QString &directoryPath) {
    for (int index = 0; index < 10; ++index) {
        const QString path = QDir(directoryPath).filePath(
            QStringLiteral("app-%1.desktop").arg(index));
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
            return false;
        const bool terminal = index == 2 || index == 7;
        const QByteArray contents = QStringLiteral(
            "[Desktop Entry]\nType=Application\nName=%1\nExec=app-%2\nCategories=%3;\n")
            .arg(terminal ? QStringLiteral("Glass Terminal %1").arg(index)
                          : QStringLiteral("Ocean App %1").arg(index),
                 QString::number(index),
                 terminal ? QStringLiteral("TerminalEmulator") : QStringLiteral("Utility"))
            .toUtf8();
        if (file.write(contents) != contents.size())
            return false;
    }
    return true;
}

} // namespace

class TestAppModelFiltering final : public QObject {
    Q_OBJECT

private slots:
    void filtersTenEntries_data();
    void filtersTenEntries();
    void filterChangeResetsModelAndNotifiesViews();
    void settingSameFilterIsNoop();
};

void TestAppModelFiltering::filtersTenEntries_data() {
    QTest::addColumn<QString>("query");
    QTest::addColumn<int>("expectedRows");
    QTest::addColumn<QStringList>("expectedNames");
    QTest::newRow("terminal-name")
        << QStringLiteral("term") << 2
        << QStringList({QStringLiteral("Glass Terminal 2"), QStringLiteral("Glass Terminal 7")});
    QTest::newRow("executable-name")
        << QStringLiteral("APP-5") << 1 << QStringList({QStringLiteral("Ocean App 5")});
    QTest::newRow("empty-query") << QString() << 10 << QStringList{};
}

void TestAppModelFiltering::filtersTenEntries() {
    QFETCH(QString, query);
    QFETCH(int, expectedRows);
    QFETCH(QStringList, expectedNames);
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createTenEntries(directory.path()));
    AppRegistryService model({directory.path()});
    QCOMPARE(model.rowCount(), 10);

    model.setFilterText(query);
    QCOMPARE(model.rowCount(), expectedRows);
    if (!expectedNames.isEmpty()) {
        QStringList actualNames;
        for (int row = 0; row < model.rowCount(); ++row) {
            actualNames.append(model.data(model.index(row, 0), AppRegistryService::NameRole).toString());
        }
        QCOMPARE(actualNames, expectedNames);
    }
}

void TestAppModelFiltering::filterChangeResetsModelAndNotifiesViews() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createTenEntries(directory.path()));
    AppRegistryService model({directory.path()});
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&model, &AppRegistryService::filterTextChanged);
    QSignalSpy countChanged(&model, &AppRegistryService::applicationCountChanged);
    QVERIFY(reset.isValid());
    QVERIFY(filterChanged.isValid());
    QVERIFY(countChanged.isValid());

    model.setFilterText(QStringLiteral("term"));
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(reset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);
    QCOMPARE(countChanged.count(), 1);
}

void TestAppModelFiltering::settingSameFilterIsNoop() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QVERIFY(createTenEntries(directory.path()));
    AppRegistryService model({directory.path()});
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy filterChanged(&model, &AppRegistryService::filterTextChanged);
    QVERIFY(reset.isValid());
    QVERIFY(filterChanged.isValid());
    model.setFilterText(QStringLiteral(" term "));
    QCOMPARE(model.filterText(), QStringLiteral("term"));
    model.setFilterText(QStringLiteral("term"));
    QCOMPARE(reset.count(), 1);
    QCOMPARE(filterChanged.count(), 1);
}

QTEST_GUILESS_MAIN(TestAppModelFiltering)
#include "tst_AppModelFiltering.moc"
