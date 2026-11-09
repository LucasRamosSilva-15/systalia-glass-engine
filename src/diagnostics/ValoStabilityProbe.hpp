#pragma once

// Opt-in measurement on the actual SDE executable. Inert unless the diagnostic
// environment variable is set; never a replacement desktop or software renderer.
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include <algorithm>
#include <rhi/qrhi.h>
#include <QQmlContext>
#include <QQmlExpression>
#include <QJSValue>
#include <QQuickWindow>
#include <QTimer>
#include <QElapsedTimer>
#include <QQuickPaintedItem>
#include <QPainter>
#include "gfx/v2/ValoGlassItemV2.hpp"
#include "gfx/v2/ValoBackdropProviderV2.hpp"
#include "gfx/v2/ValoOpticalContext.hpp"

// Enforce the stated "no interaction" condition on the observation window only.
// The normal SDE and the profile-button regression keep their input unchanged.
class ValoProbeInputBlocker final : public QObject {
public:
    using QObject::QObject;
    bool eventFilter(QObject *, QEvent *event) override {
        switch (event->type()) {
        case QEvent::MouseButtonPress: case QEvent::MouseButtonRelease:
        case QEvent::MouseButtonDblClick: case QEvent::MouseMove:
        case QEvent::KeyPress: case QEvent::KeyRelease: case QEvent::Wheel:
        case QEvent::TouchBegin: case QEvent::TouchUpdate: case QEvent::TouchEnd:
        case QEvent::TabletPress: case QEvent::TabletMove: case QEvent::TabletRelease:
        case QEvent::NativeGesture:
            return true;
        default:
            return false;
        }
    }
};

// Contrast stimulus only: the blur remains the production QRhi pipeline.
class ValoProbeBackdropPattern final : public QQuickPaintedItem {
public:
    using QQuickPaintedItem::QQuickPaintedItem;
    void paint(QPainter *painter) override {
        painter->fillRect(boundingRect(),Qt::white);
        for(int y=0;y<height();y+=48) for(int x=0;x<width();x+=48)
            if((x/48+y/48)%2==0) painter->fillRect(x,y,48,48,Qt::black);
        painter->setPen(QColor(255,0,0));
        for(int x=0;x<width();x+=8) painter->drawLine(x,0,x,int(height()));
        painter->setPen(Qt::green); painter->setFont(QFont("Sans",20,QFont::Bold));
        for(int y=30;y<height();y+=96)
            for(int x=20;x<width();x+=260) painter->drawText(x,y,"VALO GPU · Aa 0123");
    }
};

inline void installValoStabilityProbe(QQuickWindow *window) {
    const QString directory = qEnvironmentVariable("SGE_VALO_STABILITY_PROBE_DIR");
    if (directory.isEmpty()) return;
    QDir().mkpath(directory);
    auto evaluate = [window](const QString &code) {
        QQmlExpression e(QQmlEngine::contextForObject(window), window, code);
        const auto value = e.evaluate();
        if (e.hasError()) qWarning() << e.error();
        return value;
    };
    if (qEnvironmentVariableIsSet("SGE_VALO_INTERNAL_OVERLAP")) {
        evaluate("glassChassis.x=80; glassChassis.y=80; materialWindow.x=380; materialWindow.y=150; raiseMaterialWindow()");
    }
    if (qEnvironmentVariableIsSet("SGE_VALO_WALLPAPER_ONLY"))
        evaluate("internalWindowBackdrop.enabledForWorkspace=false");
    if (qEnvironmentVariableIsSet("SGE_VALO_PARTICLES_ONLY")) {
        for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) {
            item->setCausticIntensity(0);
            item->setOpacity(0); // Isolate sibling particles; keep visibility/phase policy active.
        }
    }
    if (qEnvironmentVariableIsSet("SGE_VALO_BACKDROP_BENCHMARK")) {
        struct Timing { QMutex mutex; QElapsedTimer cpu; QElapsedTimer settle; QList<double> cpuMs, gpuMs; };
        auto timing = std::make_shared<Timing>(); timing->settle.start();
        QObject::connect(window,&QQuickWindow::beforeFrameBegin,window,[timing] {
            timing->cpu.start();
        },Qt::DirectConnection);
        QObject::connect(window,&QQuickWindow::afterRendering,window,[timing,window] {
            if(timing->settle.elapsed()<2000) return;
            auto *swapchain=window->swapChain();
            auto *cb=swapchain ? swapchain->currentFrameCommandBuffer() : nullptr;
            const double gpu=cb ? cb->lastCompletedGpuTime()*1000.0 : 0.0;
            QMutexLocker lock(&timing->mutex);
            if(gpu>0) timing->gpuMs.append(gpu);
        },Qt::DirectConnection);
        QObject::connect(window,&QQuickWindow::afterFrameEnd,window,[timing] {
            if(timing->settle.elapsed()<2000 || !timing->cpu.isValid()) return;
            QMutexLocker lock(&timing->mutex);
            timing->cpuMs.append(timing->cpu.nsecsElapsed()/1000000.0);
        },Qt::DirectConnection);
        QObject::connect(qApp,&QCoreApplication::aboutToQuit,window,[timing,directory] {
            QMutexLocker lock(&timing->mutex);
            auto stats=[](QList<double> values) {
                std::sort(values.begin(),values.end()); double sum=0; for(double v:values) sum+=v;
                return QJsonObject{{"samples",values.size()},{"meanMs",values.isEmpty()?0:sum/values.size()},
                    {"p95Ms",values.isEmpty()?0:values[qMin(values.size()-1,qsizetype(values.size()*.95))]}};
            };
            QFile file(directory+"/frame-timings.json");
            if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(QJsonObject{
                {"cpuFrame",stats(timing->cpuMs)},{"gpuCompletedFrame",stats(timing->gpuMs)},
                {"gpuTimingAvailable",!timing->gpuMs.isEmpty()}, {"warmupMs",2000}}).toJson());
        });
        if(!qEnvironmentVariableIsSet("SGE_VALO_OPTICAL_TIMELINE"))
            QTimer::singleShot(8000,window,[]{QCoreApplication::quit();});
    }
    const bool opticalTimeline = qEnvironmentVariableIsSet("SGE_VALO_OPTICAL_TIMELINE");
    if (opticalTimeline || qEnvironmentVariableIsSet("SGE_VALO_OPTICAL_PROFILE")) {
        const int profile = qBound(0,qEnvironmentVariableIntValue("SGE_VALO_OPTICAL_PROFILE"),2);
        evaluate(QString("materialWindow.blurProfile = %1").arg(profile));
        for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) item->setBlurProfile(profile);
    }
    const QString pass = qEnvironmentVariable("SGE_VALO_OPTICAL_PASS");
    if (!pass.isEmpty()) {
        const int mode = pass == "E" ? 5 : pass == "I" ? 7 : pass == "J" ? 8 : pass == "K" ? 9 : 0;
        for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) {
            item->setDebugMode(mode);
            if(mode == 5) item->setCausticIntensity(0); // Isolate blur without animation-driven repair.
        }
    }
    if (qEnvironmentVariableIsSet("SGE_VALO_BLUR_PATTERN")) {
        const auto result = evaluate("backdropGrab");
        QObject *grab = result.metaType().id() == qMetaTypeId<QJSValue>()
            ? result.value<QJSValue>().toQObject() : result.value<QObject *>();
        auto *original = grab ? grab->property("sourceItem").value<QQuickItem *>() : nullptr;
        if (!original) {
            QTimer::singleShot(0,window,[] { QCoreApplication::exit(3); });
            return;
        }
        auto *pattern = new ValoProbeBackdropPattern(original->parentItem());
        pattern->setObjectName("valoDiagnosticContrastPattern");
        pattern->setSize(QSizeF(window->width(),window->height())); pattern->setZ(original->z()+1);
        const bool assigned = grab->setProperty("sourceItem",QVariant::fromValue<QQuickItem *>(pattern))
                              && grab->property("sourceItem").value<QQuickItem *>() == pattern;
        QFile config(directory + "/pattern-config.json");
        if(config.open(QIODevice::WriteOnly))
            config.write(QJsonDocument(QJsonObject{{"assigned",assigned},
                {"sourceName",grab->property("sourceItem").value<QQuickItem *>()->objectName()}}).toJson());
        if (!assigned) QTimer::singleShot(0,window,[] { QCoreApplication::exit(3); });
    }
    const int cycles = qEnvironmentVariableIntValue("SGE_VALO_PROFILE_STRESS");
    if (cycles > 0) {
        auto *timer = new QTimer(window);
        timer->setInterval(300);
        QObject::connect(timer, &QTimer::timeout, window,
            [timer, window, evaluate, cycles, directory, step = 0]() mutable {
                if (step >= cycles * 6) {
                    timer->stop();
                    QFile done(directory + "/stress.json");
                    if (done.open(QIODevice::WriteOnly))
                        done.write(QJsonDocument(QJsonObject{{"completedTransitions", step}, {"backend", "Vulkan"}}).toJson());
                    qInfo() << "VALO profile stress complete" << step;
                    QCoreApplication::quit();
                    return;
                }
                if (!window->isExposed()) {
                    window->raise();
                    window->requestActivate();
                    return;
                }
                const int profiles[] = {0, 1, 2, 1, 0, 2};
                const int profile = profiles[step++ % 6];
                evaluate(QString("materialWindow.blurProfile = %1; %2").arg(profile).arg(
                    step % 2 ? "raiseFileWindow(); raiseMaterialWindow()"
                             : "raiseMaterialWindow(); raiseFileWindow()"));
                QTimer::singleShot(100,window,[window,directory,profile] {
                const QImage rendered = window->grabWindow();
                if (rendered.isNull()) {
                    qCritical() << "VALO missing frame at profile" << profile;
                    QCoreApplication::exit(2);
                    return;
                }
                rendered.save(directory + QString("/profile-%1.png").arg(profile));
                });
            });
        QTimer::singleShot(1000, timer, [timer] { timer->start(); });
        return;
    }
    window->installEventFilter(new ValoProbeInputBlocker(window));
    auto measure = [window, directory, evaluate](int seconds) {
        QJsonObject textures;
        const auto observation = QObject::connect(window, &QQuickWindow::beforeSynchronizing,
            window, [window, &textures] {
                int index = 0;
                for (auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) {
                    auto *provider = item->backdropProvider();
                    auto *source = provider ? provider->source() : nullptr;
                    auto *tp = source && source->isTextureProvider() ? source->textureProvider() : nullptr;
                    auto *texture = tp ? tp->texture() : nullptr;
                    if (texture) textures[QString("surface%1").arg(index)] = QJsonObject{
                        {"textureId", QString::number(reinterpret_cast<quintptr>(texture),16)},
                        {"nativeId", QString::number(reinterpret_cast<quintptr>(texture->rhiTexture()),16)},
                        {"width", texture->textureSize().width()}, {"height", texture->textureSize().height()}};
                    ++index;
                }
            }, Qt::DirectConnection);
        const QImage frame = window->grabWindow();
        QObject::disconnect(observation);
        if (frame.isNull()) { qCritical() << "VALO empty capture" << seconds; return; }
        frame.save(directory + QString("/sde-%1.png").arg(seconds));
        QJsonObject result{{"seconds", seconds}, {"exposed", window->isExposed()}, {"width", frame.width()}, {"height", frame.height()}, {"backdropTextures", textures}};
        result["backdropCoverage"] = evaluate("internalWindowBackdrop.coverageMode").toString();
        result["filesZ"] = evaluate("glassChassis.z").toDouble();
        result["materialZ"] = evaluate("materialWindow.z").toDouble();
        int index = 0;
        for (auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) {
            auto *context = item->opticalContext();
            auto *provider = item->backdropProvider();
            const auto scene = item->mapRectToScene(item->boundingRect());
            // Lower interior strip avoids header/sidebar content and telemetry.
            QRectF roi(scene.x()+scene.width()*.55, scene.y()+scene.height()*.85,
                       scene.width()*.3, scene.height()*.08);
            const double scale = double(frame.width()) / window->width();
            QRect pixels(qRound(roi.x()*scale), qRound(roi.y()*scale),
                         qRound(roi.width()*scale), qRound(roi.height()*scale));
            pixels = pixels.intersected(frame.rect());
            double luminance = 0;
            for (int y=pixels.top(); y<=pixels.bottom(); ++y)
                for (int x=pixels.left(); x<=pixels.right(); ++x) {
                    const QColor c = frame.pixelColor(x,y);
                    luminance += .2126*c.redF()+.7152*c.greenF()+.0722*c.blueF();
                }
            if (!pixels.isEmpty()) luminance /= pixels.width()*pixels.height();
            QJsonObject row{{"sceneX", scene.x()}, {"sceneY", scene.y()},
                {"luminance", luminance}, {"opacity", item->surfaceOpacity()},
                {"itemOpacity", item->opacity()}, {"caustics", item->causticIntensity()},
                {"particleIntensity", item->particleIntensity()}, {"frozen", item->isCausticFrozen()}, {"causticTime", item->causticTime()},
                {"active", item->isActive()}, {"ior", item->ior()},
                {"dispersion", item->chromaticDispersion()},
                {"tint", item->volumetricTint().name(QColor::HexArgb)},
                {"profile", item->blurProfile()}, {"animationRunning", item->animationRunning()},
                {"animationIntervalMs", item->animationIntervalMs()},
                {"blurStatus", provider ? provider->blurStatus() : QStringLiteral("no-provider")},
                {"executedPasses", provider ? provider->executedPasses() : 0},
                {"dirty", provider ? provider->isDirty() : false}};
            if (context) {
                row["effectiveOpacity"] = context->effectiveOpacity(item->surfaceOpacity());
                row["ambient"] = context->ambientLuminance();
                row["adaptiveOffset"] = context->adaptiveOpacityOffset();
                row["energy"] = context->energySavingFactor();
                row["animateAllowed"] = context->shouldAnimateCaustics();
                row["reducedMotion"] = context->prefersReducedMotion();
            }
            result[QString("surface%1").arg(index++)] = row;
        }
        QFile file(directory + QString("/state-%1.json").arg(seconds));
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(result).toJson());
        qInfo() << "VALO stability sample" << seconds;
        if (seconds == 60) QCoreApplication::quit();
    };
    // Settle the actual SDE before observation; no input in timeline mode.
    QTimer::singleShot(1000, window, [window, measure, opticalTimeline, evaluate] {
        measure(0);
        if (qEnvironmentVariableIsSet("SGE_VALO_POLICY_PROBE")) {
            QTimer::singleShot(2000,window,[window,measure] {
                for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>())
                    if(item->opticalContext()) item->opticalContext()->setPrefersReducedMotion(true);
                measure(2);
            });
            QTimer::singleShot(4000,window,[window,measure] {
                measure(4);
                for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) {
                    if(item->opticalContext()) item->opticalContext()->setPrefersReducedMotion(false);
                    item->setVisible(false);
                }
            });
            QTimer::singleShot(6000,window,[window,measure] {
                measure(6);
                for(auto *item : window->findChildren<Systalia::Gfx::V2::ValoGlassItemV2 *>()) item->setVisible(true);
            });
            QTimer::singleShot(8000,window,[measure] { measure(8); QCoreApplication::quit(); });
            return;
        }
        if (qEnvironmentVariableIsSet("SGE_VALO_INTERNAL_OVERLAP")) {
            QTimer::singleShot(7000,window,[evaluate] { evaluate("raiseFileWindow()"); });
            QTimer::singleShot(22000,window,[evaluate] { evaluate("raiseMaterialWindow()"); });
            QTimer::singleShot(45000,window,[evaluate] { evaluate("raiseFileWindow()"); });
        }
        const QList<int> samples = opticalTimeline ? QList<int>{5,15,30,60} : QList<int>{10,30,60};
        for (int seconds : samples)
            QTimer::singleShot(seconds*1000, window, [measure, seconds] { measure(seconds); });
    });
}
