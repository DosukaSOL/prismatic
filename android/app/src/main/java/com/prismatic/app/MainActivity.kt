// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Activity
import android.app.AlertDialog
import android.content.Context
import android.content.Intent
import android.graphics.Color
import android.graphics.Typeface
import android.hardware.display.DisplayManager
import android.net.Uri
import android.os.Bundle
import android.provider.OpenableColumns
import android.text.InputType
import android.view.Display
import android.view.Gravity
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.Button
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import java.io.File
import kotlin.math.abs

/**
 * PRISMATIC host activity — a full-screen, branded DS emulator front-end.
 *
 * Flow: the app opens on a polished **home screen** (logo, brand colours,
 * animated cards). From there you Open a game, browse the Compatible Games
 * list, or open **Shader Studio** to craft a look. While a game runs, Back (or
 * the pad's Mode button) opens a pause menu that can Resume, tweak the look, or
 * **Save & Close / Close** the game — returning to the home screen. From the
 * home screen, Back exits Prismatic entirely.
 *
 * Presentation is two fully independent layers: an optional geometric 2.5D tilt
 * and an optional shader overlay (13 live-tweakable parameters). Either, both,
 * or neither. Looks can be saved and re-applied after a reboot.
 *
 * Saves use the game's own battery save (SRAM), written to a visible on-device
 * folder (Android/data/com.prismatic.app/files/saves) and auto-loaded on boot.
 */
class MainActivity : Activity() {

    private val shared = FrameState()
    private lateinit var topView: PrismaticSurfaceView
    private lateinit var bottomInline: PrismaticSurfaceView
    private lateinit var gameContainer: LinearLayout
    private lateinit var root: FrameLayout

    private var presentation: SecondaryPresentation? = null
    private var padMask = 0
    private val audio = AudioPlayer()
    private lateinit var registry: GameRegistry

    // ---- Session / look state (persisted) ---------------------------------
    private val look = ShaderLook()
    private var jitOn = true
    private var autoLoad = true
    private var lastRomPath = ""
    private var activeLookName = ""     // saved look currently applied ("" = per-game/default)
    private var gameLoaded = false

    // ---- Overlays ---------------------------------------------------------
    private lateinit var homeScreen: View
    private lateinit var pauseMenu: View
    private lateinit var menuColumn: LinearLayout
    private var studioOverlay: View? = null
    private var gamesOverlay: View? = null
    private var homeVisible = false
    private var menuVisible = false
    private var homeFirstButton: View? = null

    // Pause-menu buttons that reflect state.
    private lateinit var btn25D: Button
    private lateinit var btnShader: Button
    private lateinit var btnLantern: Button
    private lateinit var btnSpeed: Button
    private lateinit var btnAutoLoad: Button
    private var btnHomeSpeed: Button? = null

    // Live studio widgets (rebuilt each open).
    private var studioBars: Array<SeekBar>? = null
    private var studioVals: Array<TextView>? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        enterImmersive()

        if (!NativeBridge.nativeInit()) {
            val err = TextView(this).apply {
                text = "Native pipeline failed to initialise."
                setTextColor(Color.RED); gravity = Gravity.CENTER
            }
            setContentView(err); return
        }

        registry = GameRegistry.load(this)
        loadPrefs()

        root = FrameLayout(this).apply { setBackgroundColor(Color.BLACK) }

        gameContainer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        topView = PrismaticSurfaceView(this).apply { bottom = false }
        bottomInline = PrismaticSurfaceView(this).apply {
            bottom = true
            setZOrderMediaOverlay(true)
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
        homeScreen = buildHomeScreen()
        root.addView(homeScreen)
        setContentView(root)

        // Push persisted look to the core, then restore the last game.
        applyLook(persist = false)
        NativeBridge.nativeSetJit(jitOn)

        if (autoLoad && lastRomPath.isNotBlank() && File(lastRomPath).exists()) {
            if (loadGame(lastRomPath)) {
                val title = NativeBridge.nativeGameTitle()
                toast("Resumed ${if (title.isNotBlank()) title else File(lastRomPath).name}")
            } else {
                showHome(true)
            }
        } else {
            showHome(true)
        }
    }

    // ======================================================================
    //  Home screen (main menu)
    // ======================================================================

    private fun buildHomeScreen(): View {
        val screen = FrameLayout(this).apply {
            background = Brand.screenBackground()
            isClickable = true
            visibility = View.GONE
        }

        // Subtle spectrum accent bar along the top.
        val accent = View(this).apply {
            background = android.graphics.drawable.GradientDrawable(
                android.graphics.drawable.GradientDrawable.Orientation.LEFT_RIGHT,
                intArrayOf(Brand.PRIMARY, Brand.CYAN, Brand.PINK, Brand.PRIMARY_LT)
            )
        }
        screen.addView(
            accent,
            FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, Brand.dp(this, 4f))
        )

        val col = Brand.column(this).apply {
            gravity = Gravity.CENTER
            setPadding(Brand.dp(context, 40f), Brand.dp(context, 28f),
                Brand.dp(context, 40f), Brand.dp(context, 28f))
        }

        val logo = ImageView(this).apply {
            setImageResource(R.drawable.prismatic_logo)
            adjustViewBounds = true
        }
        col.addView(
            logo,
            LinearLayout.LayoutParams(
                (resources.displayMetrics.widthPixels * 0.52f).toInt(),
                ViewGroup.LayoutParams.WRAP_CONTENT
            )
        )

        val tagline = TextView(this).apply {
            text = "Nintendo DS · reimagined in HD-2D"
            setTextColor(Brand.TEXT_MUTED)
            textSize = 14f
            letterSpacing = 0.06f
            gravity = Gravity.CENTER
            setPadding(0, Brand.dp(context, 6f), 0, Brand.dp(context, 26f))
        }
        col.addView(tagline)

        val buttons = Brand.column(this).apply {
            layoutParams = LinearLayout.LayoutParams(
                Brand.dp(context, 340f), ViewGroup.LayoutParams.WRAP_CONTENT
            )
        }
        fun add(b: Button, topDp: Float = 12f) {
            buttons.addView(
                b, LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply { topMargin = Brand.dp(this@MainActivity, topDp) }
            )
        }
        add(Brand.primaryButton(this, "Open Game") { pickRom() }.also { homeFirstButton = it }, 0f)
        add(Brand.ghostButton(this, "Compatible Games") { openGames() })
        add(Brand.ghostButton(this, "Shader Studio") { openStudio() })
        btnHomeSpeed = Brand.ghostButton(this, speedLabel()) {
            jitOn = !jitOn; NativeBridge.nativeSetJit(jitOn); savePrefs()
            btnHomeSpeed?.text = speedLabel()
            if (gameLoaded) toast("Applies on next load")
        }
        add(btnHomeSpeed!!)
        add(Brand.ghostButton(this, "Quit Prismatic") { finishAffinity() })
        col.addView(buttons)

        val footer = TextView(this).apply {
            text = "Bring your own game dumps · saves on device"
            setTextColor(Brand.STROKE)
            textSize = 11f
            gravity = Gravity.CENTER
            setPadding(0, Brand.dp(context, 22f), 0, 0)
        }
        col.addView(footer)

        screen.addView(
            col,
            FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { gravity = Gravity.CENTER }
        )
        screen.setTag(R.id.tag_home_logo, logo)
        screen.setTag(R.id.tag_home_col, col)
        return screen
    }

    private fun showHome(show: Boolean) {
        homeVisible = show
        homeScreen.visibility = if (show) View.VISIBLE else View.GONE
        if (show) {
            btnHomeSpeed?.text = speedLabel()
            (homeScreen.getTag(R.id.tag_home_logo) as? View)?.let { Brand.popIn(it, 40) }
            (homeScreen.getTag(R.id.tag_home_col) as? ViewGroup)?.let { c ->
                Brand.enterFrom(c, 120)
            }
            (homeFirstButton ?: homeScreen).requestFocus()
        }
        updatePaused()
        if (!show) enterImmersive()
    }

    // ======================================================================
    //  Pause menu (in-game)
    // ======================================================================

    private fun buildPauseMenu(): View {
        val scrim = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(205, 4, 5, 9))
            visibility = View.GONE
            isClickable = true
        }

        val panel = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = Brand.card(20f, ctx = this@MainActivity)
            setPadding(Brand.dp(context, 26f), Brand.dp(context, 22f),
                Brand.dp(context, 26f), Brand.dp(context, 22f))
        }

        val header = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(Brand.dp(context, 4f), 0, 0, Brand.dp(context, 14f))
        }
        header.addView(ImageView(this).apply {
            setImageResource(R.drawable.prismatic_icon)
        }, LinearLayout.LayoutParams(Brand.dp(this, 34f), Brand.dp(this, 34f)))
        header.addView(TextView(this).apply {
            text = "  PRISMATIC"
            setTextColor(Brand.PRIMARY_LT)
            textSize = 20f
            letterSpacing = 0.12f
            typeface = Typeface.DEFAULT_BOLD
        })
        panel.addView(header)

        menuColumn = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        panel.addView(menuColumn)

        menuButton("Resume") { showMenu(false) }
        section("Presentation")
        btn25D = menuButton("") { look.enable25D = !look.enable25D; applyLook(); refreshMenu() }
        btnShader = menuButton("") { look.enableShader = !look.enableShader; applyLook(); refreshMenu() }
        menuButton("Shader Studio…") { showMenu(false); openStudio() }
        btnLantern = menuButton("") {
            look.lantern = !look.lantern; applyLook(); refreshMenu()
        }
        section("Session")
        btnSpeed = menuButton("") {
            jitOn = !jitOn; NativeBridge.nativeSetJit(jitOn); savePrefs(); refreshMenu()
            toast("Speed applies on next load")
        }
        btnAutoLoad = menuButton("") { autoLoad = !autoLoad; savePrefs(); refreshMenu() }
        menuButton("Reset game") { resetGame() }
        menuButton("Open a different game…") { pickRom() }
        section("Close")
        menuButton("Save & Close") { closeGame(save = true) }
        menuButton("Close (no save)") { closeGame(save = false) }

        val hint = TextView(this).apply {
            text = "Saves → Android/data/com.prismatic.app/files/saves"
            setTextColor(Brand.TEXT_MUTED)
            textSize = 11f
            setPadding(Brand.dp(context, 4f), Brand.dp(context, 16f), 0, 0)
        }
        panel.addView(hint)

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            addView(panel)
            layoutParams = FrameLayout.LayoutParams(
                Brand.dp(this@MainActivity, 380f),
                ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { gravity = Gravity.CENTER }
        }
        scrim.addView(scroll)
        refreshMenu()
        return scrim
    }

    private fun section(label: String) {
        menuColumn.addView(Brand.sectionLabel(this, label))
    }

    private fun menuButton(label: String, onClick: () -> Unit): Button {
        val b = Brand.ghostButton(this, label, onClick).apply {
            textSize = 15f
            gravity = Gravity.CENTER_VERTICAL or Gravity.START
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(this@MainActivity, 7f) }
        }
        menuColumn.addView(b)
        return b
    }

    private fun refreshMenu() {
        fun onOff(v: Boolean) = if (v) "On" else "Off"
        btn25D.text = "2.5D depth: ${onOff(look.enable25D)}"
        btnShader.text = "Shader overlay: ${onOff(look.enableShader)}"
        btnLantern.text = "Lantern glow: ${onOff(look.lantern)}"
        btnSpeed.text = speedLabel()
        btnAutoLoad.text = "Auto-load last game: ${onOff(autoLoad)}"
    }

    private fun showMenu(show: Boolean) {
        menuVisible = show
        pauseMenu.visibility = if (show) View.VISIBLE else View.GONE
        if (show) {
            refreshMenu()
            (pauseMenu as? ViewGroup)?.getChildAt(0)?.let { Brand.enterFrom(it, 0, 18f) }
            menuColumn.getChildAt(0)?.requestFocus()
        } else {
            enterImmersive()
        }
        updatePaused()
    }

    private fun speedLabel() = "Speed: ${if (jitOn) "Fast (JIT)" else "Compatible"}"

    // ======================================================================
    //  Shader Studio (live editor drawer)
    // ======================================================================

    private fun openStudio() {
        closeGames()
        if (studioOverlay != null) return
        val overlay = buildStudio()
        studioOverlay = overlay
        root.addView(overlay)
        Brand.enterFrom(overlay.findViewWithTag<View>("studio_panel") ?: overlay, 0, 24f)
        updatePaused()
    }

    private fun closeStudio() {
        studioOverlay?.let { root.removeView(it) }
        studioOverlay = null
        studioBars = null
        studioVals = null
        updatePaused()
        if (!gameLoaded) showHome(true) else enterImmersive()
    }

    private fun buildStudio(): View {
        val scrim = FrameLayout(this).apply {
            setBackgroundColor(Color.argb(120, 4, 5, 9))
            isClickable = true
        }
        val panel = LinearLayout(this).apply {
            tag = "studio_panel"
            orientation = LinearLayout.VERTICAL
            background = Brand.card(0f, top = 0xF20E1018.toInt(), bottom = 0xF2161826.toInt(),
                strokeDp = 0f, ctx = this@MainActivity)
            setPadding(Brand.dp(context, 22f), Brand.dp(context, 18f),
                Brand.dp(context, 22f), Brand.dp(context, 18f))
        }

        // Header row: title + preview hint + close.
        val head = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
        }
        head.addView(TextView(this).apply {
            text = "Shader Studio"
            setTextColor(Brand.TEXT)
            textSize = 20f
            typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        head.addView(Brand.ghostButton(this, "Done") { closeStudio() })
        panel.addView(head)
        panel.addView(TextView(this).apply {
            text = if (gameLoaded) "Live preview on the left · drag to taste"
                   else "Load a game to preview · you can still design & save"
            setTextColor(Brand.TEXT_MUTED)
            textSize = 12f
            setPadding(0, Brand.dp(context, 4f), 0, Brand.dp(context, 10f))
        })

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f
            )
        }
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        scroll.addView(body)

        // Layer toggles.
        body.addView(Brand.sectionLabel(this, "Layers"))
        body.addView(toggleRow("2.5D depth tilt", look.enable25D) {
            look.enable25D = it; applyLook()
        })
        body.addView(toggleRow("Shader overlay", look.enableShader) {
            look.enableShader = it; applyLook()
        })
        body.addView(toggleRow("Lantern glow", look.lantern) {
            look.lantern = it; applyLook()
        })
        body.addView(sliderRow("Depth tilt", look.tilt, 0f, 1f) {
            look.tilt = it; applyLook()
        })

        // Presets.
        body.addView(Brand.sectionLabel(this, "Presets"))
        val presetRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val presetScroll = android.widget.HorizontalScrollView(this).apply {
            isHorizontalScrollBarEnabled = false
            addView(presetRow)
        }
        for (i in 0 until NativeBridge.nativeShaderPresetCount()) {
            val name = NativeBridge.nativeShaderPresetName(i)
            presetRow.addView(Brand.ghostButton(this, name) {
                look.params = NativeBridge.nativeShaderPreset(i)
                look.enableShader = true
                activeLookName = ""
                applyLook(); refreshStudio()
                toast("Preset: $name")
            }.apply {
                textSize = 13f
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply { rightMargin = Brand.dp(this@MainActivity, 8f) }
            })
        }
        body.addView(presetScroll)

        // Parameter sliders.
        body.addView(Brand.sectionLabel(this, "Parameters"))
        val n = NativeBridge.SHADER_PARAM_COUNT
        val bars = arrayOfNulls<SeekBar>(n)
        val vals = arrayOfNulls<TextView>(n)
        for (i in 0 until n) {
            val row = paramRow(i)
            bars[i] = row.first
            vals[i] = row.second
            body.addView(row.third)
        }
        @Suppress("UNCHECKED_CAST")
        studioBars = bars as Array<SeekBar>
        @Suppress("UNCHECKED_CAST")
        studioVals = vals as Array<TextView>

        // Save / load / reset.
        body.addView(Brand.sectionLabel(this, "My looks"))
        val actions = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        fun act(b: Button) = actions.addView(b, LinearLayout.LayoutParams(
            0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f
        ).apply { rightMargin = Brand.dp(this@MainActivity, 8f) })
        act(Brand.primaryButton(this, "Save") { promptSaveLook() })
        act(Brand.ghostButton(this, "Load") { promptLoadLook() })
        act(Brand.ghostButton(this, "Reset") {
            look.params = NativeBridge.nativeShaderPreset(0)
            look.tilt = 0.5f; activeLookName = ""
            applyLook(); refreshStudio()
        })
        body.addView(actions)
        body.addView(Brand.spacer(this, 8f))

        panel.addView(scroll)

        // Right-side drawer (leaves the game visible on the left for preview).
        scrim.addView(
            panel,
            FrameLayout.LayoutParams(
                (resources.displayMetrics.widthPixels * 0.58f).toInt(),
                ViewGroup.LayoutParams.MATCH_PARENT
            ).apply { gravity = Gravity.END }
        )
        return scrim
    }

    private fun toggleRow(label: String, initial: Boolean, onChange: (Boolean) -> Unit): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, Brand.dp(context, 6f), 0, Brand.dp(context, 6f))
        }
        row.addView(TextView(this).apply {
            text = label; setTextColor(Brand.TEXT); textSize = 15f
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        val sw = android.widget.Switch(this).apply { isChecked = initial }
        sw.setOnCheckedChangeListener { _, v -> onChange(v) }
        row.addView(sw)
        return row
    }

    /** Generic 0..1 slider row (used for depth tilt). */
    private fun sliderRow(label: String, initial: Float, min: Float, max: Float,
                          onChange: (Float) -> Unit): View {
        val wrap = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, Brand.dp(context, 6f), 0, Brand.dp(context, 2f))
        }
        val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        val lbl = TextView(this).apply {
            text = label; setTextColor(Brand.TEXT); textSize = 14f
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        }
        val valTv = TextView(this).apply {
            text = fmt(initial); setTextColor(Brand.PRIMARY_LT); textSize = 13f
        }
        top.addView(lbl); top.addView(valTv)
        val bar = SeekBar(this).apply {
            this.max = 1000
            progress = (((initial - min) / (max - min)) * 1000f).toInt().coerceIn(0, 1000)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    val v = min + (p / 1000f) * (max - min)
                    valTv.text = fmt(v); onChange(v)
                }
                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {}
            })
        }
        wrap.addView(top); wrap.addView(bar)
        return wrap
    }

    /** Shader-parameter slider row; returns (bar, valueLabel, view) for live refresh. */
    private fun paramRow(index: Int): Triple<SeekBar, TextView, View> {
        val min = P_MIN[index]; val max = P_MAX[index]
        val wrap = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, Brand.dp(context, 5f), 0, Brand.dp(context, 1f))
        }
        val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
        top.addView(TextView(this).apply {
            text = P_LABEL[index]; setTextColor(Brand.TEXT); textSize = 14f
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        val valTv = TextView(this).apply {
            text = fmt(look.params[index]); setTextColor(Brand.PRIMARY_LT); textSize = 13f
        }
        top.addView(valTv)
        val bar = SeekBar(this).apply {
            this.max = 1000
            progress = paramToProgress(index, look.params[index])
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    val v = min + (p / 1000f) * (max - min)
                    look.params[index] = v
                    valTv.text = fmt(v)
                    if (fromUser) { activeLookName = ""; applyLook() }
                }
                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {}
            })
        }
        wrap.addView(top); wrap.addView(bar)
        return Triple(bar, valTv, wrap)
    }

    private fun paramToProgress(index: Int, value: Float): Int {
        val min = P_MIN[index]; val max = P_MAX[index]
        return (((value - min) / (max - min)) * 1000f).toInt().coerceIn(0, 1000)
    }

    /** Push current look values back into the studio widgets (after preset/load/reset). */
    private fun refreshStudio() {
        val bars = studioBars ?: return
        val vals = studioVals ?: return
        for (i in bars.indices) {
            bars[i].progress = paramToProgress(i, look.params[i])
            vals[i].text = fmt(look.params[i])
        }
    }

    private fun promptSaveLook() {
        val input = EditText(this).apply {
            inputType = InputType.TYPE_CLASS_TEXT
            hint = "Look name"
            setText(activeLookName)
        }
        AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Save look")
            .setView(input)
            .setPositiveButton("Save") { _, _ ->
                val name = input.text.toString().trim()
                if (name.isBlank()) { toast("Name required"); return@setPositiveButton }
                if (ShaderStore.save(this, name, look.snapshot())) {
                    activeLookName = name; savePrefs(); toast("Saved “$name”")
                } else toast("Could not save")
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun promptLoadLook() {
        val names = ShaderStore.list(this)
        if (names.isEmpty()) { toast("No saved looks yet"); return }
        val arr = names.toTypedArray()
        AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Load look")
            .setItems(arr) { _, which ->
                val n = arr[which]
                ShaderStore.load(this, n)?.let {
                    look.copyFrom(it); activeLookName = n
                    applyLook(); refreshStudio(); toast("Loaded “$n”")
                } ?: toast("Could not load")
            }
            .setNeutralButton("Delete…") { _, _ -> promptDeleteLook(arr) }
            .setNegativeButton("Cancel", null)
            .show()
    }

    private fun promptDeleteLook(names: Array<String>) {
        AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Delete look")
            .setItems(names) { _, which ->
                if (ShaderStore.delete(this, names[which])) {
                    if (activeLookName == names[which]) { activeLookName = ""; savePrefs() }
                    toast("Deleted")
                }
            }
            .setNegativeButton("Cancel", null)
            .show()
    }

    // ======================================================================
    //  Compatible games overlay
    // ======================================================================

    private fun openGames() {
        closeStudio()
        if (gamesOverlay != null) return
        val overlay = buildGamesOverlay()
        gamesOverlay = overlay
        root.addView(overlay)
        Brand.enterFrom(overlay.findViewWithTag<View>("games_panel") ?: overlay, 0, 22f)
        updatePaused()
    }

    private fun closeGames() {
        gamesOverlay?.let { root.removeView(it) }
        gamesOverlay = null
        updatePaused()
        if (!gameLoaded && studioOverlay == null) showHome(true) else enterImmersive()
    }

    private fun buildGamesOverlay(): View {
        val scrim = FrameLayout(this).apply {
            background = Brand.screenBackground()
            isClickable = true
        }
        val panel = LinearLayout(this).apply {
            tag = "games_panel"
            orientation = LinearLayout.VERTICAL
            setPadding(Brand.dp(context, 34f), Brand.dp(context, 26f),
                Brand.dp(context, 34f), Brand.dp(context, 26f))
        }
        val head = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        head.addView(TextView(this).apply {
            text = "Compatible Games"
            setTextColor(Brand.TEXT); textSize = 22f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        head.addView(Brand.ghostButton(this, "Back") { closeGames() })
        panel.addView(head)
        panel.addView(TextView(this).apply {
            text = "Verified DS titles for Prismatic's HD-2D pass. Bring your own dump."
            setTextColor(Brand.TEXT_MUTED); textSize = 13f
            setPadding(0, Brand.dp(context, 4f), 0, Brand.dp(context, 12f))
        })

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        }
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        if (registry.games.isEmpty()) {
            list.addView(TextView(this).apply {
                text = "Registry unavailable."; setTextColor(Brand.TEXT_MUTED)
            })
        } else {
            for (g in registry.games) list.addView(gameCard(g))
        }
        scroll.addView(list)
        panel.addView(scroll)

        scrim.addView(
            panel,
            FrameLayout.LayoutParams(
                (resources.displayMetrics.widthPixels * 0.82f).toInt().coerceAtMost(Brand.dp(this, 720f)),
                ViewGroup.LayoutParams.MATCH_PARENT
            ).apply { gravity = Gravity.CENTER }
        )
        return scrim
    }

    private fun gameCard(g: GameEntry): View {
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = Brand.card(14f, ctx = this@MainActivity)
            setPadding(Brand.dp(context, 18f), Brand.dp(context, 14f),
                Brand.dp(context, 18f), Brand.dp(context, 14f))
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(this@MainActivity, 10f) }
        }
        val titleRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        titleRow.addView(TextView(this).apply {
            text = "${g.title}  ·  ${g.region}"
            setTextColor(Brand.TEXT); textSize = 16f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        titleRow.addView(TextView(this).apply {
            text = "  ${g.status}  "
            setTextColor(Brand.BG_TOP); textSize = 12f; typeface = Typeface.DEFAULT_BOLD
            background = Brand.pill(this@MainActivity, Brand.CYAN, 8f)
            setPadding(Brand.dp(context, 10f), Brand.dp(context, 3f),
                Brand.dp(context, 10f), Brand.dp(context, 3f))
        })
        card.addView(titleRow)
        card.addView(TextView(this).apply {
            text = "Code ${g.code} · Preset ${g.preset} · ${g.notes}"
            setTextColor(Brand.TEXT_MUTED); textSize = 12f
            setPadding(0, Brand.dp(context, 6f), 0, 0)
        })
        return card
    }

    // ======================================================================
    //  Look application + native state
    // ======================================================================

    private fun applyLook(persist: Boolean = true) {
        NativeBridge.nativeSetPresentation(
            look.enable25D, look.enableShader, look.tilt, look.lantern, look.params
        )
        if (persist) savePrefs()
    }

    private fun applyState() {
        topView.timeOfDay = shared.timeOfDay
        bottomInline.timeOfDay = shared.timeOfDay
        presentation?.sync()
    }

    private fun updatePaused() {
        val paused = homeVisible || menuVisible || (gamesOverlay != null)
        topView.paused = paused
        bottomInline.paused = paused
    }

    private fun presetIndexByName(name: String): Int {
        for (i in 0 until NativeBridge.nativeShaderPresetCount())
            if (NativeBridge.nativeShaderPresetName(i).equals(name, ignoreCase = true)) return i
        return -1
    }

    /** Apply the per-game recommended look, unless the user has a saved look active. */
    private fun applyPerGameLook() {
        if (activeLookName.isNotBlank()) { applyLook(); return }
        val entry = registry.forCode(NativeBridge.nativeGameCode()) ?: run { applyLook(); return }
        val idx = presetIndexByName(entry.preset)
        if (idx >= 0) look.params = NativeBridge.nativeShaderPreset(idx)
        look.enable25D = entry.enable25D
        look.enableShader = entry.enableShader
        applyLook()
        toast("${entry.preset} profile · ${entry.title}")
    }

    // ======================================================================
    //  Game lifecycle
    // ======================================================================

    private fun loadGame(path: String): Boolean {
        if (!NativeBridge.nativeLoadRom(path, saveDir())) return false
        lastRomPath = path
        gameLoaded = true
        savePrefs()
        applyPerGameLook()
        showHome(false); showMenu(false)
        closeStudio(); closeGames()
        return true
    }

    private fun closeGame(save: Boolean) {
        if (save) { NativeBridge.nativeFlushSave(); toast("Saved & closed") }
        else toast("Closed")
        NativeBridge.nativeUnloadRom()
        gameLoaded = false
        showMenu(false)
        showHome(true)
    }

    private fun resetGame() {
        if (lastRomPath.isBlank() || !File(lastRomPath).exists()) { toast("No game loaded"); return }
        if (loadGame(lastRomPath)) { showMenu(false); toast("Reset") }
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

            if (loadGame(dest.absolutePath)) {
                val title = NativeBridge.nativeGameTitle()
                toast("Loaded ${if (title.isNotBlank()) title else dest.name}")
            } else {
                toast("Not a valid NDS ROM")
            }
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

    private fun saveDir(): String = (getExternalFilesDir(null) ?: filesDir).absolutePath

    private fun toast(msg: String) = Toast.makeText(this, msg, Toast.LENGTH_SHORT).show()

    private fun fmt(v: Float): String = String.format("%.2f", v)

    // ---- Preferences ------------------------------------------------------

    private fun loadPrefs() {
        val p = getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        look.enable25D = p.getBoolean("enable25D", false)
        look.enableShader = p.getBoolean("enableShader", false)
        look.tilt = p.getFloat("tilt", 0.5f)
        look.lantern = p.getBoolean("lantern", false)
        val n = NativeBridge.SHADER_PARAM_COUNT
        val arr = FloatArray(n)
        val csv = p.getString("shaderParams", "") ?: ""
        val parts = csv.split(',')
        if (parts.size == n) {
            for (i in 0 until n) arr[i] = parts[i].toFloatOrNull() ?: 0f
        } else {
            // First run: seed with the flagship HD-2D preset (index 0).
            val preset = NativeBridge.nativeShaderPreset(0)
            for (i in 0 until n) arr[i] = if (i < preset.size) preset[i] else 0f
        }
        look.params = arr
        jitOn = p.getBoolean("jitOn", true)
        autoLoad = p.getBoolean("autoLoad", true)
        lastRomPath = p.getString("lastRom", "") ?: ""
        activeLookName = p.getString("activeLook", "") ?: ""
        shared.timeOfDay = 12f
    }

    private fun savePrefs() {
        getSharedPreferences(PREFS, Context.MODE_PRIVATE).edit()
            .putBoolean("enable25D", look.enable25D)
            .putBoolean("enableShader", look.enableShader)
            .putFloat("tilt", look.tilt)
            .putBoolean("lantern", look.lantern)
            .putString("shaderParams", look.params.joinToString(","))
            .putBoolean("jitOn", jitOn)
            .putBoolean("autoLoad", autoLoad)
            .putString("lastRom", lastRomPath)
            .putString("activeLook", activeLookName)
            .apply()
    }

    // ---- Physical gamepad input (AYN Thor controls) -----------------------

    private fun keyToBit(keyCode: Int): Int = when (keyCode) {
        KeyEvent.KEYCODE_DPAD_UP -> NativeBridge.BTN_UP
        KeyEvent.KEYCODE_DPAD_DOWN -> NativeBridge.BTN_DOWN
        KeyEvent.KEYCODE_DPAD_LEFT -> NativeBridge.BTN_LEFT
        KeyEvent.KEYCODE_DPAD_RIGHT -> NativeBridge.BTN_RIGHT
        // Direct label mapping: the pad's A/B/X/Y drive the DS's A/B/X/Y so the
        // face buttons match exactly what the game shows.
        KeyEvent.KEYCODE_BUTTON_A -> NativeBridge.BTN_A
        KeyEvent.KEYCODE_BUTTON_B -> NativeBridge.BTN_B
        KeyEvent.KEYCODE_BUTTON_X -> NativeBridge.BTN_X
        KeyEvent.KEYCODE_BUTTON_Y -> NativeBridge.BTN_Y
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

    private fun anyOverlayOpen() =
        studioOverlay != null || gamesOverlay != null

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (isMenuToggleKey(keyCode) && event?.repeatCount == 0) {
            when {
                studioOverlay != null -> closeStudio()
                gamesOverlay != null -> closeGames()
                homeVisible -> finishAffinity()          // Back on home exits Prismatic
                gameLoaded -> showMenu(!menuVisible)
                else -> showHome(true)
            }
            return true
        }
        if (menuVisible || homeVisible || anyOverlayOpen()) {
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
        if (menuVisible || homeVisible || anyOverlayOpen()) return super.onKeyUp(keyCode, event)
        val bit = keyToBit(keyCode)
        if (bit != 0) {
            padMask = padMask and bit.inv()
            topView.inputMask = padMask
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        if (menuVisible || homeVisible || anyOverlayOpen()) return super.onGenericMotionEvent(event)
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
        if (hasFocus && !menuVisible && !homeVisible) enterImmersive()
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

        // Shader parameter UI metadata — order matches NativeBridge.SP_* indices.
        private val P_LABEL = arrayOf(
            "Brightness", "Exposure", "Contrast", "Saturation", "Temperature", "Tint",
            "Gamma", "Vignette", "Bloom", "Bloom Threshold", "Scanline", "LCD Grid", "Sharpen"
        )
        private val P_MIN = floatArrayOf(
            -0.5f, 0.5f, 0.5f, 0f, -1f, -1f, 0.6f, 0f, 0f, 0.3f, 0f, 0f, 0f
        )
        private val P_MAX = floatArrayOf(
            0.5f, 1.6f, 1.6f, 2f, 1f, 1f, 1.6f, 1f, 1f, 1f, 1f, 1f, 1f
        )
    }
}
