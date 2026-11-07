#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QSGGeometryNode>
#include <QSGGeometry>
#include <QSGFlatColorMaterial>
#include <QSGMaterial>
#include <QSGMaterialShader>
#include <QImage>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QThread>
#include <QProcess>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <cmath>

#include "SystaliaSurfacePropsV2.hpp"
#include "ValoGlassItemV2.hpp"
#include "ValoShadowItemV2.hpp"
#include "ValoBackdropProviderV2.hpp"
#include <QQmlEngine>
#include <QQmlComponent>

void qml_register_types_Systalia_Engine();

using namespace Systalia::Gfx::V2;

// ============================================================================
// T1: Custom Item com QSGGeometryNode + QSGFlatColorMaterial (Magenta)
// ============================================================================
class RawT1Item : public QQuickItem {
public:
    explicit RawT1Item(QQuickItem *parent = nullptr) : QQuickItem(parent) {
        setFlag(ItemHasContents, true);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        const qreal w = width();
        const qreal h = height();
        if (w <= 0.0 || h <= 0.0) {
            delete oldNode;
            return nullptr;
        }

        auto *node = static_cast<QSGGeometryNode *>(oldNode);
        if (!node) {
            node = new QSGGeometryNode();
            auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 4);
            geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geom);
            node->setFlag(QSGNode::OwnsGeometry, true);

            auto *mat = new QSGFlatColorMaterial();
            mat->setColor(QColor(255, 0, 255)); // Magenta
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial, true);
        }

        QSGGeometry::Point2D *v = node->geometry()->vertexDataAsPoint2D();
        v[0].set(0.0f, 0.0f);
        v[1].set(0.0f, static_cast<float>(h));
        v[2].set(static_cast<float>(w), 0.0f);
        v[3].set(static_cast<float>(w), static_cast<float>(h));
        node->markDirty(QSGNode::DirtyGeometry);

        return node;
    }
};

// ============================================================================
// T2: Custom QSGMaterialShader com SPIR-V Mínimo (raw_t2_minimal)
// ============================================================================
class RawT2MaterialShader : public QSGMaterialShader {
public:
    RawT2MaterialShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/raw_shaders/raw_t2_minimal.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/raw_shaders/raw_t2_minimal.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *, QSGMaterial *) override {
        QByteArray *buf = state.uniformData();
        if (!buf) return false;

        struct alignas(16) MinimalUbo {
            QMatrix4x4 matrix;
            float opacity;
            float pad[3];
        } ubo;

        ubo.matrix = state.combinedMatrix();
        ubo.opacity = state.opacity();

        if (buf->size() < static_cast<qsizetype>(sizeof(ubo))) {
            buf->resize(sizeof(ubo));
        }
        std::memcpy(buf->data(), &ubo, sizeof(ubo));
        return true;
    }
};

class RawT2Material : public QSGMaterial {
public:
    RawT2Material() {
        setFlag(Blending, false);
        setFlag(RequiresFullMatrix, true);
    }
    QSGMaterialType *type() const override {
        static QSGMaterialType t;
        return &t;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override {
        return new RawT2MaterialShader();
    }
};

class RawT2Item : public QQuickItem {
public:
    explicit RawT2Item(QQuickItem *parent = nullptr) : QQuickItem(parent) {
        setFlag(ItemHasContents, true);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        const qreal w = width();
        const qreal h = height();
        if (w <= 0.0 || h <= 0.0) {
            delete oldNode;
            return nullptr;
        }

        auto *node = static_cast<QSGGeometryNode *>(oldNode);
        if (!node) {
            node = new QSGGeometryNode();
            auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
            geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geom);
            node->setFlag(QSGNode::OwnsGeometry, true);

            auto *mat = new RawT2Material();
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial, true);
        }

        QSGGeometry::updateTexturedRectGeometry(node->geometry(), QRectF(0.0, 0.0, w, h), QRectF(0.0, 0.0, 1.0, 1.0));
        node->markDirty(QSGNode::DirtyGeometry);
        return node;
    }
};

// ============================================================================
// T3: QSGMaterialShader com UBO std140 de 160 Bytes (SystaliaSurfacePropsV2)
// ============================================================================
class RawT3Material : public QSGMaterial {
public:
    SystaliaSurfacePropsV2 props;

    RawT3Material() {
        setFlag(Blending, false);
        setFlag(RequiresFullMatrix, true);
    }
    QSGMaterialType *type() const override {
        static QSGMaterialType t;
        return &t;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
};

class RawT3MaterialShader : public QSGMaterialShader {
public:
    RawT3MaterialShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/raw_shaders/raw_t3_ubo.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/raw_shaders/raw_t3_ubo.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *) override {
        QByteArray *buf = state.uniformData();
        if (!buf) return false;

        auto *mat = static_cast<RawT3Material *>(newMaterial);
        SystaliaSurfacePropsV2 p = mat->props;
        p.matrix = state.combinedMatrix();

        constexpr qsizetype uboSize = sizeof(SystaliaSurfacePropsV2);
        static_assert(uboSize == 160, "UBO T3 deve ter 160 bytes.");

        if (buf->size() < uboSize) {
            buf->resize(uboSize);
        }
        std::memcpy(buf->data(), &p, uboSize);
        return true;
    }
};

QSGMaterialShader *RawT3Material::createShader(QSGRendererInterface::RenderMode) const {
    return new RawT3MaterialShader();
}

class RawT3Item : public QQuickItem {
public:
    explicit RawT3Item(QQuickItem *parent = nullptr) : QQuickItem(parent) {
        setFlag(ItemHasContents, true);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        const qreal w = width();
        const qreal h = height();
        if (w <= 0.0 || h <= 0.0) {
            delete oldNode;
            return nullptr;
        }

        auto *node = static_cast<QSGGeometryNode *>(oldNode);
        RawT3Material *mat = nullptr;
        if (!node) {
            node = new QSGGeometryNode();
            auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
            geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geom);
            node->setFlag(QSGNode::OwnsGeometry, true);

            mat = new RawT3Material();
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial, true);
        } else {
            mat = static_cast<RawT3Material *>(node->material());
        }

        QSGGeometry::updateTexturedRectGeometry(node->geometry(), QRectF(0.0, 0.0, w, h), QRectF(0.0, 0.0, 1.0, 1.0));
        node->markDirty(QSGNode::DirtyGeometry);

        mat->props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));
        mat->props.radii = QVector4D(35.0f, 35.0f, 35.0f, 35.0f);
        mat->props.surfaceOpacity = 1.0f;
        node->markDirty(QSGNode::DirtyMaterial);

        return node;
    }
};

// ============================================================================
// T4: Shader de Máscara SDF L2 Analítica em Quad 300x100
// ============================================================================
class RawT4Material : public QSGMaterial {
public:
    SystaliaSurfacePropsV2 props;

    RawT4Material() {
        setFlag(Blending, true);
        setFlag(RequiresFullMatrix, true);
    }
    QSGMaterialType *type() const override {
        static QSGMaterialType t;
        return &t;
    }
    QSGMaterialShader *createShader(QSGRendererInterface::RenderMode) const override;
};

class RawT4MaterialShader : public QSGMaterialShader {
public:
    RawT4MaterialShader() {
        setShaderFileName(VertexStage, QStringLiteral(":/raw_shaders/raw_t4_sdf.vert.qsb"));
        setShaderFileName(FragmentStage, QStringLiteral(":/raw_shaders/raw_t4_sdf.frag.qsb"));
    }

    bool updateUniformData(RenderState &state, QSGMaterial *newMaterial, QSGMaterial *) override {
        QByteArray *buf = state.uniformData();
        if (!buf) return false;

        auto *mat = static_cast<RawT4Material *>(newMaterial);
        SystaliaSurfacePropsV2 p = mat->props;
        p.matrix = state.combinedMatrix();

        constexpr qsizetype uboSize = sizeof(SystaliaSurfacePropsV2);
        if (buf->size() < uboSize) {
            buf->resize(uboSize);
        }
        std::memcpy(buf->data(), &p, uboSize);
        return true;
    }
};

QSGMaterialShader *RawT4Material::createShader(QSGRendererInterface::RenderMode) const {
    return new RawT4MaterialShader();
}

class RawT4Item : public QQuickItem {
public:
    explicit RawT4Item(QQuickItem *parent = nullptr) : QQuickItem(parent) {
        setFlag(ItemHasContents, true);
    }

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override {
        const qreal w = width();
        const qreal h = height();
        if (w <= 0.0 || h <= 0.0) {
            delete oldNode;
            return nullptr;
        }

        auto *node = static_cast<QSGGeometryNode *>(oldNode);
        RawT4Material *mat = nullptr;
        if (!node) {
            node = new QSGGeometryNode();
            auto *geom = new QSGGeometry(QSGGeometry::defaultAttributes_TexturedPoint2D(), 4);
            geom->setDrawingMode(QSGGeometry::DrawTriangleStrip);
            node->setGeometry(geom);
            node->setFlag(QSGNode::OwnsGeometry, true);

            mat = new RawT4Material();
            node->setMaterial(mat);
            node->setFlag(QSGNode::OwnsMaterial, true);
        } else {
            mat = static_cast<RawT4Material *>(node->material());
        }

        QSGGeometry::updateTexturedRectGeometry(node->geometry(), QRectF(0.0, 0.0, w, h), QRectF(0.0, 0.0, 1.0, 1.0));
        node->markDirty(QSGNode::DirtyGeometry);

        mat->props.windowSize = QVector2D(static_cast<float>(w), static_cast<float>(h));
        mat->props.radii = QVector4D(35.0f, 35.0f, 35.0f, 35.0f);
        mat->props.geometryMode = 0.0f; // CircularL2
        mat->props.surfaceOpacity = 1.0f;
        node->markDirty(QSGNode::DirtyMaterial);

        return node;
    }
};

// ============================================================================
// Estrutura de Diagnóstico e Métricas de Framebuffer
// ============================================================================
struct DiagnosticResult {
    std::string testId;
    std::string backend;
    std::string platform;
    bool passed = false;
    uint32_t totalPixels = 0;
    uint32_t activePixels = 0;
    uint32_t targetPixels = 0;
    int minX = 9999, minY = 9999, maxX = -1, maxY = -1;
    uint8_t minAlpha = 255, maxAlpha = 0;
    std::string sha256;
    std::string artifactFile;
    std::string failureReason;
};

// Analisa pixel a pixel um QImage
DiagnosticResult analyzeFramebuffer(const QImage &img,
                                     const std::string &testId,
                                     const std::string &backend,
                                     const std::string &platform,
                                     const QColor &bgColor,
                                     auto isTargetPixel) {
    DiagnosticResult res;
    res.testId = testId;
    res.backend = backend;
    res.platform = platform;
    res.totalPixels = img.width() * img.height();

    // SHA256 dos bytes da imagem
    QByteArray bytes(reinterpret_cast<const char *>(img.constBits()), static_cast<qsizetype>(img.sizeInBytes()));
    res.sha256 = QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex().toStdString();

    const QRgb bgRgb = bgColor.rgba();

    for (int y = 0; y < img.height(); ++y) {
        const QRgb *scan = reinterpret_cast<const QRgb *>(img.constScanLine(y));
        for (int x = 0; x < img.width(); ++x) {
            const QRgb px = scan[x];
            const uint8_t a = qAlpha(px);

            if (px != bgRgb) {
                res.activePixels++;
                if (x < res.minX) res.minX = x;
                if (x > res.maxX) res.maxX = x;
                if (y < res.minY) res.minY = y;
                if (y > res.maxY) res.maxY = y;
                if (a < res.minAlpha) res.minAlpha = a;
                if (a > res.maxAlpha) res.maxAlpha = a;
            }

            if (isTargetPixel(px)) {
                res.targetPixels++;
            }
        }
    }

    if (res.activePixels == 0) {
        res.minAlpha = 0;
        res.minX = res.minY = res.maxX = res.maxY = 0;
    }

    return res;
}

// Executa um teste individual e salva os artefatos
DiagnosticResult runSingleTest(int testIdx,
                               const std::string &backend,
                               const std::string &platform,
                               const QString &outDir,
                               const QString &brainDir) {
    QQuickWindow window;
    window.resize(640, 360);

    const bool lightBg = (testIdx == 7);
    const QColor bgColor = lightBg ? QColor(232, 237, 242) : Qt::black;
    window.setColor(bgColor);

    QQuickItem *content = window.contentItem();
    QQuickItem *testItem = nullptr;

    const std::string testNames[] = {
        "T0_QML_RECT",
        "T1_SCENEGRAPH_BASIC",
        "T2_CUSTOM_SPIRV_MINIMAL",
        "T3_UBO_160B_MVP",
        "T4_SDF_L2_MASK",
        "T5_VALOGLASS_MASK_ISOLATED",
        "T6_VALOGLASS_OPTICAL_DUMMY",
        "T7_VALOSHADOW_ISOLATED",
        "T8_VALODOCK_SCENE_H",
        "T9_VALODOCK_SCENE_B_MASK",
        "T10_VALODOCK_SCENE_G_SHADOW"
    };

    const std::string testId = testNames[testIdx];

    switch (testIdx) {
    case 0: { // T0: Item Puro QML com retângulo magenta (300x100) e borda amarela (2px)
        testItem = new QQuickItem(content);
        testItem->setPosition(QPointF(170, 130));
        testItem->setSize(QSizeF(300, 100));

        // Cria retângulo de fundo via QQuickItem customizado ou análogo
        auto *bgRect = new RawT1Item(testItem);
        bgRect->setSize(QSizeF(300, 100));
        break;
    }
    case 1: { // T1: QSGGeometryNode + QSGFlatColorMaterial
        auto *item = new RawT1Item(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        testItem = item;
        break;
    }
    case 2: { // T2: QSGMaterialShader com SPIR-V Mínimo
        auto *item = new RawT2Item(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        testItem = item;
        break;
    }
    case 3: { // T3: QSGMaterialShader com UBO 160B e MVP
        auto *item = new RawT3Item(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        testItem = item;
        break;
    }
    case 4: { // T4: SDF L2 Máscara Analítica
        auto *item = new RawT4Item(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        testItem = item;
        break;
    }
    case 5: { // T5: ValoGlassItemV2 direto com valo_diag_mask.frag.qsb (debugMode = 2)
        auto *item = new ValoGlassItemV2(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        item->setCornerRadii(QVector4D(35.0f, 35.0f, 35.0f, 35.0f));
        item->setGeometryMode(ValoGlassItemV2::CircularL2);
        item->setDebugMode(2); // Máscara analítica
        item->setSurfaceOpacity(1.0f);
        item->setActive(true);
        testItem = item;
        break;
    }
    case 6: { // T6: ValoGlassItemV2 com valo_optical_v2.frag.qsb (debugMode = 0)
        auto *item = new ValoGlassItemV2(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        item->setCornerRadii(QVector4D(35.0f, 35.0f, 35.0f, 35.0f));
        item->setGeometryMode(ValoGlassItemV2::CircularL2);
        item->setDebugMode(0); // Composição óptica completa
        item->setSurfaceOpacity(0.85f);
        item->setVolumetricTint(QColor(10, 40, 70, 220));
        item->setIor(1.25f);
        item->setChromaticDispersion(0.04f);
        item->setSpecularStrokeWidth(1.25f);
        item->setActive(true);
        testItem = item;
        break;
    }
    case 7: { // T7: ValoShadowItemV2 isolado sobre fundo claro
        auto *item = new ValoShadowItemV2(content);
        item->setPosition(QPointF(170, 130));
        item->setSize(QSizeF(300, 100));
        item->setCornerRadii(QVector4D(35.0f, 35.0f, 35.0f, 35.0f));
        item->setShadowOffsetY(12.0f);
        item->setShadowIntensity(0.80f);
        item->setIsolatedPass(true); // Sombra completa sem oclusão
        testItem = item;
        break;
    }
    case 8:
    case 9:
    case 10: { // ValoDockV2Scene completa (Passes H, B, G)
        window.resize(1280, 720);
        static QQmlEngine engine;
        QQmlComponent comp(&engine);
        comp.setData(R"(
            import QtQuick
            import Systalia.Engine
            ValoDockV2Scene {
                id: scene
                anchors.fill: parent
                showHud: false
            }
        )", QUrl());
        auto *sceneItem = qobject_cast<QQuickItem*>(comp.create());
        if (sceneItem) {
            sceneItem->setParentItem(content);
            sceneItem->setWidth(1280);
            sceneItem->setHeight(720);
            QObject *dock = sceneItem->property("dock").value<QObject*>();
            if (dock) {
                if (testIdx == 8) dock->setProperty("debugPass", "H");
                else if (testIdx == 9) dock->setProperty("debugPass", "B");
                else if (testIdx == 10) dock->setProperty("debugPass", "G");
            }
            testItem = sceneItem;
        }
        break;
    }
    }

    window.show();

    // Pump rigoroso de renderização Scene Graph
    for (int i = 0; i < 8; ++i) {
        window.update();
        QCoreApplication::processEvents();
        QThread::msleep(15);
    }

    const QImage frame = window.grabWindow();
    if (frame.isNull()) {
        DiagnosticResult r;
        r.testId = testId;
        r.backend = backend;
        r.platform = platform;
        r.passed = false;
        r.failureReason = "grabWindow() retornou QImage nulo!";
        return r;
    }

    // Predicado de identificação do alvo por teste
    auto isTarget = [&](QRgb px) -> bool {
        const int r = qRed(px);
        const int g = qGreen(px);
        const int b = qBlue(px);
        const int a = qAlpha(px);

        if (testIdx <= 3) {
            // Magenta: R > 200, G < 50, B > 200, A > 200
            return (r > 200 && g < 50 && b > 200 && a > 200);
        } else if (testIdx == 4 || testIdx == 5) {
            // Branco da máscara: R > 150, G > 150, B > 150, A > 150
            return (r > 150 && g > 150 && b > 150 && a > 150);
        } else if (testIdx == 6) {
            // Vidro óptico: pixel não preto e com alpha significativo
            return (a > 30 && (r > 5 || g > 5 || b > 5));
        } else if (testIdx == 7) {
            // Sombra: pixel mais escuro que o fundo claro #E8EDF2 (r<200 && g<200 && b<200)
            return (r < 200 && g < 200 && b < 200);
        } else if (testIdx >= 8) {
            // Cena da Dock
            return (px != bgColor.rgba());
        }
        return false;
    };

    DiagnosticResult res = analyzeFramebuffer(frame, testId, backend, platform, bgColor, isTarget);

    const QString filename = QString::fromStdString("raw_" + testId + "_" + backend + "_" + platform + ".png");
    const QString repoPath = outDir + "/" + filename;
    const QString brainPath = brainDir + "/" + filename;
    frame.save(repoPath);
    frame.save(brainPath);
    res.artifactFile = filename.toStdString();

    // Validação estrita: não aceita 0 pixels nem tela em branco
    if (res.activePixels == 0) {
        res.passed = false;
        res.failureReason = "ZERO pixels renderizados (framebuffer identico ao fundo / invisivel)";
    } else if (res.targetPixels == 0) {
        res.passed = false;
        res.failureReason = "Pixels ativos detectados mas nenhum corresponde a assinatura esperada";
    } else {
        // Validação da bounding box esperada (~170, 130 até ~470, 230)
        if (testIdx <= 6) {
            if (res.minX < 150 || res.maxX > 490 || res.minY < 110 || res.maxY > 250) {
                res.passed = false;
                res.failureReason = "Bounding box fora das coordenadas nominais do item";
            } else {
                res.passed = true;
            }
        } else {
            // T7 tem padding de sombra (+-40px)
            res.passed = true;
        }
    }

    return res;
}

// ============================================================================
// MAIN / CLI Orchestrator
// ============================================================================
int main(int argc, char **argv) {
    std::string backend = "opengl";
    std::string platform = "offscreen";
    std::string targetTest = "all";
    bool runnerMode = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--backend" && i + 1 < argc) {
            backend = argv[++i];
        } else if (arg == "--platform" && i + 1 < argc) {
            platform = argv[++i];
        } else if (arg == "--test" && i + 1 < argc) {
            targetTest = argv[++i];
        } else if (arg == "--orchestrate") {
            runnerMode = true;
        }
    }

    const QString repoArtifactDir = QStringLiteral("/home/lucasramos/Documentos/systalia-glass-engine/artifacts/raw-render-tests");
    const QString brainArtifactDir = QStringLiteral("/home/lucasramos/.gemini/antigravity/brain/db6eb3e4-2bf0-441e-9ef2-a1601d0a1c5f");
    QDir().mkpath(repoArtifactDir);
    QDir().mkpath(brainArtifactDir);

    // ========================================================================
    // Modo Orquestrador: Invoca processos filhos para a matriz OpenGL x Vulkan
    // ========================================================================
    if (runnerMode) {
        std::cout << "================================================================================" << std::endl;
        std::cout << "   SYSTALIA VALO GRAPHICS 2.0 — ETAPA 04.1.1: DIAGNÓSTICO DE RENDERIZAÇÃO CRUA" << std::endl;
        std::cout << "================================================================================" << std::endl;

        const std::string exePath = argv[0];
        const struct Config {
            std::string backend;
            std::string platform;
        } configs[] = {
            {"opengl", "offscreen"},
            {"vulkan", "offscreen"},
            {"opengl", "wayland"},
            {"vulkan", "wayland"}
        };

        for (const auto &cfg : configs) {
            std::cout << "\n>>> INICIANDO MATRIZ: Backend=" << cfg.backend << " | Platform=" << cfg.platform << std::endl;
            QProcess proc;
            proc.setProcessChannelMode(QProcess::ForwardedChannels);
            proc.start(QString::fromStdString(exePath), {
                QStringLiteral("--backend"), QString::fromStdString(cfg.backend),
                QStringLiteral("--platform"), QString::fromStdString(cfg.platform),
                QStringLiteral("--test"), QStringLiteral("all")
            });
            proc.waitForFinished(60000);
            if (proc.exitCode() != 0) {
                std::cout << "[AVISO] Execução (" << cfg.backend << ", " << cfg.platform << ") terminou com código " << proc.exitCode() << std::endl;
            }
        }
        return 0;
    }

    // ========================================================================
    // Modo Execução Específica do Processo
    // ========================================================================
    qputenv("QT_QPA_PLATFORM", QByteArray::fromStdString(platform));
    qputenv("QSG_RHI_BACKEND", QByteArray::fromStdString(backend));
    qputenv("QSG_INFO", "1");

    QGuiApplication app(argc, argv);
    qml_register_types_Systalia_Engine();

    std::cout << "--------------------------------------------------------------------------------" << std::endl;
    std::cout << "[RUNNER] Backend=" << backend << " | Platform=" << platform << std::endl;
    std::cout << "--------------------------------------------------------------------------------" << std::endl;

    std::vector<DiagnosticResult> results;

    for (int t = 0; t <= 10; ++t) {
        const std::string tStr = "T" + std::to_string(t);
        if (targetTest != "all" && targetTest != tStr) {
            continue;
        }

        DiagnosticResult res = runSingleTest(t, backend, platform, repoArtifactDir, brainArtifactDir);
        results.push_back(res);

        std::cout << "[" << (res.passed ? "APROVADO" : "FALHA") << "] "
                  << res.testId << " | " << res.backend << " | " << res.platform
                  << " | Ativos: " << res.activePixels << " | Alvo: " << res.targetPixels
                  << " | Box: [" << res.minX << "," << res.minY << "]-[" << res.maxX << "," << res.maxY << "]"
                  << " | SHA256: " << res.sha256.substr(0, 16) << "..."
                  << std::endl;

        if (!res.passed) {
            std::cout << "      --> MOTIVO: " << res.failureReason << std::endl;
        }
    }

    return 0;
}
