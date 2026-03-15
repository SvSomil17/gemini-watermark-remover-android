#ifndef WATERMARK_CONFIG_H
#define WATERMARK_CONFIG_H

struct WatermarkConfig {
    int logoSize;
    int marginRight;
    int marginBottom;
};

struct WatermarkPosition {
    int x, y, width, height;
};

WatermarkConfig detectWatermarkConfig(int imageWidth, int imageHeight);
WatermarkPosition calculateWatermarkPosition(int imageWidth, int imageHeight, const WatermarkConfig& config);

#endif