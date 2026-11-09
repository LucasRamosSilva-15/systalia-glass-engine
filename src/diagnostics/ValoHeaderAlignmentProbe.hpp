#pragma once

#include <QFontInfo>
#include <QFontMetricsF>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>
#include <QImage>
#include <QJsonDocument>
#include <QFile>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <QJSValue>
#include <QtWaylandCompositor/QWaylandSeat>
#include <QtWaylandCompositor/QWaylandKeyboard>
#include "compositor/SystaliaCompositor.hpp"

// Measurements from the real SDE, enabled only for this diagnostic session.
template <typename Evaluate>
bool installValoHeaderAlignmentProbe(QQuickWindow *window, const QString &directory, Evaluate evaluate) {
    if (!qEnvironmentVariableIsSet("SGE_VALO_HEADER_ALIGNMENT_PROBE")) return false;
    evaluate("waylandServer.stopXWayland()"); // X11 content belongs to Stage 07.
    auto capture = [window,directory](const QString &name) {
        QJsonArray headers;
        auto inspect = [&](auto &&self, QQuickItem *item) -> void {
            // The title lives inside titleContent; visit its header via the visual tree.
            if (item->objectName()=="windowHeaderTitle") {
                auto *title=item;
                auto *header=item->parentItem()->parentItem();
                const auto origin=header->mapToScene(QPointF());
                QJsonObject row{{"title",title->property("text").toString()},
                    {"headerX",origin.x()},{"headerY",origin.y()},
                    {"headerWidth",header->width()},{"headerHeight",header->height()}};
                const QFont font=title->property("font").value<QFont>();
                const QFontInfo info(font);
                row["requestedFamily"]=font.family(); row["resolvedFamily"]=info.family();
                row["resolvedStyle"]=info.styleName(); row["pixelSize"]=font.pixelSize();
                row["weight"]=int(font.weight()); row["hinting"]=int(font.hintingPreference());
                row["fontAscent"]=QFontMetricsF(font).ascent();
                row["renderType"]=title->property("renderType").toInt();
                auto *ancestor=header->parentItem();
                while(ancestor && !ancestor->property("clientMapped").isValid()) ancestor=ancestor->parentItem();
                if(ancestor) {
                    auto *viewport=ancestor->findChild<QQuickItem *>("clientViewport");
                    row["clientWidth"]=ancestor->property("effectiveClientWidth").toInt();
                    row["clientHeight"]=ancestor->property("effectiveClientHeight").toInt();
                    row["frameHeight"]=ancestor->height();
                    if(viewport) row["viewport"]=QJsonObject{{"x",viewport->x()},{"y",viewport->y()},
                        {"width",viewport->width()},{"height",viewport->height()}};
                }
                for(const auto &key : {"windowHeaderTitle","windowHeaderIcon","windowHeaderBadge",
                                       "minimizeToplevelButton","maximizeToplevelButton","closeToplevelButton"}) {
                    auto *part=header->findChild<QQuickItem *>(QString::fromLatin1(key));
                    if(!part) continue;
                    const auto local=part->mapToItem(header,QPointF());
                    row[key]=QJsonObject{{"x",local.x()},{"y",local.y()},
                        {"width",part->width()},{"height",part->height()},
                        {"centerY",local.y()+part->height()/2},
                        {"baseline",local.y()+part->property("baselineOffset").toDouble()},
                        {"visible",part->isVisible()}};
                }
                headers.append(row);
            }
            for(auto *child:item->childItems()) self(self,child);
        };
        inspect(inspect,window->contentItem());
        const auto frame=window->grabWindow();
        if(frame.isNull() || !frame.save(directory+"/"+name+".png")) { QCoreApplication::exit(2); return; }
        QFile file(directory+"/"+name+".json");
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(headers).toJson());
    };
    auto step=[window,evaluate,capture](int ms,QString code,QString name) {
        QTimer::singleShot(ms,window,[window,evaluate,capture,code,name] {
            evaluate(code);
            QTimer::singleShot(400,window,[capture,name] {capture(name);});
        });
    };
    step(2000,"materialWindow.visible=false; glassChassis.x=50; glassChassis.y=80; trackedClientContainers.forEach(w=>{w.x=810;w.y=80;w.dispatchClientResize(Qt.size(685,400),false)})","01-aligned");
    QTimer::singleShot(4000,window,[window,evaluate,directory] {
        evaluate("trackedClientContainers.forEach(w=>waylandServer.activateToplevel(w.toplevel))");
        const QVariant value=evaluate("waylandServer");
        auto *object=value.metaType().id()==qMetaTypeId<QJSValue>() ? value.value<QJSValue>().toQObject() : value.value<QObject *>();
        auto *server=qobject_cast<SystaliaCompositor *>(object);
        auto findClient=[](auto &&self,QQuickItem *item) -> QQuickItem * {
            if(item->objectName()=="clientSurfaceItem") return item;
            for(auto *child:item->childItems()) if(auto *found=self(self,child)) return found;
            return nullptr;
        };
        auto *client=findClient(findClient,window->contentItem());
        if(client) {client->forceActiveFocus();QMetaObject::invokeMethod(client,"takeFocus");}
        QFile input(directory+"/keyboard.json");
        if(input.open(QIODevice::WriteOnly)) input.write(QJsonDocument(QJsonObject{
            {"server",server!=nullptr},{"client",client!=nullptr},
            {"clientActiveFocus",client && client->hasActiveFocus()},
            {"seatFocus",server && server->defaultSeat() && server->defaultSeat()->keyboardFocus()!=nullptr}}).toJson());
        auto *seat=server ? server->defaultSeat() : nullptr;
        if(!seat || !seat->keyboard()) return;
        // Linux KEY_G, through the same seat API used by InputDeviceManager.
        seat->sendKeyPressEvent(34);
        seat->sendKeyReleaseEvent(34);
    });
    step(5000,"","02-typing");
    step(8000,"","02-long-title");
    step(10000,"trackedClientContainers.forEach(w=>w.toggleMaximized())","03-maximized");
    step(12000,"trackedClientContainers.forEach(w=>w.toggleMaximized())","04-restored");
    step(14000,"trackedClientContainers.forEach(w=>w.snapToZone(2))","05-snap");
    step(16000,"trackedClientContainers.forEach(w=>w.unsnapWindow())","06-unsnap");
    QTimer::singleShot(18000,window,[evaluate] {
        evaluate("trackedClientContainers.forEach(w=>w.beginResize(Qt.RightEdge,Qt.point(w.x+w.width,w.y)))");
    });
    for(int i=1;i<=10;++i) QTimer::singleShot(18000+i*70,window,[evaluate,i] {
        evaluate(QString("trackedClientContainers.forEach(w=>w.updateResize(Qt.point(w.resizeOrigin.x+%1,w.resizeOrigin.y)))").arg(i*5));
    });
    step(19000,"trackedClientContainers.forEach(w=>w.finishResize())","07-resized");
    QTimer::singleShot(21000,window,[evaluate] {evaluate("trackedClientContainers.forEach(w=>waylandServer.closeToplevel(w.toplevel))");});
    QTimer::singleShot(23000,window,[]{QCoreApplication::quit();});
    return true;
}
