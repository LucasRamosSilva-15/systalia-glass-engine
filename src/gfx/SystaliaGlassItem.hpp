#pragma once

#include <QQuickItem>
#include <QVector4D>
#include <QVector2D>
#include <QSGTexture>
#include <QSGTextureProvider>
#include <QPointer>
#include "SystaliaSurfaceProps.hpp"

class SystaliaGlassItem : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QVector4D radii READ radii WRITE setRadii NOTIFY radiiChanged)
    Q_PROPERTY(float slant READ slant WRITE setSlant NOTIFY slantChanged)
    Q_PROPERTY(float ior READ ior WRITE setIor NOTIFY iorChanged)
    Q_PROPERTY(float chromaticDispersion READ chromaticDispersion WRITE setChromaticDispersion NOTIFY chromaticDispersionChanged)
    Q_PROPERTY(float causticIntensity READ causticIntensity WRITE setCausticIntensity NOTIFY causticIntensityChanged)
    Q_PROPERTY(float surfaceOpacity READ surfaceOpacity WRITE setSurfaceOpacity NOTIFY surfaceOpacityChanged)
    Q_PROPERTY(float time READ time WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(QQuickItem* backdropSource READ backdropSource WRITE setBackdropSource NOTIFY backdropSourceChanged)
    Q_PROPERTY(QVector2D screenResolution READ screenResolution WRITE setScreenResolution NOTIFY screenResolutionChanged)

public:
    explicit SystaliaGlassItem(QQuickItem *parent = nullptr);
    ~SystaliaGlassItem() override;

    QVector4D radii() const { return m_radii; }
    void setRadii(const QVector4D &radii);

    float slant() const { return m_slant; }
    void setSlant(float slant);

    float ior() const { return m_ior; }
    void setIor(float ior);

    float chromaticDispersion() const { return m_chromaticDispersion; }
    void setChromaticDispersion(float dispersion);

    float causticIntensity() const { return m_causticIntensity; }
    void setCausticIntensity(float intensity);

    float surfaceOpacity() const { return m_surfaceOpacity; }
    void setSurfaceOpacity(float opacity);

    float time() const { return m_time; }
    void setTime(float t);

    QQuickItem *backdropSource() const { return m_backdropSource; }
    void setBackdropSource(QQuickItem *source);

    QVector2D screenResolution() const { return m_screenResolution; }
    void setScreenResolution(const QVector2D &res);

    QSGTexture *ensureFallbackTexture();
    void releaseResources() override;

signals:
    void radiiChanged();
    void slantChanged();
    void iorChanged();
    void chromaticDispersionChanged();
    void causticIntensityChanged();
    void surfaceOpacityChanged();
    void timeChanged();
    void backdropSourceChanged();
    void screenResolutionChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *data) override;

private:
    QVector4D m_radii = QVector4D(24.0f, 24.0f, 24.0f, 24.0f);
    float m_slant = 0.0f;
    float m_ior = 1.15f;
    float m_chromaticDispersion = 0.035f;
    float m_causticIntensity = 0.65f;
    float m_surfaceOpacity = 1.0f;
    float m_time = 0.0f;

    QPointer<QQuickItem> m_backdropSource;
    QVector2D m_screenResolution = QVector2D(0.0f, 0.0f);

    QPointer<QSGTextureProvider> m_connectedProvider;
    QSGTexture *m_fallbackTexture = nullptr;
};
