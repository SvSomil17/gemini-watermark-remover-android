#include "blend_modes.h"
#include <algorithm>
#include <cmath>

void removeWatermark(float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float alphaGain) {
    const float ALPHA_NOISE_FLOOR = 3.0f / 255.0f;
    const float ALPHA_THRESHOLD = 0.002f;
    const float MAX_ALPHA = 0.99f;
    const float LOGO_VALUE = 1.0f; // white in [0,1] range

    for (int row = 0; row < pos.height; ++row) {
        for (int col = 0; col < pos.width; ++col) {
            int imgIdx = ((pos.y + row) * width + (pos.x + col)) * 4;
            int alphaIdx = row * pos.width + col;

            float rawAlpha = alphaMap[alphaIdx];
            float signalAlpha = std::max(0.0f, rawAlpha - ALPHA_NOISE_FLOOR) * alphaGain;
            if (signalAlpha < ALPHA_THRESHOLD) continue;

            float alpha = std::min(rawAlpha * alphaGain, MAX_ALPHA);
            float oneMinusAlpha = 1.0f - alpha;

            for (int c = 0; c < 3; ++c) {
                float watermarked = image[imgIdx + c];
                float original = (watermarked - alpha * LOGO_VALUE) / oneMinusAlpha;
                image[imgIdx + c] = std::clamp(original, 0.0f, 1.0f);
            }
            // alpha channel unchanged
        }
    }
}