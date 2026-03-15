#include "adaptive_detector.h"
#include "blend_modes.h"
#include "utils.h"
#include <cmath>
#include <vector>
#include <algorithm>
#include <cfloat>
#include <set>
#include <map>

#define LOG_TAG "WatermarkEngine"
#include "log.h"

#include <android/log.h>

namespace {
    const float DEFAULT_THRESHOLD = 0.35f;
    const float EPSILON = 1e-8f;

    struct Candidate {
        int x, y, size;
        float confidence;
        float spatialScore;
        float gradientScore;
        float varianceScore;
        float adjustedScore;
    };

    float clamp(float v, float min, float max) {
        return std::max(min, std::min(max, v));
    }

    std::vector<float> getRegion(const float* data, int width, int x, int y, int size) {
        std::vector<float> out(size * size);
        for (int row = 0; row < size; ++row) {
            const float* srcRow = data + ((y + row) * width + x);
            float* dstRow = out.data() + row * size;
            std::copy(srcRow, srcRow + size, dstRow);
        }
        return out;
    }

    Candidate scoreCandidate(const float* gray, const float* grad, int width, int height,
            const float* alphaMap, const float* templateGrad,
            const Candidate& candidate) {
        int x = candidate.x, y = candidate.y, size = candidate.size;
        if (x < 0 || y < 0 || x + size > width || y + size > height) {
            return candidate; // confidence 0
        }

        std::vector<float> grayRegion = getRegion(gray, width, x, y, size);
        std::vector<float> gradRegion = getRegion(grad, width, x, y, size);

        float spatial = normalizedCrossCorrelation(grayRegion.data(), alphaMap, size * size);
        float gradient = normalizedCrossCorrelation(gradRegion.data(), templateGrad, size * size);

        float varianceScore = 0.0f;
        if (y > 8) {
            int refY = std::max(0, y - size);
            int refH = std::min(size, y - refY);
            if (refH > 8) {
                float wmStd = 0.0f;
                // compute std dev of grayRegion
                float sum = 0.0f, sumSq = 0.0f;
                for (float v : grayRegion) { sum += v; sumSq += v * v; }
                float mean = sum / grayRegion.size();
                float var = sumSq / grayRegion.size() - mean * mean;
                wmStd = std::sqrt(std::max(0.0f, var));

                std::vector<float> refRegion = getRegion(gray, width, x, refY, refH);
                sum = 0.0f; sumSq = 0.0f;
                for (float v : refRegion) { sum += v; sumSq += v * v; }
                mean = sum / refRegion.size();
                var = sumSq / refRegion.size() - mean * mean;
                float refStd = std::sqrt(std::max(0.0f, var));

                if (refStd > EPSILON) {
                    varianceScore = clamp(1.0f - wmStd / refStd, 0.0f, 1.0f);
                }
            }
        }

        float confidence = std::max(0.0f, spatial) * 0.5f +
                std::max(0.0f, gradient) * 0.3f +
                varianceScore * 0.2f;
        confidence = clamp(confidence, 0.0f, 1.0f);

        Candidate result = candidate;
        result.confidence = confidence;
        result.spatialScore = spatial;
        result.gradientScore = gradient;
        result.varianceScore = varianceScore;
        return result;
    }

    std::vector<int> createScaleList(int minSize, int maxSize) {
        std::set<int> sizes;
        for (int s = minSize; s <= maxSize; s += 8) sizes.insert(s);
        if (48 >= minSize && 48 <= maxSize) sizes.insert(48);
        if (96 >= minSize && 96 <= maxSize) sizes.insert(96);
        return std::vector<int>(sizes.begin(), sizes.end());
    }

    void buildTemplateGradient(const float* alphaMap, int size, float* grad) {
        sobelMagnitude(alphaMap, size, size, grad);
    }

    struct TemplateCache {
        std::map<int, std::pair<std::vector<float>, std::vector<float>>> cache; // size -> (alpha, grad)
        const float* alpha96;
        TemplateCache(const float* a96) : alpha96(a96) {}
        void get(int size, const float*& alphaOut, const float*& gradOut) {
            auto it = cache.find(size);
            if (it != cache.end()) {
                alphaOut = it->second.first.data();
                gradOut = it->second.second.data();
                return;
            }
            std::vector<float> alpha;
            if (size == 96) {
                alpha.assign(alpha96, alpha96 + 96*96);
            } else {
                alpha = interpolateAlphaMap(alpha96, 96, size);
            }
            std::vector<float> grad(alpha.size());
            buildTemplateGradient(alpha.data(), size, grad.data());
            auto& stored = cache[size];
            stored.first = std::move(alpha);
            stored.second = std::move(grad);
            alphaOut = stored.first.data();
            gradOut = stored.second.data();
        }
    };

} // anonymous namespace

AdaptiveResult detectAdaptiveWatermarkRegion(const float* image, int width, int height,
        const float* alpha96, const WatermarkConfig& defaultConfig) {

    AdaptiveResult notFound;
    notFound.found = false;
    notFound.confidence = 0;
    notFound.spatialScore = 0;
    notFound.gradientScore = 0;
    notFound.varianceScore = 0;
    notFound.x = 0;
    notFound.y = 0;
    notFound.size = 0;

    // Minimum size to attempt detection (48x48 is the smallest Gemini watermark)
    if (width < 48 || height < 48) {
        LOGW("Image too small (%dx%d) for adaptive detection", width, height);
        return notFound;
    }

    std::vector<float> gray(width * height);
    for (int i = 0; i < width * height; ++i) {
        const float* rgba = image + i * 4;
        gray[i] = 0.2126f * rgba[0] + 0.7152f * rgba[1] + 0.0722f * rgba[2];
    }
    std::vector<float> grad(width * height);
    sobelMagnitude(gray.data(), width, height, grad.data());

    TemplateCache cache(alpha96);

    int baseSize = defaultConfig.logoSize;
    Candidate defaultCandidate;
    defaultCandidate.x = width - defaultConfig.marginRight - baseSize;
    defaultCandidate.y = height - defaultConfig.marginBottom - baseSize;
    defaultCandidate.size = baseSize;

    const float* tplAlpha = nullptr;
    const float* tplGrad = nullptr;
    cache.get(baseSize, tplAlpha, tplGrad);
    Candidate defaultScored = scoreCandidate(gray.data(), grad.data(), width, height,
            tplAlpha, tplGrad, defaultCandidate);
    if (defaultScored.confidence >= DEFAULT_THRESHOLD + 0.08f) {
        AdaptiveResult res;
        res.found = true;
        res.x = defaultScored.x;
        res.y = defaultScored.y;
        res.size = defaultScored.size;
        res.confidence = defaultScored.confidence;
        res.spatialScore = defaultScored.spatialScore;
        res.gradientScore = defaultScored.gradientScore;
        res.varianceScore = defaultScored.varianceScore;
        return res;
    }

    int minSize = (int)clamp(std::round(baseSize * 0.65f), 24.0f, 144.0f);
    int maxSize = (int)clamp(std::min(std::round(baseSize * 2.8f), std::floor(std::min(width, height) * 0.4f)),
            (float)minSize, 192.0f);
    std::vector<int> scaleList = createScaleList(minSize, maxSize);

    int marginRange = std::max(32, (int)std::round(baseSize * 0.75f));
    int minMarginRight = clamp(defaultConfig.marginRight - marginRange, 8, width - minSize - 1);
    int maxMarginRight = clamp(defaultConfig.marginRight + marginRange, minMarginRight, width - minSize - 1);
    int minMarginBottom = clamp(defaultConfig.marginBottom - marginRange, 8, height - minSize - 1);
    int maxMarginBottom = clamp(defaultConfig.marginBottom + marginRange, minMarginBottom, height - minSize - 1);

    std::vector<Candidate> topK;
    auto pushTopK = [&](Candidate cand) {
        topK.push_back(cand);
        std::sort(topK.begin(), topK.end(), [](const Candidate& a, const Candidate& b) {
            return a.adjustedScore > b.adjustedScore;
        });
        if (topK.size() > 5) topK.pop_back();
    };

    for (int size : scaleList) {
        cache.get(size, tplAlpha, tplGrad);
        for (int mr = minMarginRight; mr <= maxMarginRight; mr += 8) {
            int x = width - mr - size;
            if (x < 0) continue;
            for (int mb = minMarginBottom; mb <= maxMarginBottom; mb += 8) {
                int y = height - mb - size;
                if (y < 0) continue;
                Candidate cand;
                cand.x = x; cand.y = y; cand.size = size;
                cand = scoreCandidate(gray.data(), grad.data(), width, height,
                        tplAlpha, tplGrad, cand);
                if (cand.confidence < 0.08f) continue;
                cand.adjustedScore = cand.confidence * std::min(1.0f, std::sqrt(size / 96.0f));
                pushTopK(cand);
            }
        }
    }

    Candidate best = defaultScored;

    for (const auto& coarse : topK) {
        int scaleLo = clamp(coarse.size - 10, minSize, maxSize);
        int scaleHi = clamp(coarse.size + 10, minSize, maxSize);
        for (int size = scaleLo; size <= scaleHi; size += 2) {
            cache.get(size, tplAlpha, tplGrad);
            for (int x = coarse.x - 8; x <= coarse.x + 8; x += 2) {
                if (x < 0 || x + size > width) continue;
                for (int y = coarse.y - 8; y <= coarse.y + 8; y += 2) {
                    if (y < 0 || y + size > height) continue;
                    Candidate cand;
                    cand.x = x; cand.y = y; cand.size = size;
                    cand = scoreCandidate(gray.data(), grad.data(), width, height,
                            tplAlpha, tplGrad, cand);
                    if (cand.confidence > best.confidence) {
                        best = cand;
                    }
                }
            }
        }
    }

    AdaptiveResult result;
    result.found = best.confidence >= DEFAULT_THRESHOLD;
    result.confidence = best.confidence;
    result.spatialScore = best.spatialScore;
    result.gradientScore = best.gradientScore;
    result.varianceScore = best.varianceScore;
    result.x = best.x;
    result.y = best.y;
    result.size = best.size;
    return result;
}

bool shouldAttemptAdaptiveFallback(const float* processedImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        const float* originalImage) {
    float residualScore = computeRegionSpatialCorrelation(processedImage, width, height, alphaMap, pos);
    if (residualScore >= 0.22f) return true;
    if (originalImage) {
        float originalScore = computeRegionSpatialCorrelation(originalImage, width, height, alphaMap, pos);
        if (originalScore <= 0.0f) return true;
    }
    return false;
}

WarpResult findBestTemplateWarp(const float* originalImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float baselineSpatialScore, float baselineGradientScore) {
    const float TEMPLATE_ALIGN_SHIFTS[] = {-0.5f, -0.25f, 0, 0.25f, 0.5f};
    const float TEMPLATE_ALIGN_SCALES[] = {0.99f, 1.0f, 1.01f};

    int size = pos.width;
    if (size <= 8) return {false};

    WarpResult best;
    best.valid = false;
    best.spatialScore = baselineSpatialScore;
    best.gradientScore = baselineGradientScore;

    for (float scale : TEMPLATE_ALIGN_SCALES) {
        for (float dy : TEMPLATE_ALIGN_SHIFTS) {
            for (float dx : TEMPLATE_ALIGN_SHIFTS) {
                if (dx == 0 && dy == 0 && scale == 1) continue;
                std::vector<float> warped = warpAlphaMap(alphaMap, size, dx, dy, scale);
                float spatial = computeRegionSpatialCorrelation(originalImage, width, height, warped.data(), pos);
                float gradient = computeRegionGradientCorrelation(originalImage, width, height, warped.data(), pos);

                float confidence = std::max(0.0f, spatial) * 0.7f + std::max(0.0f, gradient) * 0.3f;
                float bestConf = std::max(0.0f, best.spatialScore) * 0.7f + std::max(0.0f, best.gradientScore) * 0.3f;

                if (confidence > bestConf + 0.01f) {
                    best.valid = true;
                    best.warpedAlpha = std::move(warped);
                    best.spatialScore = spatial;
                    best.gradientScore = gradient;
                    best.dx = dx;
                    best.dy = dy;
                    best.scale = scale;
                }
            }
        }
    }

    if (best.valid && (best.spatialScore >= baselineSpatialScore + 0.01f ||
            best.gradientScore >= baselineGradientScore + 0.01f)) {
        return best;
    }
    return {false};
}

RecalibrationResult recalibrateAlphaStrength(const float* originalImage, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float originalSpatialScore, float processedSpatialScore,
        float originalNearBlackRatio) {
    const float ALPHA_GAIN_CANDIDATES[] = {1.05f, 1.12f, 1.2f, 1.28f, 1.36f, 1.45f, 1.52f, 1.6f,
            1.7f, 1.85f, 2.0f, 2.2f, 2.4f, 2.6f};
    const float MAX_NEAR_BLACK_RATIO_INCREASE = 0.05f;
    const float MIN_RECALIBRATION_SCORE_DELTA = 0.18f;

    float bestScore = processedSpatialScore;
    float bestGain = 1.0f;
    std::vector<float> bestImageData;

    float maxAllowedNearBlackRatio = std::min(1.0f, originalNearBlackRatio + MAX_NEAR_BLACK_RATIO_INCREASE);

    for (float alphaGain : ALPHA_GAIN_CANDIDATES) {
        std::vector<float> candidate(originalImage, originalImage + width * height * 4);
        removeWatermark(candidate.data(), width, height, alphaMap, pos, alphaGain);
        float nearBlack = calculateNearBlackRatio(candidate.data(), width, height, pos);
        if (nearBlack > maxAllowedNearBlackRatio) continue;

        float score = computeRegionSpatialCorrelation(candidate.data(), width, height, alphaMap, pos);
        if (score < bestScore) {
            bestScore = score;
            bestGain = alphaGain;
            bestImageData = std::move(candidate);
        }
    }

    // Refine around best gain
    for (float delta = -0.05f; delta <= 0.05f; delta += 0.01f) {
        float alphaGain = bestGain + delta;
        if (alphaGain <= 1.0f || alphaGain >= 3.0f) continue;
        std::vector<float> candidate(originalImage, originalImage + width * height * 4);
        removeWatermark(candidate.data(), width, height, alphaMap, pos, alphaGain);
        float nearBlack = calculateNearBlackRatio(candidate.data(), width, height, pos);
        if (nearBlack > maxAllowedNearBlackRatio) continue;
        float score = computeRegionSpatialCorrelation(candidate.data(), width, height, alphaMap, pos);
        if (score < bestScore) {
            bestScore = score;
            bestGain = alphaGain;
            bestImageData = std::move(candidate);
        }
    }

    float scoreDelta = processedSpatialScore - bestScore;
    if (bestImageData.empty() || scoreDelta < MIN_RECALIBRATION_SCORE_DELTA) {
        return {false};
    }

    RecalibrationResult res;
    res.valid = true;
    res.imageData = std::move(bestImageData);
    res.alphaGain = bestGain;
    res.processedSpatialScore = bestScore;
    res.suppressionGain = originalSpatialScore - bestScore;
    return res;
}

bool shouldRecalibrate(float originalSpatial, float processedSpatial, float suppressionGain) {
    const float RESIDUAL_RECALIBRATION_THRESHOLD = 0.5f;
    const float MIN_SUPPRESSION_FOR_SKIP_RECALIBRATION = 0.18f;
    return originalSpatial >= 0.6f &&
            processedSpatial >= RESIDUAL_RECALIBRATION_THRESHOLD &&
            suppressionGain <= MIN_SUPPRESSION_FOR_SKIP_RECALIBRATION;
}