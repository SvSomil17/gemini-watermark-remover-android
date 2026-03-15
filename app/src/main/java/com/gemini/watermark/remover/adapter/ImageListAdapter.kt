package com.gemini.watermark.remover.adapter

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import com.gemini.watermark.remover.R
import com.gemini.watermark.remover.databinding.ItemImagePreviewBinding
import com.gemini.watermark.remover.model.ProcessingItem

/**
 * RecyclerView Adapter responsible for displaying
 * original images and their processed (watermark removed) results.
 *
 * Each item represents a [ProcessingItem] that may be:
 * - Processing
 * - Successfully processed
 * - Invalid / Error (not a Gemini generated image)
 *
 * @param items List of images currently being processed or completed
 * @param onDownloadClick Callback triggered when user taps download button
 */
class ImageListAdapter(
    private val items: List<ProcessingItem>,
    private val onDownloadClick: (ProcessingItem) -> Unit
) : RecyclerView.Adapter<ImageListAdapter.ViewHolder>() {

    /**
     * ViewHolder containing ViewBinding reference.
     * Keeps UI references cached for performance.
     */
    class ViewHolder(val binding: ItemImagePreviewBinding) :
        RecyclerView.ViewHolder(binding.root)

    /**
     * Inflates the layout for each RecyclerView item.
     */
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val binding = ItemImagePreviewBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return ViewHolder(binding)
    }

    /**
     * Binds image data to UI components and handles
     * different states of the processing pipeline.
     */
    @SuppressLint("SetTextI18n")
    override fun onBindViewHolder(holder: ViewHolder, position: Int) {

        val item = items[position]
        val binding = holder.binding

        binding.imageIndex.text = (position + 1).toString()

        if (item.error) {
            binding.originalImage.setImageResource(R.drawable.ic_error)
            binding.processedImage.visibility = View.GONE
            binding.downloadSingle.isEnabled = false
            binding.statusText.text = "Not a Gemini image"
        } else {
            binding.originalImage.setImageURI(item.uri)
            item.processedBitmap?.let {
                binding.processedImage.setImageBitmap(
                    it
                )
                binding.processedImage.visibility = View.VISIBLE
                binding.downloadSingle.isEnabled = true
                binding.statusText.text = ""
            } ?: run {
                binding.processedImage.visibility =
                    View.GONE
                binding.downloadSingle.isEnabled = false
                binding.statusText.text = "Processing..."
            }
            binding.downloadSingle.setOnClickListener { onDownloadClick(item) }
        }
    }

    /**
     * Returns total number of items in the list.
     */
    override fun getItemCount(): Int = items.size
}