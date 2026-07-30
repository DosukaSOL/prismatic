// SPDX-License-Identifier: GPL-3.0-or-later
// JNI bridge for the PRISMATIC Android app.
//
// Runs the SAME deterministic C++ pipeline that is validated on desktop:
//   synthetic DS backend -> structured frame -> reconstruct -> light -> post.
// Each render call returns an int[] laid out as [width, height, ARGB pixels...]
// so the Kotlin side can build a Bitmap without a separate dimension query.
//
// This native path is what makes the app genuinely functional on-device without
// a ROM: it exercises the real enhancement code, not a mock. Real game graphics
// are only ever sourced from the user's own ROM via a real emulator adapter.
#include <jni.h>
#include <android/log.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "prismatic/adapter.hpp"
#include "prismatic/pipeline.hpp"
#include "prismatic/presets.hpp"
#include "prismatic/environment.hpp"
#include "prismatic/lighting.hpp"
#include "prismatic/emulator_present.hpp"
#include "synthetic_backend.hpp"
#include "nds_adapter.hpp"

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "prismatic", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "prismatic", __VA_ARGS__)

using namespace prismatic;

namespace {
std::mutex g_mtx;
std::unique_ptr<EmulatorAdapter> g_backend;
std::unique_ptr<PrismaticPipeline> g_pipe;
bool g_lantern = false;
bool g_isNds = false;         // true once a real DS ROM is loaded
InputState g_input;           // single source of truth, applied each frame
PresentationOptions g_present;// 2.5D / shader toggles for the real-ROM path
bool g_enableJit = true;      // applied on the next ROM (re)load
int  g_speed = 1;             // fast-forward multiplier (1, 2 or 5)

// Audio hand-off. The emulation thread (under g_mtx, inside nativeRenderTop)
// drains the core's SPU into g_audioBuf; the Kotlin AudioTrack thread pulls
// from it via nativeReadAudio. Only the emulation thread ever touches the core,
// so there is no cross-thread access to melonDS internals. g_audioMtx guards
// only the small PCM vector and is never held during a frame step.
std::mutex g_audioMtx;
std::vector<int16_t> g_audioBuf;                 // interleaved stereo s16
constexpr size_t kAudioMaxShorts = 8192 * 2;     // ~170 ms @48k, drop oldest past this

void configure(int presetIndex, float timeOfDay, int weather) {
    const auto names = presetNames();
    if (presetIndex < 0) presetIndex = 0;
    if (presetIndex >= static_cast<int>(names.size()))
        presetIndex = static_cast<int>(names.size()) - 1;
    g_pipe->setPresetByName(names[presetIndex]);

    EnvironmentState env;
    env.timeOfDay = timeOfDay;
    if (weather < 0) weather = 0;
    if (weather > 3) weather = 3;
    env.weather = static_cast<Weather>(weather);
    env.tag = LocationTag::Overworld;
    g_pipe->setEnvironment(env);

    g_pipe->clearLights();
    if (g_lantern) {
        Light lantern;
        lantern.type = Light::Point;
        lantern.pos = Vec2{128.0f, 96.0f};   // near the player (screen centre)
        lantern.height = 0.45f;
        lantern.color = Vec3{1.0f, 0.76f, 0.42f};
        lantern.intensity = 2.6f;
        lantern.radius = 96.0f;
        lantern.priority = 4;
        g_pipe->addLight(lantern);
    }
}

// Convert an RGBA Image to a Java int[] = [w, h, ARGB...].
jintArray toArgbArray(JNIEnv* env, const Image& img) {
    const int w = img.width, h = img.height;
    const size_t n = static_cast<size_t>(w) * static_cast<size_t>(h);
    jintArray out = env->NewIntArray(static_cast<jsize>(n + 2));
    if (!out) return nullptr;

    std::vector<jint> buf(n + 2);
    buf[0] = w;
    buf[1] = h;
    for (size_t i = 0; i < n; ++i) {
        const Color& c = img.pixels[i];
        buf[i + 2] = (static_cast<jint>(c.a) << 24) |
                     (static_cast<jint>(c.r) << 16) |
                     (static_cast<jint>(c.g) << 8) |
                     (static_cast<jint>(c.b));
    }
    env->SetIntArrayRegion(out, 0, static_cast<jsize>(n + 2), buf.data());
    return out;
}
}  // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_prismatic_app_NativeBridge_nativeInit(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    try {
        g_backend = makeSyntheticBackend();
        g_pipe = std::make_unique<PrismaticPipeline>();
        g_pipe->setPresetByName("HD-2.5D BALANCED");
        LOGI("prismatic native init ok");
        return JNI_TRUE;
    } catch (const std::exception& e) {
        LOGE("native init failed: %s", e.what());
        return JNI_FALSE;
    }
}

JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetLantern(JNIEnv*, jobject, jboolean on) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_lantern = (on == JNI_TRUE);
}

JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetTouch(JNIEnv*, jobject, jint x, jint y, jboolean down) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_input.touchActive = (down == JNI_TRUE);
    g_input.touchX = x;
    g_input.touchY = y;
}

// Full DS button input (touch is handled separately by nativeSetTouch). `mask`
// bit layout (shared with Kotlin NativeBridge):
//   0=A 1=B 2=X 3=Y 4=L 5=R 6=Start 7=Select 8=Up 9=Down 10=Left 11=Right
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetInput(JNIEnv*, jobject, jint mask) {
    std::lock_guard<std::mutex> lock(g_mtx);
    auto bit = [&](int i) { return (mask & (1 << i)) != 0; };
    g_input.a = bit(0);
    g_input.b = bit(1);
    g_input.x = bit(2);
    g_input.y = bit(3);
    g_input.l = bit(4);
    g_input.r = bit(5);
    g_input.start = bit(6);
    g_input.select = bit(7);
    g_input.up = bit(8);
    g_input.down = bit(9);
    g_input.left = bit(10);
    g_input.right = bit(11);
}

// Load a real Nintendo DS ROM. `dataDir` is a writable app directory used for
// battery saves + generated firmware. Returns false on failure (see logcat).
JNIEXPORT jboolean JNICALL
Java_com_prismatic_app_NativeBridge_nativeLoadRom(
        JNIEnv* env, jobject, jstring jrom, jstring jdata) {
    std::lock_guard<std::mutex> lock(g_mtx);
    const char* rom = env->GetStringUTFChars(jrom, nullptr);
    const char* data = env->GetStringUTFChars(jdata, nullptr);
    std::string romPath = rom ? rom : "";
    std::string dataDir = data ? data : "";
    if (rom) env->ReleaseStringUTFChars(jrom, rom);
    if (data) env->ReleaseStringUTFChars(jdata, data);

    {
        std::lock_guard<std::mutex> alock(g_audioMtx);
        g_audioBuf.clear();     // drop stale audio before replacing the backend
    }
    std::string err;
    auto adapter = makeNdsAdapter(romPath, dataDir, &err, g_enableJit);
    if (!adapter) {
        LOGE("ROM load failed: %s", err.c_str());
        return JNI_FALSE;
    }
    g_backend = std::move(adapter);
    g_isNds = true;
    g_input = InputState{};
    if (!g_pipe) g_pipe = std::make_unique<PrismaticPipeline>();
    LOGI("ROM loaded: %s (jit=%d)", romPath.c_str(), (int)g_enableJit);
    return JNI_TRUE;
}

// Return to the first-party synthetic demo backend (no ROM).
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeUnloadRom(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    {
        std::lock_guard<std::mutex> alock(g_audioMtx);
        g_audioBuf.clear();
    }
    g_backend = makeSyntheticBackend();
    g_isNds = false;
    g_input = InputState{};
}

// Force the loaded game's battery save to disk now (used by "Save & Close").
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeFlushSave(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (g_backend && g_isNds) g_backend->flushSave();
}

// Internal title of the loaded game (empty if none / synthetic).
JNIEXPORT jstring JNICALL
Java_com_prismatic_app_NativeBridge_nativeGameTitle(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend || !g_isNds) return env->NewStringUTF("");
    return env->NewStringUTF(g_backend->identity().title.c_str());
}

// Advance one frame and return the presented TOP screen as [w, h, ARGB...].
//
// Real DS ROMs use the faithful path: the raw core framebuffer, then the
// independent 2.5D and shader layers (whichever are enabled). This is fast and
// colour-accurate — no luminance-guessing reconstruction. The synthetic demo
// backend keeps using the structured enhancement pipeline.
JNIEXPORT jintArray JNICALL
Java_com_prismatic_app_NativeBridge_nativeRenderTop(
        JNIEnv* env, jobject, jint presetIndex, jfloat timeOfDay, jint weather) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend) return nullptr;
    g_backend->setInput(g_input);

    // Drain a frame's audio into the ring for the AudioTrack thread (NDS only).
    auto drainAudio = [&]() {
        int16_t tmp[2048];
        int f;
        while ((f = g_backend->readAudio(tmp, 1024)) > 0) {
            std::lock_guard<std::mutex> alock(g_audioMtx);
            g_audioBuf.insert(g_audioBuf.end(), tmp, tmp + (size_t)f * 2);
            if (g_audioBuf.size() > kAudioMaxShorts)
                g_audioBuf.erase(g_audioBuf.begin(),
                                 g_audioBuf.begin() + (g_audioBuf.size() - kAudioMaxShorts));
            if (f < 1024) break;
        }
    };

    // Fast-forward: run extra emulation steps per presented frame (NDS only). The
    // display loop stays at ~60 fps, so 2x/5x speed = 120/300 emulated fps.
    const int steps = g_isNds ? (g_speed < 1 ? 1 : g_speed) : 1;
    for (int s = 0; s < steps; ++s) {
        g_backend->advanceFrame();
        if (g_isNds) drainAudio();
    }

    if (g_isNds) {
        PresentationOptions o = g_present;
        Image fb = g_backend->framebuffer(static_cast<int>(ScreenId::Top));
        const FloatBuffer* depth = g_backend->depthBuffer(static_cast<int>(ScreenId::Top));
        return toArgbArray(env, renderEmulatorScreen(fb, depth, o));
    }

    if (!g_pipe) return nullptr;
    configure(presetIndex, timeOfDay, weather);
    RenderResult r = g_pipe->renderScreen(*g_backend, static_cast<int>(ScreenId::Top));
    return toArgbArray(env, r.enhanced);
}

// Render the current BOTTOM screen WITHOUT advancing the frame.
JNIEXPORT jintArray JNICALL
Java_com_prismatic_app_NativeBridge_nativeRenderBottom(
        JNIEnv* env, jobject, jint presetIndex, jfloat timeOfDay, jint weather) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend) return nullptr;

    if (g_isNds) {
        // Bottom screen is the DS touch UI (menus, map, text, HUD). Present it
        // faithfully: the shader grade, 2.5D depth and FXAA are TOP-screen only.
        // Applying them here overexposes the UI and smears text.
        PresentationOptions o;  // defaults = faithful passthrough
        Image fb = g_backend->framebuffer(static_cast<int>(ScreenId::Bottom));
        return toArgbArray(env, renderEmulatorScreen(fb, nullptr, o));
    }

    if (!g_pipe) return nullptr;
    configure(presetIndex, timeOfDay, weather);
    RenderResult r = g_pipe->renderScreen(*g_backend, static_cast<int>(ScreenId::Bottom));
    return toArgbArray(env, r.enhanced);
}

// Independent presentation layers for the real-ROM path. 2.5D (genuine per-pixel
// depth when the game renders 3D, else a geometric fallback), the shader overlay
// and FXAA edge smoothing are fully separable: any combination. `shaderParams`
// is a flat float[kShaderParamCount] in ShaderParams declaration order (see
// emulator_present.hpp), so the on-device editor can send any user-made look.
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetPresentation(
        JNIEnv* env, jobject, jboolean enable25D, jboolean enableShader,
        jfloat tilt, jboolean lantern, jboolean antialias, jfloatArray shaderParams) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_present.enable25D = (enable25D == JNI_TRUE);
    g_present.enableShader = (enableShader == JNI_TRUE);
    g_present.tilt = tilt;
    g_present.lantern = (lantern == JNI_TRUE);
    g_present.antialias = (antialias == JNI_TRUE);
    if (shaderParams && env->GetArrayLength(shaderParams) >= kShaderParamCount) {
        float buf[kShaderParamCount];
        env->GetFloatArrayRegion(shaderParams, 0, kShaderParamCount, buf);
        g_present.shader = shaderParamsFromArray(buf);
    }
}

// Fast-forward multiplier for the real-ROM path: 1x, 2x or 5x. Runs extra
// emulation steps per presented frame (audio is sped up / dropped accordingly).
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetSpeed(JNIEnv*, jobject, jint mult) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_speed = (mult == 2 || mult == 5) ? mult : 1;
}

// Number of built-in shader presets.
JNIEXPORT jint JNICALL
Java_com_prismatic_app_NativeBridge_nativeShaderPresetCount(JNIEnv*, jobject) {
    return static_cast<jint>(shaderPresetCount());
}

// Display name of a built-in shader preset.
JNIEXPORT jstring JNICALL
Java_com_prismatic_app_NativeBridge_nativeShaderPresetName(JNIEnv* env, jobject, jint index) {
    return env->NewStringUTF(shaderPresetName(index));
}

// A built-in preset's parameters as float[kShaderParamCount], for loading into
// the editor.
JNIEXPORT jfloatArray JNICALL
Java_com_prismatic_app_NativeBridge_nativeShaderPreset(JNIEnv* env, jobject, jint index) {
    float buf[kShaderParamCount];
    shaderParamsToArray(shaderPreset(index), buf);
    jfloatArray out = env->NewFloatArray(kShaderParamCount);
    if (out) env->SetFloatArrayRegion(out, 0, kShaderParamCount, buf);
    return out;
}

// 4-character cartridge code of the loaded ROM (empty if none). Used to look up
// per-game recommendations in the compatibility registry.
JNIEXPORT jstring JNICALL
Java_com_prismatic_app_NativeBridge_nativeGameCode(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend || !g_isNds) return env->NewStringUTF("");
    return env->NewStringUTF(g_backend->identity().gameCode.c_str());
}

// Number of shader parameters in the transport vector (keeps Kotlin in sync).
JNIEXPORT jint JNICALL
Java_com_prismatic_app_NativeBridge_nativeShaderParamCount(JNIEnv*, jobject) {
    return static_cast<jint>(kShaderParamCount);
}

// Speed mode: JIT on = fast, off = maximum compatibility. Applied on next load.
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeSetJit(JNIEnv*, jobject, jboolean on) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_enableJit = (on == JNI_TRUE);
}

// Pull decoded stereo PCM (interleaved s16 L,R) into `jbuf`. Returns the number
// of shorts written (== frames*2). Called from the AudioTrack thread.
JNIEXPORT jint JNICALL
Java_com_prismatic_app_NativeBridge_nativeReadAudio(JNIEnv* env, jobject, jshortArray jbuf) {
    if (!jbuf) return 0;
    const jsize cap = env->GetArrayLength(jbuf);
    if (cap <= 0) return 0;
    std::lock_guard<std::mutex> alock(g_audioMtx);
    jsize n = static_cast<jsize>(g_audioBuf.size());
    if (n > cap) n = cap;
    if (n <= 0) return 0;
    env->SetShortArrayRegion(jbuf, 0, n, reinterpret_cast<const jshort*>(g_audioBuf.data()));
    g_audioBuf.erase(g_audioBuf.begin(), g_audioBuf.begin() + n);
    return n;
}

JNIEXPORT jint JNICALL
Java_com_prismatic_app_NativeBridge_nativePresetCount(JNIEnv*, jobject) {
    return static_cast<jint>(presetNames().size());
}

JNIEXPORT jstring JNICALL
Java_com_prismatic_app_NativeBridge_nativePresetName(JNIEnv* env, jobject, jint index) {
    const auto names = presetNames();
    if (index < 0 || index >= static_cast<int>(names.size()))
        return env->NewStringUTF("");
    return env->NewStringUTF(names[index].c_str());
}

}  // extern "C"
