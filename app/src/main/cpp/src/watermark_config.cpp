#include "watermark_config.h"

WatermarkConfig detectWatermarkConfig(int imageWidth, int imageHeight) {
    if (imageWidth > 1024 && imageHeight > 1024) {
        return {96, 64, 64};
    }
    return {48, 32, 32};
}

WatermarkPosition calculateWatermarkPosition(int imageWidth, int imageHeight, const WatermarkConfig& config) {
    int x = imageWidth - config.marginRight - config.logoSize;
    int y = imageHeight - config.marginBottom - config.logoSize;
    return {x, y, config.logoSize, config.logoSize};
}