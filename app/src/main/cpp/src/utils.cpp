#include "utils.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cfloat>

#define LOG_TAG "Utils"
#include "log.h"

#include <android/log.h>

namespace {
    float mean(const float* data, int n) {
        double sum = 0;
        for (int i = 0; i < n; ++i) sum += data[i];
        return static_cast<float>(sum / n);
    }

    float variance(const float* data, int n, float meanVal) {
        double sum = 0;
        for (int i = 0; i < n; ++i) {
            float d = data[i] - meanVal;
            sum += d * d;
        }
        return static_cast<float>(sum / n);
    }
}

float normalizedCrossCorrelation(const float* a, const float* b, int n) {
    if (n == 0) return 0;
    float meanA = mean(a, n);
    float meanB = mean(b, n);
    float varA = variance(a, n, meanA);
    float varB = variance(b, n, meanB);
    float denom = std::sqrt(varA * varB) * n;
    if (denom < 1e-8f) return 0;
    double num = 0;
    for (int i = 0; i < n; ++i) {
        num += (a[i] - meanA) * (b[i] - meanB);
    }
    return static_cast<float>(num / denom);
}

void extractGrayscaleRegion(const float* image, int width, int height,
        int x, int y, int size, float* out) {

    if (!image || !out || x < 0 || y < 0 || x + size > width || y + size > height) {
        LOGE("extractGrayscaleRegion: invalid region");
        return;
    }

    for (int r = 0; r < size; ++r) {
        const float* srcRow = image + ((y + r) * width + x) * 4;
        float* dstRow = out + r * size;
        for (int c = 0; c < size; ++c) {
            float r0 = srcRow[c * 4];
            float g = srcRow[c * 4 + 1];
            float b = srcRow[c * 4 + 2];
            dstRow[c] = 0.2126f * r0 + 0.7152f * g + 0.0722f * b;
        }
    }
}

void sobelMagnitude(const float* gray, int width, int height, float* grad) {
    if (!gray || !grad || width < 3 || height < 3) {
        LOGE("sobelMagnitude: invalid parameters (width=%d, height=%d)", width, height);
        if (grad && width > 0 && height > 0) {
            memset(grad, 0, width * height * sizeof(float));
        }
        return;
    }

    for (int y = 0; y < height; ++y) {
        grad[y * width] = 0;
        grad[y * width + (width - 1)] = 0;
    }
    for (int x = 0; x < width; ++x) {
        grad[x] = 0;
        grad[(height - 1) * width + x] = 0;
    }

    for (int y = 1; y < height - 1; ++y) {
        for (int x = 1; x < width - 1; ++x) {
            int i = y * width + x;

            int i_nw = i - width - 1;
            int i_n  = i - width;
            int i_ne = i - width + 1;
            int i_w  = i - 1;
            int i_e  = i + 1;
            int i_sw = i + width - 1;
            int i_s  = i + width;
            int i_se = i + width + 1;

            float gx = -gray[i_nw] - 2*gray[i_n] - gray[i_ne]
                    + gray[i_sw] + 2*gray[i_s] + gray[i_se];
            float gy = -gray[i_nw] - 2*gray[i_w] - gray[i_sw]
                    + gray[i_ne] + 2*gray[i_e] + gray[i_se];

            grad[i] = std::sqrt(gx*gx + gy*gy);
        }
    }
}

float computeRegionSpatialCorrelation(const float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos) {

    if (!image || !alphaMap || pos.x < 0 || pos.y < 0 ||
            pos.x + pos.width > width || pos.y + pos.height > height) {
        LOGE("computeRegionSpatialCorrelation: invalid region");
        return 0.0f;
    }

    int size = pos.width;
    std::vector<float> region(size * size);
    extractGrayscaleRegion(image, width, height, pos.x, pos.y, size, region.data());
    return normalizedCrossCorrelation(region.data(), alphaMap, size * size);
}

float computeRegionGradientCorrelation(const float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos) {
    int size = pos.width;
    std::vector<float> region(size * size);
    extractGrayscaleRegion(image, width, height, pos.x, pos.y, size, region.data());
    std::vector<float> regionGrad(size * size);
    sobelMagnitude(region.data(), size, size, regionGrad.data());
    std::vector<float> alphaGrad(size * size);
    sobelMagnitude(alphaMap, size, size, alphaGrad.data());
    return normalizedCrossCorrelation(regionGrad.data(), alphaGrad.data(), size * size);
}

float calculateNearBlackRatio(const float* image, int width, int height,
        const WatermarkPosition& pos) {
    const float NEAR_BLACK_THRESHOLD = 5.0f / 255.0f;
    int nearBlack = 0;
    int total = pos.width * pos.height;
    for (int row = 0; row < pos.height; ++row) {
        for (int col = 0; col < pos.width; ++col) {
            int idx = ((pos.y + row) * width + (pos.x + col)) * 4;
            float r = image[idx];
            float g = image[idx + 1];
            float b = image[idx + 2];
            if (r <= NEAR_BLACK_THRESHOLD && g <= NEAR_BLACK_THRESHOLD && b <= NEAR_BLACK_THRESHOLD) {
                nearBlack++;
            }
        }
    }
    return total > 0 ? static_cast<float>(nearBlack) / total : 0;
}

std::vector<float> warpAlphaMap(const float* src, int size, float dx, float dy, float scale) {
    if (dx == 0 && dy == 0 && scale == 1) {
        return std::vector<float>(src, src + size * size);
    }
    std::vector<float> dst(size * size);
    auto sample = [&](float x, float y) -> float {
        int x0 = (int)std::floor(x);
        int y0 = (int)std::floor(y);
        float fx = x - x0;
        float fy = y - y0;

        int ix0 = std::clamp(x0, 0, size-1);
        int iy0 = std::clamp(y0, 0, size-1);
        int ix1 = std::clamp(x0+1, 0, size-1);
        int iy1 = std::clamp(y0+1, 0, size-1);

        float p00 = src[iy0 * size + ix0];
        float p10 = src[iy0 * size + ix1];
        float p01 = src[iy1 * size + ix0];
        float p11 = src[iy1 * size + ix1];

        float top = p00 + (p10 - p00) * fx;
        float bottom = p01 + (p11 - p01) * fx;
        return top + (bottom - top) * fy;
    };

    float c = (size - 1) / 2.0f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            float sx = (x - c) / scale + c + dx;
            float sy = (y - c) / scale + c + dy;
            dst[y * size + x] = sample(sx, sy);
        }
    }
    return dst;
}

std::vector<float> interpolateAlphaMap(const float* src, int srcSize, int dstSize) {
    if (dstSize == srcSize) return std::vector<float>(src, src + srcSize * srcSize);
    std::vector<float> dst(dstSize * dstSize);
    float scale = (srcSize - 1) / (float)(dstSize - 1);
    for (int y = 0; y < dstSize; ++y) {
        float sy = y * scale;
        int y0 = (int)std::floor(sy);
        int y1 = std::min(srcSize - 1, y0 + 1);
        float fy = sy - y0;
        for (int x = 0; x < dstSize; ++x) {
            float sx = x * scale;
            int x0 = (int)std::floor(sx);
            int x1 = std::min(srcSize - 1, x0 + 1);
            float fx = sx - x0;

            float p00 = src[y0 * srcSize + x0];
            float p10 = src[y0 * srcSize + x1];
            float p01 = src[y1 * srcSize + x0];
            float p11 = src[y1 * srcSize + x1];

            float top = p00 + (p10 - p00) * fx;
            float bottom = p01 + (p11 - p01) * fx;
            dst[y * dstSize + x] = top + (bottom - top) * fy;
        }
    }
    return dst;
}