#include "AppRegistryService.hpp"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QLocale>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QStandardPaths>
#include <QRegularExpression>
#include <algorithm>
#include <utility>

namespace {

QString localizedValue(const QSettings &settings, const QString &key) {
    const QString localeName = QLocale().name();
    const QString language = QLocale().name().section(QLatin1Char('_'), 0, 0);
    const QStringList localizedKeys{
        key + QLatin1Char('[') + localeName + QLatin1Char(']'),
        key + QLatin1Char('[') + language + QLatin1Char(']')
    };
    for (const QString &localizedKey : localizedKeys) {
        const QString value = settings.value(QStringLiteral("Desktop Entry/") + localizedKey)
                                  .toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return settings.value(QStringLiteral("Desktop Entry/") + key).toString().trimmed();
}

QString rawDesktopEntryValue(const QString &desktopFile, const QByteArray &key) {
    QFile file(desktopFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }

    bool inDesktopEntry = false;
    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (line.startsWith('[') && line.endsWith(']')) {
            inDesktopEntry = line == QByteArrayLiteral("[Desktop Entry]");
            continue;
        }
        if (!inDesktopEntry || line.isEmpty() || line.startsWith('#')) {
            continue;
        }
        const QByteArray prefix = key + '=';
        if (line.startsWith(prefix)) {
            return QString::fromUtf8(line.mid(prefix.size())).trimmed();
        }
    }
    return {};
}

bool isFileFieldCode(QChar code) {
    return code == QLatin1Char('f') || code == QLatin1Char('F')
           || code == QLatin1Char('u') || code == QLatin1Char('U')
           || code == QLatin1Char('d') || code == QLatin1Char('D')
           || code == QLatin1Char('n') || code == QLatin1Char('N')
           || code == QLatin1Char('v') || code == QLatin1Char('m');
}

QStringList commandArguments(const QStringList &tokens, const QString &name,
                             const QString &icon, const QString &desktopFile) {
    QStringList expanded;
    for (qsizetype tokenIndex = 1; tokenIndex < tokens.size(); ++tokenIndex) {
        const QString token = tokens.at(tokenIndex);
        if (token == QStringLiteral("%i")) {
            if (!icon.isEmpty()) {
                expanded.append(QStringLiteral("--icon"));
                expanded.append(icon);
            }
            continue;
        }

        bool discardToken = false;
        QString output;
        output.reserve(token.size());
        for (qsizetype index = 0; index < token.size(); ++index) {
            const QChar character = token.at(index);
            if (character != QLatin1Char('%') || index + 1 >= token.size()) {
                output.append(character);
                continue;
            }

            const QChar code = token.at(++index);
            if (isFileFieldCode(code)) {
                discardToken = true;
                break;
            }
            if (code == QLatin1Char('%')) {
                output.append(QLatin1Char('%'));
            } else if (code == QLatin1Char('c')) {
                output.append(name);
            } else if (code == QLatin1Char('k')) {
                output.append(desktopFile);
            } else if (code == QLatin1Char('i')) {
                output.append(icon);
            }
        }
        if (!discardToken && !output.isEmpty()) {
            expanded.append(output);
        }
    }
    return expanded;
}

QString desktopIdForPath(const QString &directory, const QString &filePath) {
    QString relativePath = QDir(directory).relativeFilePath(filePath);
    relativePath.replace(QLatin1Char('/'), QLatin1Char('-'));
    return relativePath;
}

} // namespace

AppRegistryService::AppRegistryService(QObject *parent)
    : AppRegistryService(QStringList{}, parent) {
}

AppRegistryService::AppRegistryService(QStringList applicationDirectories, QObject *parent)
    : QAbstractListModel(parent),
      m_applicationDirectories(applicationDirectories.isEmpty()
                                   ? defaultApplicationDirectories()
                                   : std::move(applicationDirectories)) {
    refresh();
}

int AppRegistryService::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_visibleRows.size();
}

QVariant AppRegistryService::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_visibleRows.size()) {
        return {};
    }

    const ApplicationEntry &entry = m_entries.at(m_visibleRows.at(index.row()));
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return entry.name;
    case ExecRole:
        return entry.command;
    case ArgumentsRole:
        return entry.arguments;
    case CategoriesRole:
        return entry.categories;
    case IconRole:
        return entry.icon;
    case DesktopIdRole:
        return entry.desktopId;
    case DesktopFileRole:
        return entry.desktopFile;
    default:
        return {};
    }
}

QHash<int, QByteArray> AppRegistryService::roleNames() const {
    return {
        {NameRole, QByteArrayLiteral("name")},
        {ExecRole, QByteArrayLiteral("command")},
        {ArgumentsRole, QByteArrayLiteral("arguments")},
        {CategoriesRole, QByteArrayLiteral("categories")},
        {IconRole, QByteArrayLiteral("icon")},
        {DesktopIdRole, QByteArrayLiteral("desktopId")},
        {DesktopFileRole, QByteArrayLiteral("desktopFile")}
    };
}

QString AppRegistryService::filterText() const {
    return m_filterText;
}

void AppRegistryService::setFilterText(const QString &filter) {
    const QString normalized = filter.trimmed();
    if (m_filterText == normalized) {
        return;
    }
    beginResetModel();
    m_filterText = normalized;
    rebuildVisibleEntries();
    endResetModel();
    emit filterTextChanged();
    emit applicationCountChanged();
}

void AppRegistryService::refresh() {
    beginResetModel();
    m_entries.clear();
    m_visibleRows.clear();

    QSet<QString> visitedDesktopIds;
    for (const QString &directoryPath : m_applicationDirectories) {
        const QDir directory(directoryPath);
        if (!directory.exists()) {
            continue;
        }

        QDirIterator iterator(directory.absolutePath(), {QStringLiteral("*.desktop")},
                              QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            const QString filePath = iterator.next();
            const QString desktopId = desktopIdForPath(directory.absolutePath(), filePath);
            if (visitedDesktopIds.contains(desktopId)) {
                continue;
            }
            visitedDesktopIds.insert(desktopId);

            std::optional<ApplicationEntry> entry = parseDesktopFile(filePath);
            if (!entry) {
                continue;
            }
            entry->desktopId = desktopId;
            m_entries.append(std::move(*entry));
        }
    }

    std::sort(m_entries.begin(), m_entries.end(), [](const ApplicationEntry &left,
                                                     const ApplicationEntry &right) {
        const int comparison = QString::compare(left.name, right.name, Qt::CaseInsensitive);
        return comparison == 0 ? left.desktopId < right.desktopId : comparison < 0;
    });
    rebuildVisibleEntries();
    endResetModel();
    emit applicationCountChanged();
}

std::optional<AppRegistryService::ApplicationEntry>
AppRegistryService::parseDesktopFile(const QString &desktopFile) {
    if (!QFileInfo(desktopFile).isFile()) {
        return std::nullopt;
    }

    QSettings settings(desktopFile, QSettings::IniFormat);
    if (settings.status() != QSettings::NoError) {
        return std::nullopt;
    }

    const QString group = QStringLiteral("Desktop Entry/");
    if (settings.value(group + QStringLiteral("Type"), QStringLiteral("Application"))
            .toString().compare(QStringLiteral("Application"), Qt::CaseInsensitive) != 0
        || settings.value(group + QStringLiteral("Hidden"), false).toBool()
        || settings.value(group + QStringLiteral("NoDisplay"), false).toBool()) {
        return std::nullopt;
    }

    ApplicationEntry entry;
    entry.name = localizedValue(settings, QStringLiteral("Name"));
    entry.exec = settings.value(group + QStringLiteral("Exec")).toString().trimmed();
    entry.icon = localizedValue(settings, QStringLiteral("Icon"));
    entry.desktopFile = QFileInfo(desktopFile).absoluteFilePath();
    entry.categories = rawDesktopEntryValue(desktopFile, QByteArrayLiteral("Categories"))
                           .split(QLatin1Char(';'), Qt::SkipEmptyParts);

    if (entry.name.isEmpty() || entry.exec.isEmpty()) {
        return std::nullopt;
    }

    const QStringList tokens = QProcess::splitCommand(entry.exec);
    if (tokens.isEmpty() || tokens.first().isEmpty()) {
        return std::nullopt;
    }
    entry.command = tokens.first();
    entry.arguments = commandArguments(tokens, entry.name, entry.icon, entry.desktopFile);
    return entry;
}

QStringList AppRegistryService::defaultApplicationDirectories() const {
    QStringList directories;
    QString dataHome = qEnvironmentVariable("XDG_DATA_HOME");
    if (dataHome.isEmpty()) {
        dataHome = QDir::home().filePath(QStringLiteral(".local/share"));
    }
    directories.append(QDir(dataHome).filePath(QStringLiteral("applications")));

    QStringList dataDirectories = qEnvironmentVariable("XDG_DATA_DIRS")
                                      .split(QLatin1Char(':'), Qt::SkipEmptyParts);
    if (dataDirectories.isEmpty()) {
        dataDirectories = {QStringLiteral("/usr/local/share"), QStringLiteral("/usr/share")};
    }
    for (const QString &dataDirectory : std::as_const(dataDirectories)) {
        directories.append(QDir(dataDirectory).filePath(QStringLiteral("applications")));
    }
    return directories;
}

void AppRegistryService::rebuildVisibleEntries() {
    const QString query = m_filterText.toCaseFolded();
    m_visibleRows.clear();
    for (qsizetype index = 0; index < m_entries.size(); ++index) {
        const ApplicationEntry &entry = m_entries.at(index);
        const QString searchText = entry.name + QLatin1Char(' ') + entry.command + QLatin1Char(' ')
                                   + entry.categories.join(QLatin1Char(' '));
        if (query.isEmpty() || searchText.toCaseFolded().contains(query)) {
            m_visibleRows.append(static_cast<int>(index));
        }
    }
}

DesktopIconProvider::DesktopIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap) {
}

QPixmap DesktopIconProvider::requestPixmap(const QString &id, QSize *size,
                                           const QSize &requestedSize) {
    const QSize targetSize = requestedSize.isValid() ? requestedSize : QSize(64, 64);
    QIcon icon = QIcon::fromTheme(id);
    if (icon.isNull() && QFileInfo::exists(id)) {
        icon = QIcon(id);
    }

    if (size) {
        *size = targetSize;
    }
    if (icon.isNull()) {
        QPixmap empty(targetSize);
        empty.fill(Qt::transparent);
        return empty;
    }
    return icon.pixmap(targetSize);
}
