// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import java.io.File

/**
 * A complete, user-tweakable "look": the two independent layers (2.5D depth
 * tilt and the shader overlay) plus the 13 shader parameters. This is what the
 * Shader Studio edits, what gets pushed to the core, and what is saved/loaded.
 */
class ShaderLook {
    var enable25D = false
    var enableShader = false
    var tilt = 0.5f
    var lantern = false
    var params = FloatArray(NativeBridge.SHADER_PARAM_COUNT)

    fun copyFrom(other: ShaderLook) {
        enable25D = other.enable25D
        enableShader = other.enableShader
        tilt = other.tilt
        lantern = other.lantern
        params = other.params.copyOf()
    }

    fun snapshot(): ShaderLook = ShaderLook().also { it.copyFrom(this) }
}

/**
 * Persists custom looks as small CSV files under filesDir/shaders/, so players
 * can build a look once and re-apply it after rebooting the game.
 */
object ShaderStore {
    private const val VERSION = 1

    private fun dir(ctx: Context): File = File(ctx.filesDir, "shaders").apply { mkdirs() }

    fun list(ctx: Context): List<String> =
        dir(ctx).listFiles { f -> f.isFile && f.name.endsWith(".txt") }
            ?.map { it.name.removeSuffix(".txt") }
            ?.sorted()
            ?: emptyList()

    fun save(ctx: Context, name: String, look: ShaderLook): Boolean = try {
        val sb = StringBuilder()
        sb.append(VERSION).append(',')
        sb.append(if (look.enable25D) 1 else 0).append(',')
        sb.append(if (look.enableShader) 1 else 0).append(',')
        sb.append(look.tilt).append(',')
        sb.append(if (look.lantern) 1 else 0)
        for (p in look.params) sb.append(',').append(p)
        File(dir(ctx), safeName(name) + ".txt").writeText(sb.toString())
        true
    } catch (e: Exception) {
        false
    }

    fun load(ctx: Context, name: String): ShaderLook? = try {
        val parts = File(dir(ctx), safeName(name) + ".txt").readText().trim().split(',')
        if (parts.size < 5) null else ShaderLook().apply {
            enable25D = parts[1] == "1"
            enableShader = parts[2] == "1"
            tilt = parts[3].toFloat()
            lantern = parts[4] == "1"
            val n = NativeBridge.SHADER_PARAM_COUNT
            val arr = FloatArray(n)
            for (i in 0 until n) {
                val idx = 5 + i
                arr[i] = if (idx < parts.size) parts[idx].toFloatOrNull() ?: 0f else 0f
            }
            params = arr
        }
    } catch (e: Exception) {
        null
    }

    fun delete(ctx: Context, name: String): Boolean =
        File(dir(ctx), safeName(name) + ".txt").delete()

    private fun safeName(name: String): String =
        name.trim().replace(Regex("[^A-Za-z0-9 _-]"), "_").take(40).ifBlank { "look" }
}
