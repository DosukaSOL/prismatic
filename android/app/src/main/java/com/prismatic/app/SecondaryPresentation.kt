// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Presentation
import android.content.Context
import android.os.Bundle
import android.view.Display
import android.view.MotionEvent
import android.view.ViewGroup
import android.widget.FrameLayout

/**
 * Renders the DS bottom screen (touch UI) on a secondary display.
 *
 * On the AYN Thor Max the second AMOLED panel appears as an additional
 * [Display]; this Presentation targets it. Touches on this surface are mapped
 * into the native backend's 256x192 touch space. If no secondary display is
 * present the app renders the bottom screen inline instead (see MainActivity).
 *
 * NOTE: dual-display routing is not device-verified in this build environment;
 * it is validated logically and must be confirmed on real hardware.
 */
class SecondaryPresentation(
    context: Context,
    display: Display,
    private val shared: FrameState
) : Presentation(context, display) {

    private lateinit var view: PrismaticSurfaceView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val root = FrameLayout(context)
        view = PrismaticSurfaceView(context).apply {
            bottom = true
            presetIndex = shared.presetIndex
            timeOfDay = shared.timeOfDay
            weather = shared.weather
        }
        root.addView(
            view,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )
        setContentView(root)
    }

    fun sync() {
        if (::view.isInitialized) {
            view.presetIndex = shared.presetIndex
            view.timeOfDay = shared.timeOfDay
            view.weather = shared.weather
        }
    }

    override fun onTouchEvent(event: MotionEvent): Boolean {
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
