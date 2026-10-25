#include <QCoreApplication>
#include <QKeyEvent>
#include <QQuickWindow>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <memory>
#include <utility>

#include "SystaliaCompositor.hpp"
#include "WindowStackModel.hpp"

class MockWindowSurface final : public QObject {
    Q_OBJECT

public:
    using QObject::QObject;
};

class MockToplevel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString title READ title CONSTANT)
    Q_PROPERTY(QString appId READ appId CONSTANT)
    Q_PROPERTY(QObject *surface READ surface CONSTANT)

public:
    MockToplevel(QString title, QString appId, QObject *parent = nullptr)
        : QObject(parent), m_title(std::move(title)), m_appId(std::move(appId)),
          m_surface(new MockWindowSurface(this)) {
    }

    QString title() const { return m_title; }
    QString appId() const { return m_appId; }
    QObject *surface() const { return m_surface; }

private:
    QString m_title;
    QString m_appId;
    MockWindowSurface *m_surface = nullptr;
};

class TestTaskSwitcher final : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();
    void registersAndPromotesWindowsInMruOrder();
    void altTabCyclesForwardAndCommitsOnAltRelease();
    void forwardCycleWrapsToMruFront();
    void altShiftTabCyclesInReverse();
    void destroyingWindowRemovesItsModelRow();
    void invalidSelectionLeavesTheStackUnchanged();
    void singleWindowDoesNotOpenTheSwitcher();

private:
    static bool populateThreeWindows(WindowStackModel &model,
                                     MockToplevel &first,
                                     MockToplevel &second,
                                     MockToplevel &third);
    static void sendKey(QQuickWindow &window, QEvent::Type type,
                        Qt::Key key, Qt::KeyboardModifiers modifiers);
    QTemporaryDir m_runtime;
};

void TestTaskSwitcher::initTestCase() {
    QVERIFY(m_runtime.isValid());
    qputenv("XDG_RUNTIME_DIR", m_runtime.path().toUtf8());
}

bool TestTaskSwitcher::populateThreeWindows(WindowStackModel &model,
                                            MockToplevel &first,
                                            MockToplevel &second,
                                            MockToplevel &third) {
    return model.registerWindow(&third)
        && model.registerWindow(&second)
        && model.registerWindow(&first);
}

void TestTaskSwitcher::sendKey(QQuickWindow &window, QEvent::Type type,
                               Qt::Key key, Qt::KeyboardModifiers modifiers) {
    QKeyEvent event(type, key, modifiers);
    event.ignore();
    QCoreApplication::sendEvent(&window, &event);
}

void TestTaskSwitcher::registersAndPromotesWindowsInMruOrder() {
    WindowStackModel model;
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    MockToplevel second(QStringLiteral("Second"), QStringLiteral("second"));
    MockToplevel third(QStringLiteral("Third"), QStringLiteral("third"));

    QVERIFY(model.registerWindow(&first));
    QVERIFY(model.registerWindow(&second));
    QVERIFY(model.registerWindow(&third));
    QCOMPARE(model.windowCount(), 3);
    QCOMPARE(model.windowAt(0), static_cast<QObject *>(&third));
    QVERIFY(model.promoteWindow(&first));
    QCOMPARE(model.currentWindow(), static_cast<QObject *>(&first));
    QCOMPARE(model.data(model.index(0, 0), WindowStackModel::TitleRole).toString(),
             QStringLiteral("First"));
    QCOMPARE(model.data(model.index(0, 0), WindowStackModel::AppIdRole).toString(),
             QStringLiteral("first"));
    QCOMPARE(model.data(model.index(0, 0), WindowStackModel::SurfaceRole).value<QObject *>(),
             first.surface());
}

void TestTaskSwitcher::altTabCyclesForwardAndCommitsOnAltRelease() {
    SystaliaCompositor compositor;
    WindowStackModel *model = compositor.windowStackModel();
    QVERIFY(model);
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    MockToplevel second(QStringLiteral("Second"), QStringLiteral("second"));
    MockToplevel third(QStringLiteral("Third"), QStringLiteral("third"));
    QVERIFY(populateThreeWindows(*model, first, second, third));

    QQuickWindow rootWindow;
    compositor.attachTaskSwitcherWindow(&rootWindow);
    QSignalSpy selected(&compositor, &SystaliaCompositor::windowSelected);
    QVERIFY(selected.isValid());

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    QKeyEvent shortcutOverride(QEvent::ShortcutOverride, Qt::Key_Tab, Qt::AltModifier);
    shortcutOverride.ignore();
    QCoreApplication::sendEvent(&rootWindow, &shortcutOverride);
    QVERIFY(shortcutOverride.isAccepted());

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier);
    QVERIFY(compositor.taskSwitcherVisible());
    QCOMPARE(compositor.taskSwitcherIndex(), 1);
    QCOMPARE(model->selectedWindow(), static_cast<QObject *>(&second));
    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Tab, Qt::AltModifier);

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier);
    QCOMPARE(compositor.taskSwitcherIndex(), 2);
    QCOMPARE(model->selectedWindow(), static_cast<QObject *>(&third));
    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Tab, Qt::AltModifier);

    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    QVERIFY(!compositor.taskSwitcherVisible());
    QCOMPARE(compositor.taskSwitcherIndex(), 0);
    QCOMPARE(model->currentWindow(), static_cast<QObject *>(&third));
    QCOMPARE(selected.count(), 1);
    QCOMPARE(selected.constFirst().constFirst().value<QObject *>(),
             static_cast<QObject *>(&third));
}

void TestTaskSwitcher::forwardCycleWrapsToMruFront() {
    SystaliaCompositor compositor;
    WindowStackModel *model = compositor.windowStackModel();
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    MockToplevel second(QStringLiteral("Second"), QStringLiteral("second"));
    MockToplevel third(QStringLiteral("Third"), QStringLiteral("third"));
    QVERIFY(populateThreeWindows(*model, first, second, third));
    QQuickWindow rootWindow;
    compositor.attachTaskSwitcherWindow(&rootWindow);

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    for (int step = 0; step < 3; ++step)
        sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier);
    QCOMPARE(compositor.taskSwitcherIndex(), 0);
    QCOMPARE(model->selectedWindow(), static_cast<QObject *>(&first));
    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    QCOMPARE(model->currentWindow(), static_cast<QObject *>(&first));
}

void TestTaskSwitcher::altShiftTabCyclesInReverse() {
    SystaliaCompositor compositor;
    WindowStackModel *model = compositor.windowStackModel();
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    MockToplevel second(QStringLiteral("Second"), QStringLiteral("second"));
    MockToplevel third(QStringLiteral("Third"), QStringLiteral("third"));
    QVERIFY(populateThreeWindows(*model, first, second, third));
    QQuickWindow rootWindow;
    compositor.attachTaskSwitcherWindow(&rootWindow);

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Tab,
            Qt::AltModifier | Qt::ShiftModifier);
    QVERIFY(compositor.taskSwitcherVisible());
    QCOMPARE(compositor.taskSwitcherIndex(), 2);
    QCOMPARE(model->selectedWindow(), static_cast<QObject *>(&third));
    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Tab,
            Qt::AltModifier | Qt::ShiftModifier);
    sendKey(rootWindow, QEvent::KeyRelease, Qt::Key_Alt, Qt::NoModifier);
    QCOMPARE(model->currentWindow(), static_cast<QObject *>(&third));
}

void TestTaskSwitcher::destroyingWindowRemovesItsModelRow() {
    WindowStackModel model;
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    auto third = std::make_unique<MockToplevel>(QStringLiteral("Third"), QStringLiteral("third"));
    auto second = std::make_unique<MockToplevel>(QStringLiteral("Second"), QStringLiteral("second"));
    QVERIFY(populateThreeWindows(model, first, *second, *third));

    QVERIFY(model.beginTaskSwitch(1));
    QCOMPARE(model.taskSwitcherIndex(), 1);
    second.reset();
    QCOMPARE(model.windowCount(), 2);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.taskSwitcherIndex(), 1);
    QCOMPARE(model.selectedWindow(), static_cast<QObject *>(third.get()));
}

void TestTaskSwitcher::invalidSelectionLeavesTheStackUnchanged() {
    WindowStackModel model;
    MockToplevel first(QStringLiteral("First"), QStringLiteral("first"));
    MockToplevel second(QStringLiteral("Second"), QStringLiteral("second"));
    QVERIFY(model.registerWindow(&first));
    QVERIFY(model.registerWindow(&second));

    QVERIFY(model.selectAndCommit(-1) == nullptr);
    QVERIFY(model.selectAndCommit(2) == nullptr);
    QCOMPARE(model.currentWindow(), static_cast<QObject *>(&second));
    QCOMPARE(model.windowCount(), 2);
    QVERIFY(!model.taskSwitcherVisible());
}

void TestTaskSwitcher::singleWindowDoesNotOpenTheSwitcher() {
    SystaliaCompositor compositor;
    MockToplevel onlyWindow(QStringLiteral("Only"), QStringLiteral("only"));
    QVERIFY(compositor.windowStackModel()->registerWindow(&onlyWindow));
    QQuickWindow rootWindow;
    compositor.attachTaskSwitcherWindow(&rootWindow);

    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Alt, Qt::NoModifier);
    sendKey(rootWindow, QEvent::KeyPress, Qt::Key_Tab, Qt::AltModifier);
    QVERIFY(!compositor.taskSwitcherVisible());
    QCOMPARE(compositor.taskSwitcherIndex(), 0);
}

QTEST_MAIN(TestTaskSwitcher)
#include "tst_TaskSwitcher.moc"
