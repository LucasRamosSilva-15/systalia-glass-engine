#include <QGuiApplication>
#include <QJSEngine>
#include <QQmlApplicationEngine>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QQuickGraphicsConfiguration>
#include <QSocketNotifier>
#include <QUrl>
#include <QDebug>
#include <csignal>
#include <memory>
#include <sys/socket.h>
#include <unistd.h>
#include "SystaliaCompositor.hpp"
#include "SystemTelemetryService.hpp"
#include "ProcessLauncherService.hpp"
#include "AppRegistryService.hpp"
#include "NotificationServer.hpp"
#include "SystemControlService.hpp"
#include "WorkspaceManagerService.hpp"
#include "LiquidTilingService.hpp"
#include "OutputManagerService.hpp"
#include "InputDeviceManager.hpp"
#include "LogindSessionService.hpp"
#include "AtomicKmsPresenter.hpp"
#include "DrmBackendController.hpp"
#include "LogindDrmProvider.hpp"
#include "VulkanQuickRenderer.hpp"
#include "diagnostics/ValoStabilityProbe.hpp"

void qml_register_types_Systalia_Engine();

namespace {
int g_signalFd[2] = {-1, -1};

void handleUnixTerminationSignal(int) {
    const char byte = 1;
    if (g_signalFd[0] >= 0) {
        const ssize_t ignored = ::write(g_signalFd[0], &byte, sizeof(byte));
        Q_UNUSED(ignored);
    }
}
} // namespace

int main(int argc, char *argv[]) {
    // Deteção dinâmica de sessão (Bare-Metal DRM/KMS via TTY vs. Aninhada Wayland/X11 vs. Headless)
    // Configura QT_QPA_PLATFORM=eglfs, QT_QPA_EGLFS_INTEGRATION=eglfs_kms e QSG_RHI_BACKEND=vulkan
    // antes da instanciação de QGuiApplication quando nenhuma sessão hospedeira está ativa.
    const auto platformCfg = InputDeviceManager::configureProcessPlatformEnvironment(
        QStringLiteral("config/eglfs_kms.json"));
    QQuickWindow::setGraphicsApi(QSGRendererInterface::Vulkan);

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("SystaliaGlassEngine"));
    app.setOrganizationName(QStringLiteral("Systalia"));

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, g_signalFd) == 0) {
        auto *signalNotifier = new QSocketNotifier(g_signalFd[1], QSocketNotifier::Read, &app);
        QObject::connect(signalNotifier, &QSocketNotifier::activated, &app, [signalNotifier]() {
            signalNotifier->setEnabled(false);
            char tmp = 0;
            const ssize_t ignored = ::read(g_signalFd[1], &tmp, sizeof(tmp));
            Q_UNUSED(ignored);
            QCoreApplication::quit();
        });
        struct sigaction sa {};
        sa.sa_handler = handleUnixTerminationSignal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT, &sa, nullptr);
    }

    NotificationServer notificationServer;
    SystemControlService systemControlService;
    WorkspaceManagerService workspaceManagerService;
    LiquidTilingService liquidTilingService;
    OutputManagerService outputManagerService;
    LogindSessionService logindSessionService;
    InputDeviceManager inputDeviceManager;

    std::unique_ptr<Systalia::Platform::Drm::LogindDrmProvider> drmProvider;
    std::unique_ptr<Systalia::Platform::Drm::LibdrmResourceBackend> kmsBackend;
    std::unique_ptr<Systalia::Platform::Drm::LibgbmBackend> gbmBackend;
    std::unique_ptr<Systalia::Platform::Drm::LibdrmAtomicKmsBackend> atomicBackend;
    std::unique_ptr<Systalia::Platform::Drm::AtomicKmsPresenter> atomicPresenter;
    std::unique_ptr<Systalia::Platform::Drm::DrmBackendController> drmController;

    if (platformCfg.mode == InputDeviceManager::SessionBareMetalKms
        && platformCfg.useExperimentalDrmBackend) {
        qInfo() << "[SGE-DRM-EXPERIMENTAL] Inicializando backend DRM/KMS/Vulkan independente (QPA="
                << platformCfg.qtQpaPlatform << ")";
        if (!logindSessionService.initializeSession(false)) {
            qCritical() << "[SGE-DRM-EXPERIMENTAL] Falha ao assumir controle via systemd-logind; abortando.";
            return 1;
        }
        drmProvider = std::make_unique<Systalia::Platform::Drm::LogindDrmProvider>(logindSessionService);
        kmsBackend = std::make_unique<Systalia::Platform::Drm::LibdrmResourceBackend>();
        gbmBackend = std::make_unique<Systalia::Platform::Drm::LibgbmBackend>();
        atomicBackend = std::make_unique<Systalia::Platform::Drm::LibdrmAtomicKmsBackend>();
        atomicPresenter = std::make_unique<Systalia::Platform::Drm::AtomicKmsPresenter>(
            *kmsBackend, *gbmBackend, *atomicBackend);
        drmController = std::make_unique<Systalia::Platform::Drm::DrmBackendController>(
            *drmProvider, *atomicPresenter);
        drmProvider->bindController(drmController.get());

        const Systalia::Platform::Drm::DrmOutputMode initialMode{
            QStringLiteral("DP-1"), 1920, 1080, 60, true};
        if (!drmController->initialize(inputDeviceManager.activeDrmDevice(), initialMode)) {
            qCritical() << "[SGE-DRM-EXPERIMENTAL] Falha ao adquirir/inicializar DRM único:"
                        << inputDeviceManager.activeDrmDevice();
            drmController->shutdown();
            logindSessionService.releaseControl();
            return 1;
        }
        if (!inputDeviceManager.initializeHardwareContext()) {
            qCritical() << "[SGE-DRM-EXPERIMENTAL] Falha ao inicializar libinput; executando rollback DRM/logind.";
            drmController->shutdown();
            logindSessionService.releaseControl();
            return 1;
        }
    } else if (platformCfg.mode == InputDeviceManager::SessionBareMetalKms) {
        const bool allowNoLogind = qEnvironmentVariableIsSet("SGE_ALLOW_NO_LOGIND");
        const bool hasLogind = logindSessionService.initializeSession(false);
        if (hasLogind) {
            if (logindSessionService.acquirePrimaryDrmDevice(inputDeviceManager.activeDrmDevice()) < 0) {
                qCritical() << "[SGE-LOGIND] Falha ao adquirir dispositivo DRM primário:"
                            << inputDeviceManager.activeDrmDevice();
                if (!allowNoLogind) {
                    logindSessionService.releaseControl();
                    return 1;
                }
            }
        } else if (LogindSessionService::shouldAbortOnLogindFailure(
                       true,
                       static_cast<quint32>(::geteuid()),
                       allowNoLogind)) {
            qCritical() << "[SGE-LOGIND] Sessão bare-metal abortada: falha ao assumir controle via systemd-logind"
                        << "(defina SGE_ALLOW_NO_LOGIND=1 apenas para depuração controlada).";
            return 1;
        } else {
            qWarning() << "[SGE-LOGIND] Sessão logind não inicializada; prosseguindo por override explícito ou UID 0.";
        }
        if (!inputDeviceManager.initializeHardwareContext()) {
            qCritical() << "[SGE-INPUT] Backend de hardware libinput/udev não inicializado em bare-metal.";
            if (!qEnvironmentVariableIsSet("SGE_ALLOW_NO_INPUT") && !allowNoLogind) {
                logindSessionService.releaseControl();
                return 1;
            }
        }
    }

    // Registro dos tipos QML
    qml_register_types_Systalia_Engine();
    qmlRegisterType<SystaliaCompositor>("Systalia.Compositor", 1, 0, "SystaliaCompositor");
    qmlRegisterSingletonType<SystemTelemetryService>(
        "Systalia.System", 1, 0, "SystemTelemetry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new SystemTelemetryService();
        });
    qmlRegisterSingletonType<ProcessLauncherService>(
        "Systalia.System", 1, 0, "ProcessLauncher",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new ProcessLauncherService();
        });
    qmlRegisterSingletonType<AppRegistryService>(
        "Systalia.System", 1, 0, "AppRegistry",
        [](QQmlEngine *, QJSEngine *) -> QObject * {
            return new AppRegistryService();
        });
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "Notifications",
                                 &notificationServer);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "SystemControl",
                                 &systemControlService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "WorkspaceManager",
                                 &workspaceManagerService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LiquidTiling",
                                 &liquidTilingService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "OutputManager",
                                 &outputManagerService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "LogindSession",
                                 &logindSessionService);
    qmlRegisterSingletonInstance("Systalia.System", 1, 0, "InputDeviceManager",
                                 &inputDeviceManager);

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("desktopIcons"), new DesktopIconProvider());

    const QUrl url(QStringLiteral("qrc:/qt/qml/Systalia/Engine/src/qml/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
            qCritical() << "Falha ao instanciar o objeto raiz a partir de" << url;
            QCoreApplication::exit(-1);
        }
    }, Qt::QueuedConnection);

    engine.load(url);

    if (engine.rootObjects().isEmpty()) {
        qWarning() << "Tentando carregar a partir do caminho de arquivo local...";
        engine.load(QUrl::fromLocalFile(QStringLiteral("src/shell/core/main.qml")));
    }

    if (auto *window = engine.rootObjects().isEmpty() ? nullptr
                     : qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
        if (qEnvironmentVariableIsSet("SGE_VALO_BACKDROP_BENCHMARK")) {
            auto configuration = window->graphicsConfiguration();
            configuration.setTimestamps(true);
            window->setGraphicsConfiguration(configuration);
        }
        installValoStabilityProbe(window);
        QObject::connect(window, &QQuickWindow::sceneGraphError, &app,
                         [&inputDeviceManager, &logindSessionService](
                             QQuickWindow::SceneGraphError error, const QString &message) {
            qCritical() << "[SGE-RHI] Erro fatal no Scene Graph/Vulkan (" << error << "):" << message;
            inputDeviceManager.shutdownHardwareContext();
            logindSessionService.releaseControl();
            QCoreApplication::exit(2);
        });
        QObject::connect(&logindSessionService, &LogindSessionService::gpuSubmissionSuspendedChanged,
                         window, [window, wasVisible = false](bool suspended) mutable {
            if (suspended) {
                wasVisible = window->isVisible();
                window->hide();
                window->releaseResources();
            } else if (wasVisible) {
                window->show();
                window->update();
            }
        });
    }

    return app.exec();
}
