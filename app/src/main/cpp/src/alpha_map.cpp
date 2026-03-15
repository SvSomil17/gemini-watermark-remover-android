#include "alpha_map.h"
#include <android/log.h>
#include <vector>
#include <cstdint>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION

#include "../third_party/stb_image.h"

#include "bg_48.h"
#include "bg_96.h"

#define LOG_TAG "AlphaMap"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

AlphaMapLoader &AlphaMapLoader::getInstance() {
    static AlphaMapLoader instance;
    return instance;
}

bool AlphaMapLoader::loadAlphaMap(const unsigned char *pngData, int pngLen,
        std::vector<float> &out, int expectedSize) {
    int w, h, channels;
    unsigned char *imgData = stbi_load_from_memory(pngData, pngLen, &w, &h, &channels, 4);
    if (!imgData) {
        LOGE("stbi_load_from_memory failed");
        return false;
    }

    if (w != expectedSize || h != expectedSize) {
        LOGE("Size mismatch: expected %dx%d, got %dx%d", expectedSize, expectedSize, w, h);
        stbi_image_free(imgData);
        return false;
    }

    out.resize(expectedSize * expectedSize);
    for (int i = 0; i < expectedSize * expectedSize; ++i) {
        unsigned char r = imgData[i * 4];
        unsigned char g = imgData[i * 4 + 1];
        unsigned char b = imgData[i * 4 + 2];
        unsigned char maxChannel = std::max({r, g, b});
        out[i] = maxChannel / 255.0f;
    }

    stbi_image_free(imgData);
    LOGI("Loaded %dx%d alpha map", w, h);
    return true;
}

bool AlphaMapLoader::loadAlphaMaps() {
    bool ok = true;
    ok = ok && loadAlphaMap(bg_48_png, bg_48_png_len, alpha48, 48);
    ok = ok && loadAlphaMap(bg_96_png, bg_96_png_len, alpha96, 96);
    return ok;
}