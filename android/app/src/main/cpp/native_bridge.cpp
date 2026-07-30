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

    std::string err;
    auto adapter = makeNdsAdapter(romPath, dataDir, &err);
    if (!adapter) {
        LOGE("ROM load failed: %s", err.c_str());
        return JNI_FALSE;
    }
    g_backend = std::move(adapter);
    g_isNds = true;
    g_input = InputState{};
    if (!g_pipe) g_pipe = std::make_unique<PrismaticPipeline>();
    LOGI("ROM loaded: %s", romPath.c_str());
    return JNI_TRUE;
}

// Return to the first-party synthetic demo backend (no ROM).
JNIEXPORT void JNICALL
Java_com_prismatic_app_NativeBridge_nativeUnloadRom(JNIEnv*, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    g_backend = makeSyntheticBackend();
    g_isNds = false;
    g_input = InputState{};
}

// Internal title of the loaded game (empty if none / synthetic).
JNIEXPORT jstring JNICALL
Java_com_prismatic_app_NativeBridge_nativeGameTitle(JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend || !g_isNds) return env->NewStringUTF("");
    return env->NewStringUTF(g_backend->identity().title.c_str());
}

// Advance one frame and return the enhanced TOP screen as [w, h, ARGB...].
JNIEXPORT jintArray JNICALL
Java_com_prismatic_app_NativeBridge_nativeRenderTop(
        JNIEnv* env, jobject, jint presetIndex, jfloat timeOfDay, jint weather) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend || !g_pipe) return nullptr;
    configure(presetIndex, timeOfDay, weather);
    g_backend->setInput(g_input);
    g_backend->advanceFrame();
    RenderResult r = g_pipe->renderScreen(*g_backend, static_cast<int>(ScreenId::Top));
    return toArgbArray(env, r.enhanced);
}

// Render the current BOTTOM screen (touch UI) WITHOUT advancing the frame.
JNIEXPORT jintArray JNICALL
Java_com_prismatic_app_NativeBridge_nativeRenderBottom(
        JNIEnv* env, jobject, jint presetIndex, jfloat timeOfDay, jint weather) {
    std::lock_guard<std::mutex> lock(g_mtx);
    if (!g_backend || !g_pipe) return nullptr;
    configure(presetIndex, timeOfDay, weather);
    RenderResult r = g_pipe->renderScreen(*g_backend, static_cast<int>(ScreenId::Bottom));
    return toArgbArray(env, r.enhanced);
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
