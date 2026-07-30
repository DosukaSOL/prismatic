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
import android.provider.DocumentsContract
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
    // Remappable controls: physical Android keyCode -> action id (see ACTIONS).
    private val keyMap = HashMap<Int, String>()
    private var rebindAction: String? = null
    private var rebindDialog: AlertDialog? = null
    private var speedIndex = 0                 // 0=1x, 1=2x, 2=5x
    private var lastSpeedCycle = 0L
    private var l2Down = false
    private var r2Down = false
    private val audio = AudioPlayer()
    private lateinit var registry: GameRegistry

    // ---- Session / look state (persisted) ---------------------------------
    private val look = ShaderLook()
    private var jitOn = true
    private var autoLoad = true
    private var lastRomPath = ""
    private var activeLookName = ""     // saved look currently applied ("" = per-game/default)
    private var gamesTreeUri = ""       // SAF tree URI of the user's games folder ("" = unset)
    private var gameLoaded = false

    // ---- Overlays ---------------------------------------------------------
    private lateinit var homeScreen: View
    private lateinit var pauseMenu: View
    private lateinit var menuColumn: LinearLayout
    private var studioOverlay: View? = null
    private var gamesOverlay: View? = null
    private var remapOverlay: View? = null
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
    private var presetRebind: ((String?) -> Unit)? = null
    private var remapList: LinearLayout? = null
    private var libraryList: LinearLayout? = null
    private var libraryFolderLabel: TextView? = null

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
        add(Brand.ghostButton(this, "Games") { openGames() })
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
        section("Controls")
        menuButton("Button mapping…") { openRemap() }
        menuButton("Games library…") { showMenu(false); openGames() }
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

    private fun speedLabel() = "Core: ${if (jitOn) "Fast (JIT)" else "Compatible"}"

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
            setBackgroundColor(Color.argb(32, 4, 5, 9))   // near-clear: see the game
            isClickable = true
        }
        val panel = LinearLayout(this).apply {
            tag = "studio_panel"
            orientation = LinearLayout.VERTICAL
            // Translucent so live shader/2.5D changes read through onto the game.
            background = Brand.card(0f, top = 0xC00B0B12.toInt(), bottom = 0xC0141626.toInt(),
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
        body.addView(toggleRow("2.5D depth (genuine)", look.enable25D) {
            look.enable25D = it; applyLook()
        })
        body.addView(toggleRow("Shader overlay", look.enableShader) {
            look.enableShader = it; applyLook()
        })
        body.addView(toggleRow("Anti-aliasing (FXAA)", look.antialias) {
            look.antialias = it; applyLook()
        })
        body.addView(toggleRow("Lantern glow", look.lantern) {
            look.lantern = it; applyLook()
        })
        body.addView(sliderRow("Depth strength", look.tilt, 0f, 1f) {
            look.tilt = it; applyLook()
        })

        // Presets — a dropdown of built-in looks + your saved looks.
        body.addView(Brand.sectionLabel(this, "Presets"))
        val spinner = android.widget.Spinner(this)
        var presetActions: List<() -> Unit> = emptyList()
        fun rebuildPresets(selectLabel: String? = null) {
            val (labels, actions) = presetChoices()
            presetActions = actions
            val adapter = android.widget.ArrayAdapter(
                this, android.R.layout.simple_spinner_dropdown_item, labels
            )
            spinner.adapter = adapter
            val sel = if (selectLabel != null) labels.indexOf(selectLabel) else -1
            if (sel >= 0) spinner.setSelection(sel, false)
        }
        rebuildPresets()
        spinner.onItemSelectedListener = object : android.widget.AdapterView.OnItemSelectedListener {
            override fun onItemSelected(p: android.widget.AdapterView<*>?, v: View?, pos: Int, id: Long) {
                if (pos in presetActions.indices) presetActions[pos]()
            }
            override fun onNothingSelected(p: android.widget.AdapterView<*>?) {}
        }
        body.addView(spinner)
        presetRebind = { name -> rebuildPresets(name) }

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
                (resources.displayMetrics.widthPixels * 0.50f).toInt(),
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

    /** Build the preset dropdown: a no-op header, the built-in looks, then your saved looks. */
    private fun presetChoices(): Pair<List<String>, List<() -> Unit>> {
        val labels = ArrayList<String>()
        val actions = ArrayList<() -> Unit>()
        labels.add("Choose a preset…"); actions.add {}
        for (i in 0 until NativeBridge.nativeShaderPresetCount()) {
            val name = NativeBridge.nativeShaderPresetName(i)
            labels.add(name)
            actions.add {
                look.params = NativeBridge.nativeShaderPreset(i)
                look.enableShader = true
                activeLookName = ""
                applyLook(); refreshStudio()
                toast("Preset: $name")
            }
        }
        for (name in ShaderStore.list(this)) {
            labels.add("★ $name")
            actions.add {
                ShaderStore.load(this, name)?.let {
                    look.copyFrom(it); activeLookName = name
                    applyLook(); refreshStudio()
                    toast("Loaded “$name”")
                } ?: toast("Could not load")
            }
        }
        return labels to actions
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
                    // Show the freshly-saved look in the preset dropdown right away.
                    presetRebind?.invoke("★ $name")
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
        libraryList = null
        libraryFolderLabel = null
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
            text = "Games"
            setTextColor(Brand.TEXT); textSize = 22f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        head.addView(Brand.ghostButton(this, "Back") { closeGames() })
        panel.addView(head)
        panel.addView(TextView(this).apply {
            text = "Point Prismatic at your games folder — it scans every .nds and tags " +
                "each one Compatible or Untested. Bring your own dump."
            setTextColor(Brand.TEXT_MUTED); textSize = 13f
            setPadding(0, Brand.dp(context, 4f), 0, Brand.dp(context, 10f))
        })

        // Folder row: current folder + choose / rescan.
        val folderRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, 0, Brand.dp(context, 8f))
        }
        val folderLbl = TextView(this).apply {
            text = folderDisplay()
            setTextColor(Brand.PRIMARY_LT); textSize = 14f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        }
        libraryFolderLabel = folderLbl
        folderRow.addView(folderLbl)
        folderRow.addView(Brand.ghostButton(this, "Choose folder") { pickGamesFolder() })
        folderRow.addView(Brand.ghostButton(this, "Rescan") { refreshLibrary() }.apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { leftMargin = Brand.dp(this@MainActivity, 8f) }
        })
        panel.addView(folderRow)

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        }
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        libraryList = list
        populateLibrary(list)
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
            look.enable25D, look.enableShader, look.tilt, look.lantern, look.antialias, look.params
        )
        if (persist) savePrefs()
    }

    private fun applyState() {
        topView.timeOfDay = shared.timeOfDay
        bottomInline.timeOfDay = shared.timeOfDay
        presentation?.sync()
    }

    private fun updatePaused() {
        val paused = homeVisible || menuVisible || (gamesOverlay != null) || (remapOverlay != null)
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
        look.tilt = entry.tilt
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
        resetSpeed()
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
        resetSpeed()
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
        } else if (requestCode == FOLDER_PICK_REQUEST && resultCode == RESULT_OK) {
            data?.data?.let { uri ->
                try {
                    contentResolver.takePersistableUriPermission(
                        uri, Intent.FLAG_GRANT_READ_URI_PERMISSION)
                } catch (_: Exception) { /* best effort */ }
                gamesTreeUri = uri.toString()
                savePrefs()
                refreshLibrary()
                toast("Games folder set")
            }
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
        look.antialias = p.getBoolean("antialias", false)
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
        gamesTreeUri = p.getString("gamesTree", "") ?: ""
        loadKeyMap(p.getString("keymap", "") ?: "")
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
            .putBoolean("antialias", look.antialias)
            .putString("gamesTree", gamesTreeUri)
            .putString("keymap", keyMap.entries.joinToString(";") { "${it.key}:${it.value}" })
            .apply()
    }

    // ---- Physical gamepad input (remappable — all AYN Thor buttons) -------

    private fun actionToBit(action: String): Int = when (action) {
        "A" -> NativeBridge.BTN_A
        "B" -> NativeBridge.BTN_B
        "X" -> NativeBridge.BTN_X
        "Y" -> NativeBridge.BTN_Y
        "L" -> NativeBridge.BTN_L
        "R" -> NativeBridge.BTN_R
        "START" -> NativeBridge.BTN_START
        "SELECT" -> NativeBridge.BTN_SELECT
        "UP" -> NativeBridge.BTN_UP
        "DOWN" -> NativeBridge.BTN_DOWN
        "LEFT" -> NativeBridge.BTN_LEFT
        "RIGHT" -> NativeBridge.BTN_RIGHT
        else -> 0    // "SPEED" and "NONE" carry no DS bit
    }

    private fun defaultKeyMap(): HashMap<Int, String> = hashMapOf(
        KeyEvent.KEYCODE_DPAD_UP to "UP",
        KeyEvent.KEYCODE_DPAD_DOWN to "DOWN",
        KeyEvent.KEYCODE_DPAD_LEFT to "LEFT",
        KeyEvent.KEYCODE_DPAD_RIGHT to "RIGHT",
        KeyEvent.KEYCODE_BUTTON_A to "A",
        KeyEvent.KEYCODE_BUTTON_B to "B",
        KeyEvent.KEYCODE_BUTTON_X to "X",
        KeyEvent.KEYCODE_BUTTON_Y to "Y",
        KeyEvent.KEYCODE_BUTTON_L1 to "L",
        KeyEvent.KEYCODE_BUTTON_R1 to "R",
        KeyEvent.KEYCODE_BUTTON_START to "START",
        KeyEvent.KEYCODE_ENTER to "START",
        KeyEvent.KEYCODE_BUTTON_SELECT to "SELECT",
        KeyEvent.KEYCODE_BUTTON_R2 to "SPEED"   // back-right trigger = fast-forward
    )

    private fun loadKeyMap(csv: String) {
        keyMap.clear()
        if (csv.isNotBlank()) {
            for (pair in csv.split(';')) {
                val kv = pair.split(':')
                val kc = kv.getOrNull(0)?.toIntOrNull()
                val act = kv.getOrNull(1)
                if (kc != null && !act.isNullOrBlank()) keyMap[kc] = act
            }
        }
        if (keyMap.isEmpty()) keyMap.putAll(defaultKeyMap())
    }

    private fun keyForAction(action: String): Int? =
        keyMap.entries.firstOrNull { it.value == action }?.key

    /** Friendly display name for a physical key. */
    private fun keyLabel(keyCode: Int): String = when (keyCode) {
        KeyEvent.KEYCODE_BUTTON_A -> "A"
        KeyEvent.KEYCODE_BUTTON_B -> "B"
        KeyEvent.KEYCODE_BUTTON_X -> "X"
        KeyEvent.KEYCODE_BUTTON_Y -> "Y"
        KeyEvent.KEYCODE_BUTTON_L1 -> "L1"
        KeyEvent.KEYCODE_BUTTON_R1 -> "R1"
        KeyEvent.KEYCODE_BUTTON_L2 -> "L2"
        KeyEvent.KEYCODE_BUTTON_R2 -> "R2"
        KeyEvent.KEYCODE_BUTTON_THUMBL -> "L3"
        KeyEvent.KEYCODE_BUTTON_THUMBR -> "R3"
        KeyEvent.KEYCODE_BUTTON_START, KeyEvent.KEYCODE_ENTER -> "Start"
        KeyEvent.KEYCODE_BUTTON_SELECT -> "Select"
        KeyEvent.KEYCODE_DPAD_UP -> "D-Up"
        KeyEvent.KEYCODE_DPAD_DOWN -> "D-Down"
        KeyEvent.KEYCODE_DPAD_LEFT -> "D-Left"
        KeyEvent.KEYCODE_DPAD_RIGHT -> "D-Right"
        else -> KeyEvent.keyCodeToString(keyCode).removePrefix("KEYCODE_")
    }

    /** Reserved keys that must keep their system role and cannot be remapped. */
    private fun isBindableKey(kc: Int): Boolean = when (kc) {
        KeyEvent.KEYCODE_BACK, KeyEvent.KEYCODE_BUTTON_MODE, KeyEvent.KEYCODE_MENU,
        KeyEvent.KEYCODE_VOLUME_UP, KeyEvent.KEYCODE_VOLUME_DOWN, KeyEvent.KEYCODE_VOLUME_MUTE,
        KeyEvent.KEYCODE_HOME, KeyEvent.KEYCODE_POWER -> false
        else -> true
    }

    private fun cycleSpeed() {
        val now = android.os.SystemClock.uptimeMillis()
        if (now - lastSpeedCycle < 200L) return    // debounce key + trigger double-fire
        lastSpeedCycle = now
        speedIndex = (speedIndex + 1) % SPEED_MULTS.size
        val m = SPEED_MULTS[speedIndex]
        NativeBridge.nativeSetSpeed(m)
        toast(if (m == 1) "Speed: Normal" else "Fast-forward: ${m}x")
    }

    private fun resetSpeed() {
        speedIndex = 0
        lastSpeedCycle = 0L
        NativeBridge.nativeSetSpeed(1)
    }

    /** A mapped key went down while a game is active. Returns true if consumed. */
    private fun onActionDown(keyCode: Int, firstPress: Boolean): Boolean {
        val action = keyMap[keyCode] ?: return false
        if (action == "SPEED") { if (firstPress) cycleSpeed(); return true }
        val bit = actionToBit(action)
        if (bit != 0) {
            if (firstPress) { padMask = padMask or bit; topView.inputMask = padMask }
            return true
        }
        return false
    }

    private fun onActionUp(keyCode: Int): Boolean {
        val action = keyMap[keyCode] ?: return false
        if (action == "SPEED") return true
        val bit = actionToBit(action)
        if (bit != 0) { padMask = padMask and bit.inv(); topView.inputMask = padMask; return true }
        return false
    }

    private fun beginRebind(action: String) {
        rebindAction = action
        rebindDialog = AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle("Press a button")
            .setMessage("Press the button to use for “${ACTIONS[action]}”.\nBack cancels.")
            .setOnCancelListener { rebindAction = null }
            .show()
    }

    private fun cancelRebind() {
        rebindAction = null
        rebindDialog?.dismiss(); rebindDialog = null
    }

    private fun bindKey(keyCode: Int, action: String) {
        keyMap.entries.removeAll { it.value == action }   // one physical key per action
        keyMap[keyCode] = action
        savePrefs()
        cancelRebind()
        refreshRemapList()
        toast("${keyLabel(keyCode)} → ${ACTIONS[action]}")
    }

    private fun isMenuToggleKey(keyCode: Int) =
        keyCode == KeyEvent.KEYCODE_BACK ||
            keyCode == KeyEvent.KEYCODE_BUTTON_MODE ||
            keyCode == KeyEvent.KEYCODE_MENU

    private fun anyOverlayOpen() =
        studioOverlay != null || gamesOverlay != null || remapOverlay != null

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        val target = rebindAction
        if (target != null) {
            if (event.action == KeyEvent.ACTION_DOWN) {
                val kc = event.keyCode
                if (kc == KeyEvent.KEYCODE_BACK) { cancelRebind(); return true }
                if (isBindableKey(kc)) { bindKey(kc, target); return true }
            }
            return true    // swallow everything else while listening for a bind
        }
        return super.dispatchKeyEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent?): Boolean {
        if (isMenuToggleKey(keyCode) && event?.repeatCount == 0) {
            when {
                remapOverlay != null -> closeRemap()
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
        if (onActionDown(keyCode, event?.repeatCount == 0)) return true
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent?): Boolean {
        if (isMenuToggleKey(keyCode)) return true
        if (menuVisible || homeVisible || anyOverlayOpen()) return super.onKeyUp(keyCode, event)
        if (onActionUp(keyCode)) return true
        return super.onKeyUp(keyCode, event)
    }

    override fun onGenericMotionEvent(event: MotionEvent): Boolean {
        // Analog triggers (L2/R2) often arrive as axes; convert them to L2/R2 key
        // edges so they can be mapped and used like any other button.
        if (event.source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK &&
            event.action == MotionEvent.ACTION_MOVE) {
            l2Down = handleTrigger(event, MotionEvent.AXIS_LTRIGGER, MotionEvent.AXIS_BRAKE,
                KeyEvent.KEYCODE_BUTTON_L2, l2Down)
            r2Down = handleTrigger(event, MotionEvent.AXIS_RTRIGGER, MotionEvent.AXIS_GAS,
                KeyEvent.KEYCODE_BUTTON_R2, r2Down)
        }
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

    /** Edge-detect an analog trigger and route it as a synthetic key. Returns new down-state. */
    private fun handleTrigger(ev: MotionEvent, axisA: Int, axisB: Int, keyCode: Int,
                              was: Boolean): Boolean {
        val v = maxOf(ev.getAxisValue(axisA), ev.getAxisValue(axisB))
        val now = if (was) v > 0.35f else v > 0.6f     // hysteresis
        if (now && !was) {
            if (rebindAction != null) {
                if (isBindableKey(keyCode)) bindKey(keyCode, rebindAction!!)
            } else if (!(menuVisible || homeVisible || anyOverlayOpen())) {
                onActionDown(keyCode, true)
            }
        } else if (!now && was) {
            if (rebindAction == null) onActionUp(keyCode)
        }
        return now
    }

    // ======================================================================
    //  Button-mapping overlay
    // ======================================================================

    private fun openRemap() {
        closeStudio(); closeGames()
        showMenu(false)
        if (remapOverlay != null) return
        val overlay = buildRemapOverlay()
        remapOverlay = overlay
        root.addView(overlay)
        Brand.enterFrom(overlay.findViewWithTag<View>("remap_panel") ?: overlay, 0, 22f)
        updatePaused()
    }

    private fun closeRemap() {
        cancelRebind()
        remapOverlay?.let { root.removeView(it) }
        remapOverlay = null
        remapList = null
        updatePaused()
        if (gameLoaded) showMenu(true) else showHome(true)
    }

    private fun buildRemapOverlay(): View {
        val scrim = FrameLayout(this).apply {
            background = Brand.screenBackground()
            isClickable = true
        }
        val panel = LinearLayout(this).apply {
            tag = "remap_panel"
            orientation = LinearLayout.VERTICAL
            setPadding(Brand.dp(context, 34f), Brand.dp(context, 26f),
                Brand.dp(context, 34f), Brand.dp(context, 26f))
        }
        val head = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        head.addView(TextView(this).apply {
            text = "Button Mapping"
            setTextColor(Brand.TEXT); textSize = 22f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        head.addView(Brand.ghostButton(this, "Back") { closeRemap() })
        panel.addView(head)
        panel.addView(TextView(this).apply {
            text = "Map any AYN Thor button. Back always opens the menu and can't be " +
                "remapped — use L2/R2 (the DS has no back triggers) for fast-forward or macros."
            setTextColor(Brand.TEXT_MUTED); textSize = 13f
            setPadding(0, Brand.dp(context, 4f), 0, Brand.dp(context, 12f))
        })

        val scroll = ScrollView(this).apply {
            isVerticalScrollBarEnabled = false
            layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f)
        }
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        remapList = list
        scroll.addView(list)
        panel.addView(scroll)

        val footer = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, Brand.dp(context, 12f), 0, 0)
        }
        footer.addView(Brand.ghostButton(this, "Reset to defaults") {
            keyMap.clear(); keyMap.putAll(defaultKeyMap()); savePrefs(); refreshRemapList()
            toast("Defaults restored")
        })
        panel.addView(footer)
        refreshRemapList()

        scrim.addView(
            panel,
            FrameLayout.LayoutParams(
                (resources.displayMetrics.widthPixels * 0.82f).toInt().coerceAtMost(Brand.dp(this, 720f)),
                ViewGroup.LayoutParams.MATCH_PARENT
            ).apply { gravity = Gravity.CENTER }
        )
        return scrim
    }

    private fun refreshRemapList() {
        val list = remapList ?: return
        list.removeAllViews()
        for ((action, label) in ACTIONS) list.addView(remapRow(action, label))
    }

    private fun remapRow(action: String, label: String): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = Brand.card(12f, ctx = this@MainActivity)
            setPadding(Brand.dp(context, 16f), Brand.dp(context, 12f),
                Brand.dp(context, 16f), Brand.dp(context, 12f))
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(this@MainActivity, 8f) }
        }
        row.addView(TextView(this).apply {
            text = label
            setTextColor(Brand.TEXT); textSize = 15f
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        val kc = keyForAction(action)
        row.addView(TextView(this).apply {
            text = if (kc != null) "  ${keyLabel(kc)}  " else "  —  "
            setTextColor(Brand.BG_TOP); textSize = 13f; typeface = Typeface.DEFAULT_BOLD
            background = Brand.pill(this@MainActivity,
                if (kc != null) Brand.CYAN else Brand.TEXT_MUTED, 8f)
            setPadding(Brand.dp(context, 10f), Brand.dp(context, 3f),
                Brand.dp(context, 10f), Brand.dp(context, 3f))
        })
        row.addView(Brand.ghostButton(this, "Rebind") { beginRebind(action) }.apply {
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { leftMargin = Brand.dp(this@MainActivity, 10f) }
        })
        return row
    }

    // ======================================================================
    //  Games library (user folder scan + compatibility)
    // ======================================================================

    private data class ScannedRom(val uri: Uri, val name: String, val title: String, val code: String)

    private fun pickGamesFolder() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).apply {
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or
                Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION)
        }
        try {
            startActivityForResult(intent, FOLDER_PICK_REQUEST)
        } catch (e: Exception) {
            toast("No folder picker available: ${e.message}")
        }
    }

    private fun folderDisplay(): String {
        if (gamesTreeUri.isBlank()) return "No folder set"
        return try {
            val id = DocumentsContract.getTreeDocumentId(Uri.parse(gamesTreeUri))
            id.substringAfterLast(':').substringAfterLast('/').ifBlank { "Selected folder" }
        } catch (e: Exception) { "Selected folder" }
    }

    /** Read the 4-char cartridge code (0x0C) and 12-char title (0x00) from an NDS header. */
    private fun readNdsHeader(uri: Uri): Pair<String, String> = try {
        contentResolver.openInputStream(uri)?.use { s ->
            val buf = ByteArray(16)
            var read = 0
            while (read < 16) {
                val nrd = s.read(buf, read, 16 - read)
                if (nrd < 0) break
                read += nrd
            }
            val title = String(buf, 0, 12, Charsets.US_ASCII).trim { it <= ' ' || it == '\u0000' }
            val code = if (read >= 16)
                String(buf, 12, 4, Charsets.US_ASCII).trim { it <= ' ' || it == '\u0000' } else ""
            title to code
        } ?: ("" to "")
    } catch (e: Exception) { "" to "" }

    private fun scanGames(): List<ScannedRom> {
        if (gamesTreeUri.isBlank()) return emptyList()
        val tree = Uri.parse(gamesTreeUri)
        val childrenUri = try {
            DocumentsContract.buildChildDocumentsUriUsingTree(
                tree, DocumentsContract.getTreeDocumentId(tree))
        } catch (e: Exception) { return emptyList() }
        val out = ArrayList<ScannedRom>()
        val proj = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME
        )
        try {
            contentResolver.query(childrenUri, proj, null, null, null)?.use { c ->
                val idId = c.getColumnIndex(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
                val idName = c.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME)
                while (c.moveToNext()) {
                    val name = c.getString(idName) ?: continue
                    if (!name.lowercase().endsWith(".nds")) continue
                    val docId = c.getString(idId) ?: continue
                    val docUri = DocumentsContract.buildDocumentUriUsingTree(tree, docId)
                    val (title, code) = readNdsHeader(docUri)
                    out.add(ScannedRom(docUri, name, title, code))
                }
            }
        } catch (e: Exception) { /* unreadable tree — leave list empty */ }
        out.sortBy { it.name.lowercase() }
        return out
    }

    private fun refreshLibrary() {
        libraryFolderLabel?.text = folderDisplay()
        libraryList?.let { populateLibrary(it) }
    }

    private fun populateLibrary(list: LinearLayout) {
        list.removeAllViews()
        fun hint(msg: String) = list.addView(TextView(this).apply {
            text = msg; setTextColor(Brand.TEXT_MUTED); textSize = 13f
            setPadding(0, Brand.dp(context, 8f), 0, Brand.dp(context, 4f))
        })
        if (gamesTreeUri.isBlank()) {
            hint("Choose a games folder and Prismatic scans it for .nds dumps.")
        } else {
            val roms = scanGames()
            if (roms.isEmpty()) hint("No .nds files found in the selected folder.")
            else for (r in roms) list.addView(libraryCard(r))
        }
        list.addView(Brand.sectionLabel(this, "Known-compatible titles"))
        if (registry.games.isEmpty()) hint("Registry unavailable.")
        else for (g in registry.games) list.addView(gameCard(g))
    }

    private fun libraryCard(rom: ScannedRom): View {
        val entry = registry.forCode(rom.code)
        val compatible = entry != null
        val displayTitle = when {
            entry != null -> entry.title
            rom.title.isNotBlank() -> rom.title
            else -> rom.name.removeSuffix(".nds").removeSuffix(".NDS")
        }
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = Brand.card(14f, ctx = this@MainActivity)
            setPadding(Brand.dp(context, 18f), Brand.dp(context, 14f),
                Brand.dp(context, 18f), Brand.dp(context, 14f))
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(this@MainActivity, 10f) }
            isClickable = true; isFocusable = true
            setOnClickListener { loadRomFromUri(rom.uri) }
            Brand.attachPress(this)
        }
        val titleRow = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
        }
        titleRow.addView(TextView(this).apply {
            text = displayTitle
            setTextColor(Brand.TEXT); textSize = 16f; typeface = Typeface.DEFAULT_BOLD
            layoutParams = LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
        })
        titleRow.addView(TextView(this).apply {
            text = if (compatible) "  Compatible  " else "  Untested  "
            setTextColor(Brand.BG_TOP); textSize = 12f; typeface = Typeface.DEFAULT_BOLD
            background = Brand.pill(this@MainActivity,
                if (compatible) Brand.CYAN else 0xFFF59E0B.toInt(), 8f)
            setPadding(Brand.dp(context, 10f), Brand.dp(context, 3f),
                Brand.dp(context, 10f), Brand.dp(context, 3f))
        })
        card.addView(titleRow)
        card.addView(TextView(this).apply {
            text = buildString {
                append(rom.name)
                if (rom.code.isNotBlank()) append("  ·  ").append(rom.code)
                if (entry != null) append("  ·  ").append(entry.preset).append(" · ").append(entry.status)
            }
            setTextColor(Brand.TEXT_MUTED); textSize = 12f
            setPadding(0, Brand.dp(context, 6f), 0, 0)
        })
        return card
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
        private const val FOLDER_PICK_REQUEST = 0x2002
        private const val PREFS = "prismatic"
        private val SPEED_MULTS = intArrayOf(1, 2, 5)
        // Remappable actions in display order: id -> label.
        private val ACTIONS = linkedMapOf(
            "A" to "A", "B" to "B", "X" to "X", "Y" to "Y",
            "L" to "L (shoulder)", "R" to "R (shoulder)",
            "START" to "Start", "SELECT" to "Select",
            "UP" to "D-pad Up", "DOWN" to "D-pad Down",
            "LEFT" to "D-pad Left", "RIGHT" to "D-pad Right",
            "SPEED" to "Fast-forward (2x -> 5x)"
        )

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
