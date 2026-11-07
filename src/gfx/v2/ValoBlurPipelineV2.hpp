#pragma once

#include <QImage>
#include <QObject>
#include <QSize>
#include <vector>

namespace Systalia::Gfx::V2 {

enum class BlurProfile {
    Performance = 0,
    Balanced = 1,
    Quality = 2
};

class ValoBlurPipelineV2 : public QObject {
    Q_OBJECT

public:
    explicit ValoBlurPipelineV2(QObject *parent = nullptr);
    ~ValoBlurPipelineV2() override = default;

    static int passCountForProfile(BlurProfile profile);

    // Filtragem determinística CPU / Offline (garante testes unitários headless com idêntico kernel)
    static QImage filterOffline(const QImage &source, BlurProfile profile = BlurProfile::Quality);

    // Processamento de um passo downsample offline (5 amostras: centro peso 4, 4 cantos peso 1)
    static QImage downsampleStepOffline(const QImage &src, float offset = 1.0f);

    // Processamento de um passo upsample offline (8 amostras: 4 diagonais peso 2, 4 cardinais peso 1)
    static QImage upsampleStepOffline(const QImage &src, const QSize &targetSize, float offset = 1.0f);

    // Medição de resposta ao impulso (teste de espalhamento Gaussiano-like)
    static float measureImpulseSpread(const QImage &blurredImpulse);

signals:
    void blurUpdated();
};

} // namespace Systalia::Gfx::V2
