package com.gemini.watermark.remover.model

import android.graphics.Bitmap
import android.net.Uri

/**
 * Represents an image item in the processing pipeline.
 *
 * @property uri URI of the original image
 * @property processedBitmap The result after watermark removal, or null if still processing
 * @property error True if the image could not be processed (e.g., not a Gemini image)
 */
data class ProcessingItem(
    val uri: Uri,
    var processedBitmap: Bitmap? = null,
    var error: Boolean = false
)