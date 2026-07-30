// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
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
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import java.io.File
import kotlin.math.abs

/**
 * PRISMATIC host activity — a full-screen DS emulator front-end.
 *
 * The game runs full-screen: on a single-panel device both DS screens are
 * stacked (top over bottom, like a normal emulator); when a second display is
 * present (e.g. the AYN Thor Max's second panel) the bottom screen is routed
 * there instead. There is NO always-on overlay — pressing Back (or the pad's
 * Mode button) opens a translucent pause menu, RetroArch-style.
 *
 * Presentation is layered and fully independent: the raw emulated frame is
 * faithful by default; an optional geometric 2.5D tilt and an optional shader
 * overlay can each be toggled on their own (either, both, or neither).
 *
 * Saves use the game's own battery save (SRAM), written to a visible on-device
 * folder (Android/data/com.prismatic.app/files/saves) and auto-loaded whenever
 * the ROM boots. With "Auto-load last game" enabled, relaunching the app boots
 * the last ROM automatically so the player continues where they left off.
 */
class MainActivity : Activity() {

    private val shared = FrameState()
    private lateinit var topView: PrismaticSurfaceView
    private lateinit var bottomInline: PrismaticSurfaceView
    private lateinit var gameContainer: LinearLayout

    private var presentation: SecondaryPresentation? = null
    private var padMask = 0                         // live DS button state (physical pad)
    private val audio = AudioPlayer()

    // ---- Presentation / session state (persisted) -------------------------
    private var enable25D = false
    private var enableShader = false
    private var shaderStyle = 0                     // 0=CRT 1=LCD 2=Warm 3=Night 4=Vivid
    private var jitOn = true                        // Fast (JIT) vs Compatible
    private var lanternOn = false
    private var timeIdx = 1
    private var autoLoad = true
    private var lastRomPath = ""

    private val styleNames = arrayOf("CRT", "LCD", "Warm", "Night", "Vivid")
    private val timeCycle = floatArrayOf(8f, 12f, 18f, 22f)

    // ---- Pause menu -------------------------------------------------------
    private lateinit var pauseMenu: View
    private lateinit var menuColumn: LinearLayout
    private var menuVisible = false
    private lateinit var btn25D: Button
    private lateinit var btnShader: Button
    private lateinit var btnStyle: Button
    private lateinit var btnTime: Button
    private lateinit var btnLantern: Button
    private lateinit var btnSpeed: Button
    private lateinit var btnAutoLoad: Button

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        enterImmersive()

        if (!NativeBridge.nativeInit()) {
            val err = TextView(this).apply {
                text = "Native pipeline failed to initialise."
                setTextColor(Color.RED)
                gravity = Gravity.CENTER
            }
            setContentView(err)
            return
        }

        loadPrefs()

        val root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }

        // Stacked DS screens (bottom is hidden when a secondary display is used).
        gameContainer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        topView = PrismaticSurfaceView(this).apply { bottom = false }
        bottomInline = PrismaticSurfaceView(this).apply {
            bottom = true
            setZOrderMediaOverlay(true)             // above the top surface, below UI views
            setOnTouchListener { v, e -> onBottomTouch(v, e); true }
        }
        gameContainer.addView(
            topView, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        )
        gameContainer.addView(
            bottomInline, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        )
        root.addView(
            gameContainer,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT
            )
        )

        pauseMenu = buildPauseMenu()
        root.addView(pauseMenu)
        setContentView(root)

        // Push persisted presentation to the core, then restore the last game.
        applyPresentation()
        NativeBridge.nativeSetJit(jitOn)
        NativeBridge.nativeSetLantern(lanternOn)
        applyState()

        if (autoLoad && lastRomPath.isNotBlank() && File(lastRomPath).exists()) {
            if (NativeBridge.nativeLoadRom(lastRomPath, saveDir())) {
                val title = NativeBridge.nativeGameTitle()
                toast("Resumed ${if (title.isNotBlank()) title else File(lastRomPath).name}")
            }
        } else {
            // No game yet: open the menu so the player can load a ROM.
            showMenu(true)
        }
    }

    // ---- Pause menu construction ------------------------------------------

    private fun buildPauseMenu(): View {
        val scrim = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(200, 4, 5, 9))
            visibility = View.GONE
            isClickable = true                      // swallow touches to the game
        }

        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(Color.argb(235, 14, 16, 24))
            setPadding(40, 36, 40, 36)
        }
        val title = TextView(this).apply {
            text = "PRISMATIC"
            setTextColor(Color.parseColor("#B79CFF"))
            textSize = 22f
            typeface = Typeface.DEFAULT_BOLD
            setPadding(6, 0, 0, 22)
        }
        panel.addView(title)

        menuColumn = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        panel.addView(menuColumn)

        menuButton("Resume") { showMenu(false) }
        btn25D = menuButton("") { enable25D = !enable25D; applyPresentation(); refreshMenu() }
        btnShader = menuButton("") { enableShader = !enableShader; applyPresentation(); refreshMenu() }
        btnStyle = menuButton("") {
            shaderStyle = (shaderStyle + 1) % styleNames.size; applyPresentation(); refreshMenu()
        }
        btnTime = menuButton("") {
            timeIdx = (timeIdx + 1) % timeCycle.size
            shared.timeOfDay = timeCycle[timeIdx]
            applyState(); savePrefs(); refreshMenu()
        }
        btnLantern = menuButton("") {
            lanternOn = !lanternOn; NativeBridge.nativeSetLantern(lanternOn); savePrefs(); refreshMenu()
        }
        btnSpeed = menuButton("") { jitOn = !jitOn; NativeBridge.nativeSetJit(jitOn); savePrefs(); refreshMenu() }
        btnAutoLoad = menuButton("") { autoLoad = !autoLoad; savePrefs(); refreshMenu() }
        menuButton("Load ROM…") { pickRom() }
        menuButton("Reset game") { resetGame() }

        val hint = TextView(this).apply {
            text = "Saves: Android/data/com.prismatic.app/files/saves"
            setTextColor(Color.parseColor("#7C859B"))
            textSize = 11f
            setPadding(6, 18, 0, 0)
        }
        panel.addView(hint)

        val scroll = ScrollView(this).apply {
            addView(panel)
            layoutParams = FrameLayout.LayoutParams(
                (resources.displayMetrics.widthPixels * 0.62f).toInt().coerceAtLeast(560),
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { gravity = Gravity.CENTER }
        }
        scrim.addView(scroll)
        refreshMenu()
        return scrim
    }

    private fun menuButton(label: String, onClick: () -> Unit): Button {
        val b = Button(this).apply {
            text = label
            isAllCaps = false
            textSize = 16f
            setTextColor(Color.WHITE)
            setBackgroundColor(Color.argb(255, 30, 34, 48))
            val lp = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = 8 }
            layoutParams = lp
            setPadding(28, 22, 28, 22)
            setOnClickListener { onClick() }
        }
        menuColumn.addView(b)
        return b
    }

    private fun refreshMenu() {
        fun onOff(v: Boolean) = if (v) "On" else "Off"
        btn25D.text = "2.5D depth: ${onOff(enable25D)}"
        btnShader.text = "Shader overlay: ${onOff(enableShader)}"
        btnStyle.text = "Shader style: ${styleNames[shaderStyle]}"
        btnTime.text = "Time of day: %02d:00".format(timeCycle[timeIdx].toInt())
        btnLantern.text = "Lantern (night): ${onOff(lanternOn)}"
        btnSpeed.text = "Speed: ${if (jitOn) "Fast (JIT)" else "Compatible"}"
        btnAutoLoad.text = "Auto-load last game: ${onOff(autoLoad)}"
    }

    private fun showMenu(show: Boolean) {
        menuVisible = show
        pauseMenu.visibility = if (show) View.VISIBLE else View.GONE
        topView.paused = show
        if (show) {
            refreshMenu()
            menuColumn.getChildAt(0)?.requestFocus()
        } else {
            enterImmersive()
        }
    }

    // ---- Native state application -----------------------------------------

    private fun applyPresentation() {
        NativeBridge.nativeSetPresentation(enable25D, enableShader, shaderStyle)
        savePrefs()
    }

    private fun applyState() {
        topView.timeOfDay = shared.timeOfDay
        bottomInline.timeOfDay = shared.timeOfDay
        presentation?.sync()
    }

    // ---- Bottom-screen touch ----------------------------------------------

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

            val ok = NativeBridge.nativeLoadRom(dest.absolutePath, saveDir())
            if (ok) {
                lastRomPath = dest.absolutePath
                savePrefs()
                val title = NativeBridge.nativeGameTitle()
                toast("Loaded ${if (title.isNotBlank()) title else dest.name}")
                showMenu(false)
            } else {
                toast("Not a valid NDS ROM")
            }
        } catch (e: Exception) {
            toast("ROM load error: ${e.message}")
        }
    }

    private fun resetGame() {
        if (lastRomPath.isBlank() || !File(lastRomPath).exists()) {
            toast("No game loaded"); return
        }
        // Reloading re-boots the core (applying the current speed mode) and
        // auto-loads the on-device battery save.
        if (NativeBridge.nativeLoadRom(lastRomPath, saveDir())) {
            showMenu(false)
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

    /** Visible on-device data dir; the core writes battery saves to its saves/ subfolder. */
    private fun saveDir(): String = (getExternalFilesDir(null) ?: filesDir).absolutePath

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()

    // ---- Preferences ------------------------------------------------------

    private fun loadPrefs() {
        val p = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        enable25D = p.getBoolean("enable25D", false)
        enableShader = p.getBoolean("enableShader", false)
        shaderStyle = p.getInt("shaderStyle", 0).coerceIn(0, styleNames.size - 1)
        jitOn = p.getBoolean("jitOn", true)
        lanternOn = p.getBoolean("lanternOn", false)
        timeIdx = p.getInt("timeIdx", 1).coerceIn(0, timeCycle.size - 1)
        autoLoad = p.getBoolean("autoLoad", true)
        lastRomPath = p.getString("lastRom", "") ?: ""
        shared.timeOfDay = timeCycle[timeIdx]
    }

    private fun savePrefs() {
        getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putBoolean("enable25D", enable25D)
            .putBoolean("enableShader", enableShader)
            .putInt("shaderStyle", shaderStyle)
            .putBoolean("jitOn", jitOn)
            .putBoolean("lanternOn", lanternOn)
            .putInt("timeIdx", timeIdx)
            .putBoolean("autoLoad", autoLoad)
            .putString("lastRom", lastRomPath)
            .apply()
    }

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

    private fun isMenuToggleKey(keyCode: Int) =
        keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_BUTTON_MODE ||
            keyCode == KeyEvent.KEYCODE_MENU

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (isMenuToggleKey(keyCode) && event?.repeatCount == 0) {
            showMenu(!menuVisible)
            return true
        }
        if (menuVisible) {
            // Let the pad drive menu focus; A / Start activate the selection.
            return when (keyCode) {
                KeyEvent.KEYCODE_BUTTON_A, KeyEvent.KEYCODE_BUTTON_START,
                KeyEvent.KEYCODE_DPAD_CENTER, KeyEvent.KEYCODE_ENTER -> {
                    currentFocus?.performClick(); true
                }
                else -> super.onKeyDown(keyCode, event)   // DPAD moves focus
            }
        }
        val bit = keyToBit(keyCode)
        if (bit != 0 && event?.repeatCount == 0) {
            padMask = padMask or bit
            topView.inputMask = padMask
        }
        return if (bit != 0) true else super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (isMenuToggleKey(keyCode)) return true
        if (menuVisible) return super.onKeyUp(keyCode, event)
        val bit = keyToBit(keyCode)
        if (bit != 0) {
            padMask = padMask and bit.inv()
            topView.inputMask = padMask
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (menuVisible) return super.onGenericMotionEvent(event)
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

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus && !menuVisible) enterImmersive()
    }

    @Suppress("DEPRECATION")
    private fun enterImmersive() {
        window.decorView.systemUiVisibility = (
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY)
    }

    override fun onResume() {
        super.onResume()
        enterImmersive()
        val dm = getSystemService(DISPLAY_SERVICE) as DisplayManager
        val secondary = dm.displays.firstOrNull { it.displayId != Display.DEFAULT_DISPLAY }
        if (secondary != null) {
            bottomInline.visibility = View.GONE
            presentation = SecondaryPresentation(this, secondary, shared).also { it.show() }
        } else {
            bottomInline.visibility = View.VISIBLE
        }
        applyState()
        audio.start()
    }

    override fun onPause() {
        audio.stop()
        presentation?.dismiss()
        presentation = null
        super.onPause()
    }

    companion object {
        private const val ROM_PICK_REQUEST = 0x2001
        private const val PREFS = "prismatic"
    }
}
