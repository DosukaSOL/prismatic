// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.LayerDrawable
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.animation.DecelerateInterpolator
import android.view.animation.OvershootInterpolator
import android.widget.Button
import android.widget.LinearLayout
import android.widget.TextView

/**
 * Prismatic brand system — colours, rounded/gradient drawables, buttons and
 * entrance/press animations, all built programmatically (no XML). Keeps the
 * emulator's look consistent and "premium" like Eden / melonDS / Cemu.
 */
object Brand {
    // Core palette (spectrum-on-graphite).
    const val BG_TOP = 0xFF0B0B12.toInt()
    const val BG_BOTTOM = 0xFF161826.toInt()
    const val SURFACE = 0xFF161826.toInt()
    const val SURFACE_HI = 0xFF1E2230.toInt()
    const val STROKE = 0xFF2A2F42.toInt()
    const val PRIMARY = 0xFF7C3AED.toInt()      // violet
    const val PRIMARY_DK = 0xFF5B21B6.toInt()
    const val PRIMARY_LT = 0xFFB79CFF.toInt()
    const val CYAN = 0xFF22D3EE.toInt()
    const val PINK = 0xFFF471B5.toInt()
    const val TEXT = 0xFFFFFFFF.toInt()
    const val TEXT_MUTED = 0xFF8A93A6.toInt()

    fun dp(ctx: Context, v: Float): Int =
        TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, v, ctx.resources.displayMetrics).toInt()

    /** Full-screen brand background: vertical graphite gradient + soft violet glow. */
    fun screenBackground(): GradientDrawable = GradientDrawable(
        GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(BG_TOP, BG_BOTTOM, BG_TOP)
    ).apply {
        gradientType = GradientDrawable.LINEAR_GRADIENT
    }

    /** A rounded surface "card" with optional gradient + hairline stroke. */
    fun card(
        radiusDp: Float,
        top: Int = SURFACE,
        bottom: Int = SURFACE_HI,
        stroke: Int = STROKE,
        strokeDp: Float = 1f,
        ctx: Context,
    ): GradientDrawable = GradientDrawable(
        GradientDrawable.Orientation.TOP_BOTTOM, intArrayOf(top, bottom)
    ).apply {
        cornerRadius = dp(ctx, radiusDp).toFloat()
        if (strokeDp > 0f) setStroke(dp(ctx, strokeDp), stroke)
    }

    /** Solid rounded pill (single colour). */
    fun pill(ctx: Context, color: Int, radiusDp: Float = 999f): GradientDrawable =
        GradientDrawable().apply {
            setColor(color)
            cornerRadius = dp(ctx, radiusDp).toFloat()
        }

    /** Primary (violet gradient) action button with a bright edge + press animation. */
    fun primaryButton(ctx: Context, label: String, onClick: () -> Unit): Button =
        buildButton(ctx, label, onClick).apply {
            background = GradientDrawable(
                GradientDrawable.Orientation.LEFT_RIGHT, intArrayOf(PRIMARY, 0xFF9333EA.toInt())
            ).apply {
                cornerRadius = dp(ctx, 16f).toFloat()
                setStroke(dp(ctx, 1f), PRIMARY_LT)
            }
            setTextColor(TEXT)
        }

    /** Secondary "ghost" button (surface card). */
    fun ghostButton(ctx: Context, label: String, onClick: () -> Unit): Button =
        buildButton(ctx, label, onClick).apply {
            background = card(14f, ctx = ctx)
            setTextColor(TEXT)
        }

    private fun buildButton(ctx: Context, label: String, onClick: () -> Unit): Button =
        Button(ctx).apply {
            text = label
            isAllCaps = false
            textSize = 16f
            typeface = Typeface.DEFAULT_BOLD
            stateListAnimator = null
            setPadding(dp(ctx, 22f), dp(ctx, 16f), dp(ctx, 22f), dp(ctx, 16f))
            attachPress(this)
            setOnClickListener { onClick() }
        }

    /** A section heading label in muted spectrum styling. */
    fun sectionLabel(ctx: Context, text: String): TextView = TextView(ctx).apply {
        this.text = text.uppercase()
        setTextColor(PRIMARY_LT)
        textSize = 12f
        letterSpacing = 0.14f
        typeface = Typeface.DEFAULT_BOLD
        setPadding(dp(ctx, 4f), dp(ctx, 14f), 0, dp(ctx, 6f))
    }

    /** Scale-down feedback while a view is pressed. */
    fun attachPress(view: View) {
        view.setOnTouchListener { v, e ->
            when (e.actionMasked) {
                MotionEvent.ACTION_DOWN ->
                    v.animate().scaleX(0.96f).scaleY(0.96f).setDuration(90).start()
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL ->
                    v.animate().scaleX(1f).scaleY(1f).setDuration(120)
                        .setInterpolator(OvershootInterpolator()).start()
            }
            false   // don't consume — let click + focus still work
        }
    }

    /** Fade + rise entrance animation, optionally staggered. */
    fun enterFrom(view: View, delayMs: Long = 0L, rise: Float = 26f) {
        view.alpha = 0f
        view.translationY = dp(view.context, rise).toFloat()
        view.animate().alpha(1f).translationY(0f)
            .setStartDelay(delayMs).setDuration(360)
            .setInterpolator(DecelerateInterpolator()).start()
    }

    /** Pop entrance for hero elements (logo). */
    fun popIn(view: View, delayMs: Long = 0L) {
        view.alpha = 0f
        view.scaleX = 0.9f
        view.scaleY = 0.9f
        view.animate().alpha(1f).scaleX(1f).scaleY(1f)
            .setStartDelay(delayMs).setDuration(520)
            .setInterpolator(OvershootInterpolator(1.4f)).start()
    }

    /** Convenience: a vertical LinearLayout with standard padding. */
    fun column(ctx: Context): LinearLayout = LinearLayout(ctx).apply {
        orientation = LinearLayout.VERTICAL
        gravity = Gravity.CENTER_HORIZONTAL
    }

    fun spacer(ctx: Context, heightDp: Float): View = View(ctx).apply {
        layoutParams = LinearLayout.LayoutParams(1, dp(ctx, heightDp))
    }
}
