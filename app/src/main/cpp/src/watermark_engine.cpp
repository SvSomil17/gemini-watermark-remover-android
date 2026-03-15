#include <jni.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/log.h>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <stdexcept>

#include "alpha_map.h"
#include "blend_modes.h"
#include "adaptive_detector.h"
#include "watermark_config.h"
#include "utils.h"

#define LOG_TAG "WatermarkEngine"
#include "log.h"

#define LOG_TAG "WatermarkEngine"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

struct EngineContext {
    std::vector<float> alpha48;
    std::vector<float> alpha96;
};

extern "C" {

JNIEXPORT jlong
JNICALL
Java_com_gemini_watermark_remover_core_WatermarkEngine_nativeInitialize(JNIEnv *env, jobject thiz, jobject context) {
    try {
        auto &loader = AlphaMapLoader::getInstance();
        if (!loader.loadAlphaMaps()) {
            LOGE("Failed to load alpha maps from assets");
            return 0;
        }

        auto *ctx = new EngineContext();
        ctx->alpha48 = loader.getAlpha48();
        ctx->alpha96 = loader.getAlpha96();
        return reinterpret_cast<jlong>(ctx);
    } catch (const std::exception& e) {
        LOGE("Exception in nativeInitialize: %s", e.what());
        return 0;
    } catch (...) {
        LOGE("Unknown exception in nativeInitialize");
        return 0;
    }
}

JNIEXPORT void JNICALL
Java_com_gemini_watermark_remover_core_WatermarkEngine_nativeRelease(JNIEnv *env, jobject thiz, jlong handle) {
    try {
        delete reinterpret_cast<EngineContext*>(handle);
    } catch (const std::exception& e) {
        LOGE("Exception in nativeRelease: %s", e.what());
    } catch (...) {
        LOGE("Unknown exception in nativeRelease");
    }
}

JNIEXPORT jboolean
JNICALL
Java_com_gemini_watermark_remover_core_WatermarkEngine_nativeRemoveWatermark(
        JNIEnv *env, jobject thiz,
        jlong handle,
        jfloatArray pixels,
        jint width,
        jint height,
        jstring adaptiveMode) {

    try {
        // Validate handle
        auto *ctx = reinterpret_cast<EngineContext*>(handle);
        if (!ctx) {
            LOGE("Engine context is null");
            return JNI_FALSE;
        }

        // Validate dimensions
        if (width <= 0 || height <= 0) {
            LOGE("Invalid dimensions: %dx%d", width, height);
            return JNI_FALSE;
        }

        // Validate pixel array
        if (!pixels) {
            LOGE("Pixel array is null");
            return JNI_FALSE;
        }

        // Get float array elements
        jfloat *elements = env->GetFloatArrayElements(pixels, nullptr);
        if (!elements) {
            LOGE("Failed to get float array elements");
            return JNI_FALSE;
        }

        float *imageData = elements; // RGBA float [0..1]

        // Get adaptive mode string
        const char *modeStr = env->GetStringUTFChars(adaptiveMode, nullptr);
        if (!modeStr) {
            LOGE("Failed to get adaptive mode string");
            env->ReleaseFloatArrayElements(pixels, elements, JNI_ABORT);
            return JNI_FALSE;
        }
        std::string adaptiveModeStr(modeStr);
        env->ReleaseStringUTFChars(adaptiveMode, modeStr);

        // Quick size check: minimum 48x48 for watermark detection
        if (width < 48 || height < 48) {
            LOGW("Image too small (%dx%d), cannot be Gemini generated", width, height);
            // Still return false to indicate failure, but not crash
            env->ReleaseFloatArrayElements(pixels, elements, JNI_ABORT);
            return JNI_FALSE;
        }

        // 1. Detect default config
        WatermarkConfig config = detectWatermarkConfig(width, height);

        // 2. Get alpha maps
        const float *alpha48 = ctx->alpha48.data();
        const float *alpha96 = ctx->alpha96.data();

        // 3. Calculate initial position
        WatermarkPosition pos = calculateWatermarkPosition(width, height, config);
        const float *alphaMap = (config.logoSize == 96) ? alpha96 : alpha48;

        // 4. First pass removal (on a copy)
        std::vector<float> processedData(imageData, imageData + width * height * 4);
        removeWatermark(processedData.data(), width, height, alphaMap, pos, 1.0f);

        bool shouldFallback = (adaptiveModeStr == "always") ||
                shouldAttemptAdaptiveFallback(processedData.data(), width, height,
                        alphaMap, pos, imageData);

        if (shouldFallback) {
            AdaptiveResult adaptive = detectAdaptiveWatermarkRegion(imageData, width, height,
                    alpha96, config);
            if (adaptive.found) {
                pos = {adaptive.x, adaptive.y, adaptive.size, adaptive.size};
                // Get alpha map for the detected size (interpolate from 96 if needed)
                std::vector<float> adaptedAlpha;
                if (adaptive.size == 96) {
                    alphaMap = alpha96;
                } else if (adaptive.size == 48) {
                    alphaMap = alpha48;
                } else {
                    // Interpolate from 96
                    adaptedAlpha = interpolateAlphaMap(alpha96, 96, adaptive.size);
                    alphaMap = adaptedAlpha.data();
                }
                // Re‑run removal with adaptive position
                removeWatermark(processedData.data(), width, height, alphaMap, pos, 1.0f);
            }
        }

        // 5. Compute original scores (using original image)
        float originalSpatial = computeRegionSpatialCorrelation(imageData, width, height, alphaMap, pos);
        float originalGradient = computeRegionGradientCorrelation(imageData, width, height, alphaMap, pos);

        // 6. Template alignment (optional)
        WarpResult warp = findBestTemplateWarp(imageData, width, height, alphaMap, pos,
                originalSpatial, originalGradient);
        std::vector<float> warpedAlphaStorage;
        if (warp.valid) {
            warpedAlphaStorage = std::move(warp.warpedAlpha);
            alphaMap = warpedAlphaStorage.data();
            originalSpatial = warp.spatialScore;
            originalGradient = warp.gradientScore;
        }

        // 7. Compute processed scores and maybe recalibrate alpha gain
        float processedSpatial = computeRegionSpatialCorrelation(processedData.data(), width, height, alphaMap, pos);
        float processedGradient = computeRegionGradientCorrelation(processedData.data(), width, height, alphaMap, pos);
        float suppressionGain = originalSpatial - processedSpatial;

        if (shouldRecalibrate(originalSpatial, processedSpatial, suppressionGain)) {
            float originalNearBlack = calculateNearBlackRatio(imageData, width, height, pos);
            RecalibrationResult recal = recalibrateAlphaStrength(imageData, width, height,
                    alphaMap, pos, originalSpatial,
                    processedSpatial, originalNearBlack);
            if (recal.valid) {
                processedData = std::move(recal.imageData);
                // update scores if needed
            }
        }

        // 8. Copy processed data back to imageData
        std::copy(processedData.begin(), processedData.end(), imageData);

        // Release JNI array with commit (0)
        env->ReleaseFloatArrayElements(pixels, elements, 0);

        return JNI_TRUE;

    } catch (const std::exception& e) {
        LOGE("Exception in nativeRemoveWatermark: %s", e.what());
        // Ensure we release array if we had pinned it
        if (pixels) {
            env->ReleaseFloatArrayElements(pixels, nullptr, JNI_ABORT);
        }
        return JNI_FALSE;
    } catch (...) {
        LOGE("Unknown exception in nativeRemoveWatermark");
        if (pixels) {
            env->ReleaseFloatArrayElements(pixels, nullptr, JNI_ABORT);
        }
        return JNI_FALSE;
    }
}

} // extern "C"
