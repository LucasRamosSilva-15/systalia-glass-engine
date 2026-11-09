#include <QQuickItem>
#include <cmath>
#include <QPainter>
#include <QSGSimpleTextureNode>
#include <QTemporaryDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QFile>
#include <QQmlEngine>
#include <QQmlComponent>
#include "ValoBackdropProviderV2.hpp"
#include "ValoGlassItemV2.hpp"
#include "ValoOpticalContext.hpp"
#include <QTest>
#include <QQuickWindow>
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGTexture>
#include "ValoGlassMaterialV2.hpp"

using namespace Systalia::Gfx::V2;

// Render the production QSBs with a known backdrop, independent of synthetic fallback.
class ProbeNode : public QSGGeometryNode {
public:
    QSGTexture *texture = nullptr;
    ~ProbeNode() override { delete texture; }
};
class ProbeItem : public QQuickItem {
public:
    SystaliaSurfacePropsV2 props;
    QImage backdrop{256, 192, QImage::Format_RGBA8888};
    int mode = 0;
    ProbeItem(QQuickItem *parent) : QQuickItem(parent) {
        setFlag(ItemHasContents);
        setSize(QSizeF(256, 192));
        props.windowSize = QVector2D(256, 192);
        props.radii = QVector4D(64, 64, 64, 64);
        props.causticIntensity = 0;
        props.ior = 1;
        props.volumetricTint = QVector4D(0, 0, 0, 0);
        backdrop.fill(QColor(64, 128, 192));
    }
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override {
        delete old;
        auto *node = new ProbeNode;
        auto *g = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
        g->setDrawingMode(QSGGeometry::DrawTriangleStrip);
        QSGGeometry::updateTexturedRectGeometry(g, boundingRect(), QRectF(0, 0, 1, 1));
        node->setGeometry(g);
        node->setFlag(QSGNode::OwnsGeometry);
        node->texture = window()->createTextureFromImage(backdrop);
        node->texture->setFiltering(QSGTexture::Nearest);
        auto *mat = new ValoGlassMaterialV2(mode);
        props.screenResolution = QVector2D(window()->width() * window()->devicePixelRatio(),
                                           window()->height() * window()->devicePixelRatio());
        mat->setProps(props);
        mat->setBackdropTexture(node->texture);
        node->setMaterial(mat);
        node->setFlag(QSGNode::OwnsMaterial);
        return node;
    }
};
class StaticBackdrop : public QQuickItem {
    class Provider : public QSGTextureProvider {
    public:
        QSGTexture *value = nullptr;
        QSGTexture *texture() const override { return value; }
    };
    mutable Provider provider;
    QImage image;
public:
    StaticBackdrop(const QImage &pattern, QQuickItem *parent) : QQuickItem(parent), image(pattern) {
        setFlag(ItemHasContents); setSize(QSizeF(256,192));
    }
    bool isTextureProvider() const override { return true; }
    QSGTextureProvider *textureProvider() const override { return &provider; }
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *) override {
        auto *node=static_cast<QSGSimpleTextureNode *>(old);
        if(!node) {
            node=new QSGSimpleTextureNode;
            auto *texture=window()->createTextureFromImage(image);
            node->setTexture(texture); node->setOwnsTexture(true);
            provider.value=texture; // Deliberately no textureChanged on unchanged source.
        }
        node->setRect(boundingRect()); return node;
    }
};

class ObservedGlass : public ValoGlassItemV2 {
public:
    using ValoGlassItemV2::ValoGlassItemV2;
    ValoGlassMaterialV2 *observedMaterial = nullptr; // render-thread observation only
    QSGNode *updatePaintNode(QSGNode *old, UpdatePaintNodeData *data) override {
        auto *node = ValoGlassItemV2::updatePaintNode(old, data);
        observedMaterial = node ? static_cast<ValoGlassMaterialV2 *>(static_cast<QSGGeometryNode *>(node)->material()) : nullptr;
        return node;
    }
};

class OpticalContract : public QObject {
    Q_OBJECT
    static QImage frame(QQuickWindow &w, ProbeItem &item) {
        item.update(); w.update();
        QTest::qWait(50);
        return w.grabWindow();
    }
    static QColor pixel(const QImage &im, int x, int y) {
        // grabWindow may return logical-sized pixels with DPR metadata on offscreen.
        return im.pixelColor(qFloor((x + .5) * im.width() / 256.),
                             qFloor((y + .5) * im.height() / 192.));
    }
private slots:
    void transmission_data() {
        QTest::addColumn<int>("mode"); QTest::addColumn<float>("mix"); QTest::addColumn<QColor>("background");
        for (int mode : {0, 9}) for (float s : {0.f, .4f, 1.f})
            for (QColor bg : {QColor(Qt::red), QColor(Qt::black), QColor(Qt::white)})
                QTest::newRow(qPrintable(QString("%1-%2-%3").arg(mode).arg(s).arg(bg.name()))) << mode << s << bg;
    }
    void transmission() {
        QFETCH(int, mode); QFETCH(float, mix); QFETCH(QColor, background);
        QQuickWindow w; w.resize(256,192); w.setColor(background);
        auto *item = new ProbeItem(w.contentItem()); item->mode = mode;
        item->props.surfaceOpacity = mix;
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        const auto im = frame(w,*item); QVERIFY(!im.isNull());
        const auto c = pixel(im,128,96);
        const float y = 96.5f/192;
        const float sheen = (1-y)*.08f + std::exp(-y*3)*.08f;
        const float volume[3] = {.035f+(.020f-.035f)*y+sheen,
                                 .245f+(.095f-.245f)*y+sheen,
                                 .435f+(.180f-.435f)*y+sheen};
        const int b[3] = {64,128,192}; const int actual[3] = {c.red(),c.green(),c.blue()};
        for(int i=0;i<3;++i) QVERIFY2(std::abs(actual[i]-qRound(b[i]*(1-mix)+255*volume[i]*mix))<=3,
                                     qPrintable(QString("channel %1 got %2").arg(i).arg(actual[i])));
        QCOMPARE(c.alpha(),255);
    }
    void inheritedOpacityAndDisabledCaustics() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::transparent);
        auto *item = new ProbeItem(w.contentItem()); w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (int mode : {0,7,8,9}) {
            item->mode=mode; item->setOpacity(.5);
            const auto half=frame(w,*item); QVERIFY(!half.isNull());
            if(mode==0 || mode==9) QVERIFY(std::abs(pixel(half,128,96).alpha()-128)<=1);
            if(mode==7) for(int y=0;y<192;++y) for(int x=0;x<256;++x)
                QCOMPARE(pixel(half,x,y).alpha(),0);
            item->setOpacity(0);
            const auto zero=frame(w,*item); QVERIFY(!zero.isNull());
            for(int y=0;y<192;++y) for(int x=0;x<256;++x) QCOMPARE(pixel(zero,x,y).rgba(),QRgb(0));
        }
    }
    void refractionDoesNotRetainOriginalLine() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::white);
        auto *item=new ProbeItem(w.contentItem()); item->mode=9;
        item->props.surfaceOpacity=0; item->backdrop.fill(Qt::black);
        for(int y=0;y<192;++y) for(int x=8;x<12;++x) item->backdrop.setPixelColor(x,y,Qt::white);
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        auto straight=frame(w,*item); QVERIFY(!straight.isNull());
        QVERIFY(pixel(straight,8,96).red()>240);
        item->props.ior=1.25;
        auto bent=frame(w,*item); QVERIFY(!bent.isNull());
        QVERIFY2(pixel(bent,8,96).red()<5,"The original line must not survive a refracted backdrop replacement");
        int peak=0; for(int x=12;x<20;++x) peak=std::max(peak,pixel(bent,x,96).red());
        QVERIFY(peak>240);
    }
    void diagnosticAlphaOnKnownBackgrounds() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::transparent);
        auto *item=new ProbeItem(w.contentItem()); item->props.causticIntensity=.6f;
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        for(int mode : {7,8}) {
            item->mode=mode;
            const QPoint pos = mode==7 ? QPoint(128,96) : QPoint(8,96);
            w.setColor(Qt::transparent); item->setOpacity(1);
            auto full=frame(w,*item); QVERIFY(!full.isNull());
            const QColor c=pixel(full,pos.x(),pos.y()); QVERIFY(c.alpha()>0);
            item->setOpacity(.5); auto half=frame(w,*item); QVERIFY(!half.isNull());
            QVERIFY(std::abs(pixel(half,pos.x(),pos.y()).alpha()*2-c.alpha())<=2);
            item->setOpacity(1);
            w.setColor(Qt::black); auto black=frame(w,*item); QVERIFY(!black.isNull());
            w.setColor(Qt::white); auto white=frame(w,*item); QVERIFY(!white.isNull());
            const auto b=pixel(black,pos.x(),pos.y()), a=pixel(white,pos.x(),pos.y());
            for(int difference : {a.red()-b.red(),a.green()-b.green(),a.blue()-b.blue()})
                QVERIFY(std::abs(difference-(255-c.alpha()))<=2);
        }
    }
    void meniscusCuts() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::transparent);
        auto *item = new ProbeItem(w.contentItem()); item->mode=8;
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        for (float geometry : {0.f, 1.f}) {
            item->props.geometryMode=geometry;
            const auto im=frame(w,*item); QVERIFY(!im.isNull());
            const double boundary=64-64/std::pow(2.,geometry==0 ? .5 : .25);
            auto side=[&](int d) { return pixel(im,d,96).alpha(); };
            auto corner=[&](int d) {
                return pixel(im,qRound(boundary+d/std::sqrt(2.)),
                             qRound(192-boundary-d/std::sqrt(2.))).alpha();
            };
            // Beyond the 1.25px stroke, energy must fall continuously into the body.
            QVERIFY(side(3)>side(10)); QVERIFY(side(10)>side(18));
            QVERIFY(corner(3)>corner(10)); QVERIFY(corner(10)>corner(18));
            QVERIFY(side(24)<=1); QVERIFY(corner(26)<=1);
            int sideHalf=0, cornerHalf=0;
            for(int d=4;d<=26;++d) {
                if(!sideHalf && side(d)<=side(3)/2) sideHalf=d;
                if(!cornerHalf && corner(d)<=corner(3)/2) cornerHalf=d;
            }
            QVERIFY(sideHalf>0 && cornerHalf>0);
            QVERIFY2(std::abs(sideHalf-cornerHalf)<=3,"G2 physical widths must agree within raster/field approximation");
            QVERIFY(corner(3)<side(3)*2.5);
        }
    }
    void backdropUpdateAndCoordinates_data() {
        QTest::addColumn<int>("profile");
        QTest::newRow("raw") << 0;
        QTest::newRow("quality") << 2;
    }
    void backdropUpdateAndCoordinates() {
        QFETCH(int, profile);
        QQmlEngine engine;
        QQmlComponent component(&engine);
        component.setData(R"(
import QtQuick
Item {
    width: 256; height: 192
    property color topColor: "red"
    Rectangle { id: wall; width: 256; height: 192; color: parent.topColor
        Rectangle { y: 96; width: 256; height: 96; color: "blue" }
    }
    ShaderEffectSource { objectName: "capture"; sourceItem: wall; live: true; hideSource: false }
})", QUrl());
        QScopedPointer<QObject> object(component.create());
        QVERIFY2(object, qPrintable(component.errorString()));
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::black);
        auto *root=qobject_cast<QQuickItem *>(object.data()); QVERIFY(root);
        root->setParentItem(w.contentItem());
        ValoBackdropProviderV2 provider;
        provider.setSource(root->findChild<QQuickItem *>("capture"));
        provider.setBlurProfile(static_cast<BlurProfile>(profile));
        auto *glass=new ValoGlassItemV2(w.contentItem());
        glass->setSize(QSizeF(100,140)); glass->setPosition(QPointF(80,24));
        glass->setDebugMode(9); glass->setSurfaceOpacity(0); glass->setIor(1);
        glass->setBlurProfile(profile);
        glass->setBackdropProvider(&provider);
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(200); auto first=w.grabWindow(); QVERIFY(!first.isNull());
        qInfo() << "profile" << profile << "top" << pixel(first,128,48) << "bottom" << pixel(first,128,144);
        QVERIFY(pixel(first,128,48).red()>200);
        QVERIFY(pixel(first,128,144).blue()>200);
        root->setProperty("topColor",QColor(Qt::green));
        QTest::qWait(200); auto changed=w.grabWindow(); QVERIFY(!changed.isNull());
        QVERIFY2(pixel(changed,128,48).green()>100,"Wallpaper must update after the first GPU blur");
        QVERIFY(pixel(changed,128,48).red()<30);
        glass->setX(100); QTest::qWait(100);
        auto moved=w.grabWindow(); QVERIFY(!moved.isNull());
        QVERIFY(pixel(moved,128,48).green()>100);
        QVERIFY(pixel(moved,128,144).blue()>200);
        // Destroy consumer before provider and QML source.
        delete glass;
        root->setParentItem(nullptr);
    }
    void profileAnimationAndStaticEnergy() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::transparent);
        auto *glass = new ObservedGlass(w.contentItem());
        glass->setSize(QSizeF(256,192)); glass->setCausticIntensity(1); glass->setDebugMode(7);
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        QVERIFY(QMetaObject::invokeMethod(glass, "onIdleTimeout"));
        QVERIFY(!glass->isCausticFrozen());
        QCOMPARE(glass->animationIntervalMs(),16);
        QTest::qWait(5200); // Regress the old five-second unconditional freeze.
        const float t=glass->causticTime();
        QVERIFY(t>0); QVERIFY(glass->animationRunning());
        const auto a=w.grabWindow(); QVERIFY(!a.isNull());
        QTest::qWait(500); const auto b=w.grabWindow(); QVERIFY(!b.isNull());
        QVERIFY(glass->causticTime()>t+.1f); QVERIFY(a!=b); // Actual production pass I changes.
        glass->setBlurProfile(1); QCOMPARE(glass->animationIntervalMs(),33);
        glass->setBlurProfile(0); const float paused=glass->causticTime();
        QTest::qWait(100); QCOMPARE(glass->causticTime(),paused);
        const auto frozen=w.grabWindow(); QVERIFY(!frozen.isNull());
        QVERIFY(pixel(frozen,128,96).alpha()>0);
        ValoOpticalContext context; context.setPrefersReducedMotion(true);
        glass->setOpticalContext(&context); glass->setBlurProfile(2);
        QTest::qWait(100); QCOMPARE(glass->causticTime(),paused);
        QVERIFY(!glass->animationRunning());
        QVERIFY(pixel(w.grabWindow(),128,96).alpha()>0); // Motion policy does not extinguish light.
        context.setPrefersReducedMotion(false); QVERIFY(glass->animationRunning());
        glass->setVisible(false); const float hidden=glass->causticTime();
        QTest::qWait(150); QCOMPARE(glass->causticTime(),hidden);
        glass->setVisible(true); QTest::qWait(50);
        QVERIFY(glass->causticTime()>=hidden); QVERIFY(glass->causticTime()-hidden<.15f);
        context.setEnergySavingFactor(.2f); const float energy=glass->causticTime();
        QTest::qWait(100); QCOMPARE(glass->causticTime(),energy);
        glass->setOpticalContext(nullptr);
    }
    void gpuBackdropProfileSharpness() {
        QTemporaryDir directory; QVERIFY(directory.isValid());
        QImage pattern(256,192,QImage::Format_RGBA8888); pattern.fill(Qt::white);
        QPainter painter(&pattern);
        for(int y=0;y<192;y+=24) for(int x=0;x<256;x+=24)
            if((x/24+y/24)%2==0) painter.fillRect(x,y,24,24,Qt::black);
        painter.setPen(Qt::red);
        for(int x=0;x<256;x+=6) painter.drawLine(x,0,x,191);
        painter.setPen(Qt::green); painter.setFont(QFont("Sans",18,QFont::Bold));
        painter.drawText(QRect(16,40,224,110),Qt::AlignCenter,"VALO GPU\nDual-Kawase 2.0"); painter.end();
        QVERIFY(pattern.save(directory.path()+"/pattern.png"));
        QQuickWindow window; window.resize(256,192); window.setColor(Qt::black);
        auto *source=new StaticBackdrop(pattern,window.contentItem());
        ValoBackdropProviderV2 provider; provider.setSource(source);
        auto *glass=new ObservedGlass(window.contentItem()); glass->setSize(QSizeF(256,192));
        glass->setCornerRadii(QVector4D(0,0,0,0)); glass->setCausticIntensity(0);
        glass->setDebugMode(5); glass->setBackdropProvider(&provider);
        glass->setBlurProfile(0);
        // No animation-driven updatePaintNode may accidentally repair the handoff.
        QVERIFY(QMetaObject::invokeMethod(glass,"onIdleTimeout"));
        std::atomic_bool filtered{false}; std::atomic<quintptr> wrapper{0};
        QObject::connect(&window,&QQuickWindow::afterRendering,&window,[&] {
            auto *material=glass->observedMaterial;
            auto *used=material ? material->backdropTexture() : nullptr;
            auto *sourceTexture=source->textureProvider()->texture();
            filtered = used && sourceTexture && used->rhiTexture()!=sourceTexture->rhiTexture();
            wrapper = reinterpret_cast<quintptr>(used);
        },Qt::DirectConnection);
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(100); glass->update(); QVERIFY(!window.grabWindow().isNull());
        QJsonArray metrics; double energies[3]{}; quintptr stable=0;
        for(int profile : {0,1,2,0,2,1}) {
            glass->setBlurProfile(profile); window.update(); QTest::qWait(220);
            const auto image=window.grabWindow(); QVERIFY(!image.isNull());
            double energy=0, mean=0; int count=0;
            for(int y=32;y<160;++y) for(int x=32;x<224;++x) {
                auto luma=[](QColor c) { return .2126*c.redF()+.7152*c.greenF()+.0722*c.blueF(); };
                const double center=luma(pixel(image,x,y));
                energy+=std::pow(center-luma(pixel(image,x+1,y)),2)+std::pow(center-luma(pixel(image,x,y+1)),2); mean+=center; ++count;
            }
            energies[profile]=energy/count;
            metrics.append(QJsonObject{{"profile",profile},{"gradientEnergy",energies[profile]}, {"meanLuminance",mean/count}, {"filteredMaterial",filtered.load()}, {"executedPasses",provider.executedPasses()}, {"status",provider.blurStatus()}});
            qInfo()<<"GPU profile"<<profile<<"gradient energy"<<energies[profile]<<"filtered material"<<filtered;
            QVERIFY(mean/count>.2 && mean/count<.8); // A black/empty output is not successful blur.
            QCOMPARE(filtered.load(),profile!=0);
            QCOMPARE(provider.executedPasses(),profile==0 ? 0 : profile==1 ? 4 : 8);
            QCOMPARE(provider.blurStatus(),profile==0 ? QString("bypass") : QString("filtered"));
            if(profile!=0) { if(stable) QCOMPARE(wrapper.load(),stable); stable=wrapper.load(); }
            const QString output=qEnvironmentVariable("SGE_VALO_GPU_METRICS_DIR");
            if(!output.isEmpty()) { QDir().mkpath(output); image.save(output+QString("/gpu-profile-%1.png").arg(profile)); }
        }
        const QString output=qEnvironmentVariable("SGE_VALO_GPU_METRICS_DIR");
        if(!output.isEmpty()) { QFile file(output+"/gpu-metrics.json"); QVERIFY(file.open(QIODevice::WriteOnly)); file.write(QJsonDocument(metrics).toJson()); }
        QVERIFY(energies[1]<energies[0]*.5); QVERIFY(energies[2]<energies[1]*.7);
        QObject::disconnect(&window,&QQuickWindow::afterRendering,&window,nullptr);
        delete glass;
    }

    void diagnosticEnergy() {
        QQuickWindow w; w.resize(256,192); w.setColor(Qt::black);
        auto *item = new ProbeItem(w.contentItem()); item->backdrop.fill(Qt::black);
        item->props.surfaceOpacity=0; item->props.causticIntensity=.6f;
        w.show(); QVERIFY(QTest::qWaitForWindowExposed(&w));
        item->mode=0; const auto h=frame(w,*item);
        item->mode=9; const auto k=frame(w,*item);
        item->mode=7; const auto i=frame(w,*item);
        item->mode=8; const auto j=frame(w,*item);
        QVERIFY(!h.isNull()&&!k.isNull()&&!i.isNull()&&!j.isNull());
        // At 8px from the left edge the stroke is absent and caustic mask is zero.
        for (auto pos : {QPoint(8,96),QPoint(128,96)}) {
            const auto hc=pixel(h,pos.x(),pos.y()), kc=pixel(k,pos.x(),pos.y());
            const auto ic=pixel(i,pos.x(),pos.y()), jc=pixel(j,pos.x(),pos.y());
            QVERIFY(std::abs(hc.red()-kc.red()-ic.red()-jc.red())<=3);
            QVERIFY(std::abs(hc.green()-kc.green()-ic.green()-jc.green())<=3);
            QVERIFY(std::abs(hc.blue()-kc.blue()-ic.blue()-jc.blue())<=3);
        }
    }
};
QTEST_MAIN(OpticalContract)
#include "tst_ValoOpticalContractV2.moc"
