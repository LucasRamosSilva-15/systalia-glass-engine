#pragma once

#include <wglobal.h>
#include <wseat.h>
#include <wxdgpopupsurface.h>
#include <wxdgtoplevelsurface.h>

#include <QAbstractListModel>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WOutput;
class WSurface;
class WXdgShell;
WAYLIB_SERVER_END_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

class WaylibOutputAdapter;

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

class WaylibPopupListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        SurfaceRole = Qt::UserRole + 1,
        PopupSurfaceRole,
        ParentSurfaceRole,
        PopupXRole,
        PopupYRole
    };

    struct Entry {
        QPointer<WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface> popupSurface;
    };

    explicit WaylibPopupListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int indexOf(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup) const;
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
    Q_PROPERTY(QAbstractListModel *popupModel READ popupModel CONSTANT)
    Q_PROPERTY(int surfaceCount READ surfaceCount NOTIFY surfacesChanged)
    Q_PROPERTY(int popupCount READ popupCount NOTIFY popupsChanged)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *activeSurface READ activeSurface NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QString activeAppId READ activeAppId NOTIFY activeSurfaceChanged)
    Q_PROPERTY(QStringList activeAppIds READ activeAppIds NOTIFY surfacesChanged)

public:
    explicit WaylibSurfaceAdapter(QObject *parent = nullptr);
    ~WaylibSurfaceAdapter() override = default;

    void attachXdgShell(WAYLIB_SERVER_NAMESPACE::WXdgShell *xdgShell);
    void attachSeat(WAYLIB_SERVER_NAMESPACE::WSeat *seat);
    void attachOutputAdapter(WaylibOutputAdapter *outputAdapter);
    void reset();

    [[nodiscard]] QList<QObject *> surfaces() const;
    [[nodiscard]] QAbstractListModel *windowModel() const noexcept { return m_model; }
    [[nodiscard]] QAbstractListModel *popupModel() const noexcept { return m_popupModel; }
    [[nodiscard]] int surfaceCount() const noexcept { return m_model->rowCount(); }
    [[nodiscard]] int popupCount() const noexcept { return m_popupModel->rowCount(); }
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
    Q_INVOKABLE bool isSurfaceOpaque(WAYLIB_SERVER_NAMESPACE::WSurface *surface) const;

Q_SIGNALS:
    void surfaceAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfaceRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void surfacesChanged();
    void popupAdded(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup);
    void popupRemoved(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup);
    void popupsChanged();
    void activeSurfaceChanged();
    void titleChanged(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, const QString &title);
    void appIdChanged(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, const QString &appId);
    void moveRequested(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void resizeRequested(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, int edgeFlags);

private:
    void onToplevelAdded(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onToplevelRemoved(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);
    void onPopupAdded(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup);
    void onPopupRemoved(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup);
    void removeSurfaceInternal(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface, bool emitRemoved);
    void removePopupInternal(WAYLIB_SERVER_NAMESPACE::WXdgPopupSurface *popup, bool emitRemoved);
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *topVisibleSurface(
        WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *exclude = nullptr) const;
    void syncSeatFocus(WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface *surface);

    WaylibSurfaceListModel *m_model = nullptr;
    WaylibPopupListModel *m_popupModel = nullptr;
    QPointer<WAYLIB_SERVER_NAMESPACE::WSeat> m_seat;
    QPointer<WaylibOutputAdapter> m_outputAdapter;
    QPointer<WAYLIB_SERVER_NAMESPACE::WXdgToplevelSurface> m_activeSurface;
    quint64 m_nextSurfaceId = 0;
    int m_nextZOrder = 0;
};

} // namespace Systalia::Compositor::WaylibBackend
