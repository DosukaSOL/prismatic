// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Activity
import android.content.Intent
import android.graphics.Color
import android.hardware.display.DisplayManager
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.view.Display
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import java.io.File
import kotlin.math.abs

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
    private var padMask = 0                       // live DS button state (physical pad)

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
        bar.addView(button("Load ROM") { pickRom() })
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
        val game = NativeBridge.nativeGameTitle()
        val gamePrefix = if (game.isNotBlank()) "$game  ·  " else ""
        status.text = gamePrefix + "Preset: $name   Time: %02d:00   Weather: %s   Lantern: %s"
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

    // ---- ROM loading (Storage Access Framework) ---------------------------

    private fun pickRom() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            // .nds has no registered MIME type, so accept any file and validate
            // by attempting to boot it (makeNdsAdapter rejects non-DS ROMs).
            type = "*/*"
        }
        try {
            startActivityForResult(intent, ROM_PICK_REQUEST)
        } catch (e: Exception) {
            toast("No file picker available: ${e.message}")
        }
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (requestCode == ROM_PICK_REQUEST && resultCode == RESULT_OK) {
            data?.data?.let { loadRomFromUri(it) }
        }
    }

    private fun loadRomFromUri(uri: Uri) {
        try {
            val display = queryDisplayName(uri) ?: "game.nds"
            val romsDir = File(filesDir, "roms").apply { mkdirs() }
            val dest = File(romsDir, sanitizeFileName(display))
            contentResolver.openInputStream(uri)?.use { input ->
                dest.outputStream().use { input.copyTo(it) }
            } ?: run { toast("Could not open the selected file"); return }

            val ok = NativeBridge.nativeLoadRom(dest.absolutePath, filesDir.absolutePath)
            if (ok) {
                val title = NativeBridge.nativeGameTitle()
                toast("Loaded ${if (title.isNotBlank()) title else dest.name}")
            } else {
                toast("Not a valid NDS ROM")
            }
            applyState()
        } catch (e: Exception) {
            toast("ROM load error: ${e.message}")
        }
    }

    private fun queryDisplayName(uri: Uri): String? {
        contentResolver.query(uri, null, null, null, null)?.use { c ->
            val idx = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (idx >= 0 && c.moveToFirst()) return c.getString(idx)
        }
        return null
    }

    private fun sanitizeFileName(name: String): String =
        name.replace(Regex("[^A-Za-z0-9._-]"), "_").take(120)

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()

    // ---- Physical gamepad input (AYN Thor controls) -----------------------

    private fun keyToBit(keyCode: Int): Int = when (keyCode) {
        KeyEvent.KEYCODE_DPAD_UP -> NativeBridge.BTN_UP
        KeyEvent.KEYCODE_DPAD_DOWN -> NativeBridge.BTN_DOWN
        KeyEvent.KEYCODE_DPAD_LEFT -> NativeBridge.BTN_LEFT
        KeyEvent.KEYCODE_DPAD_RIGHT -> NativeBridge.BTN_RIGHT
        // Map by physical position (DS layout): A=right, B=bottom, X=top, Y=left.
        KeyEvent.KEYCODE_BUTTON_A -> NativeBridge.BTN_B   // bottom face
        KeyEvent.KEYCODE_BUTTON_B -> NativeBridge.BTN_A   // right face
        KeyEvent.KEYCODE_BUTTON_X -> NativeBridge.BTN_Y   // left face
        KeyEvent.KEYCODE_BUTTON_Y -> NativeBridge.BTN_X   // top face
        KeyEvent.KEYCODE_BUTTON_L1 -> NativeBridge.BTN_L
        KeyEvent.KEYCODE_BUTTON_R1 -> NativeBridge.BTN_R
        KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> NativeBridge.BTN_START
        KeyEvent.KEYCODE_BUTTON_SELECT -> NativeBridge.BTN_SELECT
        else -> 0
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        val bit = keyToBit(keyCode)
        if (bit != 0 && event?.repeatCount == 0) {
            padMask = padMask or bit
            topView.inputMask = padMask
        }
        return if (bit != 0) true else super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        val bit = keyToBit(keyCode)
        if (bit != 0) {
            padMask = padMask and bit.inv()
            topView.inputMask = padMask
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE) {
            val hx = event.getAxisValue(MotionEvent.AXIS_HAT_X)
            val hy = event.getAxisValue(MotionEvent.AXIS_HAT_Y)
            val x = if (abs(hx) > 0.5f) hx else event.getAxisValue(MotionEvent.AXIS_X)
            val y = if (abs(hy) > 0.5f) hy else event.getAxisValue(MotionEvent.AXIS_Y)
            val dpad = NativeBridge.BTN_UP or NativeBridge.BTN_DOWN or
                NativeBridge.BTN_LEFT or NativeBridge.BTN_RIGHT
            var m = padMask and dpad.inv()
            if (x < -0.5f) m = m or NativeBridge.BTN_LEFT else if (x > 0.5f) m = m or NativeBridge.BTN_RIGHT
            if (y < -0.5f) m = m or NativeBridge.BTN_UP else if (y > 0.5f) m = m or NativeBridge.BTN_DOWN
            padMask = m
            topView.inputMask = padMask
            return true
        }
        return super.onGenericMotionEvent(event)
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

    companion object {
        private const val ROM_PICK_REQUEST = 0x2001
    }
}
