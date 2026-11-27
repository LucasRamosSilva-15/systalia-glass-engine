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
#include <QtMath>

WAYLIB_SERVER_USE_NAMESPACE

namespace Systalia::Compositor::WaylibBackend {

WaylibOutputAdapter::WaylibOutputAdapter(WServer *server, QObject *parent)
    : QObject(parent),
      m_outputCreator(new WQmlCreator(this)),
      m_outputLayout(new WQuickOutputLayout(server)) {}

WaylibOutputAdapter::~WaylibOutputAdapter() {
    reset();
}

void WaylibOutputAdapter::attachBackend(WBackend *backend, WOutputRenderWindow *window, QQmlEngine *qmlEngine) {
    if (!backend || !qmlEngine) {
        return;
    }

    m_window = window;
    if (m_backend != backend) {
        if (m_backend) {
            m_backend->disconnect(this);
        }
        m_backend = backend;
        connect(backend, &WBackend::outputAdded, this, [this, qmlEngine](WOutput *output) {
            onOutputAdded(output, qmlEngine);
        });
        connect(backend, &WBackend::outputRemoved, this, &WaylibOutputAdapter::onOutputRemoved);
    }

    for (WOutput *existing : backend->outputList()) {
        onOutputAdded(existing, qmlEngine);
    }

    if (window && !m_windowConnected) {
        m_windowConnected = true;
        connect(window, &WOutputRenderWindow::outputViewportInitialized,
                this, &WaylibOutputAdapter::onViewportInitialized);
    }
}

void WaylibOutputAdapter::reset() {
    if (m_backend) {
        m_backend->disconnect(this);
        m_backend = nullptr;
    }
    for (WOutput *out : std::as_const(m_outputs)) {
        if (out) {
            out->disconnect(this);
            const auto cursors = out->cursorList();
            for (WCursor *cursor : cursors) {
                out->removeCursor(cursor);
            }
        }
        removeRequestStateListener(out);
        m_outputCreator->removeByOwner(out);
    }
    const auto remaining = m_requestListeners.keys();
    for (WOutput *out : remaining) {
        removeRequestStateListener(out);
    }
    if (!m_outputs.isEmpty()) {
        m_outputs.clear();
        Q_EMIT outputCountChanged();
    }
    if (m_primaryOutputSize.isValid()) {
        m_primaryOutputSize = QSize();
        Q_EMIT primaryOutputSizeChanged(m_primaryOutputSize);
    }
}

bool WaylibOutputAdapter::requestOutputResize(int width, int height) {
    if (width <= 0 || height <= 0 || m_outputs.isEmpty()) {
        return false;
    }
    WOutput *primary = m_outputs.first();
    if (!primary || !primary->handle()) {
        return false;
    }
    wlr_output *wlrOut = primary->handle();
    wlr_output_state newState{};
    wlr_output_state_init(&newState);
    wlr_output_state_set_custom_mode(&newState, width, height, 0);
    wlr_output_state_set_enabled(&newState, true);
    const bool ok = wlr_output_commit_state(wlrOut, &newState);
    wlr_output_state_finish(&newState);
    if (ok) {
        syncPrimaryOutputSize();
    }
    return ok;
}

void WaylibOutputAdapter::onOutputAdded(WOutput *output, QQmlEngine *qmlEngine) {
    if (!output || !qmlEngine || m_outputs.contains(output)) {
        return;
    }
    m_outputs.append(output);

    installRequestStateListener(output);
    connect(output, &WOutput::effectiveSizeChanged,
            this, &WaylibOutputAdapter::syncPrimaryOutputSize);
    connect(output, &WOutput::transformedSizeChanged,
            this, &WaylibOutputAdapter::syncPrimaryOutputSize);
    connect(output, &WOutput::scaleChanged,
            this, &WaylibOutputAdapter::syncPrimaryOutputSize);

    auto initProps = qmlEngine->newObject();
    initProps.setProperty(QStringLiteral("waylandOutput"), qmlEngine->toScriptValue(output));
    initProps.setProperty(QStringLiteral("layout"), qmlEngine->toScriptValue(m_outputLayout));
    initProps.setProperty(QStringLiteral("x"), qmlEngine->toScriptValue(m_outputLayout->implicitWidth()));
    m_outputCreator->add(output, initProps);

    syncPrimaryOutputSize();
    Q_EMIT outputAdded(output->name(), output->size());
    Q_EMIT outputCountChanged();
}

void WaylibOutputAdapter::onOutputRemoved(WOutput *output) {
    if (!output) {
        return;
    }
    removeRequestStateListener(output);
    output->disconnect(this);
    if (!m_outputs.removeOne(output)) {
        return;
    }
    const QString name = output->name();
    m_outputCreator->removeByOwner(output);
    syncPrimaryOutputSize();
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
        output->setForceSoftwareCursor(true);
        wlr_output_state newState{};
        wlr_output_state_init(&newState);
        if (!wlrOut->current_mode) {
            if (auto *mode = wlr_output_preferred_mode(wlrOut)) {
                wlr_output_state_set_mode(&newState, mode);
            } else {
                const int targetW = (m_window && m_window->width() > 0) ? qRound(m_window->width()) : 1280;
                const int targetH = (m_window && m_window->height() > 0) ? qRound(m_window->height()) : 760;
                wlr_output_state_set_custom_mode(&newState, targetW, targetH, 0);
            }
        }
        wlr_output_state_set_enabled(&newState, true);
        if (!wlr_output_commit_state(wlrOut, &newState)) {
            qWarning() << "[SGE-WAYLIB] Falha ao habilitar output nested:" << wlrOut->name;
        }
        wlr_output_state_finish(&newState);
    }
    syncPrimaryOutputSize();
    Q_EMIT outputReady(output);
}

void WaylibOutputAdapter::installRequestStateListener(WOutput *output) {
    if (!output || !output->handle() || m_requestListeners.contains(output)) {
        return;
    }
    auto *entry = new RequestStateListener();
    entry->adapter = this;
    entry->output = output;
    wl_list_init(&entry->listener.link);
    entry->listener.notify = &WaylibOutputAdapter::handleWlrRequestState;
    wl_signal_add(&output->handle()->events.request_state, &entry->listener);
    m_requestListeners.insert(output, entry);
}

void WaylibOutputAdapter::removeRequestStateListener(WOutput *output) {
    if (!output) {
        return;
    }
    if (RequestStateListener *entry = m_requestListeners.take(output)) {
        wl_list_remove(&entry->listener.link);
        delete entry;
    }
}

void WaylibOutputAdapter::handleWlrRequestState(wl_listener *listener, void *data) {
    auto *entry = wl_container_of(listener, static_cast<RequestStateListener *>(nullptr), listener);
    auto *event = static_cast<const wlr_output_event_request_state *>(data);
    if (!entry || !entry->adapter || !event || !event->output || !event->state) {
        return;
    }

    wlr_output_state newState{};
    wlr_output_state_init(&newState);

    bool hasChanges = false;
    if (event->state->committed & WLR_OUTPUT_STATE_MODE) {
        if (event->state->mode_type == WLR_OUTPUT_STATE_MODE_FIXED && event->state->mode) {
            wlr_output_state_set_mode(&newState, event->state->mode);
            hasChanges = true;
        } else if (event->state->custom_mode.width > 0 && event->state->custom_mode.height > 0) {
            wlr_output_state_set_custom_mode(&newState,
                                             event->state->custom_mode.width,
                                             event->state->custom_mode.height,
                                             event->state->custom_mode.refresh);
            hasChanges = true;
        }
    }
    if (event->state->committed & WLR_OUTPUT_STATE_ENABLED) {
        wlr_output_state_set_enabled(&newState, event->state->enabled);
        hasChanges = true;
    }
    if (event->state->committed & WLR_OUTPUT_STATE_SCALE) {
        wlr_output_state_set_scale(&newState, event->state->scale);
        hasChanges = true;
    }

    if (hasChanges) {
        if (!wlr_output_commit_state(event->output, &newState)) {
            qWarning() << "[SGE-WAYLIB] Falha ao aplicar request_state no output:" << event->output->name;
        } else {
            entry->adapter->syncPrimaryOutputSize();
        }
    }
    wlr_output_state_finish(&newState);
}

void WaylibOutputAdapter::syncPrimaryOutputSize() {
    if (m_outputs.isEmpty()) {
        return;
    }
    WOutput *primary = m_outputs.first();
    if (!primary) {
        return;
    }
    const QSize logicalSize = primary->effectiveSize();
    if (!logicalSize.isValid() || logicalSize.isEmpty()) {
        return;
    }
    if (m_primaryOutputSize != logicalSize) {
        m_primaryOutputSize = logicalSize;
        if (m_window) {
            if (qRound(m_window->width()) != logicalSize.width()) {
                m_window->setWidth(logicalSize.width());
            }
            if (qRound(m_window->height()) != logicalSize.height()) {
                m_window->setHeight(logicalSize.height());
            }
            m_window->update();
        }
        Q_EMIT primaryOutputSizeChanged(logicalSize);
    }
}

} // namespace Systalia::Compositor::WaylibBackend
