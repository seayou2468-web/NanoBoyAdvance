#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include "helpers.hpp"

enum class HttpActionType { None, Screenshot, Key, TogglePause, Reset, LoadRom, Step };

class Emulator;

// Wrapper that allows the HTTP server thread to wait for emulator-side action completion
struct DeferredHttpResponse {
	std::mutex mutex;
	std::condition_variable cv;
	bool ready = false;
	int status = 200;
	std::string contentType = "text/plain";
	std::string textBody{};
	std::vector<char> binaryBody{};
};

// Actions derive from this class and are used to communicate with the HTTP server
class HttpAction {
	HttpActionType type;

  public:
	HttpAction(HttpActionType type) : type(type) {}
	virtual ~HttpAction() = default;

	HttpActionType getType() const { return type; }

	static std::unique_ptr<HttpAction> createScreenshotAction(DeferredHttpResponse& response);
	static std::unique_ptr<HttpAction> createKeyAction(u32 key, bool state);
	static std::unique_ptr<HttpAction> createLoadRomAction(DeferredHttpResponse& response, const std::filesystem::path& path, bool paused);
	static std::unique_ptr<HttpAction> createTogglePauseAction();
	static std::unique_ptr<HttpAction> createResetAction();
	static std::unique_ptr<HttpAction> createStepAction(DeferredHttpResponse& response, int frames);
};

struct HttpServer {
	HttpServer(Emulator* emulator);
	~HttpServer();
	void processActions();

  private:
	static constexpr const char* httpServerScreenshotPath = "screenshot.png";

	Emulator* emulator;
	std::atomic<bool> running{true};
	int listenFd = -1;

	std::thread httpServerThread;
	std::queue<std::unique_ptr<HttpAction>> actionQueue;
	std::mutex actionQueueMutex;
	std::unique_ptr<HttpAction> currentStepAction{};

	std::map<std::string, u32> keyMap;
	bool paused = false;
	int framesToRun = 0;

	void startHttpServer();
	void pushAction(std::unique_ptr<HttpAction> action);
	std::string status();
	u32 stringToKey(const std::string& key_name);

	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;
};

#endif  // PANDA3DS_ENABLE_HTTP_SERVER
