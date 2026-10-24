#pragma once

#include <QtWaylandCompositor/QWaylandCompositor>
#include <QtWaylandCompositor/QWaylandSurface>
#include <QtWaylandCompositor/QWaylandClient>
#include <QSet>
#include <QString>
#include <QByteArray>
#include <QtQml/qqmlregistration.h>

class SystaliaCompositor : public QWaylandCompositor {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int clientCount READ clientCount NOTIFY clientCountChanged)

public:
    explicit SystaliaCompositor(QObject *parent = nullptr);
    ~SystaliaCompositor() override;

    int clientCount() const { return m_clientCount; }

    Q_INVOKABLE void create() override;

signals:
    void clientCountChanged();

private slots:
    void onSurfaceCreated(QWaylandSurface *surface);
    void onSurfaceAboutToBeDestroyed(QWaylandSurface *surface);

private:
    void recalculateClientCount();

    int m_clientCount = 0;
    QSet<QWaylandSurface *> m_activeSurfaces;
};
