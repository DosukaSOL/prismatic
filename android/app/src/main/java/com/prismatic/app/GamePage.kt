// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.app.Activity
import android.graphics.Typeface
import android.text.format.DateFormat
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import java.io.File

/**
 * Per-game management page (Play / Mods / Camera / Performance / Saves /
 * Save States / Installation / Compatibility / Diagnostics), built in the
 * Prismatic brand style with vector icons and entrance animations. One page
 * per installed game — HeartGold data never mixes with SoulSilver data.
 */
class GamePage(
    private val activity: Activity,
    private val store: GameStore,
    private val game: GameStore.InstalledGame,
    private val isRunningThis: () -> Boolean,
    private val onPlay: (GameStore.InstalledGame) -> Unit,
    private val onBack: () -> Unit,
) {
    private val ctx = activity
    private lateinit var profileRows: MutableList<Pair<GameStore.Profile, TextView>>
    private lateinit var buildStatus: TextView

    fun build(): View {
        val scrim = FrameLayout(ctx).apply {
            background = Brand.screenBackground()
            isClickable = true
        }
        val scroll = ScrollView(ctx).apply { isVerticalScrollBarEnabled = false }
        val col = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            val p = Brand.dp(ctx, 22f)
            setPadding(p, Brand.dp(ctx, 16f), p, Brand.dp(ctx, 28f))
        }
        scroll.addView(col, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT)
        scrim.addView(scroll, ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)

        col.addView(header())
        col.addView(heroCard())
        col.addView(modsCard())
        col.addView(cameraCard())
        col.addView(performanceCard())
        col.addView(savesCard())
        col.addView(statesCard())
        col.addView(installationCard())
        col.addView(compatibilityCard())
        col.addView(diagnosticsCard())
        Brand.enterFrom(col, 0, 20f)
        return scrim
    }

    // ---- pieces ---------------------------------------------------------------

    private fun header(): View {
        val row = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, 0, Brand.dp(ctx, 12f))
        }
        val back = ImageView(ctx).apply {
            setImageDrawable(Icons.back(Brand.TEXT))
            isFocusable = true
            isClickable = true
            contentDescription = "Back"
            setPadding(Brand.dp(ctx, 6f), Brand.dp(ctx, 6f), Brand.dp(ctx, 10f), Brand.dp(ctx, 6f))
            Brand.attachPress(this)
            setOnClickListener { onBack() }
        }
        row.addView(back, LinearLayout.LayoutParams(Brand.dp(ctx, 44f), Brand.dp(ctx, 44f)))
        row.addView(TextView(ctx).apply {
            text = game.displayName
            setTextColor(Brand.TEXT)
            textSize = 24f
            typeface = Typeface.DEFAULT_BOLD
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        row.addView(pill(if (game.isHeartGold) "HEARTGOLD" else "SOULSILVER",
            if (game.isHeartGold) 0xFFF5A623.toInt() else 0xFF9BB0C9.toInt()))
        row.addView(Brand.spacer(ctx, 0f).apply {
            layoutParams = LinearLayout.LayoutParams(Brand.dp(ctx, 8f), 1)
        })
        row.addView(pill(game.verdict.uppercase(),
            if (game.verdict == "Verified") 0xFF34D399.toInt() else 0xFFF59E0B.toInt()))
        return row
    }

    private fun pill(text: String, color: Int): TextView = TextView(ctx).apply {
        this.text = text
        setTextColor(color)
        textSize = 11f
        letterSpacing = 0.1f
        typeface = Typeface.DEFAULT_BOLD
        background = Brand.pill(ctx, (color and 0x00FFFFFF) or 0x22000000)
        val h = Brand.dp(ctx, 10f); val v = Brand.dp(ctx, 5f)
        setPadding(h, v, h, v)
    }

    private fun card(title: String, icon: android.graphics.drawable.Drawable): LinearLayout {
        val c = LinearLayout(ctx).apply {
            orientation = LinearLayout.VERTICAL
            background = Brand.card(18f, ctx = ctx)
            val p = Brand.dp(ctx, 18f)
            setPadding(p, Brand.dp(ctx, 14f), p, p)
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(ctx, 12f) }
        }
        val head = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, 0, 0, Brand.dp(ctx, 8f))
        }
        head.addView(ImageView(ctx).apply { setImageDrawable(icon) },
            LinearLayout.LayoutParams(Brand.dp(ctx, 22f), Brand.dp(ctx, 22f)))
        head.addView(TextView(ctx).apply {
            text = "  $title"
            setTextColor(Brand.PRIMARY_LT)
            textSize = 13f
            letterSpacing = 0.12f
            typeface = Typeface.DEFAULT_BOLD
        })
        c.addView(head)
        return c
    }

    private fun infoRow(label: String, value: String): View {
        val row = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(0, Brand.dp(ctx, 4f), 0, Brand.dp(ctx, 4f))
        }
        row.addView(TextView(ctx).apply {
            text = label
            setTextColor(Brand.TEXT_MUTED); textSize = 13f
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.42f))
        row.addView(TextView(ctx).apply {
            text = value
            setTextColor(Brand.TEXT); textSize = 13f
        }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.58f))
        return row
    }

    private fun note(text: String): TextView = TextView(ctx).apply {
        this.text = text
        setTextColor(Brand.TEXT_MUTED)
        textSize = 12f
        setPadding(0, Brand.dp(ctx, 6f), 0, 0)
    }

    // ---- sections ---------------------------------------------------------------

    private fun heroCard(): View {
        val c = LinearLayout(ctx).apply {
            orientation = LinearLayout.HORIZONTAL
            layoutParams = LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
            ).apply { topMargin = Brand.dp(ctx, 4f) }
        }
        val play = Brand.primaryButton(ctx, "Play") { onPlay(game) }.apply {
            setCompoundDrawables(Icons.sized(ctx, Icons.play(Brand.TEXT), 18f), null, null, null)
            compoundDrawablePadding = Brand.dp(ctx, 10f)
        }
        c.addView(play, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        val hasSave = File(game.savesDir).listFiles()?.any { it.extension == "sav" } == true
        if (hasSave) {
            c.addView(Brand.spacer(ctx, 0f).apply {
                layoutParams = LinearLayout.LayoutParams(Brand.dp(ctx, 10f), 1)
            })
            val cont = Brand.ghostButton(ctx, "Continue") { onPlay(game) }.apply {
                setCompoundDrawables(Icons.sized(ctx, Icons.resume(Brand.TEXT), 18f), null, null, null)
                compoundDrawablePadding = Brand.dp(ctx, 10f)
            }
            c.addView(cont, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
        return c
    }

    private fun modsCard(): View {
        val c = card("Mods", Icons.mods(Brand.PRIMARY_LT))
        profileRows = mutableListOf()
        for (p in store.profiles) {
            val row = LinearLayout(ctx).apply {
                orientation = LinearLayout.VERTICAL
                isClickable = true; isFocusable = true
                background = Brand.card(12f, top = Brand.SURFACE_HI, bottom = Brand.SURFACE_HI, ctx = ctx)
                val pd = Brand.dp(ctx, 12f)
                setPadding(pd, Brand.dp(ctx, 10f), pd, Brand.dp(ctx, 10f))
                layoutParams = LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
                ).apply { topMargin = Brand.dp(ctx, 6f) }
                Brand.attachPress(this)
                setOnClickListener { applyProfile(p) }
            }
            val title = TextView(ctx).apply {
                textSize = 15f
                typeface = Typeface.DEFAULT_BOLD
            }
            row.addView(title)
            row.addView(TextView(ctx).apply {
                text = p.desc
                setTextColor(Brand.TEXT_MUTED); textSize = 12f
            })
            c.addView(row)
            profileRows.add(p to title)
        }
        buildStatus = note("")
        c.addView(buildStatus)
        c.addView(note("Visual+ is installed from its own repository " +
            "(${GameStore.VP_REPO}, v${GameStore.VP_VERSION}). Your clean ROM is never modified — " +
            "each option builds a separate private copy, verified before launch."))
        refreshProfileRows()
        return c
    }

    private fun refreshProfileRows() {
        for ((p, title) in profileRows) {
            val active = p.id == game.activeProfile
            title.text = if (active) "● ${p.name}" else p.name
            title.setTextColor(if (active) Brand.PRIMARY_LT else Brand.TEXT)
        }
    }

    private fun applyProfile(p: GameStore.Profile) {
        if (isRunningThis()) { buildStatus.text = "Close the game before changing mods."; return }
        buildStatus.text = "Preparing…"
        Thread {
            val res = store.buildProfile(game, p.id) { msg ->
                activity.runOnUiThread { buildStatus.text = msg }
            }
            activity.runOnUiThread {
                when (res) {
                    is GameStore.BuildResult.Ok -> {
                        buildStatus.text = "${p.name} is active."
                        refreshProfileRows()
                    }
                    is GameStore.BuildResult.Failed ->
                        buildStatus.text = res.reason
                }
            }
        }.start()
    }

    private fun cameraCard(): View {
        val c = card("Camera", Icons.camera(Brand.PRIMARY_LT))
        val prefs = ctx.getSharedPreferences("prismatic", 0)
        val key = "cam_${game.id}"
        // stored as "pitch,fov,height" ("" = original)
        fun stored(): FloatArray {
            val s = prefs.getString(key, "") ?: ""
            val p = s.split(",").mapNotNull { it.toFloatOrNull() }
            return if (p.size == 3) floatArrayOf(p[0], p[1], p[2]) else floatArrayOf(0f, 1f, 0f)
        }
        fun apply(v: FloatArray, persist: Boolean = true) {
            val enabled = v[0] != 0f || v[1] != 1f || v[2] != 0f
            NativeBridge.nativeSetCamera(enabled, v[0], 0f, v[1], v[2], 0f)
            if (persist) prefs.edit().putString(key, "${v[0]},${v[1]},${v[2]}").apply()
        }

        val cur = stored()
        val status = note(if (isRunningThis()) "Adjusts live while playing."
                          else "Applied when the game starts.")

        // Presets: display-only view adjustments (game logic untouched).
        data class Preset(val name: String, val pitch: Float, val fov: Float, val h: Float)
        val presets = listOf(
            Preset("Original", 0f, 1f, 0f),
            Preset("Conservative", -8f, 1.10f, 0f),
            Preset("Cinematic", -18f, 1.25f, 0f),
            Preset("Wide", 0f, 0.85f, 0f),
            Preset("Close", -6f, 1.35f, 0f),
        )
        val presetRow = LinearLayout(ctx).apply { orientation = LinearLayout.HORIZONTAL }
        for (p in presets) {
            val b = Brand.ghostButton(ctx, p.name) {
                apply(floatArrayOf(p.pitch, p.fov, p.h))
                status.text = "${p.name} camera active."
                rebuildSliders(floatArrayOf(p.pitch, p.fov, p.h))
            }
            b.textSize = 12f
            presetRow.addView(b, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f)
                .apply { marginEnd = Brand.dp(ctx, 6f) })
        }
        c.addView(presetRow)

        // Live sliders (custom).
        sliderHost = LinearLayout(ctx).apply { orientation = LinearLayout.VERTICAL }
        c.addView(sliderHost)
        camApply = { v -> apply(v); status.text = "Custom camera active." }
        rebuildSliders(cur)

        c.addView(status)
        c.addView(note("Display-only: the projection is adjusted between the game's own " +
            "camera and the screen. Collision, scripts, timing and saves are untouched. " +
            "Visual+ camera variants (Mods) change the game's own camera and stack with this."))
        return c
    }

    private lateinit var sliderHost: LinearLayout
    private var camApply: ((FloatArray) -> Unit)? = null

    private fun rebuildSliders(v: FloatArray) {
        if (!::sliderHost.isInitialized) return
        sliderHost.removeAllViews()
        val cur = v.copyOf()
        data class Spec(val label: String, val idx: Int, val min: Float, val max: Float, val fmt: (Float) -> String)
        val specs = listOf(
            Spec("Tilt", 0, -30f, 15f) { f -> "%.0f°".format(f) },
            Spec("Zoom", 1, 0.7f, 1.6f) { f -> "%.2fx".format(f) },
            Spec("Height", 2, -60f, 60f) { f -> "%.0f".format(f) },
        )
        for (s in specs) {
            val row = LinearLayout(ctx).apply {
                orientation = LinearLayout.HORIZONTAL
                gravity = Gravity.CENTER_VERTICAL
                setPadding(0, Brand.dp(ctx, 4f), 0, 0)
            }
            val valueLabel = TextView(ctx).apply {
                text = s.fmt(cur[s.idx]); setTextColor(Brand.TEXT); textSize = 12f
                gravity = Gravity.END
            }
            row.addView(TextView(ctx).apply {
                text = s.label; setTextColor(Brand.TEXT_MUTED); textSize = 12f
            }, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.25f))
            val bar = android.widget.SeekBar(ctx).apply {
                max = 1000
                progress = ((cur[s.idx] - s.min) / (s.max - s.min) * 1000).toInt().coerceIn(0, 1000)
                setOnSeekBarChangeListener(object : android.widget.SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(sb: android.widget.SeekBar, p: Int, user: Boolean) {
                        if (!user) return
                        cur[s.idx] = s.min + (s.max - s.min) * p / 1000f
                        valueLabel.text = s.fmt(cur[s.idx])
                        camApply?.invoke(cur)   // live update every tick
                    }
                    override fun onStartTrackingTouch(sb: android.widget.SeekBar) {}
                    override fun onStopTrackingTouch(sb: android.widget.SeekBar) {}
                })
            }
            row.addView(bar, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.6f))
            row.addView(valueLabel, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 0.15f))
            sliderHost.addView(row)
        }
    }

    private fun performanceCard(): View {
        val c = card("Performance", Icons.gauge(Brand.PRIMARY_LT))
        val jit = ctx.getSharedPreferences("prismatic", 0).getBoolean("jit", true)
        c.addView(infoRow("Execution", if (jit) "JIT recompiler (fast)" else "Interpreter (safe)"))
        val display = activity.windowManager.defaultDisplay
        c.addView(infoRow("Display", "${display.refreshRate.toInt()} Hz panel, vsync presentation"))
        c.addView(infoRow("Simulation", "Always authentic 60 Hz — never accelerated"))
        c.addView(note("Execution mode and fast-forward are in the in-game menu. Performance " +
            "Mode never reduces resolution, sprites, effects or audio quality."))
        return c
    }

    private fun savesCard(): View {
        val c = card("Saves", Icons.save(Brand.PRIMARY_LT))
        val saves = File(game.savesDir).listFiles { f -> f.extension == "sav" }?.sortedBy { it.name }
        if (saves.isNullOrEmpty()) {
            c.addView(note("No battery save yet — it is created automatically when the game saves."))
        } else {
            for (s in saves) {
                c.addView(infoRow(s.name,
                    "${s.length() / 1024} KB · ${DateFormat.format("yyyy-MM-dd HH:mm", s.lastModified())}"))
            }
            c.addView(note("Battery saves flush to disk automatically when you leave the app, and on " +
                "Save & Close. Changing mod profiles never touches saves."))
        }
        return c
    }

    private fun statesCard(): View {
        val c = card("Save States", Icons.states(Brand.PRIMARY_LT))
        val dir = File(game.statesDir).apply { mkdirs() }
        val running = isRunningThis()

        val row = LinearLayout(ctx).apply { orientation = LinearLayout.HORIZONTAL }
        val slot = File(dir, "quick.state")
        val saveB = Brand.ghostButton(ctx, "Quick save") {
            if (!isRunningThis()) return@ghostButton
            val ok = NativeBridge.nativeSaveState(slot.absolutePath)
            buildToast(if (ok) "State saved" else "State save failed")
        }
        val loadB = Brand.ghostButton(ctx, "Quick load") {
            if (!isRunningThis() || !slot.exists()) return@ghostButton
            val ok = NativeBridge.nativeLoadState(slot.absolutePath)
            buildToast(if (ok) "State loaded" else "State load failed")
        }
        saveB.isEnabled = running
        loadB.isEnabled = running && slot.exists()
        saveB.alpha = if (saveB.isEnabled) 1f else 0.45f
        loadB.alpha = if (loadB.isEnabled) 1f else 0.45f
        row.addView(saveB, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        row.addView(Brand.spacer(ctx, 0f).apply {
            layoutParams = LinearLayout.LayoutParams(Brand.dp(ctx, 10f), 1)
        })
        row.addView(loadB, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        c.addView(row)
        if (slot.exists())
            c.addView(infoRow("quick.state",
                "${slot.length() / (1024 * 1024)} MB · ${DateFormat.format("yyyy-MM-dd HH:mm", slot.lastModified())}"))
        c.addView(note(if (running)
            "States are snapshots of the whole console — separate from cartridge saves."
        else
            "Start the game to use save states. States are separate from cartridge saves."))
        return c
    }

    private fun installationCard(): View {
        val c = card("Game Installation", Icons.cartridge(Brand.PRIMARY_LT))
        c.addView(infoRow("Game code", "${game.gameCode} rev ${game.revision}"))
        c.addView(infoRow("Language", "${game.language} (${game.region})"))
        c.addView(infoRow("Source SHA-256", game.sourceSha256.take(16) + "…"))
        c.addView(infoRow("Active build", File(game.playRomPath).name))
        val status = note("")
        val verify = Brand.ghostButton(ctx, "Verify installation") {
            status.text = "Verifying…"
            Thread {
                val ok = NativeBridge.nativeFileSha256(game.sourceRomPath) == game.sourceSha256
                activity.runOnUiThread {
                    status.text = if (ok) "Source copy verified — matches the imported ROM."
                    else "VERIFICATION FAILED — re-import this game from your clean ROM."
                }
            }.start()
        }
        c.addView(verify, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = Brand.dp(ctx, 8f) })
        c.addView(status)
        val remove = Brand.ghostButton(ctx, "Remove from library (keeps saves)") {
            store.remove(game.id, deleteData = false)
            onBack()
        }
        c.addView(remove, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = Brand.dp(ctx, 6f) })
        return c
    }

    private fun compatibilityCard(): View {
        val c = card("Compatibility", Icons.shield(Brand.PRIMARY_LT))
        val verified = game.verdict == "Verified"
        c.addView(infoRow("Emulation", if (verified) "Verified (melonDS core)" else "Playable — unverified dump"))
        c.addView(infoRow("Native runtime", "Not available yet (roadmap)"))
        c.addView(infoRow("ROM mapping", if (verified) "Verified dump" else game.verdict))
        c.addView(infoRow("Mods", if (verified) "Full (Visual+ ${GameStore.VP_VERSION})" else "Unavailable — needs a verified clean ROM"))
        c.addView(infoRow("Camera", "Visual+ presets"))
        c.addView(infoRow("High refresh", "Untested on this device"))
        return c
    }

    private fun diagnosticsCard(): View {
        val c = card("Diagnostics", Icons.wrench(Brand.PRIMARY_LT))
        val installSize = File(game.installDir).walkTopDown().filter { it.isFile }.sumOf { it.length() }
        c.addView(infoRow("Install size", "${installSize / (1024 * 1024)} MB"))
        c.addView(infoRow("Install dir", game.installDir.substringAfterLast("/files/")))
        val status = note("")
        val export = Brand.ghostButton(ctx, "Export diagnostics") {
            val f = File(game.installDir, "diagnostics.txt")
            f.writeText(buildString {
                appendLine("Prismatic diagnostics — ${game.displayName}")
                appendLine("id: ${game.id}")
                appendLine("gameCode: ${game.gameCode} rev ${game.revision}")
                appendLine("verdict: ${game.verdict}")
                appendLine("sourceSha256: ${game.sourceSha256}")
                appendLine("activeProfile: ${game.activeProfile}")
                appendLine("playRom: ${game.playRomPath}")
                appendLine("installSizeBytes: $installSize")
            })
            status.text = "Written to ${f.name} inside the install folder."
        }
        c.addView(export, LinearLayout.LayoutParams(
            ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT
        ).apply { topMargin = Brand.dp(ctx, 8f) })
        c.addView(status)
        return c
    }

    private fun buildToast(msg: String) {
        android.widget.Toast.makeText(ctx, msg, android.widget.Toast.LENGTH_SHORT).show()
    }
}
