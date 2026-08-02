// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Presentation
import android.content.Context
import android.graphics.Color
import android.os.Bundle
import android.view.Display
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView

/**
 * Owns the AYN Thor's lower physical display.
 *
 * Two modes:
 *  - LOGO (default): the official full Prismatic logo, centred on a near-black
 *    background (AMOLED-safe, dimmed) — shown on the home screen and every
 *    game-management screen.
 *  - GAME: the DS bottom touchscreen with correct 256x192 touch mapping —
 *    shown only while a game is actually running.
 *
 * If no secondary display is present MainActivity renders inline instead.
 * Dual-display routing must still be confirmed on real hardware.
 */
class SecondaryPresentation(
    context: Context,
    display: Display,
    private val shared: FrameState
) : Presentation(context, display) {

    private lateinit var view: PrismaticSurfaceView
    private lateinit var logo: ImageView
    private var gameMode = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = FrameLayout(context).apply { setBackgroundColor(Color.rgb(4, 5, 10)) }
        view = PrismaticSurfaceView(context).apply {
            bottom = true
            presetIndex = shared.presetIndex
            timeOfDay = shared.timeOfDay
            weather = shared.weather
            visibility = View.GONE
        }
        root.addView(
            view,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
        // Official full Prismatic logo: centred, aspect-preserved, dimmed to be
        // easy on the AMOLED panel during long menu sessions.
        logo = ImageView(context).apply {
            setImageResource(R.drawable.prismatic_logo)
            scaleType = ImageView.ScaleType.FIT_CENTER
            alpha = 0.85f
            val dm = context.resources.displayMetrics
            val pad = (minOf(dm.widthPixels, dm.heightPixels) * 0.12f).toInt()
            setPadding(pad, pad, pad, pad)
        }
        root.addView(
            logo,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
        setContentView(root)
        applyMode()
    }

    /** GAME shows the DS bottom screen; LOGO (false) shows the Prismatic logo. */
    fun setGameMode(game: Boolean) {
        gameMode = game
        if (::view.isInitialized) applyMode()
    }

    private fun applyMode() {
        view.visibility = if (gameMode) View.VISIBLE else View.GONE
        logo.visibility = if (gameMode) View.GONE else View.VISIBLE
        view.paused = !gameMode
    }

    fun sync() {
        if (::view.isInitialized) {
            view.presetIndex = shared.presetIndex
            view.timeOfDay = shared.timeOfDay
            view.weather = shared.weather
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (!gameMode) return false   // logo screen is not a touch surface
        val w = view.width.coerceAtLeast(1)
        val h = view.height.coerceAtLeast(1)
        val tx = (event.x / w * 256f).toInt().coerceIn(0, 255)
        val ty = (event.y / h * 192f).toInt().coerceIn(0, 191)
        val down = event.actionMasked != MotionEvent.ACTION_UP &&
            event.actionMasked != MotionEvent.ACTION_CANCEL
        NativeBridge.nativeSetTouch(tx, ty, down)
        return true
    }
}

/** Shared render parameters mirrored across the primary and secondary screens. */
class FrameState {
    @Volatile var presetIndex: Int = 2
    @Volatile var timeOfDay: Float = 12.0f
    @Volatile var weather: Int = 0
}
