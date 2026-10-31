package com.carriez.flutter_hbb.desktopctl

import android.graphics.Bitmap
import android.media.Image
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicReference

internal class RustDeskFrameCapture {
    private val pendingRequest = AtomicReference<CaptureRequest?>()

    fun capture(timeoutMilliseconds: Long): Bitmap? {
        val request = CaptureRequest()
        if (!pendingRequest.compareAndSet(null, request)) {
            return null
        }
        if (!request.latch.await(timeoutMilliseconds, TimeUnit.MILLISECONDS)) {
            pendingRequest.compareAndSet(request, null)
            return null
        }
        return request.bitmap
    }

    fun onImage(image: Image, width: Int, height: Int) {
        val request = pendingRequest.getAndSet(null) ?: return
        try {
            request.bitmap = copyBitmap(image, width, height)
        } finally {
            request.latch.countDown()
        }
    }

    fun cancel() {
        pendingRequest.getAndSet(null)?.latch?.countDown()
    }

    private fun copyBitmap(image: Image, width: Int, height: Int): Bitmap? {
        if (width <= 0 || height <= 0 || image.planes.isEmpty()) {
            return null
        }
        val plane = image.planes[0]
        val pixelStride = plane.pixelStride
        val rowStride = plane.rowStride
        if (pixelStride <= 0 || rowStride < width * pixelStride) {
            return null
        }
        val rowPadding = rowStride - width * pixelStride
        val paddedWidth = width + rowPadding / pixelStride
        val buffer = plane.buffer.duplicate()
        buffer.rewind()
        val paddedBitmap = Bitmap.createBitmap(
            paddedWidth,
            height,
            Bitmap.Config.ARGB_8888
        )
        paddedBitmap.copyPixelsFromBuffer(buffer)
        if (paddedWidth == width) {
            return paddedBitmap
        }
        val croppedBitmap = Bitmap.createBitmap(paddedBitmap, 0, 0, width, height)
        paddedBitmap.recycle()
        return croppedBitmap
    }

    private class CaptureRequest {
        val latch = CountDownLatch(1)
        var bitmap: Bitmap? = null
    }
}
