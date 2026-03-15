#ifndef BLEND_MODES_H
#define BLEND_MODES_H

#include "watermark_config.h"

void removeWatermark(float* image, int width, int height,
        const float* alphaMap, const WatermarkPosition& pos,
        float alphaGain = 1.0f);

#endif