#pragma once

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>
#include <iostream>
#include <memory>

#if defined(Q_OS_UNIX)
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

namespace Systalia::Tools {

/**
 * @brief Lock-protected per-run temporary artifact manager for C++ diagnostic tools.
 */
class ScopedArtifactRun {
public:
    ScopedArtifactRun(const QString &toolName, const QString &explicitDir = QString())
        : m_toolName(toolName)
    {
        const QString envDir = qEnvironmentVariable("SGE_KEEP_EVIDENCE_DIR").trimmed();
        const QString userDir = !explicitDir.trimmed().isEmpty() ? explicitDir.trimmed() : envDir;
        m_keepRequested = (qEnvironmentVariable("SGE_KEEP_EVIDENCE") == QStringLiteral("1"));
        m_createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

        if (!userDir.isEmpty()) {
            m_userDir = QFileInfo(userDir).absoluteFilePath();
            QDir().mkpath(m_userDir);
            m_ownsTempDir = false;
        } else {
            QString tmpBase = qEnvironmentVariable("TMPDIR").trimmed();
            if (tmpBase.isEmpty()) {
                tmpBase = QDir::tempPath();
            }
            m_runsRoot = QFileInfo(QDir(tmpBase).filePath(QStringLiteral("systalia-runs"))).canonicalFilePath();
            if (m_runsRoot.isEmpty()) {
                m_runsRoot = QDir(tmpBase).filePath(QStringLiteral("systalia-runs"));
            }
            QDir().mkpath(m_runsRoot);
            m_runsRoot = QFileInfo(m_runsRoot).canonicalFilePath();
            pruneExpiredRuns(m_runsRoot, 24);

            const QString tmpl = QDir(m_runsRoot).filePath(toolName + QStringLiteral("-XXXXXX"));
            m_tempDir = std::make_unique<QTemporaryDir>(tmpl);
            m_tempDir->setAutoRemove(false);
            m_ownsTempDir = m_tempDir->isValid();
            if (m_ownsTempDir) {
                acquireLock(QDir(m_tempDir->path()).filePath(QStringLiteral(".sge-run.lock")));
                writeStatus(QStringLiteral("active"), m_keepRequested);
            }
        }
    }

    ~ScopedArtifactRun() {
        if (!m_ownsTempDir || !m_tempDir || !m_tempDir->isValid()) {
            releaseLock();
            return;
        }
        const bool shouldKeep = m_keepRequested || !m_succeeded;
        if (shouldKeep) {
            const QString status = m_succeeded ? QStringLiteral("success-kept") : QStringLiteral("failed");
            writeStatus(status, m_keepRequested);
            releaseLock();
            std::cout << "[SGE-EVIDENCE] Preserved ("
                      << status.toStdString()
                      << ") in " << m_tempDir->path().toStdString() << std::endl;
            m_tempDir->setAutoRemove(false);
        } else {
            releaseLock();
            m_tempDir->setAutoRemove(true);
        }
    }

    ScopedArtifactRun(const ScopedArtifactRun &) = delete;
    ScopedArtifactRun &operator=(const ScopedArtifactRun &) = delete;

    QString path() const {
        if (!m_userDir.isEmpty()) {
            return m_userDir;
        }
        return m_tempDir ? m_tempDir->path() : QString();
    }

    void markSuccess() noexcept { m_succeeded = true; }
    void markFailure() noexcept { m_succeeded = false; }

    static int pruneExpiredRuns(const QString &runsRoot, qint64 maxAgeHours) {
        QFileInfo rootInfo(runsRoot);
        if (!rootInfo.exists() || !rootInfo.isDir() || rootInfo.isSymLink()) {
            return 0;
        }
        const QString canonicalRoot = rootInfo.canonicalFilePath();
        QDir root(canonicalRoot);
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-maxAgeHours * 3600);
        const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        int removed = 0;

        for (const QFileInfo &info : entries) {
            if (info.isSymLink()) {
                continue;
            }
            const QString canonicalEntry = info.canonicalFilePath();
            if (canonicalEntry.isEmpty() || !canonicalEntry.startsWith(canonicalRoot + QLatin1Char('/'))) {
                continue;
            }
            const QString statusPath = QDir(canonicalEntry).filePath(QStringLiteral("run-status.json"));
            QFile statusFile(statusPath);
            if (!statusFile.exists() || QFileInfo(statusPath).isSymLink() || !statusFile.open(QIODevice::ReadOnly)) {
                continue;
            }
            const QJsonDocument doc = QJsonDocument::fromJson(statusFile.readAll());
            statusFile.close();
            if (!doc.isObject()) {
                continue;
            }
            const QJsonObject obj = doc.object();
            if (obj.value(QStringLiteral("managed_by")).toString() != QStringLiteral("sge-evidence-v1")) {
                continue;
            }
            const QString status = obj.value(QStringLiteral("status")).toString();
            if (obj.value(QStringLiteral("explicit_keep")).toBool() || status == QStringLiteral("success-kept")) {
                continue;
            }
            if (status != QStringLiteral("failed") && status != QStringLiteral("interrupted")) {
                continue;
            }
            if (info.lastModified().toUTC() >= cutoff) {
                continue;
            }

#if defined(Q_OS_UNIX)
            const QString lockPath = QDir(canonicalEntry).filePath(QStringLiteral(".sge-run.lock"));
            const QByteArray lockBytes = QFile::encodeName(lockPath);
            const int fd = ::open(lockBytes.constData(), O_CREAT | O_RDWR, 0600);
            if (fd < 0) {
                continue;
            }
            if (::flock(fd, LOCK_EX | LOCK_NB) != 0) {
                ::close(fd);
                continue;
            }
            QDir(canonicalEntry).removeRecursively();
            ::flock(fd, LOCK_UN);
            ::close(fd);
            ++removed;
#endif
        }
        return removed;
    }

private:
    void acquireLock(const QString &lockPath) {
#if defined(Q_OS_UNIX)
        const QByteArray lockBytes = QFile::encodeName(lockPath);
        m_lockFd = ::open(lockBytes.constData(), O_CREAT | O_RDWR, 0600);
        if (m_lockFd >= 0) {
            ::flock(m_lockFd, LOCK_EX | LOCK_NB);
        }
#else
        Q_UNUSED(lockPath);
#endif
    }

    void releaseLock() {
#if defined(Q_OS_UNIX)
        if (m_lockFd >= 0) {
            ::flock(m_lockFd, LOCK_UN);
            ::close(m_lockFd);
            m_lockFd = -1;
        }
#endif
    }

    void writeStatus(const QString &status, bool explicitKeep) {
        if (!m_ownsTempDir || !m_tempDir || !m_tempDir->isValid()) {
            return;
        }
        QJsonObject obj;
        obj.insert(QStringLiteral("managed_by"), QStringLiteral("sge-evidence-v1"));
        obj.insert(QStringLiteral("tool"), m_toolName);
        obj.insert(QStringLiteral("status"), status);
        obj.insert(QStringLiteral("pid"), static_cast<qint64>(QCoreApplication::applicationPid()));
        obj.insert(QStringLiteral("explicit_keep"), explicitKeep);
        obj.insert(QStringLiteral("created_at"), m_createdAt);
        obj.insert(QStringLiteral("updated_at"), QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
        obj.insert(QStringLiteral("retention_hours"), 24);

        QFile f(QDir(m_tempDir->path()).filePath(QStringLiteral("run-status.json")));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
        }
    }

    QString m_toolName;
    QString m_runsRoot;
    QString m_userDir;
    QString m_createdAt;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    int m_lockFd = -1;
    bool m_ownsTempDir = false;
    bool m_keepRequested = false;
    bool m_succeeded = false;
};

} // namespace Systalia::Tools
