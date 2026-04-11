#include "Platform.h"

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace Platform {

struct ThreadImpl {
  std::thread worker;
};

struct SemaphoreImpl {
  std::mutex lock;
  std::condition_variable cv;
  int count = 0;
};

struct MutexImpl {
  std::mutex lock;
};

namespace {

std::mutex g_config_lock;
std::unordered_map<int, int> g_config_int;
std::unordered_map<int, bool> g_config_bool;
std::unordered_map<int, std::string> g_config_string;
std::unordered_map<int, std::vector<u8>> g_config_array;

}  // namespace

void SetConfigInt(ConfigEntry entry, int value) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  g_config_int[entry] = value;
}

void SetConfigBool(ConfigEntry entry, bool value) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  g_config_bool[entry] = value;
}

void SetConfigString(ConfigEntry entry, std::string value) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  g_config_string[entry] = std::move(value);
}

void SetConfigArray(ConfigEntry entry, std::vector<u8> value) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  g_config_array[entry] = std::move(value);
}

void Init(int, char**) {}
void DeInit() {}

void StopEmu() {}

int InstanceID() { return 0; }
std::string InstanceFileSuffix() { return ""; }

int GetConfigInt(ConfigEntry entry) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  auto it = g_config_int.find(entry);
  return it == g_config_int.end() ? 0 : it->second;
}

bool GetConfigBool(ConfigEntry entry) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  auto it = g_config_bool.find(entry);
  return it != g_config_bool.end() && it->second;
}

std::string GetConfigString(ConfigEntry entry) {
  std::lock_guard<std::mutex> guard(g_config_lock);
  auto it = g_config_string.find(entry);
  return it == g_config_string.end() ? std::string() : it->second;
}

bool GetConfigArray(ConfigEntry entry, void* data) {
  if (data == nullptr) {
    return false;
  }
  std::lock_guard<std::mutex> guard(g_config_lock);
  auto it = g_config_array.find(entry);
  if (it == g_config_array.end()) {
    return false;
  }
  std::memcpy(data, it->second.data(), it->second.size());
  return true;
}

FILE* OpenFile(std::string path, std::string mode, bool mustexist) {
  if (mustexist && !std::filesystem::exists(path)) {
    return nullptr;
  }
  return std::fopen(path.c_str(), mode.c_str());
}

FILE* OpenLocalFile(std::string path, std::string mode) {
  return OpenFile(std::move(path), std::move(mode), false);
}

FILE* OpenDataFile(std::string path) {
  return OpenFile(std::move(path), "rb", true);
}

Thread* Thread_Create(std::function<void()> func) {
  auto* thread = new ThreadImpl();
  thread->worker = std::thread([func = std::move(func)]() { func(); });
  return reinterpret_cast<Thread*>(thread);
}

void Thread_Free(Thread* thread) {
  auto* impl = reinterpret_cast<ThreadImpl*>(thread);
  if (impl == nullptr) {
    return;
  }
  if (impl->worker.joinable()) {
    impl->worker.join();
  }
  delete impl;
}

void Thread_Wait(Thread* thread) {
  auto* impl = reinterpret_cast<ThreadImpl*>(thread);
  if (impl != nullptr && impl->worker.joinable()) {
    impl->worker.join();
  }
}

Semaphore* Semaphore_Create() {
  return reinterpret_cast<Semaphore*>(new SemaphoreImpl());
}

void Semaphore_Free(Semaphore* sema) {
  delete reinterpret_cast<SemaphoreImpl*>(sema);
}

void Semaphore_Reset(Semaphore* sema) {
  auto* impl = reinterpret_cast<SemaphoreImpl*>(sema);
  if (impl == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> guard(impl->lock);
  impl->count = 0;
}

void Semaphore_Wait(Semaphore* sema) {
  auto* impl = reinterpret_cast<SemaphoreImpl*>(sema);
  if (impl == nullptr) {
    return;
  }
  std::unique_lock<std::mutex> guard(impl->lock);
  impl->cv.wait(guard, [&]() { return impl->count > 0; });
  --impl->count;
}

void Semaphore_Post(Semaphore* sema, int count) {
  auto* impl = reinterpret_cast<SemaphoreImpl*>(sema);
  if (impl == nullptr || count <= 0) {
    return;
  }
  {
    std::lock_guard<std::mutex> guard(impl->lock);
    impl->count += count;
  }
  impl->cv.notify_all();
}

Mutex* Mutex_Create() {
  return reinterpret_cast<Mutex*>(new MutexImpl());
}

void Mutex_Free(Mutex* mutex) {
  delete reinterpret_cast<MutexImpl*>(mutex);
}

void Mutex_Lock(Mutex* mutex) {
  auto* impl = reinterpret_cast<MutexImpl*>(mutex);
  if (impl != nullptr) {
    impl->lock.lock();
  }
}

void Mutex_Unlock(Mutex* mutex) {
  auto* impl = reinterpret_cast<MutexImpl*>(mutex);
  if (impl != nullptr) {
    impl->lock.unlock();
  }
}

bool Mutex_TryLock(Mutex* mutex) {
  auto* impl = reinterpret_cast<MutexImpl*>(mutex);
  return impl != nullptr && impl->lock.try_lock();
}

void Sleep(u64 usecs) {
  std::this_thread::sleep_for(std::chrono::microseconds(usecs));
}

void WriteNDSSave(const u8*, u32, u32, u32) {}
void WriteGBASave(const u8*, u32, u32, u32) {}

bool MP_Init() { return false; }
void MP_DeInit() {}
void MP_Begin() {}
void MP_End() {}
int MP_SendPacket(u8*, int, u64) { return 0; }
int MP_RecvPacket(u8*, u64*) { return 0; }
int MP_SendCmd(u8*, int, u64) { return 0; }
int MP_SendReply(u8*, int, u64, u16) { return 0; }
int MP_SendAck(u8*, int, u64) { return 0; }
int MP_RecvHostPacket(u8*, u64*) { return 0; }
u16 MP_RecvReplies(u8*, u64, u16) { return 0; }

bool LAN_Init() { return false; }
void LAN_DeInit() {}
int LAN_SendPacket(u8*, int) { return 0; }
int LAN_RecvPacket(u8*) { return 0; }

void Camera_Start(int) {}
void Camera_Stop(int) {}
void Camera_CaptureFrame(int, u32* frame, int width, int height, bool) {
  if (frame == nullptr) {
    return;
  }
  std::fill_n(frame, static_cast<size_t>(width * height / 2), 0U);
}

void Mic_Prepare() {}

}  // namespace Platform
