// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

import android.media.AudioAttributes
import android.media.AudioFormat
import android.media.AudioManager
import android.media.AudioTrack
import android.os.Process

/**
 * Streams emulator audio to the device speakers.
 *
 * The native side decodes the core's SPU into a small ring on the emulation
 * thread; this player pulls that PCM via [NativeBridge.nativeReadAudio] on a
 * dedicated high-priority thread and writes it to a streaming [AudioTrack].
 * When the core has produced nothing yet, we feed a little silence so the track
 * never underruns (which would click).
 */
class AudioPlayer(
    private val sampleRate: Int = 48000,
) {
    @Volatile private var running = false
    private var thread: Thread? = null
    private var track: AudioTrack? = null

    fun start() {
        if (running) return
        running = true
        thread = Thread({ loop() }, "prismatic-audio").apply {
            priority = Thread.MAX_PRIORITY
            start()
        }
    }

    fun stop() {
        running = false
        thread?.let {
            try {
                it.join(500)
            } catch (_: InterruptedException) {
            }
        }
        thread = null
        track?.run {
            try {
                pause()
                flush()
                release()
            } catch (_: Exception) {
            }
        }
        track = null
    }

    private fun loop() {
        Process.setThreadPriority(Process.THREAD_PRIORITY_URGENT_AUDIO)

        val minBytes = AudioTrack.getMinBufferSize(
            sampleRate,
            AudioFormat.CHANNEL_OUT_STEREO,
            AudioFormat.ENCODING_PCM_16BIT,
        ).coerceAtLeast(4096)

        val at = try {
            @Suppress("DEPRECATION")
            AudioTrack(
                AudioAttributes.Builder()
                    .setUsage(AudioAttributes.USAGE_GAME)
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .build(),
                AudioFormat.Builder()
                    .setSampleRate(sampleRate)
                    .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                    .setChannelMask(AudioFormat.CHANNEL_OUT_STEREO)
                    .build(),
                minBytes * 2,
                AudioTrack.MODE_STREAM,
                AudioManager.AUDIO_SESSION_ID_GENERATE,
            )
        } catch (_: Exception) {
            running = false
            return
        }
        track = at

        // Pull buffer: ~1024 stereo frames per iteration.
        val pull = ShortArray(2048)
        val silence = ShortArray(1024)
        try {
            at.play()
        } catch (_: Exception) {
            running = false
            return
        }

        while (running) {
            val n = try {
                NativeBridge.nativeReadAudio(pull)
            } catch (_: Throwable) {
                0
            }
            if (n > 0) {
                at.write(pull, 0, n)
            } else {
                // Nothing decoded yet — write a touch of silence so the stream
                // stays primed without spinning the CPU.
                at.write(silence, 0, silence.size)
            }
        }
    }
}
