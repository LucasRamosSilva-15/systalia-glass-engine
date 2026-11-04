#include "InputDeviceManager.hpp"

#include "LogindSessionService.hpp"
#include "OutputManagerService.hpp"
#include "WorkspaceManagerService.hpp"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include <QtWaylandCompositor/QWaylandPointer>
#include <QtWaylandCompositor/QWaylandView>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>

#ifdef SGE_HAVE_LIBUDEV
#include <libudev.h>
#endif

#ifdef SGE_HAVE_LIBINPUT
#include <libinput.h>
#endif

namespace {
InputDeviceManager *s_instance = nullptr;
constexpr qreal kMinPointerAcceleration = 0.1;
constexpr qreal kMaxPointerAcceleration = 5.0;
constexpr qreal kSwipeTriggerThresholdPx = 90.0;

// Linux input-event-codes.h constants
constexpr quint32 kBtnLeft = 0x110u;   // 272
constexpr quint32 kBtnRight = 0x111u;  // 273
constexpr quint32 kBtnMiddle = 0x112u; // 274
constexpr quint32 kBtnSide = 0x113u;   // 275
constexpr quint32 kBtnExtra = 0x114u;  // 276
} // namespace

InputDeviceManager::InputDeviceManager(QObject *parent)
    : QObject(parent) {
    s_instance = this;

    const PlatformEnvironmentConfig envCfg = evaluatePlatformEnvironment(
        qgetenv("WAYLAND_DISPLAY"),
        qgetenv("DISPLAY"),
        qgetenv("QT_QPA_PLATFORM"));
    m_sessionMode = envCfg.mode;
    if (m_sessionMode == SessionBareMetalKms) {
        m_platformModeName = QStringLiteral("bare-metal-drm-kms");
    } else if (m_sessionMode == SessionHeadless) {
        m_platformModeName = QStringLiteral("headless-offscreen");
    } else {
        m_platformModeName = QStringLiteral("nested");
    }

    if (auto *outMgr = OutputManagerService::instance()) {
        const QRect bounds = outMgr->virtualCanvasBounds();
        if (bounds.isValid() && bounds.width() > 0 && bounds.height() > 0) {
            m_kmsDisplayBounds = bounds;
            m_pointerPosition = QRectF(bounds).center();
        }
    }
}

InputDeviceManager::~InputDeviceManager() {
    shutdownHardwareContext();
    if (s_instance == this) {
        s_instance = nullptr;
    }
}

InputDeviceManager *InputDeviceManager::instance() noexcept {
    return s_instance;
}

InputDeviceManager::PlatformEnvironmentConfig InputDeviceManager::evaluatePlatformEnvironment(
    const QByteArray &waylandDisplay,
    const QByteArray &x11Display,
    const QByteArray &existingQpaPlatform,
    const QString &kmsConfigPath) noexcept {
    PlatformEnvironmentConfig cfg;
    cfg.qsgRhiBackend = QByteArrayLiteral("vulkan");

    const QByteArray trimmedQpa = existingQpaPlatform.trimmed().toLower();
    if (trimmedQpa == QByteArrayLiteral("offscreen")
        || trimmedQpa == QByteArrayLiteral("minimal")) {
        cfg.mode = SessionHeadless;
        cfg.qtQpaPlatform = existingQpaPlatform.trimmed();
        cfg.atomicDrmEnabled = false;
        return cfg;
    }

    const bool hasWayland = !waylandDisplay.trimmed().isEmpty();
    const bool hasX11 = !x11Display.trimmed().isEmpty();

    if (!hasWayland && !hasX11) {
        // Execução direta de um TTY Linux puro (Bare-Metal DRM/KMS)
        cfg.mode = SessionBareMetalKms;
        cfg.qtQpaPlatform = QByteArrayLiteral("eglfs");
        cfg.qtQpaEglfsIntegration = QByteArrayLiteral("eglfs_kms");
        cfg.qsgRhiBackend = QByteArrayLiteral("vulkan");
        cfg.atomicDrmEnabled = true;
        if (!kmsConfigPath.trimmed().isEmpty()) {
            cfg.eglfsKmsConfigPath = kmsConfigPath.trimmed().toUtf8();
        } else {
            cfg.eglfsKmsConfigPath = QByteArrayLiteral("config/eglfs_kms.json");
        }
    } else {
        // Execução aninhada em sessão gráfica existente (Wayland / X11)
        cfg.mode = SessionNested;
        cfg.qtQpaPlatform = existingQpaPlatform.trimmed().isEmpty()
                            ? QByteArrayLiteral("wayland;xcb")
                            : existingQpaPlatform.trimmed();
        cfg.atomicDrmEnabled = false;
    }

    return cfg;
}

InputDeviceManager::PlatformEnvironmentConfig InputDeviceManager::configureProcessPlatformEnvironment(
    const QString &kmsConfigPath) {
    const PlatformEnvironmentConfig cfg = evaluatePlatformEnvironment(
        qgetenv("WAYLAND_DISPLAY"),
        qgetenv("DISPLAY"),
        qgetenv("QT_QPA_PLATFORM"),
        kmsConfigPath);

    qputenv("QSG_RHI_BACKEND", cfg.qsgRhiBackend);

    if (cfg.mode == SessionBareMetalKms) {
        qputenv("QT_QPA_PLATFORM", cfg.qtQpaPlatform);
        qputenv("QT_QPA_EGLFS_INTEGRATION", cfg.qtQpaEglfsIntegration);
        qputenv("QT_QPA_EGLFS_KMS_ATOMIC", QByteArrayLiteral("1"));
        if (!cfg.eglfsKmsConfigPath.isEmpty() && QFile::exists(QString::fromUtf8(cfg.eglfsKmsConfigPath))) {
            qputenv("QT_QPA_EGLFS_KMS_CONFIG", cfg.eglfsKmsConfigPath);
        }
    } else if (cfg.mode == SessionNested) {
        if (qgetenv("QT_QPA_PLATFORM").trimmed().isEmpty()) {
            qputenv("QT_QPA_PLATFORM", cfg.qtQpaPlatform);
        }
    }

    return cfg;
}

InputDeviceManager::KmsConnectorMode InputDeviceManager::selectBestKmsConnectorMode(
    const QList<KmsConnectorMode> &candidates) noexcept {
    KmsConnectorMode best;
    best.connectorName = QStringLiteral("DP-1");
    best.resolution = QSize(1920, 1080);
    best.refreshRateHz = 60;
    best.connected = true;
    best.preferred = true;

    bool foundConnected = false;
    qint64 bestPixels = -1;
    int bestRefresh = -1;

    for (const KmsConnectorMode &mode : candidates) {
        if (!mode.connected || mode.resolution.width() <= 0 || mode.resolution.height() <= 0) {
            continue;
        }
        const qint64 pixels = static_cast<qint64>(mode.resolution.width())
                              * static_cast<qint64>(mode.resolution.height());
        if (!foundConnected
            || pixels > bestPixels
            || (pixels == bestPixels && mode.refreshRateHz > bestRefresh)
            || (pixels == bestPixels && mode.refreshRateHz == bestRefresh && mode.preferred)) {
            best = mode;
            bestPixels = pixels;
            bestRefresh = mode.refreshRateHz;
            foundConnected = true;
        }
    }

    return best;
}

QByteArray InputDeviceManager::generateEglfsKmsConfigJson(
    const QString &drmDevice,
    const KmsConnectorMode &preferredMode) const {
    QJsonObject root;
    root.insert(QStringLiteral("device"), drmDevice.isEmpty()
                                          ? QStringLiteral("/dev/dri/card0")
                                          : drmDevice);
    root.insert(QStringLiteral("hwcursor"), true);
    root.insert(QStringLiteral("pbuffers"), true);
    root.insert(QStringLiteral("separateScreens"), false);

    QJsonArray outputs;
    QJsonObject primaryOutput;
    primaryOutput.insert(QStringLiteral("name"),
                         preferredMode.connectorName.isEmpty()
                             ? QStringLiteral("DP-1")
                             : preferredMode.connectorName);
    if (preferredMode.resolution.isValid()
        && preferredMode.resolution.width() > 0
        && preferredMode.resolution.height() > 0) {
        primaryOutput.insert(
            QStringLiteral("mode"),
            QStringLiteral("%1x%2@%3")
                .arg(preferredMode.resolution.width())
                .arg(preferredMode.resolution.height())
                .arg(std::max(30, preferredMode.refreshRateHz)));
    } else {
        primaryOutput.insert(QStringLiteral("mode"), QStringLiteral("preferred"));
    }
    primaryOutput.insert(QStringLiteral("primary"), true);
    outputs.append(primaryOutput);
    root.insert(QStringLiteral("outputs"), outputs);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QByteArray InputDeviceManager::generateEglfsKmsConfigJson(const QString &drmDevice) const {
    return generateEglfsKmsConfigJson(drmDevice, KmsConnectorMode{});
}

bool InputDeviceManager::writeEglfsKmsConfigFile(const QString &filePath,
                                                 const QString &drmDevice) {
    if (filePath.trimmed().isEmpty()) {
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    KmsConnectorMode mode;
    mode.connectorName = QStringLiteral("DP-1");
    mode.resolution = m_kmsDisplayBounds.size();
    mode.refreshRateHz = m_targetRefreshRateHz;
    mode.connected = true;
    mode.preferred = true;

    const QByteArray payload = generateEglfsKmsConfigJson(drmDevice, mode);
    const qint64 written = file.write(payload);
    file.close();
    return written == payload.size();
}

void InputDeviceManager::attachCompositor(QWaylandCompositor *compositor) {
    if (m_compositor == compositor) {
        return;
    }
    m_compositor = compositor;
}

QWaylandSeat *InputDeviceManager::targetSeat() const noexcept {
    if (!m_compositor) {
        return nullptr;
    }
    return m_compositor->defaultSeat();
}

void InputDeviceManager::setPointerAcceleration(qreal factor) {
    const qreal clamped = std::clamp(factor, kMinPointerAcceleration, kMaxPointerAcceleration);
    if (qFuzzyCompare(m_pointerAcceleration, clamped)) {
        return;
    }
    m_pointerAcceleration = clamped;
    emit pointerAccelerationChanged(m_pointerAcceleration);
}

void InputDeviceManager::setKmsDisplayBounds(const QRect &bounds) {
    if (!bounds.isValid() || bounds.width() <= 0 || bounds.height() <= 0) {
        return;
    }
    if (m_kmsDisplayBounds == bounds) {
        return;
    }
    m_kmsDisplayBounds = bounds;
    m_pointerPosition = clampPointerToBounds(m_pointerPosition, m_kmsDisplayBounds);
    emit kmsDisplayBoundsChanged(m_kmsDisplayBounds);
    emit pointerPositionChanged(m_pointerPosition);
}

void InputDeviceManager::setPointerPosition(const QPointF &pos) {
    const QPointF clamped = clampPointerToBounds(pos, m_kmsDisplayBounds);
    if (m_pointerPosition == clamped) {
        return;
    }
    m_pointerPosition = clamped;
    emit pointerPositionChanged(m_pointerPosition);
}

void InputDeviceManager::resetTelemetryCounters() noexcept {
    m_dispatchedKeyEvents = 0;
    m_dispatchedPointerEvents = 0;
    m_dispatchedSwipeGestures = 0;
    m_accumulatedSwipeDx = 0.0;
    emit inputTelemetryChanged();
}

int InputDeviceManager::openRestrictedCallback(const char *path, int flags, void *userData) {
    Q_UNUSED(userData);
    if (!path) {
        return -EINVAL;
    }
    if (auto *logind = LogindSessionService::instance(); logind && logind->hasSessionControl()) {
        const int logindFd = logind->takeDeviceByPath(QString::fromLocal8Bit(path));
        if (logindFd >= 0) {
            return logindFd;
        }
    }
    const int fd = ::open(path, flags | O_CLOEXEC);
    return (fd < 0) ? -errno : fd;
}

void InputDeviceManager::closeRestrictedCallback(int fd, void *userData) {
    Q_UNUSED(userData);
    if (fd < 0) {
        return;
    }
    if (auto *logind = LogindSessionService::instance(); logind && logind->hasSessionControl()) {
        if (logind->releaseDeviceByFd(fd)) {
            return;
        }
    }
    ::close(fd);
}

const libinput_interface &InputDeviceManager::restrictedInterface() noexcept {
    static const libinput_interface kInterface = {
        &InputDeviceManager::openRestrictedCallback,
        &InputDeviceManager::closeRestrictedCallback
    };
    return kInterface;
}

bool InputDeviceManager::initializeHardwareContext() {
    if (m_hardwareInitialized) {
        return true;
    }

#if defined(SGE_HAVE_LIBUDEV) && defined(SGE_HAVE_LIBINPUT)
    m_udev = udev_new();
    if (!m_udev) {
        qWarning().noquote() << "[SGE-INPUT] Falha ao inicializar contexto udev.";
        return false;
    }

    m_libinput = libinput_udev_create_context(&restrictedInterface(), this, m_udev);
    if (!m_libinput) {
        qWarning().noquote() << "[SGE-INPUT] Falha ao criar contexto libinput via udev.";
        udev_unref(m_udev);
        m_udev = nullptr;
        return false;
    }

    if (libinput_udev_assign_seat(m_libinput, "seat0") != 0) {
        qWarning().noquote() << "[SGE-INPUT] Não foi possível atribuir seat0 ao libinput (permissões restritas ou sessão de teste).";
        libinput_unref(m_libinput);
        m_libinput = nullptr;
        udev_unref(m_udev);
        m_udev = nullptr;
        return false;
    }

    const int fd = libinput_get_fd(m_libinput);
    if (fd >= 0) {
        m_libinputNotifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(m_libinputNotifier, &QSocketNotifier::activated,
                this, &InputDeviceManager::pollHardwareEvents);
    }

    m_hardwareInitialized = true;
    qInfo().noquote() << "[SGE-INPUT] Backend nativo libinput/udev inicializado em seat0.";
    return true;
#elif defined(SGE_HAVE_LIBUDEV)
    m_udev = udev_new();
    if (m_udev) {
        m_hardwareInitialized = true;
        return true;
    }
    return false;
#else
    return false;
#endif
}

void InputDeviceManager::shutdownHardwareContext() {
    if (m_libinputNotifier) {
        m_libinputNotifier->setEnabled(false);
        delete m_libinputNotifier;
        m_libinputNotifier = nullptr;
    }

#ifdef SGE_HAVE_LIBINPUT
    if (m_libinput) {
        libinput_unref(m_libinput);
        m_libinput = nullptr;
    }
#endif

#ifdef SGE_HAVE_LIBUDEV
    if (m_udev) {
        udev_unref(m_udev);
        m_udev = nullptr;
    }
#endif

    m_hardwareInitialized = false;
}

void InputDeviceManager::pollHardwareEvents() {
#ifdef SGE_HAVE_LIBINPUT
    if (!m_libinput) {
        return;
    }
    if (libinput_dispatch(m_libinput) != 0) {
        return;
    }

    while (libinput_event *rawEvent = libinput_get_event(m_libinput)) {
        const libinput_event_type type = libinput_event_get_type(rawEvent);
        switch (type) {
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            auto *kbEvent = libinput_event_get_keyboard_event(rawEvent);
            const uint32_t key = libinput_event_keyboard_get_key(kbEvent);
            const bool pressed = (libinput_event_keyboard_get_key_state(kbEvent)
                                  == LIBINPUT_KEY_STATE_PRESSED);
            processLibinputKeyEvent(key, pressed);
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION: {
            auto *ptrEvent = libinput_event_get_pointer_event(rawEvent);
            const double dx = libinput_event_pointer_get_dx(ptrEvent);
            const double dy = libinput_event_pointer_get_dy(ptrEvent);
            processLibinputPointerMotion(dx, dy);
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            auto *ptrEvent = libinput_event_get_pointer_event(rawEvent);
            const double normX = libinput_event_pointer_get_absolute_x_transformed(ptrEvent, 1.0);
            const double normY = libinput_event_pointer_get_absolute_y_transformed(ptrEvent, 1.0);
            processLibinputPointerMotionAbsolute(normX, normY);
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            auto *ptrEvent = libinput_event_get_pointer_event(rawEvent);
            const uint32_t btn = libinput_event_pointer_get_button(ptrEvent);
            const bool pressed = (libinput_event_pointer_get_button_state(ptrEvent)
                                  == LIBINPUT_BUTTON_STATE_PRESSED);
            processLibinputPointerButton(btn, pressed);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_UPDATE: {
            auto *gestEvent = libinput_event_get_gesture_event(rawEvent);
            const int fingers = libinput_event_gesture_get_finger_count(gestEvent);
            const double dx = libinput_event_gesture_get_dx(gestEvent);
            const double dy = libinput_event_gesture_get_dy(gestEvent);
            processLibinputSwipeGesture(fingers, dx, dy, false, false);
            break;
        }
        case LIBINPUT_EVENT_GESTURE_SWIPE_END: {
            auto *gestEvent = libinput_event_get_gesture_event(rawEvent);
            const int fingers = libinput_event_gesture_get_finger_count(gestEvent);
            const bool cancelled = (libinput_event_gesture_get_cancelled(gestEvent) != 0);
            processLibinputSwipeGesture(fingers, 0.0, 0.0, true, cancelled);
            break;
        }
        default:
            break;
        }
        libinput_event_destroy(rawEvent);
    }
#endif
}

QPointF InputDeviceManager::clampPointerToBounds(const QPointF &candidate,
                                                 const QRect &bounds) noexcept {
    if (!bounds.isValid() || bounds.width() <= 0 || bounds.height() <= 0) {
        return candidate;
    }
    const qreal minX = static_cast<qreal>(bounds.x());
    const qreal minY = static_cast<qreal>(bounds.y());
    const qreal maxX = static_cast<qreal>(bounds.x() + bounds.width() - 1);
    const qreal maxY = static_cast<qreal>(bounds.y() + bounds.height() - 1);

    return QPointF(std::clamp(candidate.x(), minX, std::max(minX, maxX)),
                   std::clamp(candidate.y(), minY, std::max(minY, maxY)));
}

QPointF InputDeviceManager::applyPointerMotionDelta(qreal dx, qreal dy) noexcept {
    const qreal accelDx = dx * m_pointerAcceleration;
    const qreal accelDy = dy * m_pointerAcceleration;
    const QPointF next(m_pointerPosition.x() + accelDx,
                       m_pointerPosition.y() + accelDy);
    m_pointerPosition = clampPointerToBounds(next, m_kmsDisplayBounds);
    return m_pointerPosition;
}

Qt::MouseButton InputDeviceManager::translateLinuxButtonToQt(quint32 linuxButton) noexcept {
    switch (linuxButton) {
    case kBtnLeft:
        return Qt::LeftButton;
    case kBtnRight:
        return Qt::RightButton;
    case kBtnMiddle:
        return Qt::MiddleButton;
    case kBtnSide:
        return Qt::BackButton;
    case kBtnExtra:
        return Qt::ForwardButton;
    default:
        return Qt::NoButton;
    }
}

int InputDeviceManager::translateLinuxKeyToQtKey(quint32 linuxScanCode) noexcept {
    // Mapeamento dos códigos Linux evdev mais comuns para Qt::Key
    switch (linuxScanCode) {
    case 1: return Qt::Key_Escape;
    case 2: return Qt::Key_1;
    case 3: return Qt::Key_2;
    case 4: return Qt::Key_3;
    case 5: return Qt::Key_4;
    case 6: return Qt::Key_5;
    case 7: return Qt::Key_6;
    case 8: return Qt::Key_7;
    case 9: return Qt::Key_8;
    case 10: return Qt::Key_9;
    case 11: return Qt::Key_0;
    case 14: return Qt::Key_Backspace;
    case 15: return Qt::Key_Tab;
    case 16: return Qt::Key_Q;
    case 17: return Qt::Key_W;
    case 18: return Qt::Key_E;
    case 19: return Qt::Key_R;
    case 20: return Qt::Key_T;
    case 21: return Qt::Key_Y;
    case 22: return Qt::Key_U;
    case 23: return Qt::Key_I;
    case 24: return Qt::Key_O;
    case 25: return Qt::Key_P;
    case 28: return Qt::Key_Return;
    case 29: return Qt::Key_Control;
    case 30: return Qt::Key_A;
    case 31: return Qt::Key_S;
    case 32: return Qt::Key_D;
    case 33: return Qt::Key_F;
    case 34: return Qt::Key_G;
    case 35: return Qt::Key_H;
    case 36: return Qt::Key_J;
    case 37: return Qt::Key_K;
    case 38: return Qt::Key_L;
    case 42: return Qt::Key_Shift;
    case 44: return Qt::Key_Z;
    case 45: return Qt::Key_X;
    case 46: return Qt::Key_C;
    case 47: return Qt::Key_V;
    case 48: return Qt::Key_B;
    case 49: return Qt::Key_N;
    case 50: return Qt::Key_M;
    case 56: return Qt::Key_Alt;
    case 57: return Qt::Key_Space;
    case 103: return Qt::Key_Up;
    case 105: return Qt::Key_Left;
    case 106: return Qt::Key_Right;
    case 108: return Qt::Key_Down;
    case 125: return Qt::Key_Meta;
    case 126: return Qt::Key_Meta;
    default:
        return Qt::Key_unknown;
    }
}

bool InputDeviceManager::processLibinputKeyEvent(quint32 linuxScanCode, bool pressed) {
    if (linuxScanCode == 0) {
        return false;
    }

    const quint32 xkbKeycode = linuxScanCodeToXkbKeycode(linuxScanCode);
    const int qtKey = translateLinuxKeyToQtKey(linuxScanCode);

    if (QWaylandSeat *seat = targetSeat()) {
        if (pressed) {
            seat->sendKeyPressEvent(linuxScanCode);
        } else {
            seat->sendKeyReleaseEvent(linuxScanCode);
        }
    }

    ++m_dispatchedKeyEvents;
    emit inputTelemetryChanged();
    emit keyEventDispatched(linuxScanCode, xkbKeycode, qtKey, pressed);
    return true;
}

QPointF InputDeviceManager::processLibinputPointerMotion(qreal dx, qreal dy) {
    const qreal accelDx = dx * m_pointerAcceleration;
    const qreal accelDy = dy * m_pointerAcceleration;
    const QPointF newPos = applyPointerMotionDelta(dx, dy);

    if (auto *outMgr = OutputManagerService::instance()) {
        outMgr->updateCursorPosition(newPos);
    }

    if (QWaylandSeat *seat = targetSeat()) {
        if (QWaylandPointer *pointer = seat->pointer()) {
            if (QWaylandView *focusView = seat-> mouseFocus()) {
                pointer->sendMouseMoveEvent(focusView, newPos, newPos);
            }
        }
    }

    ++m_dispatchedPointerEvents;
    emit pointerPositionChanged(newPos);
    emit inputTelemetryChanged();
    emit pointerMotionDispatched(newPos, accelDx, accelDy);
    return newPos;
}

QPointF InputDeviceManager::processLibinputPointerMotionAbsolute(qreal normalizedX,
                                                                 qreal normalizedY) {
    const qreal clampedNormX = std::clamp(normalizedX, 0.0, 1.0);
    const qreal clampedNormY = std::clamp(normalizedY, 0.0, 1.0);

    const qreal targetX = m_kmsDisplayBounds.x()
                          + clampedNormX * std::max(1, m_kmsDisplayBounds.width() - 1);
    const qreal targetY = m_kmsDisplayBounds.y()
                          + clampedNormY * std::max(1, m_kmsDisplayBounds.height() - 1);

    m_pointerPosition = clampPointerToBounds(QPointF(targetX, targetY), m_kmsDisplayBounds);

    if (auto *outMgr = OutputManagerService::instance()) {
        outMgr->updateCursorPosition(m_pointerPosition);
    }

    ++m_dispatchedPointerEvents;
    emit pointerPositionChanged(m_pointerPosition);
    emit inputTelemetryChanged();
    emit pointerMotionDispatched(m_pointerPosition, 0.0, 0.0);
    return m_pointerPosition;
}

bool InputDeviceManager::processLibinputPointerButton(quint32 linuxButtonCode, bool pressed) {
    const Qt::MouseButton button = translateLinuxButtonToQt(linuxButtonCode);
    if (button == Qt::NoButton) {
        return false;
    }

    if (pressed) {
        m_pressedButtons |= button;
    } else {
        m_pressedButtons &= ~button;
    }

    if (QWaylandSeat *seat = targetSeat()) {
        if (QWaylandPointer *pointer = seat->pointer()) {
            if (pressed) {
                pointer->sendMousePressEvent(button);
            } else {
                pointer->sendMouseReleaseEvent(button);
            }
        }
    }

    ++m_dispatchedPointerEvents;
    emit inputTelemetryChanged();
    emit pointerButtonDispatched(button, pressed, m_pointerPosition);
    return true;
}

bool InputDeviceManager::processLibinputSwipeGesture(int fingerCount,
                                                     qreal cumulativeDx,
                                                     qreal cumulativeDy,
                                                     bool ended,
                                                     bool cancelled) {
    Q_UNUSED(cumulativeDy);
    if (fingerCount < 3) {
        return false;
    }

    if (cancelled) {
        m_accumulatedSwipeDx = 0.0;
        return false;
    }

    m_accumulatedSwipeDx += cumulativeDx;

    if (!ended) {
        return true;
    }

    const qreal totalDx = m_accumulatedSwipeDx;
    m_accumulatedSwipeDx = 0.0;

    if (std::abs(totalDx) < kSwipeTriggerThresholdPx) {
        return false;
    }

    // Swipe para a esquerda (dx < 0) avança para o próximo workspace;
    // Swipe para a direita (dx > 0) recua para o workspace anterior.
    const int direction = (totalDx < 0.0) ? 1 : -1;
    if (auto *wsMgr = WorkspaceManagerService::instance()) {
        if (direction > 0) {
            wsMgr->nextWorkspace();
        } else {
            wsMgr->previousWorkspace();
        }
    }

    ++m_dispatchedSwipeGestures;
    emit inputTelemetryChanged();
    emit workspaceSwipeTriggered(direction);
    return true;
}

bool InputDeviceManager::dispatchSimulatedEvent(const SimulatedLibinputEvent &ev) {
    switch (ev.type) {
    case LibinputEventKeyboardKey:
        return processLibinputKeyEvent(ev.linuxScanCode, ev.pressed);
    case LibinputEventPointerMotion:
        processLibinputPointerMotion(ev.dx, ev.dy);
        return true;
    case LibinputEventPointerMotionAbsolute:
        processLibinputPointerMotionAbsolute(ev.dx, ev.dy);
        return true;
    case LibinputEventPointerButton:
        return processLibinputPointerButton(ev.linuxButtonCode, ev.pressed);
    case LibinputEventGestureSwipeBegin:
        m_accumulatedSwipeDx = 0.0;
        return ev.fingerCount >= 3;
    case LibinputEventGestureSwipeUpdate:
        return processLibinputSwipeGesture(ev.fingerCount, ev.dx, ev.dy, false, ev.cancelled);
    case LibinputEventGestureSwipeEnd:
        return processLibinputSwipeGesture(ev.fingerCount, ev.dx, ev.dy, true, ev.cancelled);
    default:
        return false;
    }
}
