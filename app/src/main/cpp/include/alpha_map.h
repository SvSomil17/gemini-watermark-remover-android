#ifndef ALPHA_MAP_H
#define ALPHA_MAP_H

#include <vector>
#include <jni.h>

class AlphaMapLoader {
public:
    static AlphaMapLoader& getInstance();
    bool loadAlphaMaps(); // No parameters needed – uses embedded data
    const std::vector<float>& getAlpha48() const { return alpha48; }
    const std::vector<float>& getAlpha96() const { return alpha96; }
private:
    AlphaMapLoader() = default;
    std::vector<float> alpha48;
    std::vector<float> alpha96;
    bool loadAlphaMap(const unsigned char* pngData, int pngLen, std::vector<float>& out, int expectedSize);
};

#endif