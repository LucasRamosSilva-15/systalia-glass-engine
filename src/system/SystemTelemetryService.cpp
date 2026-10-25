#include "SystemTelemetryService.hpp"

#include <QIODevice>
#include <QLocale>
#include <QQuickWindow>
#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <limits>
#include <system_error>

namespace {

bool isAsciiSpace(char value) noexcept {
    return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

void skipSpaces(std::string_view &text) noexcept {
    while (!text.empty() && isAsciiSpace(text.front()))
        text.remove_prefix(1);
}

bool parseUnsigned(std::string_view text, quint64 &value) noexcept {
    skipSpaces(text);
    const auto end = text.find_first_of(" \t\r\n");
    const std::string_view token = text.substr(0, end);
    if (token.empty())
        return false;

    quint64 parsed = 0;
    const auto result = std::from_chars(token.data(), token.data() + token.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != token.data() + token.size())
        return false;
    value = parsed;
    return true;
}

bool addWithoutOverflow(quint64 value, quint64 &sum) noexcept {
    if (value > std::numeric_limits<quint64>::max() - sum)
        return false;
    sum += value;
    return true;
}

} // namespace

SystemTelemetryService::SystemTelemetryService(QObject *parent)
    : QObject(parent) {
    m_cpuStatFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);
    m_memInfoFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered);

    m_sampleTimer.setInterval(1000);
    m_sampleTimer.setTimerType(Qt::CoarseTimer);
    m_fpsSamplingClock.start();
    connect(&m_sampleTimer, &QTimer::timeout,
            this, &SystemTelemetryService::sampleSystem);

    sampleSystem();
    m_sampleTimer.start();
}

void SystemTelemetryService::attachWindow(QQuickWindow *window) {
    if (m_window == window)
        return;

    disconnect(m_frameSwapConnection);
    m_window = window;
    m_pendingFrames.store(0, std::memory_order_relaxed);
    m_fpsSamplingClock.restart();
    if (window) {
        m_frameSwapConnection = connect(
            window, &QQuickWindow::frameSwapped, this,
            [this] { m_pendingFrames.fetch_add(1, std::memory_order_relaxed); },
            Qt::DirectConnection);
    }
    setFps(0);
}

bool SystemTelemetryService::parseCpuStat(std::string_view line,
                                          CpuCounters &counters) noexcept {
    skipSpaces(line);
    if (!line.starts_with("cpu") || line.size() <= 3 || !isAsciiSpace(line[3]))
        return false;
    line.remove_prefix(3);

    quint64 total = 0;
    quint64 idle = 0;
    quint64 iowait = 0;
    std::size_t fieldIndex = 0;
    while (true) {
        skipSpaces(line);
        if (line.empty())
            break;

        const auto end = line.find_first_of(" \t\r\n");
        const std::string_view token = line.substr(0, end);
        quint64 value = 0;
        const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
        if (token.empty() || result.ec != std::errc{}
            || result.ptr != token.data() + token.size()) {
            return false;
        }

        if (fieldIndex < 8 && !addWithoutOverflow(value, total))
            return false;
        if (fieldIndex == 3)
            idle = value;
        else if (fieldIndex == 4)
            iowait = value;

        ++fieldIndex;
        if (end == std::string_view::npos)
            break;
        line.remove_prefix(end);
    }

    if (fieldIndex < 4 || !addWithoutOverflow(iowait, idle))
        return false;
    counters = {total, idle};
    return true;
}

float SystemTelemetryService::calculateCpuUsage(const CpuCounters &previous,
                                                const CpuCounters &current) noexcept {
    if (current.total <= previous.total || current.idle < previous.idle)
        return 0.0f;

    const quint64 deltaTotal = current.total - previous.total;
    const quint64 deltaIdle = current.idle - previous.idle;
    if (deltaIdle > deltaTotal)
        return 0.0f;

    const double idleRatio = static_cast<double>(deltaIdle)
                             / static_cast<double>(deltaTotal);
    return static_cast<float>(std::clamp((1.0 - idleRatio) * 100.0, 0.0, 100.0));
}

bool SystemTelemetryService::parseMemInfo(std::string_view contents,
                                          MemoryCounters &counters) noexcept {
    quint64 total = 0;
    quint64 available = 0;
    quint64 free = 0;
    bool hasTotal = false;
    bool hasAvailable = false;
    bool hasFree = false;

    while (!contents.empty()) {
        const auto newline = contents.find('\n');
        std::string_view line = contents.substr(0, newline);
        contents = newline == std::string_view::npos
                       ? std::string_view{}
                       : contents.substr(newline + 1);

        if (line.starts_with("MemTotal:")) {
            hasTotal = parseUnsigned(line.substr(9), total);
        } else if (line.starts_with("MemAvailable:")) {
            hasAvailable = parseUnsigned(line.substr(13), available);
        } else if (line.starts_with("MemFree:")) {
            hasFree = parseUnsigned(line.substr(8), free);
        }
    }

    if (!hasTotal || total == 0 || (!hasAvailable && !hasFree))
        return false;
    if (!hasAvailable)
        available = free;
    counters = {total, std::min(available, total)};
    return true;
}

float SystemTelemetryService::kibibytesToGigabytes(quint64 kibibytes) noexcept {
    constexpr double kibibytesPerGibibyte = 1024.0 * 1024.0;
    return static_cast<float>(static_cast<double>(kibibytes) / kibibytesPerGibibyte);
}

QString SystemTelemetryService::formatSystemTime(const QDateTime &dateTime) {
    const QLocale locale(QLocale::C);
    const QString dayAndMonth = locale.toString(dateTime.date(), QStringLiteral("ddd dd MMM"))
                                    .toUpper();
    return dayAndMonth + QStringLiteral(" > ")
           + dateTime.time().toString(QStringLiteral("HH:mm:ss"));
}

bool SystemTelemetryService::readCpuStat(CpuCounters &counters) {
    if (!m_cpuStatFile.isOpen()
        && !m_cpuStatFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        return false;
    }
    if (!m_cpuStatFile.seek(0))
        return false;

    std::array<char, 512> line{};
    const qint64 length = m_cpuStatFile.readLine(line.data(), static_cast<qint64>(line.size()));
    if (length <= 0)
        return false;
    return parseCpuStat(std::string_view(line.data(), static_cast<std::size_t>(length)), counters);
}

bool SystemTelemetryService::readMemInfo(MemoryCounters &counters) {
    if (!m_memInfoFile.isOpen()
        && !m_memInfoFile.open(QIODevice::ReadOnly | QIODevice::Unbuffered)) {
        return false;
    }
    if (!m_memInfoFile.seek(0))
        return false;

    std::array<char, 4096> contents{};
    const qint64 length = m_memInfoFile.read(contents.data(),
                                             static_cast<qint64>(contents.size()));
    if (length <= 0)
        return false;
    return parseMemInfo(std::string_view(contents.data(), static_cast<std::size_t>(length)),
                        counters);
}

void SystemTelemetryService::sampleSystem() {
    CpuCounters currentCpu;
    if (readCpuStat(currentCpu)) {
        if (m_havePreviousCpuCounters)
            setCpuUsage(calculateCpuUsage(m_previousCpuCounters, currentCpu));
        m_previousCpuCounters = currentCpu;
        m_havePreviousCpuCounters = true;
    }

    MemoryCounters memory;
    if (readMemInfo(memory)) {
        const float totalGb = kibibytesToGigabytes(memory.totalKiB);
        const float usedGb = kibibytesToGigabytes(memory.totalKiB - memory.availableKiB);
        setMemory(usedGb, totalGb);
    }

    const quint32 frames = m_pendingFrames.exchange(0, std::memory_order_relaxed);
    const qint64 elapsedMs = m_fpsSamplingClock.restart();
    const double measuredFps = elapsedMs > 0
                                   ? static_cast<double>(frames) * 1000.0
                                         / static_cast<double>(elapsedMs)
                                   : 0.0;
    const int currentFps = static_cast<int>(std::clamp(
        std::lround(measuredFps), 0L,
        static_cast<long>(std::numeric_limits<int>::max())));
    setFps(currentFps);
    setSystemTime(formatSystemTime(QDateTime::currentDateTime()));
}

void SystemTelemetryService::setCpuUsage(float value) {
    if (qFuzzyCompare(m_cpuUsage + 1.0f, value + 1.0f))
        return;
    m_cpuUsage = value;
    emit cpuUsageChanged();
}

void SystemTelemetryService::setMemory(float usedGb, float totalGb) {
    if (!qFuzzyCompare(m_ramUsageGb + 1.0f, usedGb + 1.0f)) {
        m_ramUsageGb = usedGb;
        emit ramUsageGbChanged();
    }
    if (!qFuzzyCompare(m_ramTotalGb + 1.0f, totalGb + 1.0f)) {
        m_ramTotalGb = totalGb;
        emit ramTotalGbChanged();
    }
}

void SystemTelemetryService::setFps(int value) {
    if (m_fps == value)
        return;
    m_fps = value;
    emit fpsChanged();
}

void SystemTelemetryService::setSystemTime(const QString &value) {
    if (m_systemTime == value)
        return;
    m_systemTime = value;
    emit systemTimeChanged();
}
