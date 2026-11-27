#pragma once

#include <wglobal.h>
#include <wseat.h>
#include <wxdgtoplevelsurface.h>

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WSeat;
class WSurface;
class WXdgShell;
class WXdgToplevelSurface;
WAYLIB_SERVER_END_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

class WaylibSurfaceListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SurfaceRole = Qt::UserRole + 1,
        ShellSurfaceRole,
        TitleRole,
        AppIdRole,
        ActivatedRole,
        MinimizedRole,
        MaximizedRole,
        SurfaceIdRole,
        ZOrderRole
    };

    struct Entry {
        quint64 surfaceId = 0;
        QPointer<WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface> shellSurface;
        int zOrder = 1;
        bool minimized = false;
        bool maximized = false;
        bool activated = false;
    };

    explicit WaylibSurfaceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int indexOf(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface) const;
    Entry *entryAt(int row);
    const Entry *entryAt(int row) const;
    const QList<Entry> &entries() const noexcept { return m_entries; }

    void appendEntry(const Entry &entry);
    void removeRowAt(int row);
    void notifyRowChanged(int row, const QList<int> &roles = {});
    void clearAll();

private:
    QList<Entry> m_entries;
};

class WaylibSurfaceAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QList<QObject *> surfaces READ surfaces NOTIFY surfacesChanged)
    Q_PROPERTY(QAbstractListModel *windowModel READ windowModel CONSTANT)
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY surfacesChanged)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *activeSurface READ activeSurface NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QString activeAppId READ activeAppId NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QStringList activeAppIds READ activeAppIds NOTIFY surfacesChanged)

public:
    explicit WaylibSurfaceAdapter(QObject *parent = nullptr);
    ~WaylibSurfaceAdapter() override = default;

    void attachXdgShell(WAYLIB_SERVER_NAMESPACE::WXdgShell *xdgShell);
    void attachSeat(WAYLIB_SERVER_NAMESPACE::WSeat *seat);
    void reset();

    [[nodiscard]] QList<QObject *> surfaces() const;
    [[nodiscard]] QAbstractListModel *windowModel() const noexcept { return m_model; }
    [[nodiscard]] int surfaceCount() const noexcept { return m_model->rowCount(); }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *activeSurface() const noexcept {
        return m_activeSurface.data();
    }
    [[nodiscard]] QString activeTitle() const;
    [[nodiscard]] QString activeAppId() const;
    [[nodiscard]] QStringList activeAppIds() const;

    Q_INVOKABLE void activateSurface(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void raiseSurface(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void requestClose(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void toggleMaximize(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void toggleMinimize(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void focusOrRestore(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    Q_INVOKABLE void resizeSurface(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface,
                                   int width, int height);
    Q_INVOKABLE void closeActiveSurface();
    Q_INVOKABLE void cycleFocus();
    Q_INVOKABLE int zOrderForSurface(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface) const;
    Q_INVOKABLE bool isMinimized(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface) const;
    Q_INVOKABLE bool isMaximized(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface) const;

Q_SIGNALS:
    void surfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfacesChanged();
    void activeSurfaceChanged();
    void titleChanged(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, const QString &title);
    void appIdChanged(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, const QString &appId);

private:
    void onToplevelAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onToplevelRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void removeSurfaceInternal(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, bool emitRemoved);
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *topVisibleSurface(
        WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *exclude = nullptr) const;
    void syncSeatFocus(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);

    WaylibSurfaceListModel *m_model = nullptr;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSeat> m_seat;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface> m_activeSurface;
    quint64 m_nextSurfaceId = 0;
    int m_nextZOrder = 0;
};

} // namespace Systalia::Compositor::WaylibBackend
