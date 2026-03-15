package com.gemini.watermark.remover.core

/**
 * Configuration settings for watermark detection and removal.
 *
 * @property logoSize Expected size of the watermark logo in pixels
 * @property marginRight Right margin offset for watermark detection
 * @property marginBottom Bottom margin offset for watermark detection
 */
data class WatermarkConfig(
    val logoSize: Int,
    val marginRight: Int,
    val marginBottom: Int
)

/**
 * Represents the position and size of a watermark within an image.
 *
 * @property x X-coordinate of the top-left corner
 * @property y Y-coordinate of the top-left corner
 * @property width Width of the watermark in pixels
 * @property height Height of the watermark in pixels
 */
data class WatermarkPosition(
    val x: Int,
    val y: Int,
    val width: Int,
    val height: Int
)

/**
 * Metadata for a detected watermark.
 *
 * @property size Size of the watermark in pixels
 * @property position Position and dimensions of the watermark
 * @property config Configuration used for detection and removal
 */
data class WatermarkInfo(
    val size: Int,
    val position: WatermarkPosition,
    val config: WatermarkConfig
)