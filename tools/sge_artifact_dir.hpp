#pragma once

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>
#include <QTemporaryDir>
#include <QDateTime>
#include <iostream>
#include <memory>

namespace Systalia::Tools {

/**
 * @brief Manages unique per-run temporary artifact directories for C++ diagnostic tools.
 *
 * - Honors explicit user directory (--artifact-dir or SGE_KEEP_EVIDENCE_DIR) without ever
 *   auto-deleting it.
 * - Otherwise creates a unique directory under ${TMPDIR:-/tmp}/systalia-runs/<toolName>-XXXXXX.
 * - Cleans up owned temporary directory on success unless SGE_KEEP_EVIDENCE=1.
 * - Preserves owned temporary directory on failure for diagnosis.
 */
class ScopedArtifactRun {
public:
    ScopedArtifactRun(const QString &toolName, const QString &explicitDir = QString())
        : m_toolName(toolName)
    {
        const QString envDir = qEnvironmentVariable("SGE_KEEP_EVIDENCE_DIR").trimmed();
        const QString userDir = !explicitDir.trimmed().isEmpty() ? explicitDir.trimmed() : envDir;
        m_keepRequested = (qEnvironmentVariable("SGE_KEEP_EVIDENCE") == QStringLiteral("1"));

        if (!userDir.isEmpty()) {
            m_userDir = QFileInfo(userDir).absoluteFilePath();
            QDir().mkpath(m_userDir);
            m_ownsTempDir = false;
        } else {
            QString tmpBase = qEnvironmentVariable("TMPDIR").trimmed();
            if (tmpBase.isEmpty()) {
                tmpBase = QDir::tempPath();
            }
            const QString runsRoot = QDir(tmpBase).filePath(QStringLiteral("systalia-runs"));
            QDir().mkpath(runsRoot);
            pruneExpiredRuns(runsRoot, 24);

            const QString tmpl = QDir(runsRoot).filePath(toolName + QStringLiteral("-XXXXXX"));
            m_tempDir = std::make_unique<QTemporaryDir>(tmpl);
            m_tempDir->setAutoRemove(false);
            m_ownsTempDir = m_tempDir->isValid();
        }
    }

    ~ScopedArtifactRun() {
        if (!m_ownsTempDir || !m_tempDir || !m_tempDir->isValid()) {
            return;
        }
        const bool shouldKeep = m_keepRequested || !m_succeeded;
        if (shouldKeep) {
            std::cout << "[SGE-EVIDENCE] Preserved ("
                      << (m_succeeded ? "success-kept" : "failed")
                      << ") in " << m_tempDir->path().toStdString() << std::endl;
            m_tempDir->setAutoRemove(false);
        } else {
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

private:
    static void pruneExpiredRuns(const QString &runsRoot, qint64 maxAgeHours) {
        QDir root(runsRoot);
        const QDateTime cutoff = QDateTime::currentDateTimeUtc().addSecs(-maxAgeHours * 3600);
        const auto entries = root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
        for (const QFileInfo &info : entries) {
            if (info.lastModified().toUTC() < cutoff) {
                QDir(info.absoluteFilePath()).removeRecursively();
            }
        }
    }

    QString m_toolName;
    QString m_userDir;
    std::unique_ptr<QTemporaryDir> m_tempDir;
    bool m_ownsTempDir = false;
    bool m_keepRequested = false;
    bool m_succeeded = false;
};

} // namespace Systalia::Tools
