#pragma once
#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>

// Sends Qt pointer events to the actual SDE; available only in a diagnostic run.
template<class Evaluate>
bool installValoInternalResizeProbe(QQuickWindow *window, const QString &directory, Evaluate evaluate) {
    if (!qEnvironmentVariableIsSet("SGE_VALO_INTERNAL_RESIZE_PROBE")) return false;
    auto results=std::make_shared<QJsonArray>();
    auto find=[](auto &&self,QQuickItem *root,const QString &name)->QQuickItem * {
        if(root->objectName()==name) return root;
        for(auto *child:root->childItems()) if(auto *found=self(self,child,name)) return found;
        return nullptr;
    };
    struct Direction {const char *name;int x;int y;};
    static const Direction directions[]={{"top",0,-1},{"bottom",0,1},{"left",-1,0},{"right",1,0},
        {"topLeft",-1,-1},{"bottomRight",1,1},{"topRight",1,-1},{"bottomLeft",-1,1}};
    for(int index=0;index<16;++index) {
        QTimer::singleShot(1500+index*150,window,[=] {
            const bool files=index<8;
            evaluate(files ? "materialWindow.visible=false; glassChassis.visible=true; glassChassis.x=200; glassChassis.y=100; glassChassis.width=750; glassChassis.height=600; raiseFileWindow()"
                           : "glassChassis.visible=false; materialWindow.visible=true; materialWindow.x=200; materialWindow.y=100; materialWindow.width=750; materialWindow.height=600; raiseMaterialWindow()");
            const auto dir=directions[index%8];
            auto *handles=find(find,window->contentItem(),files?"filesResizeHandles":"materialResizeHandles");
            auto *area=handles?find(find,handles,QString("internalResize_")+dir.name):nullptr;
            auto *target=handles?handles->parentItem():nullptr;
            bool ok=false;
            if(area && target && handles->isEnabled()) {
                const QPointF point=area->mapToScene(QPointF(area->width()/2,area->height()/2));
                const QPointF moved=point+QPointF(dir.x*30,dir.y*25);
                auto send=[&](QEvent::Type type,QPointF local,Qt::MouseButton button,Qt::MouseButtons buttons) {
                    QMouseEvent event(type,local,local,QPointF(window->mapToGlobal(local.toPoint())),button,buttons,Qt::NoModifier);
                    QCoreApplication::sendEvent(window,&event);
                };
                send(QEvent::MouseButtonPress,point,Qt::LeftButton,Qt::LeftButton);
                send(QEvent::MouseMove,moved,Qt::NoButton,Qt::LeftButton);
                send(QEvent::MouseButtonRelease,moved,Qt::LeftButton,Qt::NoButton);
                ok=qAbs(target->width()-(750+(dir.x?30:0)))<.1 && qAbs(target->height()-(600+(dir.y?25:0)))<.1
                    && qAbs(target->x()-(200+(dir.x<0?-30:0)))<.1 && qAbs(target->y()-(100+(dir.y<0?-25:0)))<.1;
            }
            results->append(QJsonObject{{"window",files?"Files":"Material"},{"edge",dir.name},{"passed",ok},
                {"width",target?target->width():0},{"height",target?target->height():0}});
        });
    }
    QTimer::singleShot(4500,window,[=] {
        evaluate("glassChassis.visible=true; materialWindow.visible=true; glassChassis.x=80; glassChassis.y=80; materialWindow.x=810; materialWindow.y=80; raiseMaterialWindow()");
        QFile file(directory+"/resize-results.json");
        if(file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(*results).toJson());
        const auto frame=window->grabWindow();
        if(!frame.isNull()) frame.save(directory+"/internal-windows.png");
        bool ok=results->size()==16;
        for(const auto &v:*results) ok=ok && v.toObject().value("passed").toBool();
        QCoreApplication::exit(ok ? 0 : 2);
    });
    return true;
}
