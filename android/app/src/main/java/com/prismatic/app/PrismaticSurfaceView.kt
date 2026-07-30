// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Rect
import android.util.AttributeSet
import android.view.SurfaceHolder
import android.view.SurfaceView

/**
 * A [SurfaceView] that continuously pulls enhanced frames from [NativeBridge]
 * and blits them to its surface on a dedicated render thread.
 *
 * The native side runs the exact deterministic pipeline validated on desktop,
 * so this shows genuinely enhanced output (upscaled, lit, graded) rather than a
 * mock. Set [bottom] = true to render the DS bottom (touch UI) screen.
 */
class PrismaticSurfaceView @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : SurfaceView(context, attrs), SurfaceHolder.Callback {

    @Volatile var presetIndex: Int = 2          // HD-2.5D BALANCED
    @Volatile var timeOfDay: Float = 12.0f
    @Volatile var weather: Int = 0
    @Volatile var bottom: Boolean = false

    private var thread: RenderThread? = null
    private val bgPaint = Paint().apply { color = Color.BLACK }

    init {
        holder.addCallback(this)
    }

    override fun surfaceCreated(holder: SurfaceHolder) {
        thread = RenderThread(holder).also { it.running = true; it.start() }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {}

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        thread?.let {
            it.running = false
            it.join(500)
        }
        thread = null
    }

    private inner class RenderThread(private val holder: SurfaceHolder) : Thread("PrismaticRender") {
        @Volatile var running = false
        private var bitmap: Bitmap? = null
        private val src = Rect()
        private val dst = Rect()
        private val blit = Paint().apply { isFilterBitmap = false; isAntiAlias = false }

        override fun run() {
            while (running) {
                val frameStart = System.nanoTime()
                val data = if (bottom)
                    NativeBridge.nativeRenderBottom(presetIndex, timeOfDay, weather)
                else
                    NativeBridge.nativeRenderTop(presetIndex, timeOfDay, weather)

                if (data != null && data.size > 2) {
                    val w = data[0]
                    val h = data[1]
                    var bmp = bitmap
                    if (bmp == null || bmp.width != w || bmp.height != h) {
                        bmp = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                        bitmap = bmp
                        src.set(0, 0, w, h)
                    }
                    bmp.setPixels(data, 2, w, 0, 0, w, h)

                    val canvas = holder.lockCanvas() ?: continue
                    try {
                        canvas.drawRect(0f, 0f, width.toFloat(), height.toFloat(), bgPaint)
                        // Letterbox to preserve the DS 4:3 aspect ratio.
                        val scale = minOf(width.toFloat() / w, height.toFloat() / h)
                        val dw = (w * scale).toInt()
                        val dh = (h * scale).toInt()
                        val ox = (width - dw) / 2
                        val oy = (height - dh) / 2
                        dst.set(ox, oy, ox + dw, oy + dh)
                        canvas.drawBitmap(bmp, src, dst, blit)
                    } finally {
                        holder.unlockCanvasAndPost(canvas)
                    }
                }

                // Target ~60 fps; the synthetic backend advances one frame per top render.
                val elapsedMs = (System.nanoTime() - frameStart) / 1_000_000L
                val sleep = 16L - elapsedMs
                if (sleep > 0) {
                    try { sleep(sleep) } catch (_: InterruptedException) {}
                }
            }
        }
    }
}
