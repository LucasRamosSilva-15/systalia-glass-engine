#pragma once

#include <wglobal.h>
#include <wqmlcreator.h>
#include <wquickoutputlayout.h>

#include <QObject>
#include <QSize>
#include <QString>

WAYLIB_SERVER_BEGIN_NAMESPACE
class WBackend;
class WOutput;
class WOutputRenderWindow;
class WOutputViewport;
class WQuickOutputLayout;
class WServer;
WAYLIB_SERVER_END_NAMESPACE

class QQmlEngine;

namespace Systalia::Compositor::WaylibBackend {

class WaylibOutputAdapter final : public QObject {
    Q_OBJECT
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WQmlCreator *outputCreator READ outputCreator CONSTANT)
    Q_PROPERTY(WAYLIB_SERVER_NAMESPACE::WQuickOutputLayout *outputLayout READ outputLayout CONSTANT)
    Q_PROPERTY(int outputCount READ outputCount NOTIFY outputCountChanged)

public:
    explicit WaylibOutputAdapter(WAYLIB_SERVER_NAMESPACE::WServer *server, QObject *parent = nullptr);
    ~WaylibOutputAdapter() override = default;

    void attachBackend(WAYLIB_SERVER_NAMESPACE::WBackend *backend,
                       WAYLIB_SERVER_NAMESPACE::WOutputRenderWindow *window,
                       QQmlEngine *qmlEngine);
    void reset();

    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WQmlCreator *outputCreator() const noexcept {
        return m_outputCreator;
    }
    [[nodiscard]] WAYLIB_SERVER_NAMESPACE::WQuickOutputLayout *outputLayout() const noexcept {
        return m_outputLayout;
    }
    [[nodiscard]] int outputCount() const noexcept { return m_outputs.size(); }

Q_SIGNALS:
    void outputAdded(const QString &name, const QSize &size);
    void outputRemoved(const QString &name);
    void outputCountChanged();

private:
    void onOutputAdded(WAYLIB_SERVER_NAMESPACE::WOutput *output, QQmlEngine *qmlEngine);
    void onOutputRemoved(WAYLIB_SERVER_NAMESPACE::WOutput *output);
    void onViewportInitialized(WAYLIB_SERVER_NAMESPACE::WOutputViewport *viewport);

    WAYLIB_SERVER_NAMESPACE::WQmlCreator *m_outputCreator = nullptr;
    WAYLIB_SERVER_NAMESPACE::WQuickOutputLayout *m_outputLayout = nullptr;
    QList<WAYLIB_SERVER_NAMESPACE::WOutput *> m_outputs;
    bool m_windowConnected = false;
};

} // namespace Systalia::Compositor::WaylibBackend
