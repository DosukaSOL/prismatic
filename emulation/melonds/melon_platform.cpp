// SPDX-License-Identifier: GPL-3.0-or-later
// PRISMATIC — melonDS Platform implementation.
//
// melonDS delegates every OS service (files, threads, timing, logging, save
// persistence) to the embedder through melonDS::Platform. This is the minimal
// but complete implementation for a plain-DS build: real file I/O + threading +
// timing + save writeback, and safe no-ops for features PRISMATIC does not use
// yet (local multiplayer, ethernet, camera, DSi AAC, GBA addons, dynamic libs).
//
// No copyrighted BIOS/firmware is required: NDSArgs defaults to melonDS' own
// FreeBIOS + generated firmware, so none of the BIOS file paths are hit.

#include "Platform.h"

#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <string>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>

#if defined(__ANDROID__)
#include <android/log.h>
#endif

#include "melon_platform.hpp"

namespace prismatic::melon {

// Global hooks the adapter configures before booting a ROM.
static std::string g_baseDir = ".";
static SaveSink g_ndsSaveSink = nullptr;
static void* g_saveUserdata = nullptr;

void setBaseDir(const std::string& dir) { g_baseDir = dir; }
void setNdsSaveSink(SaveSink sink, void* userdata) {
    g_ndsSaveSink = sink;
    g_saveUserdata = userdata;
}

}  // namespace prismatic::melon

namespace melonDS::Platform {

namespace {

std::string resolveLocal(const std::string& name) {
    if (!name.empty() && (name[0] == '/' )) return name;  // already absolute
    return prismatic::melon::g_baseDir + "/" + name;
}

const char* fopenMode(FileMode mode) {
    bool read = (mode & Read) != 0;
    bool write = (mode & Write) != 0;
    bool preserve = (mode & Preserve) != 0;
    bool append = (mode & Append) != 0;
    bool text = (mode & Text) != 0;
    // Choose the stdio mode string that matches the requested semantics.
    const char* base;
    if (append)                 base = read ? "a+" : "a";
    else if (read && write)     base = preserve ? "r+" : "w+";
    else if (write)             base = "w";
    else                        base = "r";
    // Binary is the default for the DS core; only opt into text when asked.
    static thread_local char buf[8];
    std::snprintf(buf, sizeof(buf), "%s%s", base, text ? "" : "b");
    return buf;
}

}  // namespace

void SignalStop(StopReason, void*) {}

std::string GetLocalFilePath(const std::string& filename) { return resolveLocal(filename); }

FileHandle* OpenFile(const std::string& path, FileMode mode) {
    if ((mode & (Read | Write)) == 0) return nullptr;
    if ((mode & NoCreate) && (mode & Write)) {
        FILE* probe = std::fopen(path.c_str(), "rb");
        if (!probe) return nullptr;
        std::fclose(probe);
    }
    FILE* fp = std::fopen(path.c_str(), fopenMode(mode));
    return reinterpret_cast<FileHandle*>(fp);
}

FileHandle* OpenLocalFile(const std::string& path, FileMode mode) {
    return OpenFile(resolveLocal(path), mode);
}

bool FileExists(const std::string& name) {
    FILE* fp = std::fopen(name.c_str(), "rb");
    if (!fp) return false;
    std::fclose(fp);
    return true;
}
bool LocalFileExists(const std::string& name) { return FileExists(resolveLocal(name)); }

bool CheckFileWritable(const std::string& filepath) {
    FILE* fp = std::fopen(filepath.c_str(), "ab");
    if (!fp) return false;
    std::fclose(fp);
    return true;
}
bool CheckLocalFileWritable(const std::string& filepath) { return CheckFileWritable(resolveLocal(filepath)); }

bool CloseFile(FileHandle* file) { return std::fclose(reinterpret_cast<FILE*>(file)) == 0; }
bool IsEndOfFile(FileHandle* file) { return std::feof(reinterpret_cast<FILE*>(file)) != 0; }

bool FileReadLine(char* str, int count, FileHandle* file) {
    return std::fgets(str, count, reinterpret_cast<FILE*>(file)) != nullptr;
}
u64 FilePosition(FileHandle* file) { return (u64)std::ftell(reinterpret_cast<FILE*>(file)); }

bool FileSeek(FileHandle* file, s64 offset, FileSeekOrigin origin) {
    int o = SEEK_SET;
    if (origin == FileSeekOrigin::Current) o = SEEK_CUR;
    else if (origin == FileSeekOrigin::End) o = SEEK_END;
    return std::fseek(reinterpret_cast<FILE*>(file), (long)offset, o) == 0;
}
void FileRewind(FileHandle* file) { std::rewind(reinterpret_cast<FILE*>(file)); }

u64 FileRead(void* data, u64 size, u64 count, FileHandle* file) {
    return (u64)std::fread(data, (size_t)size, (size_t)count, reinterpret_cast<FILE*>(file));
}
bool FileFlush(FileHandle* file) { return std::fflush(reinterpret_cast<FILE*>(file)) == 0; }
u64 FileWrite(const void* data, u64 size, u64 count, FileHandle* file) {
    return (u64)std::fwrite(data, (size_t)size, (size_t)count, reinterpret_cast<FILE*>(file));
}
u64 FileWriteFormatted(FileHandle* file, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = std::vfprintf(reinterpret_cast<FILE*>(file), fmt, args);
    va_end(args);
    return n < 0 ? 0 : (u64)n;
}
u64 FileLength(FileHandle* file) {
    FILE* fp = reinterpret_cast<FILE*>(file);
    long cur = std::ftell(fp);
    if (std::fseek(fp, 0, SEEK_END) != 0) return 0;
    long len = std::ftell(fp);
    std::fseek(fp, cur, SEEK_SET);
    return len < 0 ? 0 : (u64)len;
}

void Log(LogLevel level, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
#if defined(__ANDROID__)
    int prio = ANDROID_LOG_INFO;
    switch (level) {
        case Debug: prio = ANDROID_LOG_DEBUG; break;
        case Info:  prio = ANDROID_LOG_INFO;  break;
        case Warn:  prio = ANDROID_LOG_WARN;  break;
        case Error: prio = ANDROID_LOG_ERROR; break;
    }
    __android_log_vprint(prio, "melonDS", fmt, args);
#else
    (void)level;
    std::vfprintf(stderr, fmt, args);
#endif
    va_end(args);
}

// ---- Threading -----------------------------------------------------------
struct Thread { std::thread t; };
Thread* Thread_Create(std::function<void()> func) { return new Thread{std::thread(std::move(func))}; }
void Thread_Free(Thread* thread) {
    if (thread->t.joinable()) thread->t.join();
    delete thread;
}
void Thread_Wait(Thread* thread) { if (thread->t.joinable()) thread->t.join(); }

struct Semaphore {
    std::mutex m;
    std::condition_variable cv;
    int count = 0;
};
Semaphore* Semaphore_Create() { return new Semaphore(); }
void Semaphore_Free(Semaphore* sema) { delete sema; }
void Semaphore_Reset(Semaphore* sema) {
    std::lock_guard<std::mutex> lk(sema->m);
    sema->count = 0;
}
void Semaphore_Wait(Semaphore* sema) {
    std::unique_lock<std::mutex> lk(sema->m);
    sema->cv.wait(lk, [&] { return sema->count > 0; });
    --sema->count;
}
bool Semaphore_TryWait(Semaphore* sema, int timeout_ms) {
    std::unique_lock<std::mutex> lk(sema->m);
    if (sema->count == 0) {
        if (timeout_ms <= 0) return false;
        if (!sema->cv.wait_for(lk, std::chrono::milliseconds(timeout_ms),
                               [&] { return sema->count > 0; }))
            return false;
    }
    --sema->count;
    return true;
}
void Semaphore_Post(Semaphore* sema, int count) {
    std::lock_guard<std::mutex> lk(sema->m);
    sema->count += count;
    for (int i = 0; i < count; ++i) sema->cv.notify_one();
}

struct Mutex { std::mutex m; };
Mutex* Mutex_Create() { return new Mutex(); }
void Mutex_Free(Mutex* mutex) { delete mutex; }
void Mutex_Lock(Mutex* mutex) { mutex->m.lock(); }
void Mutex_Unlock(Mutex* mutex) { mutex->m.unlock(); }
bool Mutex_TryLock(Mutex* mutex) { return mutex->m.try_lock(); }

void Sleep(u64 usecs) { std::this_thread::sleep_for(std::chrono::microseconds(usecs)); }

static const auto g_epoch = std::chrono::steady_clock::now();
u64 GetMSCount() {
    return (u64)std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - g_epoch).count();
}
u64 GetUSCount() {
    return (u64)std::chrono::duration_cast<std::chrono::microseconds>(
               std::chrono::steady_clock::now() - g_epoch).count();
}

// ---- Save / firmware persistence ----------------------------------------
void WriteNDSSave(const u8* savedata, u32 savelen, u32 /*writeoffset*/, u32 /*writelen*/, void*) {
    if (prismatic::melon::g_ndsSaveSink)
        prismatic::melon::g_ndsSaveSink(savedata, savelen, prismatic::melon::g_saveUserdata);
}
void WriteGBASave(const u8*, u32, u32, u32, void*) {}
void WriteFirmware(const Firmware&, u32, u32, void*) {}
void WriteDateTime(int, int, int, int, int, int, void*) {}

// ---- Local multiplayer (unused) -----------------------------------------
void MP_Begin(void*) {}
void MP_End(void*) {}
int MP_SendPacket(u8*, int, u64, void*) { return 0; }
int MP_RecvPacket(u8*, u64*, void*) { return 0; }
int MP_SendCmd(u8*, int, u64, void*) { return 0; }
int MP_SendReply(u8*, int, u64, u16, void*) { return 0; }
int MP_SendAck(u8*, int, u64, void*) { return 0; }
int MP_RecvHostPacket(u8*, u64*, void*) { return 0; }
u16 MP_RecvReplies(u8*, u64, u16, void*) { return 0; }

// ---- Ethernet (unused) ---------------------------------------------------
int Net_SendPacket(u8*, int, void*) { return 0; }
int Net_RecvPacket(u8*, void*) { return 0; }

// ---- Camera (unused) -----------------------------------------------------
void Camera_Start(int, void*) {}
void Camera_Stop(int, void*) {}
void Camera_CaptureFrame(int, u32* frame, int width, int height, bool, void*) {
    if (frame) std::memset(frame, 0, (size_t)width * height * sizeof(u32));
}

// ---- Microphone (silence) ------------------------------------------------
void Mic_Start(void*) {}
void Mic_Stop(void*) {}
int Mic_ReadInput(s16*, int, void*) { return 0; }

// ---- DSi AAC (unused for DS) --------------------------------------------
AACDecoder* AAC_Init() { return nullptr; }
void AAC_DeInit(AACDecoder*) {}
bool AAC_Configure(AACDecoder*, int, int) { return false; }
bool AAC_DecodeFrame(AACDecoder*, const void*, int, void*, int) { return false; }

// ---- GBA-slot addons (unused) -------------------------------------------
bool Addon_KeyDown(KeyType, void*) { return false; }
void Addon_RumbleStart(u32, void*) {}
void Addon_RumbleStop(void*) {}
float Addon_MotionQuery(MotionQueryType, void*) { return 0.0f; }

// ---- Dynamic libraries (unused with software renderer) -------------------
DynamicLibrary* DynamicLibrary_Load(const char*) { return nullptr; }
void DynamicLibrary_Unload(DynamicLibrary*) {}
void* DynamicLibrary_LoadFunction(DynamicLibrary*, const char*) { return nullptr; }

}  // namespace melonDS::Platform
