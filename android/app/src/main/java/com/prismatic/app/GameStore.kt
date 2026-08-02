// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.net.HttpURLConnection
import java.net.URL

/**
 * The game platform: installed-game library, ROM identity, private
 * installations and the gen1recomp-style mod workflow.
 *
 * Invariants:
 *  - the user's original ROM (SAF source) is copied ONCE into the install as
 *    the pristine `source.nds` and never modified afterwards;
 *  - every mod profile is a separate generated build under builds/;
 *  - every patch and every generated build is SHA-256 verified against the
 *    canonical Visual+ release manifest before it is ever launched.
 */
class GameStore(private val ctx: Context) {

    data class InstalledGame(
        val id: String,
        val edition: String,          // "heartgold" | "soulsilver"
        val displayName: String,
        val sourceSha256: String,
        val gameCode: String,
        val revision: Int,
        val language: String,
        val region: String,
        val verdict: String,          // Verified / Identified / Modified
        val installDir: String,
        var activeProfile: String,    // vanilla / visual-plus / ...
        var playRomPath: String,
        var lastPlayedMs: Long,
        var playSeconds: Long,
    ) {
        val sourceRomPath: String get() = "$installDir/source.nds"
        val savesDir: String get() = "$installDir/saves"
        val statesDir: String get() = "$installDir/states"
        val isHeartGold: Boolean get() = edition == "heartgold"
    }

    data class Profile(val id: String, val name: String, val variant: String, val desc: String)

    data class Artifact(
        val edition: String, val variant: String, val file: String,
        val patchSha256: String, val sourceSha256: String, val patchedSha256: String,
    )

    val games = mutableListOf<InstalledGame>()

    private val libFile: File get() = File(ctx.filesDir, "library.json")
    private val packagesDir: File get() = File(ctx.filesDir, "packages/visual-plus-hgss")
    private val installsRoot: File get() = File(ctx.filesDir, "installs")

    // ---- persistence ---------------------------------------------------------

    fun load() {
        games.clear()
        if (!libFile.exists()) return
        runCatching {
            val arr = JSONObject(libFile.readText()).optJSONArray("games") ?: JSONArray()
            for (i in 0 until arr.length()) {
                val o = arr.getJSONObject(i)
                games.add(
                    InstalledGame(
                        id = o.getString("id"),
                        edition = o.getString("edition"),
                        displayName = o.getString("displayName"),
                        sourceSha256 = o.getString("sourceSha256"),
                        gameCode = o.optString("gameCode"),
                        revision = o.optInt("revision"),
                        language = o.optString("language"),
                        region = o.optString("region"),
                        verdict = o.optString("verdict"),
                        installDir = o.getString("installDir"),
                        activeProfile = o.optString("activeProfile", "vanilla"),
                        playRomPath = o.optString("playRomPath"),
                        lastPlayedMs = o.optLong("lastPlayedMs"),
                        playSeconds = o.optLong("playSeconds"),
                    )
                )
            }
        }
    }

    fun save() {
        val arr = JSONArray()
        for (g in games) arr.put(
            JSONObject()
                .put("id", g.id).put("edition", g.edition)
                .put("displayName", g.displayName).put("sourceSha256", g.sourceSha256)
                .put("gameCode", g.gameCode).put("revision", g.revision)
                .put("language", g.language).put("region", g.region)
                .put("verdict", g.verdict).put("installDir", g.installDir)
                .put("activeProfile", g.activeProfile).put("playRomPath", g.playRomPath)
                .put("lastPlayedMs", g.lastPlayedMs).put("playSeconds", g.playSeconds)
        )
        libFile.writeText(JSONObject().put("version", 1).put("games", arr).toString(2))
    }

    fun find(id: String): InstalledGame? = games.firstOrNull { it.id == id }

    // ---- import --------------------------------------------------------------

    sealed class ImportResult {
        data class Ok(val game: InstalledGame) : ImportResult()
        data class Rejected(val reason: String) : ImportResult()
    }

    /**
     * Import a ROM from a SAF stream: stage privately, identify, verify, then
     * promote to a permanent install. Heavy (128MB copy + hash) — call off the
     * main thread.
     */
    fun importRom(input: InputStream, approxName: String): ImportResult {
        installsRoot.mkdirs()
        val staging = File(installsRoot, ".staging.nds")
        staging.delete()
        input.use { src -> FileOutputStream(staging).use { dst -> src.copyTo(dst, 1 shl 20) } }

        val info = JSONObject(NativeBridge.nativeIdentifyRom(staging.absolutePath))
        if (info.has("error")) {
            staging.delete()
            return ImportResult.Rejected(info.getString("error"))
        }
        val family = info.optString("family")
        val verdict = info.optString("verdict")
        if (family != "pokemon-hgss") {
            staging.delete()
            return ImportResult.Rejected(
                "\"$approxName\" is not a supported game (found: ${info.optString("title", "?")}). " +
                    "Prismatic currently supports Pokémon HeartGold and SoulSilver."
            )
        }
        val sha = info.getString("sha256")
        val edition = info.getString("edition")
        val id = "${edition}_${sha.substring(0, 8)}"
        if (find(id) != null) {
            staging.delete()
            return ImportResult.Rejected("This exact ROM is already in your library.")
        }

        val dir = File(installsRoot, id)
        File(dir, "saves").mkdirs()
        File(dir, "states").mkdirs()
        File(dir, "builds").mkdirs()
        val source = File(dir, "source.nds")
        if (!staging.renameTo(source)) {
            staging.copyTo(source, overwrite = true); staging.delete()
        }

        val g = InstalledGame(
            id = id, edition = edition,
            displayName = info.optString("displayName", "Pokémon"),
            sourceSha256 = sha,
            gameCode = info.optString("gameCode"),
            revision = info.optInt("revision"),
            language = info.optString("language"),
            region = info.optString("region"),
            verdict = verdict,
            installDir = dir.absolutePath,
            activeProfile = "vanilla",
            playRomPath = source.absolutePath,  // vanilla plays the pristine copy read-only
            lastPlayedMs = 0L, playSeconds = 0L,
        )
        games.add(g)
        save()
        return ImportResult.Ok(g)
    }

    fun remove(id: String, deleteData: Boolean) {
        val g = find(id) ?: return
        games.remove(g)
        save()
        if (deleteData) File(g.installDir).deleteRecursively()
    }

    // ---- mod profiles (Visual+ via the canonical repository) ------------------

    val profiles = listOf(
        Profile("vanilla", "Vanilla", "", "Original game. No modifications."),
        Profile("visual-plus", "Visual+ (Full)", "full",
            "Battle backgrounds, full camera and fast HP bars."),
        Profile("conservative", "Visual+ (Conservative)", "conservative-camera",
            "Visual+ backgrounds with a subtler camera."),
        Profile("visual-only", "Visual+ (Visuals only)", "visual-only",
            "Battle backgrounds only — original camera and HP speed."),
        Profile("safe", "Visual+ (Safe)", "safe",
            "The most compatibility-cautious Visual+ variant."),
    )

    sealed class BuildResult {
        data class Ok(val playRomPath: String) : BuildResult()
        data class Failed(val reason: String) : BuildResult()
    }

    /**
     * Ensure the build for `profileId` exists (download patch -> verify ->
     * apply -> verify output), then activate it. Blocking; run on a worker
     * thread. Progress strings are UI-ready.
     */
    fun buildProfile(g: InstalledGame, profileId: String, progress: (String) -> Unit): BuildResult {
        val prof = profiles.firstOrNull { it.id == profileId }
            ?: return BuildResult.Failed("Unknown profile")
        if (prof.variant.isEmpty()) {  // vanilla
            g.activeProfile = "vanilla"
            g.playRomPath = g.sourceRomPath
            save()
            return BuildResult.Ok(g.playRomPath)
        }
        val art = ARTIFACTS.firstOrNull { it.edition == g.edition && it.variant == prof.variant }
            ?: return BuildResult.Failed("No ${prof.variant} package for ${g.edition}")
        if (art.sourceSha256 != g.sourceSha256)
            return BuildResult.Failed(
                "Visual+ ${VP_VERSION} requires the verified clean ${g.displayName} (USA) ROM. " +
                    "This install's ROM has a different hash."
            )

        val out = File(g.installDir, "builds/${prof.id}.nds")
        if (out.exists()) {
            progress("Verifying cached build…")
            if (NativeBridge.nativeFileSha256(out.absolutePath) == art.patchedSha256) {
                g.activeProfile = prof.id
                g.playRomPath = out.absolutePath
                save()
                return BuildResult.Ok(g.playRomPath)
            }
            out.delete()
        }

        val patch = File(packagesDir, art.file)
        if (!patch.exists() || NativeBridge.nativeFileSha256(patch.absolutePath) != art.patchSha256) {
            progress("Downloading ${prof.name} package…")
            val err = download("$VP_RELEASE_URL/${art.file}", patch)
            if (err != null) return BuildResult.Failed("Download failed: $err")
            progress("Verifying package…")
            if (NativeBridge.nativeFileSha256(patch.absolutePath) != art.patchSha256) {
                patch.delete()
                return BuildResult.Failed("Package failed verification (corrupted download).")
            }
        }

        progress("Building ${prof.name}…")
        val err = NativeBridge.nativeApplyPatch(g.sourceRomPath, patch.absolutePath, out.absolutePath)
        if (err.isNotEmpty()) { out.delete(); return BuildResult.Failed(err) }
        progress("Verifying build…")
        if (NativeBridge.nativeFileSha256(out.absolutePath) != art.patchedSha256) {
            out.delete()
            return BuildResult.Failed("Generated build failed verification.")
        }
        g.activeProfile = prof.id
        g.playRomPath = out.absolutePath
        save()
        return BuildResult.Ok(g.playRomPath)
    }

    private fun download(url: String, dest: File): String? {
        dest.parentFile?.mkdirs()
        return runCatching {
            val conn = URL(url).openConnection() as HttpURLConnection
            conn.instanceFollowRedirects = true
            conn.connectTimeout = 15000
            conn.readTimeout = 30000
            conn.inputStream.use { src -> FileOutputStream(dest).use { dst -> src.copyTo(dst, 1 shl 16) } }
            conn.disconnect()
            null
        }.getOrElse { it.message ?: "network error" }
    }

    companion object {
        // Canonical Visual+ repository (spec: Prismatic consumes, never forks).
        const val VP_REPO = "DosukaSOL/pokemon-hgss-visual-mod"
        const val VP_VERSION = "1.0.0"
        const val VP_RELEASE_URL =
            "https://github.com/DosukaSOL/pokemon-hgss-visual-mod/releases/download/v$VP_VERSION"

        // Portable (RFC 3284, no secondary compression) artifacts published on
        // the canonical release; hashes pin both the patch and its output.
        val ARTIFACTS = listOf(
            Artifact("heartgold", "visual-only", "heartgold-english-visual-plus-visual-only-1.0.0-portable.xdelta", "38afc01de78550e9eeeb108dd819d3479f8e59248267584c654f94cd34174515", "65f02a56842b75aa92d775d56d657a56fe3fa993550b04dc20704ab82d760105", "6e67832ea6d0800c21a887063af2aa26d227c23899050e7f09f42c9063093c47"),
            Artifact("heartgold", "safe", "heartgold-english-visual-plus-safe-1.0.0-portable.xdelta", "c5c32db265ff3f03171b756565d763b4f522a871d38ebf787d1e32c3787e2cc8", "65f02a56842b75aa92d775d56d657a56fe3fa993550b04dc20704ab82d760105", "4ea4e2a419bc9a190ada3d5a7e60e70a45347162f252043b5cef235dea5cacbd"),
            Artifact("heartgold", "full", "heartgold-english-visual-plus-full-1.0.0-portable.xdelta", "5f56cce8466e0562755615889a2480dba4dc323b86c59da5f2c31606fe81bb5f", "65f02a56842b75aa92d775d56d657a56fe3fa993550b04dc20704ab82d760105", "eb0aaed35ba445f26a3fdfdc00c86ab8e1abac72b2191984f57290b383fb8379"),
            Artifact("heartgold", "conservative-camera", "heartgold-english-visual-plus-conservative-camera-1.0.0-portable.xdelta", "2f62a0eb1ce8c933a9ce63983063a7171fd3492f9397ddff55c0cc87fe048d5b", "65f02a56842b75aa92d775d56d657a56fe3fa993550b04dc20704ab82d760105", "69606a89c8d3eeada76d4bb79bb4b5641cbc59743473434c38511cef9309718b"),
            Artifact("soulsilver", "visual-only", "soulsilver-english-visual-plus-visual-only-1.0.0-portable.xdelta", "a57e5f42a2a3d41d13d900b94d466d611a39c22069f49a0667392e367657b5d8", "51d0f94a16af7d77c067b4cb7d821ba890a13203a2e2c76049623332c0582e20", "001287c2cd253bd15a1e78dd227d350acdfec4d60af4648df4ba75cbe486d378"),
            Artifact("soulsilver", "safe", "soulsilver-english-visual-plus-safe-1.0.0-portable.xdelta", "7e20021e1216c1c01bd11da792b5f441c2359ce24a3092a317e461bbd939a03c", "51d0f94a16af7d77c067b4cb7d821ba890a13203a2e2c76049623332c0582e20", "6b5f5fc8d2fd50918269389609da4da6351731691ee97d6c45cbcfba8bcfbb82"),
            Artifact("soulsilver", "full", "soulsilver-english-visual-plus-full-1.0.0-portable.xdelta", "1790c374fb33c265f3568cac1651696fb6d7e4166f08b6a09541cc651fee7615", "51d0f94a16af7d77c067b4cb7d821ba890a13203a2e2c76049623332c0582e20", "5b3a377d20b2aa45fa1b9e6fbab842aaddc75e4fef38c38624662c9f5e48bf8e"),
            Artifact("soulsilver", "conservative-camera", "soulsilver-english-visual-plus-conservative-camera-1.0.0-portable.xdelta", "4425642eef3871ba8a0ff7e3df4279ac3cb90d79c5ede55021fcdd3857871c5e", "51d0f94a16af7d77c067b4cb7d821ba890a13203a2e2c76049623332c0582e20", "46083603366766d2489ab81d71a62ccf15ad093196aca1943f9e2cc7edc29841"),
        )
    }
}
