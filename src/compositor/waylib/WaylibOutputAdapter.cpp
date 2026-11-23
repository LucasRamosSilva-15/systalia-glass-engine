#include "WaylibOutputAdapter.hpp"

#include <wbackend.h>
#include <woutput.h>
#include <woutputrenderwindow.h>
#include <woutputviewport.h>
#include <wquickoutputlayout.h>
#include <wserver.h>

#include <wlr_all.h>

#include <QDebug>
#include <QQmlEngine>

WAYLIB_SERVER_USE_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

WaylibOutputAdapter::WaylibOutputAdapter(WServer *server, QObject *parent)
    : QObject(parent),
      m_outputCreator(new WQmlCreator(this)),
      m_outputLayout(new WQuickOutputLayout(server)) {}

void WaylibOutputAdapter::attachBackend(WBackend *backend, WOutputRenderWindow *window, QQmlEngine *qmlEngine) {
    if (!backend || !qmlEngine) {
        return;
    }

    connect(backend, &WBackend::outputAdded, this, [this, qmlEngine](WOutput *output) {
        onOutputAdded(output, qmlEngine);
    });
    connect(backend, &WBackend::outputRemoved, this, &WaylibOutputAdapter::onOutputRemoved);

    if (window && !m_windowConnected) {
        m_windowConnected = true;
        connect(window, &WOutputRenderWindow::outputViewportInitialized,
                this, &WaylibOutputAdapter::onViewportInitialized);
    }
}

void WaylibOutputAdapter::reset() {
    for (WOutput *out : std::as_const(m_outputs)) {
        m_outputCreator->removeByOwner(out);
    }
    if (!m_outputs.isEmpty()) {
        m_outputs.clear();
        Q_EMIT outputCountChanged();
    }
}

void WaylibOutputAdapter::onOutputAdded(WOutput *output, QQmlEngine *qmlEngine) {
    if (!output || !qmlEngine || m_outputs.contains(output)) {
        return;
    }
    m_outputs.append(output);

    auto initProps = qmlEngine->newObject();
    initProps.setProperty(QStringLiteral("waylandOutput"), qmlEngine->toScriptValue(output));
    initProps.setProperty(QStringLiteral("layout"), qmlEngine->toScriptValue(m_outputLayout));
    initProps.setProperty(QStringLiteral("x"), qmlEngine->toScriptValue(m_outputLayout->implicitWidth()));
    m_outputCreator->add(output, initProps);

    const QString name = output->name();
    const QSize size = output->size();
    Q_EMIT outputAdded(name, size);
    Q_EMIT outputCountChanged();
}

void WaylibOutputAdapter::onOutputRemoved(WOutput *output) {
    if (!output || !m_outputs.removeOne(output)) {
        return;
    }
    const QString name = output->name();
    m_outputCreator->removeByOwner(output);
    Q_EMIT outputRemoved(name);
    Q_EMIT outputCountChanged();
}

void WaylibOutputAdapter::onViewportInitialized(WOutputViewport *viewport) {
    if (!viewport || !viewport->output()) {
        return;
    }
    WOutput *output = viewport->output();
    wlr_output *wlrOut = output->handle();
    if (!wlrOut) {
        return;
    }

    if (!output->property("_sgeEnabled").toBool()) {
        output->setProperty("_sgeEnabled", true);
        wlr_output_state newState{};
        wlr_output_state_init(&newState);
        if (!wlrOut->current_mode) {
            if (auto *mode = wlr_output_preferred_mode(wlrOut)) {
                wlr_output_state_set_mode(&newState, mode);
            } else {
                wlr_output_state_set_custom_mode(&newState, 1280, 720, 0);
            }
        }
        wlr_output_state_set_enabled(&newState, true);
        if (!wlr_output_commit_state(wlrOut, &newState)) {
            qWarning() << "[SGE-WAYLIB] Falha ao habilitar output nested:" << wlrOut->name;
        }
        wlr_output_state_finish(&newState);
    }
}

} // namespace Systalia::Compositor::WaylibBackend
