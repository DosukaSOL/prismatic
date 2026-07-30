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

    external fun nativeInit(): Boolean
    external fun nativeSetLantern(on: Boolean)
    external fun nativeSetTouch(x: Int, y: Int, down: Boolean)
    external fun nativeRenderTop(presetIndex: Int, timeOfDay: Float, weather: Int): IntArray?
    external fun nativeRenderBottom(presetIndex: Int, timeOfDay: Float, weather: Int): IntArray?
    external fun nativePresetCount(): Int
    external fun nativePresetName(index: Int): String
}
