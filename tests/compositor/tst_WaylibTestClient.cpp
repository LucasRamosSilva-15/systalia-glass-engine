#include <QGuiApplication>
#include <QRasterWindow>
#include <QTimer>

class SolidWindow final : public QRasterWindow {
public:
    void paintEvent(QPaintEvent *) override {}
};

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    SolidWindow win;
    win.setTitle(QStringLiteral("SGE Waylib Test Client"));
    win.resize(320, 220);
    win.show();

    const int lifeMs = qEnvironmentVariableIntValue("SGE_TEST_CLIENT_MS") > 0
                           ? qEnvironmentVariableIntValue("SGE_TEST_CLIENT_MS")
                           : 1200;
    QTimer::singleShot(lifeMs, &app, &QCoreApplication::quit);
    return app.exec();
}
