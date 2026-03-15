package com.gemini.watermark.remover.core

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import androidx.annotation.WorkerThread
import java.util.concurrent.atomic.AtomicBoolean

/**
 * Core engine for watermark detection and removal.
 *
 * This class wraps native C++ processing via JNI.
 * Designed as a thread-safe singleton.
 *
 * Usage:
 * ```
 * val engine = WatermarkEngine.create(context)
 * val processedBitmap = engine.removeWatermark(inputBitmap, "auto")
 * engine.release()
 * ```
 *
 * @param context Application context
 */
class WatermarkEngine private constructor(private val context: Context) {

    companion object {
        @SuppressLint("StaticFieldLeak")
        @Volatile
        private var instance: WatermarkEngine? = null

        /**
         * Thread-safe singleton creation.
         * Initializes native resources on first call.
         *
         * @param context Context for native initialization
         * @return Singleton instance of [WatermarkEngine]
         */
        @WorkerThread
        fun create(context: Context): WatermarkEngine {
            return instance ?: synchronized(this) {
                instance ?: WatermarkEngine(context.applicationContext).also {
                    it.initialize()
                }
            }
        }
    }

    /** Native handle to C++ engine */
    private var nativeHandle: Long = 0

    /** Tracks whether engine has been released */
    private val isReleased = AtomicBoolean(false)

    init {
        // Load native library containing watermark remover logic
        System.loadLibrary("watermark_remover")
    }

    // ====== Native JNI Functions ======

    private external fun nativeInitialize(context: Context): Long
    private external fun nativeRelease(handle: Long)
    private external fun nativeRemoveWatermark(
        handle: Long,
        pixels: FloatArray,
        width: Int,
        height: Int,
        adaptiveMode: String
    ): Boolean

    /** Initializes native engine */
    private fun initialize() {
        nativeHandle = nativeInitialize(context)
    }

    /**
     * Removes watermark from the given [input] Bitmap.
     *
     * Converts Bitmap to FloatArray, calls native C++ engine,
     * and reconstructs the Bitmap.
     *
     * @param input Bitmap to process
     * @param adaptiveMode Adaptive removal mode ("auto", "strong", etc.)
     * @return Bitmap with watermark removed
     * @throws RuntimeException if removal fails
     */
    @WorkerThread
    fun removeWatermark(
        input: Bitmap,
        adaptiveMode: String = "auto"
    ): Bitmap {
        require(!isReleased.get()) { "Engine already released" }

        val floatPixels = input.toFloatArray()

        val success = nativeRemoveWatermark(
            nativeHandle,
            floatPixels,
            input.width,
            input.height,
            adaptiveMode
        )

        if (!success) throw RuntimeException("Watermark removal failed")

        return input.fromFloatArray(floatPixels)
    }

    /**
     * Releases native resources.
     * Safe to call multiple times.
     */
    fun release() {
        if (isReleased.compareAndSet(false, true)) {
            nativeRelease(nativeHandle)
            nativeHandle = 0
        }
    }

    /**
     * Fallback cleanup in case release() is not called explicitly.
     */
    protected fun finalize() {
        release()
    }
}