#include <QEvent>
#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>

class SolidWindow final : public QRasterWindow {
public:
    void paintEvent(QPaintEvent *) override {}
    bool event(QEvent *e) override {
        if (e->type() == QEvent::Close) {
            QCoreApplication::quit();
        }
        return QRasterWindow::event(e);
    }
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);

    const QString appId = qEnvironmentVariable("SGE_TEST_CLIENT_APP_ID");
    if (!appId.isEmpty()) {
        app.setDesktopFileName(appId);
    } else {
        app.setDesktopFileName(QStringLiteral("sge-test-client"));
    }

    SolidWindow win;
    const QString title = qEnvironmentVariable("SGE_TEST_CLIENT_TITLE");
    win.setTitle(title.isEmpty() ? QStringLiteral("SGE Waylib Test Client") : title);
    win.resize(320, 220);
    win.show();

    if (qEnvironmentVariableIsSet("SGE_TEST_CLIENT_MS")) {
        const int lifeMs = qEnvironmentVariableIntValue("SGE_TEST_CLIENT_MS");
        if (lifeMs > 0) {
            QTimer::singleShot(lifeMs, &app, &QCoreApplication::quit);
        }
    } else {
        QTimer::singleShot(1200, &app, &QCoreApplication::quit);
    }
    return app.exec();
}
