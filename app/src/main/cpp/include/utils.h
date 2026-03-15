#ifndef UTILS_H
#define UTILS_H

#include "watermark_config.h"
#include <vector>

float computeRegionSpatialCorrelation(const float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos);

float computeRegionGradientCorrelation(const float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos);

float calculateNearBlackRatio(const float* image, int width, int height,
        const WatermarkPosition& pos);

std::vector<float> warpAlphaMap(const float* src, int size, float dx, float dy, float scale);
std::vector<float> interpolateAlphaMap(const float* src, int srcSize, int dstSize);

void extractGrayscaleRegion(const float* image, int width, int height,
        int x, int y, int size, float* out);

float normalizedCrossCorrelation(const float* a, const float* b, int n);

void sobelMagnitude(const float* gray, int width, int height, float* grad);

#endif