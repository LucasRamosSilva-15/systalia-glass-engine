#pragma once

#include <QAbstractListModel>
#include <QQuickImageProvider>
#include <QStringList>
#include <QVector>
#include <optional>

class AppRegistryService final : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterTextChanged)
    Q_PROPERTY(int applicationCount READ applicationCount NOTIFY applicationCountChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        ExecRole,
        ArgumentsRole,
        CategoriesRole,
        IconRole,
        DesktopIdRole,
        DesktopFileRole
    };
    Q_ENUM(Role)

    struct ApplicationEntry {
        QString name;
        QString exec;
        QString command;
        QStringList arguments;
        QStringList categories;
        QString icon;
        QString desktopId;
        QString desktopFile;
    };

    explicit AppRegistryService(QObject *parent = nullptr);
    explicit AppRegistryService(QStringList applicationDirectories, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filterText() const;
    int applicationCount() const { return rowCount(); }
    void setFilterText(const QString &filter);

    Q_INVOKABLE void refresh();

    static std::optional<ApplicationEntry> parseDesktopFile(const QString &desktopFile);

signals:
    void filterTextChanged();
    void applicationCountChanged();

private:
    QStringList defaultApplicationDirectories() const;
    void rebuildVisibleEntries();

    QStringList m_applicationDirectories;
    QVector<ApplicationEntry> m_entries;
    QVector<int> m_visibleRows;
    QString m_filterText;
};

class DesktopIconProvider final : public QQuickImageProvider {
public:
    DesktopIconProvider();

    QPixmap requestPixmap(const QString &id, QSize *size,
                          const QSize &requestedSize) override;
};
