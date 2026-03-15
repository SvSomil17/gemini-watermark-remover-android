package com.gemini.watermark.remover.core

import android.graphics.Bitmap
import androidx.core.graphics.createBitmap

/**
 * Extension function to convert a [Bitmap] into a FloatArray
 * in RGBA order normalized between 0f and 1f.
 *
 * Useful for native processing pipelines or ML models.
 *
 * @receiver Bitmap to convert
 * @return FloatArray of size width * height * 4
 */
fun Bitmap.toFloatArray(): FloatArray {
    val pixels = IntArray(width * height)
    getPixels(pixels, 0, width, 0, 0, width, height)

    // Each pixel has 4 channels: R, G, B, A
    val floatArray = FloatArray(width * height * 4)

    for (i in pixels.indices) {
        val p = pixels[i]
        floatArray[i * 4] = ((p shr 16) and 0xFF) / 255f   // Red
        floatArray[i * 4 + 1] = ((p shr 8) and 0xFF) / 255f // Green
        floatArray[i * 4 + 2] = (p and 0xFF) / 255f         // Blue
        floatArray[i * 4 + 3] = ((p shr 24) and 0xFF) / 255f // Alpha
    }

    return floatArray
}

/**
 * Converts a normalized FloatArray (RGBA) back into a [Bitmap].
 *
 * @receiver Bitmap used for width/height reference
 * @param floatArray Input array with values [0f, 1f]
 * @return Bitmap reconstructed from float array
 */
fun Bitmap.fromFloatArray(floatArray: FloatArray): Bitmap {
    require(floatArray.size == width * height * 4) {
        "Float array size must match bitmap dimensions (width * height * 4)"
    }

    val out = createBitmap(width, height)
    val pixels = IntArray(width * height)

    for (i in pixels.indices) {
        val r = (floatArray[i * 4] * 255f).toInt().coerceIn(0, 255)
        val g = (floatArray[i * 4 + 1] * 255f).toInt().coerceIn(0, 255)
        val b = (floatArray[i * 4 + 2] * 255f).toInt().coerceIn(0, 255)
        val a = (floatArray[i * 4 + 3] * 255f).toInt().coerceIn(0, 255)

        pixels[i] = (a shl 24) or (r shl 16) or (g shl 8) or b
    }

    out.setPixels(pixels, 0, width, 0, 0, width, height)
    return out
}