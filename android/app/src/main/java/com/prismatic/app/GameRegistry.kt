// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import org.json.JSONObject

/** One entry from the compatibility registry (assets/games.json). */
data class GameEntry(
    val code: String,
    val title: String,
    val region: String,
    val status: String,
    val preset: String,
    val enable25D: Boolean,
    val enableShader: Boolean,
    val notes: String,
)

/**
 * Loads the bundled compatibility list and looks up per-game recommended looks
 * by cartridge code. Prismatic ships only this metadata — never ROMs.
 */
class GameRegistry private constructor(val games: List<GameEntry>) {

    private val byCode: Map<String, GameEntry> = games.associateBy { it.code.uppercase() }

    fun forCode(code: String?): GameEntry? =
        if (code.isNullOrBlank()) null else byCode[code.uppercase()]

    companion object {
        fun load(ctx: Context): GameRegistry = try {
            val text = ctx.assets.open("games.json").bufferedReader().use { it.readText() }
            val arr = JSONObject(text).getJSONArray("games")
            val list = ArrayList<GameEntry>(arr.length())
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                list.add(
                    GameEntry(
                        code = o.optString("code"),
                        title = o.optString("title"),
                        region = o.optString("region"),
                        status = o.optString("status"),
                        preset = o.optString("preset"),
                        enable25D = o.optBoolean("enable25D", false),
                        enableShader = o.optBoolean("enableShader", false),
                        notes = o.optString("notes"),
                    )
                )
            }
            GameRegistry(list)
        } catch (e: Exception) {
            GameRegistry(emptyList())
        }
    }
}
