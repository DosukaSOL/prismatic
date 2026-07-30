// SPDX-License-Identifier: GPL-3.0-or-later
package com.prismatic.app

/**
 * Thin Kotlin wrapper over the native PRISMATIC pipeline.
 *
 * Each render call returns an IntArray laid out as [width, height, ARGB...],
 * matching the C++ [toArgbArray] helper. Colours are non-premultiplied ARGB8888
 * suitable for [android.graphics.Bitmap.Config.ARGB_8888].
 */
object NativeBridge {
    init {
        System.loadLibrary("prismaticnative")
    }

    // Button bits for [nativeSetInput].
    const val BTN_A = 1 shl 0
    const val BTN_B = 1 shl 1
    const val BTN_X = 1 shl 2
    const val BTN_Y = 1 shl 3
    const val BTN_L = 1 shl 4
    const val BTN_R = 1 shl 5
    const val BTN_START = 1 shl 6
    const val BTN_SELECT = 1 shl 7
    const val BTN_UP = 1 shl 8
    const val BTN_DOWN = 1 shl 9
    const val BTN_LEFT = 1 shl 10
    const val BTN_RIGHT = 1 shl 11

    external fun nativeInit(): Boolean
    external fun nativeSetLantern(on: Boolean)
    external fun nativeSetTouch(x: Int, y: Int, down: Boolean)
    external fun nativeSetInput(mask: Int)
    external fun nativeRenderTop(presetIndex: Int, timeOfDay: Float, weather: Int): IntArray?
    external fun nativeRenderBottom(presetIndex: Int, timeOfDay: Float, weather: Int): IntArray?
    external fun nativePresetCount(): Int
    external fun nativePresetName(index: Int): String

    /**
     * Independent presentation layers for a real ROM. 2.5D (geometric tilt +
     * tilt-shift) and the shader overlay are fully separable — either, both, or
     * neither. [style]: 0=CRT 1=LCD 2=Warm 3=Night 4=Vivid.
     */
    external fun nativeSetPresentation(enable25D: Boolean, enableShader: Boolean, style: Int)

    /** Speed mode: JIT on = fast, off = maximum compatibility. Applied on next load. */
    external fun nativeSetJit(on: Boolean)

    /**
     * Pull decoded stereo PCM (interleaved s16 L,R) into [buf]. Returns the
     * number of shorts written (frames*2). Call from the audio thread.
     */
    external fun nativeReadAudio(buf: ShortArray): Int

    /** Load a real DS ROM. [dataDir] must be a writable app directory (saves/firmware). */
    external fun nativeLoadRom(romPath: String, dataDir: String): Boolean

    /** Return to the first-party synthetic demo (no ROM). */
    external fun nativeUnloadRom()

    /** Internal title of the loaded game, or empty when none is loaded. */
    external fun nativeGameTitle(): String
}
