// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import android.graphics.Canvas
import android.graphics.ColorFilter
import android.graphics.Paint
import android.graphics.Path
import android.graphics.PixelFormat
import android.graphics.drawable.Drawable

/**
 * Prismatic vector icon set. Each icon is authored as SVG-style path commands
 * in a 24x24 grid and rendered as a crisp stroked/filled drawable at any size
 * — no emoji, no bitmaps.
 */
object Icons {

    private class PathIcon(
        private val build: (Path, Path) -> Unit,   // (stroke path, fill path)
        private val color: Int,
        private val strokeWidthDp: Float,
    ) : Drawable() {
        private val stroke = Path()
        private val fill = Path()
        private val paintS = Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style = Paint.Style.STROKE
            strokeCap = Paint.Cap.ROUND
            strokeJoin = Paint.Join.ROUND
        }
        private val paintF = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }

        override fun draw(canvas: Canvas) {
            val b = bounds
            if (b.isEmpty) return
            stroke.reset(); fill.reset()
            build(stroke, fill)
            val s = minOf(b.width(), b.height()) / 24f
            canvas.save()
            canvas.translate(b.left + (b.width() - 24f * s) / 2f, b.top + (b.height() - 24f * s) / 2f)
            canvas.scale(s, s)
            paintS.color = color
            paintS.strokeWidth = strokeWidthDp
            paintF.color = color
            canvas.drawPath(fill, paintF)
            canvas.drawPath(stroke, paintS)
            canvas.restore()
        }

        override fun setAlpha(alpha: Int) { paintS.alpha = alpha; paintF.alpha = alpha }
        override fun setColorFilter(cf: ColorFilter?) { paintS.colorFilter = cf; paintF.colorFilter = cf }
        @Deprecated("Deprecated in Java")
        override fun getOpacity(): Int = PixelFormat.TRANSLUCENT
    }

    private fun icon(color: Int, sw: Float = 1.8f, build: (Path, Path) -> Unit): Drawable =
        PathIcon(build, color, sw)

    /** Solid play triangle. */
    fun play(color: Int): Drawable = icon(color) { _, f ->
        f.moveTo(8f, 5f); f.lineTo(19f, 12f); f.lineTo(8f, 19f); f.close()
    }

    /** Resume/continue: play triangle + right bar. */
    fun resume(color: Int): Drawable = icon(color) { s, f ->
        f.moveTo(6f, 5f); f.lineTo(15f, 12f); f.lineTo(6f, 19f); f.close()
        s.moveTo(18.4f, 5f); s.lineTo(18.4f, 19f)
    }

    /** Mods: puzzle piece. */
    fun mods(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(9f, 4f); s.lineTo(9f, 6.4f)
        s.cubicTo(9f, 7.6f, 10f, 8.4f, 11f, 8.4f)
        s.cubicTo(12f, 8.4f, 13f, 7.6f, 13f, 6.4f)
        s.lineTo(13f, 4f); s.lineTo(20f, 4f); s.lineTo(20f, 11f); s.lineTo(17.6f, 11f)
        s.cubicTo(16.4f, 11f, 15.6f, 12f, 15.6f, 13f)
        s.cubicTo(15.6f, 14f, 16.4f, 15f, 17.6f, 15f)
        s.lineTo(20f, 15f); s.lineTo(20f, 20f); s.lineTo(4f, 20f); s.lineTo(4f, 4f); s.close()
    }

    /** Camera: movie-camera body + lens wedge. */
    fun camera(color: Int): Drawable = icon(color) { s, _ ->
        s.addRoundRect(3f, 7f, 14.5f, 17f, 2f, 2f, Path.Direction.CW)
        s.moveTo(14.5f, 10.6f); s.lineTo(20.5f, 7.5f); s.lineTo(20.5f, 16.5f)
        s.lineTo(14.5f, 13.4f)
    }

    /** Performance: speedometer. */
    fun gauge(color: Int): Drawable = icon(color) { s, f ->
        s.moveTo(4f, 16.5f)
        s.cubicTo(4f, 11.5f, 7.6f, 8f, 12f, 8f)
        s.cubicTo(16.4f, 8f, 20f, 11.5f, 20f, 16.5f)
        s.moveTo(12f, 16.5f); s.lineTo(16.2f, 11.4f)
        f.addCircle(12f, 16.5f, 1.4f, Path.Direction.CW)
    }

    /** Saves: floppy disk. */
    fun save(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(5f, 4f); s.lineTo(16.5f, 4f); s.lineTo(20f, 7.5f); s.lineTo(20f, 20f)
        s.lineTo(5f, 20f); s.close()
        s.addRect(8f, 4f, 15f, 8.5f, Path.Direction.CW)
        s.addRoundRect(8f, 12.5f, 17f, 20f, 1f, 1f, Path.Direction.CW)
    }

    /** Save states: layered snapshots. */
    fun states(color: Int): Drawable = icon(color) { s, _ ->
        s.addRoundRect(7.5f, 4f, 20f, 13.5f, 2f, 2f, Path.Direction.CW)
        s.moveTo(16.5f, 17f); s.lineTo(6f, 17f)
        s.cubicTo(4.9f, 17f, 4f, 16.1f, 4f, 15f); s.lineTo(4f, 8f)
    }

    /** Installation/info: cartridge. */
    fun cartridge(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(6f, 4f); s.lineTo(18f, 4f); s.lineTo(18f, 20f); s.lineTo(6f, 20f)
        s.lineTo(6f, 9f); s.lineTo(8.5f, 6.5f); s.close()
        s.addRoundRect(9f, 8f, 15f, 12f, 1f, 1f, Path.Direction.CW)
    }

    /** Compatibility: shield + check. */
    fun shield(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(12f, 3.5f); s.lineTo(19f, 6f); s.lineTo(19f, 12f)
        s.cubicTo(19f, 16.5f, 16f, 19.5f, 12f, 21f)
        s.cubicTo(8f, 19.5f, 5f, 16.5f, 5f, 12f)
        s.lineTo(5f, 6f); s.close()
        s.moveTo(8.8f, 12f); s.lineTo(11.2f, 14.4f); s.lineTo(15.4f, 9.6f)
    }

    /** Diagnostics: wrench. */
    fun wrench(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(14.5f, 4.5f)
        s.cubicTo(16.8f, 3.6f, 19.4f, 4.6f, 20.2f, 6.9f)
        s.moveTo(14.5f, 4.5f); s.lineTo(13f, 9f); s.lineTo(4.8f, 17.2f)
        s.cubicTo(4f, 18f, 4f, 19.2f, 4.8f, 20f)
        s.cubicTo(5.6f, 20.8f, 6.8f, 20.8f, 7.6f, 20f)
        s.lineTo(15.8f, 11.8f); s.lineTo(20.2f, 10.4f)
    }

    /** Add game: rounded square + plus. */
    fun add(color: Int): Drawable = icon(color) { s, _ ->
        s.addRoundRect(4f, 4f, 20f, 20f, 4f, 4f, Path.Direction.CW)
        s.moveTo(12f, 8.5f); s.lineTo(12f, 15.5f)
        s.moveTo(8.5f, 12f); s.lineTo(15.5f, 12f)
    }

    /** Settings gear (simplified). */
    fun gear(color: Int): Drawable = icon(color) { s, _ ->
        s.addCircle(12f, 12f, 3f, Path.Direction.CW)
        for (i in 0 until 8) {
            val a = Math.toRadians(i * 45.0)
            val c = Math.cos(a).toFloat(); val si = Math.sin(a).toFloat()
            s.moveTo(12f + 6.2f * c, 12f + 6.2f * si)
            s.lineTo(12f + 8.6f * c, 12f + 8.6f * si)
        }
    }

    /** Exit: door + arrow. */
    fun exit(color: Int): Drawable = icon(color) { s, _ ->
        s.moveTo(13f, 4f); s.lineTo(5f, 4f); s.lineTo(5f, 20f); s.lineTo(13f, 20f)
        s.moveTo(10f, 12f); s.lineTo(20f, 12f)
        s.moveTo(16.8f, 8.8f); s.lineTo(20f, 12f); s.lineTo(16.8f, 15.2f)
    }

    /** Back chevron. */
    fun back(color: Int): Drawable = icon(color, 2.2f) { s, _ ->
        s.moveTo(14.5f, 6f); s.lineTo(9f, 12f); s.lineTo(14.5f, 18f)
    }

    /** Helper: fixed-size icon bounds for a TextView compound drawable. */
    fun sized(ctx: Context, d: Drawable, dp: Float): Drawable {
        val px = Brand.dp(ctx, dp)
        d.setBounds(0, 0, px, px)
        return d
    }
}
