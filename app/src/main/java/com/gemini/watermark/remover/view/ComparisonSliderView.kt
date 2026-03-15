package com.gemini.watermark.remover.view

import android.annotation.SuppressLint
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.RectF
import android.util.AttributeSet
import android.view.MotionEvent
import android.view.View
import androidx.core.graphics.withClip
import com.gemini.watermark.remover.R
import kotlin.math.min

/**
 * Custom view for comparing an original image and its processed version
 * using a draggable slider.
 *
 * Features:
 * - Displays original and processed images side-by-side
 * - Draggable handle to adjust comparison split
 * - Handles scaling to fit images in view while preserving aspect ratio
 */
class ComparisonSliderView @JvmOverloads constructor(
    context: Context, attrs: AttributeSet? = null, defStyleAttr: Int = 0
) : View(context, attrs, defStyleAttr) {

    private var originalBitmap: Bitmap? = null
    private var processedBitmap: Bitmap? = null

    @SuppressLint("UseCompatLoadingForDrawables")
    private val handleIcon = context.getDrawable(R.drawable.ic_slider_handle)

    /** Relative split position of slider (0f = left, 1f = right) */
    private var position = 0.5f

    private val paint = Paint(Paint.ANTI_ALIAS_FLAG)

    private val handlePaint = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        color = Color.WHITE
        setShadowLayer(8f, 0f, 0f, Color.argb(80, 0, 0, 0))
    }

    private var handleBounds = RectF()
    private var isDragging = false

    // Image scaling and offset info
    private var scale = 1f
    private var imageWidth = 0
    private var imageHeight = 0
    private var offsetX = 0f
    private var offsetY = 0f

    /**
     * Sets the original and processed images to display.
     *
     * @param original Original bitmap
     * @param processed Processed bitmap with watermark removed
     */
    fun setImages(original: Bitmap, processed: Bitmap) {
        originalBitmap = original
        processedBitmap = processed
        requestLayout()
        invalidate()
    }

    /**
     * Measures the view and calculates scale to fit images while
     * preserving aspect ratio.
     */
    override fun onMeasure(widthMeasureSpec: Int, heightMeasureSpec: Int) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec)

        originalBitmap?.let { bmp ->
            val viewWidth = measuredWidth
            val viewHeight = measuredHeight

            // Scale to fit inside view
            scale = min(viewWidth / bmp.width.toFloat(), viewHeight / bmp.height.toFloat())
            imageWidth = (bmp.width * scale).toInt()
            imageHeight = (bmp.height * scale).toInt()
            offsetX = (viewWidth - imageWidth) / 2f
            offsetY = (viewHeight - imageHeight) / 2f
        }
    }

    /**
     * Draws original and processed images, divider line, and draggable handle.
     */
    @SuppressLint("DrawAllocation")
    override fun onDraw(canvas: Canvas) {
        super.onDraw(canvas)

        val original = originalBitmap ?: return
        val processed = processedBitmap ?: return

        // Center and scale canvas
        canvas.save()
        canvas.translate(offsetX, offsetY)
        canvas.scale(scale, scale)

        // Draw original image
        canvas.drawBitmap(original, 0f, 0f, paint)

        // Compute split point in image coordinates
        val splitImageX = original.width * position

        // Clip left part and draw processed image
        canvas.withClip(0f, 0f, splitImageX, original.height.toFloat()) {
            canvas.drawBitmap(processed, 0f, 0f, paint)
        }

        // Divider line
        paint.color = Color.WHITE
        paint.strokeWidth = 4f / scale
        canvas.drawLine(splitImageX, 0f, splitImageX, original.height.toFloat(), paint)

        canvas.restore()

        // Draw draggable handle in view coordinates
        val splitViewX = offsetX + splitImageX * scale
        val handleRadius = 40f
        handleBounds.set(
            splitViewX - handleRadius,
            offsetY + (imageHeight / 2f) - handleRadius,
            splitViewX + handleRadius,
            offsetY + (imageHeight / 2f) + handleRadius
        )
        canvas.drawCircle(splitViewX, offsetY + (imageHeight / 2f), handleRadius, handlePaint)

        handleIcon?.let { drawable ->
            val iconSize = 60  // size in pixels
            val left = (splitViewX - iconSize / 2).toInt()
            val top = (offsetY + imageHeight / 2f - iconSize / 2).toInt()
            val right = left + iconSize
            val bottom = top + iconSize
            drawable.setBounds(left, top, right, bottom)
            drawable.draw(canvas)
        }
    }

    /**
     * Handles touch events to drag the comparison handle.
     */
    @SuppressLint("ClickableViewAccessibility")
    override fun onTouchEvent(event: MotionEvent): Boolean {
        when (event.action) {
            MotionEvent.ACTION_DOWN -> {
                if (handleBounds.contains(event.x, event.y)) {
                    isDragging = true
                    parent?.requestDisallowInterceptTouchEvent(true)
                    return true
                }
            }
            MotionEvent.ACTION_MOVE -> {
                if (isDragging) {
                    val imageX = (event.x - offsetX) / scale
                    position = (imageX / (originalBitmap?.width ?: 1)).coerceIn(0f, 1f)
                    invalidate()
                    return true
                }
            }
            MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                if (isDragging) {
                    isDragging = false
                    parent?.requestDisallowInterceptTouchEvent(false)
                }
            }
        }
        return super.onTouchEvent(event)
    }
}