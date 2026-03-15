#ifndef ADAPTIVE_DETECTOR_H
#define ADAPTIVE_DETECTOR_H

#include "watermark_config.h"
#include <vector>

struct AdaptiveResult {
    bool found;
    int x, y, size;
    float confidence;
    float spatialScore;
    float gradientScore;
    float varianceScore;
};

struct WarpResult {
    bool valid;
    std::vector<float> warpedAlpha;
    float spatialScore;
    float gradientScore;
    float dx, dy, scale;
};

struct RecalibrationResult {
    bool valid;
    std::vector<float> imageData;
    float alphaGain;
    float processedSpatialScore;
    float suppressionGain;
};

AdaptiveResult detectAdaptiveWatermarkRegion(const float* image, int width, int height,
        const float* alpha96, const WatermarkConfig& defaultConfig);

bool shouldAttemptAdaptiveFallback(const float* processedImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        const float* originalImage);

WarpResult findBestTemplateWarp(const float* originalImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float baselineSpatialScore, float baselineGradientScore);

RecalibrationResult recalibrateAlphaStrength(const float* originalImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float originalSpatialScore, float processedSpatialScore,
        float originalNearBlackRatio);

bool shouldRecalibrate(float originalSpatial, float processedSpatial, float suppressionGain);

#endif