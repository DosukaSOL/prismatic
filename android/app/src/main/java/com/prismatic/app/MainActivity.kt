// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Activity
import android.graphics.Color
import android.hardware.display.DisplayManager
import android.os.Bundle
import android.view.Display
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView

/**
 * PRISMATIC host activity.
 *
 * Primary display shows the enhanced DS top screen (overworld). The DS bottom
 * screen (touch UI) is shown on a secondary display when one is present (e.g.
 * the AYN Thor Max's second panel), otherwise inline as a picture-in-picture
 * overlay so the whole device works on a single panel too.
 *
 * On-screen controls cycle the enhancement preset, time-of-day, weather and the
 * lantern point light — all driven through the shared native pipeline.
 */
class MainActivity : Activity() {

    private val shared = FrameState()
    private lateinit var topView: PrismaticSurfaceView
    private lateinit var bottomInline: PrismaticSurfaceView
    private lateinit var status: TextView

    private var presentation: SecondaryPresentation? = null
    private var presetCount = 1
    private var lanternOn = false

    private val timeCycle = floatArrayOf(8f, 12f, 18f, 22f)
    private var timeIdx = 1
    private val weatherNames = arrayOf("Clear", "Rain", "Fog", "Snow")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        if (!NativeBridge.nativeInit()) {
            val err = TextView(this).apply {
                text = "Native pipeline failed to initialise."
                setTextColor(Color.RED)
                gravity = Gravity.CENTER
            }
            setContentView(err)
            return
        }
        presetCount = NativeBridge.nativePresetCount().coerceAtLeast(1)

        val root = FrameLayout(this)

        topView = PrismaticSurfaceView(this).apply {
            bottom = false
            setOnTouchListener { _, e -> onTopTouch(e); true }
        }
        root.addView(
            topView,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT
            )
        )

        // Inline bottom-screen overlay (used when there is no secondary display).
        bottomInline = PrismaticSurfaceView(this).apply {
            bottom = true
            setZOrderOnTop(true)
            setOnTouchListener { v, e -> onBottomTouch(v, e); true }
        }
        val inlineParams = FrameLayout.LayoutParams(320, 240).apply {
            gravity = Gravity.TOP or Gravity.END
            topMargin = 24
            rightMargin = 24
        }
        root.addView(bottomInline, inlineParams)

        root.addView(buildControls())
        setContentView(root)
        applyState()
    }

    private fun buildControls(): View {
        val bar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setBackgroundColor(Color.argb(150, 8, 10, 16))
            setPadding(16, 12, 16, 12)
            gravity = Gravity.CENTER_VERTICAL
        }
        fun button(label: String, onClick: () -> Unit) = Button(this).apply {
            text = label
            setOnClickListener { onClick(); applyState() }
        }
        bar.addView(button("Preset -") { shared.presetIndex = (shared.presetIndex - 1 + presetCount) % presetCount })
        bar.addView(button("Preset +") { shared.presetIndex = (shared.presetIndex + 1) % presetCount })
        bar.addView(button("Time") {
            timeIdx = (timeIdx + 1) % timeCycle.size
            shared.timeOfDay = timeCycle[timeIdx]
            lanternOn = shared.timeOfDay >= 19f || shared.timeOfDay < 6f
            NativeBridge.nativeSetLantern(lanternOn)
        })
        bar.addView(button("Weather") { shared.weather = (shared.weather + 1) % weatherNames.size })
        bar.addView(button("Lantern") {
            lanternOn = !lanternOn
            NativeBridge.nativeSetLantern(lanternOn)
        })
        status = TextView(this).apply {
            setTextColor(Color.WHITE)
            setPadding(24, 0, 0, 0)
        }
        bar.addView(status)

        val params = FrameLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT,
            ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { gravity = Gravity.BOTTOM }
        bar.layoutParams = params
        return bar
    }

    private fun applyState() {
        topView.presetIndex = shared.presetIndex
        topView.timeOfDay = shared.timeOfDay
        topView.weather = shared.weather
        bottomInline.presetIndex = shared.presetIndex
        bottomInline.timeOfDay = shared.timeOfDay
        bottomInline.weather = shared.weather
        presentation?.sync()

        val name = NativeBridge.nativePresetName(shared.presetIndex)
        val hh = shared.timeOfDay.toInt()
        status.text = "Preset: $name   Time: %02d:00   Weather: %s   Lantern: %s"
            .format(hh, weatherNames[shared.weather], if (lanternOn) "on" else "off")
    }

    private fun onTopTouch(e: MotionEvent) {
        if (e.actionMasked != MotionEvent.ACTION_DOWN) return
        // Left half = previous preset, right half = next preset.
        if (e.x < topView.width / 2f)
            shared.presetIndex = (shared.presetIndex - 1 + presetCount) % presetCount
        else
            shared.presetIndex = (shared.presetIndex + 1) % presetCount
        applyState()
    }

    private fun onBottomTouch(v: View, e: MotionEvent) {
        val w = v.width.coerceAtLeast(1)
        val h = v.height.coerceAtLeast(1)
        val tx = (e.x / w * 256f).toInt().coerceIn(0, 255)
        val ty = (e.y / h * 192f).toInt().coerceIn(0, 191)
        val down = e.actionMasked != MotionEvent.ACTION_UP &&
            e.actionMasked != MotionEvent.ACTION_CANCEL
        NativeBridge.nativeSetTouch(tx, ty, down)
    }

    override fun onResume() {
        super.onResume()
        val dm = getSystemService(DISPLAY_SERVICE) as DisplayManager
        val secondary = dm.displays.firstOrNull { it.displayId != Display.DEFAULT_DISPLAY }
        if (secondary != null) {
            bottomInline.visibility = View.GONE
            presentation = SecondaryPresentation(this, secondary, shared).also {
                it.show()
            }
        } else {
            bottomInline.visibility = View.VISIBLE
        }
        applyState()
    }

    override fun onPause() {
        presentation?.dismiss()
        presentation = null
        super.onPause()
    }
}
