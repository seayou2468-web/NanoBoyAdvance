#ifdef PANDA3DS_ENABLE_HTTP_SERVER
#include "http_server.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "emulator.hpp"
#include "helpers.hpp"

namespace {
struct ParsedRequest {
	std::string path;
	std::map<std::string, std::string> params;
	bool valid = false;
};

std::string urlDecode(const std::string& in) {
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); i++) {
		if (in[i] == '%' && i + 2 < in.size()) {
			const auto hex = in.substr(i + 1, 2);
			char* end = nullptr;
			const long v = std::strtol(hex.c_str(), &end, 16);
			if (end && *end == '\0') {
				out.push_back(static_cast<char>(v));
				i += 2;
				continue;
			}
		}
		if (in[i] == '+') out.push_back(' ');
		else out.push_back(in[i]);
	}
	return out;
}

ParsedRequest parseRequest(const std::string& req) {
	ParsedRequest parsed{};
	std::istringstream ss(req);
	std::string method;
	std::string target;
	std::string version;
	ss >> method >> target >> version;
	if (method != "GET" || target.empty()) return parsed;

	auto q = target.find('?');
	parsed.path = target.substr(0, q);
	if (q != std::string::npos) {
		std::string query = target.substr(q + 1);
		std::stringstream qss(query);
		std::string pair;
		while (std::getline(qss, pair, '&')) {
			auto eq = pair.find('=');
			if (eq == std::string::npos) {
				parsed.params[urlDecode(pair)] = "";
			} else {
				parsed.params[urlDecode(pair.substr(0, eq))] = urlDecode(pair.substr(eq + 1));
			}
		}
	}
	parsed.valid = true;
	return parsed;
}

void sendHttpResponse(int fd, int status, const std::string& contentType, const char* data, size_t size) {
	std::ostringstream header;
	header << "HTTP/1.1 " << status << " " << (status == 200 ? "OK" : "Error") << "\r\n";
	header << "Content-Type: " << contentType << "\r\n";
	header << "Content-Length: " << size << "\r\n";
	header << "Connection: close\r\n\r\n";
	const std::string h = header.str();
	send(fd, h.data(), h.size(), 0);
	if (size > 0) send(fd, data, size, 0);
}

void sendHttpText(int fd, int status, const std::string& body) {
	sendHttpResponse(fd, status, "text/plain", body.data(), body.size());
}

}  // namespace

class HttpActionScreenshot : public HttpAction {
	DeferredHttpResponse& response;

  public:
	HttpActionScreenshot(DeferredHttpResponse& response) : HttpAction(HttpActionType::Screenshot), response(response) {}
	DeferredHttpResponse& getResponse() { return response; }
};

class HttpActionTogglePause : public HttpAction {
  public:
	HttpActionTogglePause() : HttpAction(HttpActionType::TogglePause) {}
};

class HttpActionReset : public HttpAction {
  public:
	HttpActionReset() : HttpAction(HttpActionType::Reset) {}
};

class HttpActionKey : public HttpAction {
	u32 key;
	bool state;

  public:
	HttpActionKey(u32 key, bool state) : HttpAction(HttpActionType::Key), key(key), state(state) {}

	u32 getKey() const { return key; }
	bool getState() const { return state; }
};

class HttpActionLoadRom : public HttpAction {
	DeferredHttpResponse& response;
	const std::filesystem::path& path;
	bool paused;

  public:
	HttpActionLoadRom(DeferredHttpResponse& response, const std::filesystem::path& path, bool paused)
		: HttpAction(HttpActionType::LoadRom), response(response), path(path), paused(paused) {}

	DeferredHttpResponse& getResponse() { return response; }
	const std::filesystem::path& getPath() const { return path; }
	bool getPaused() const { return paused; }
};

class HttpActionStep : public HttpAction {
	DeferredHttpResponse& response;
	int frames;

  public:
	HttpActionStep(DeferredHttpResponse& response, int frames)
		: HttpAction(HttpActionType::Step), response(response), frames(frames) {}

	DeferredHttpResponse& getResponse() { return response; }
	int getFrames() const { return frames; }
};

std::unique_ptr<HttpAction> HttpAction::createScreenshotAction(DeferredHttpResponse& response) {
	return std::make_unique<HttpActionScreenshot>(response);
}

std::unique_ptr<HttpAction> HttpAction::createKeyAction(u32 key, bool state) { return std::make_unique<HttpActionKey>(key, state); }
std::unique_ptr<HttpAction> HttpAction::createTogglePauseAction() { return std::make_unique<HttpActionTogglePause>(); }
std::unique_ptr<HttpAction> HttpAction::createResetAction() { return std::make_unique<HttpActionReset>(); }

std::unique_ptr<HttpAction> HttpAction::createLoadRomAction(DeferredHttpResponse& response, const std::filesystem::path& path, bool paused) {
	return std::make_unique<HttpActionLoadRom>(response, path, paused);
}

std::unique_ptr<HttpAction> HttpAction::createStepAction(DeferredHttpResponse& response, int frames) {
	return std::make_unique<HttpActionStep>(response, frames);
}

HttpServer::HttpServer(Emulator* emulator)
	: emulator(emulator), keyMap({
											   {"A", {HID::Keys::A}},
											   {"B", {HID::Keys::B}},
											   {"Select", {HID::Keys::Select}},
											   {"Start", {HID::Keys::Start}},
											   {"Right", {HID::Keys::Right}},
											   {"Left", {HID::Keys::Left}},
											   {"Up", {HID::Keys::Up}},
											   {"Down", {HID::Keys::Down}},
											   {"L", {HID::Keys::L}},
											   {"R", {HID::Keys::R}},
											   {"ZL", {HID::Keys::ZL}},
											   {"ZR", {HID::Keys::ZR}},
											   {"X", {HID::Keys::X}},
											   {"Y", {HID::Keys::Y}},
										   }) {
	httpServerThread = std::thread(&HttpServer::startHttpServer, this);
}

HttpServer::~HttpServer() {
	printf("Stopping http server...\n");
	running = false;
	if (listenFd >= 0) {
		shutdown(listenFd, SHUT_RDWR);
		close(listenFd);
		listenFd = -1;
	}
	if (httpServerThread.joinable()) {
		httpServerThread.join();
	}
}

void HttpServer::pushAction(std::unique_ptr<HttpAction> action) {
	std::scoped_lock lock(actionQueueMutex);
	actionQueue.push(std::move(action));
}

void HttpServer::startHttpServer() {
	listenFd = socket(AF_INET, SOCK_STREAM, 0);
	if (listenFd < 0) {
		printf("Failed to create HTTP server socket\n");
		return;
	}

	int opt = 1;
	setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1234);
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
		printf("Failed to bind HTTP server socket\n");
		close(listenFd);
		listenFd = -1;
		return;
	}

	if (listen(listenFd, 8) != 0) {
		printf("Failed to listen HTTP server socket\n");
		close(listenFd);
		listenFd = -1;
		return;
	}

	printf("Starting HTTP server on port 1234\n");

	while (running) {
		int client = accept(listenFd, nullptr, nullptr);
		if (client < 0) {
			if (!running) break;
			continue;
		}

		char buffer[8192]{};
		ssize_t n = recv(client, buffer, sizeof(buffer) - 1, 0);
		if (n <= 0) {
			close(client);
			continue;
		}
		buffer[n] = '\0';

		ParsedRequest request = parseRequest(std::string(buffer, static_cast<size_t>(n)));
		if (!request.valid) {
			sendHttpText(client, 400, "error");
			close(client);
			continue;
		}

		if (request.path == "/ping") {
			sendHttpText(client, 200, "pong");
		} else if (request.path == "/status") {
			sendHttpText(client, 200, status());
		} else if (request.path == "/togglepause") {
			pushAction(HttpAction::createTogglePauseAction());
			sendHttpText(client, 200, "ok");
		} else if (request.path == "/reset") {
			pushAction(HttpAction::createResetAction());
			sendHttpText(client, 200, "ok");
		} else if (request.path == "/input") {
			bool ok = true;
			for (auto& [keyStr, value] : request.params) {
				u32 key = stringToKey(keyStr);
				if (key == 0 || (value != "0" && value != "1")) {
					ok = false;
					break;
				}
				pushAction(HttpAction::createKeyAction(key, value == "1"));
			}
			sendHttpText(client, ok ? 200 : 400, ok ? "ok" : "error");
		} else if (request.path == "/screen") {
			DeferredHttpResponse response;
			std::unique_lock lock(response.mutex);
			pushAction(HttpAction::createScreenshotAction(response));
			response.cv.wait(lock, [&response] { return response.ready; });
			if (!response.binaryBody.empty()) {
				sendHttpResponse(client, response.status, response.contentType, response.binaryBody.data(), response.binaryBody.size());
			} else {
				sendHttpText(client, response.status, response.textBody);
			}
		} else if (request.path == "/step") {
			auto it = request.params.find("frames");
			if (it == request.params.end()) {
				sendHttpText(client, 400, "error");
			} else {
				int frames = 0;
				try { frames = std::stoi(it->second); } catch (...) { frames = 0; }
				if (frames <= 0) {
					sendHttpText(client, 400, "error");
				} else {
					DeferredHttpResponse response;
					std::unique_lock lock(response.mutex);
					pushAction(HttpAction::createStepAction(response, frames));
					response.cv.wait(lock, [&response] { return response.ready; });
					sendHttpText(client, response.status, response.textBody);
				}
			}
		} else if (request.path == "/load_rom") {
			auto it = request.params.find("path");
			if (it == request.params.end()) {
				sendHttpText(client, 400, "error");
			} else {
				std::filesystem::path romPath = it->second;
				std::error_code error;
				if (romPath.empty() || !std::filesystem::is_regular_file(romPath, error)) {
					sendHttpText(client, 400, "error");
				} else {
					bool reqPaused = false;
					auto p = request.params.find("paused");
					if (p != request.params.end()) reqPaused = (p->second == "1");
					DeferredHttpResponse response;
					std::unique_lock lock(response.mutex);
					pushAction(HttpAction::createLoadRomAction(response, romPath, reqPaused));
					response.cv.wait(lock, [&response] { return response.ready; });
					sendHttpText(client, response.status, response.textBody);
				}
			}
		} else {
			sendHttpText(client, 404, "error");
		}

		close(client);
	}
}

std::string HttpServer::status() {
	HIDService& hid = emulator->getServiceManager().getHID();
	std::stringstream stringStream;

	stringStream << "Panda3DS\n";
	stringStream << "Status: " << (paused ? "Paused" : "Running") << "\n";

	// TODO: This currently doesn't work for N3DS buttons
	auto keyPressed = [](const HIDService& hid, u32 mask) { return (hid.getOldButtons() & mask) != 0; };
	for (auto& [keyStr, value] : keyMap) {
		stringStream << keyStr << ": " << keyPressed(hid, value) << "\n";
	}

	return stringStream.str();
}

void HttpServer::processActions() {
	std::scoped_lock lock(actionQueueMutex);

	if (framesToRun > 0) {
		if (!currentStepAction) {
			printf("framesToRun > 0 but no currentStepAction\n");
			return;
		}

		emulator->resume();
		framesToRun--;

		if (framesToRun == 0) {
			paused = true;
			emulator->pause();

			DeferredHttpResponse& response = reinterpret_cast<HttpActionStep*>(currentStepAction.get())->getResponse();
			response.textBody = "ok";
			std::unique_lock<std::mutex> lock(response.mutex);
			response.ready = true;
			response.cv.notify_one();
		}

		return;
	}

	HIDService& hid = emulator->getServiceManager().getHID();

	while (!actionQueue.empty()) {
		std::unique_ptr<HttpAction> action = std::move(actionQueue.front());
		actionQueue.pop();

		switch (action->getType()) {
			case HttpActionType::Screenshot: {
				HttpActionScreenshot* screenshotAction = static_cast<HttpActionScreenshot*>(action.get());
				emulator->gpu.screenshot(httpServerScreenshotPath);
				std::ifstream file(httpServerScreenshotPath, std::ios::binary);
				std::vector<char> buffer(std::istreambuf_iterator<char>(file), {});

				DeferredHttpResponse& response = screenshotAction->getResponse();
				response.binaryBody = std::move(buffer);
				response.contentType = "image/png";
				std::unique_lock<std::mutex> lock(response.mutex);
				response.ready = true;
				response.cv.notify_one();
				break;
			}

			case HttpActionType::Key: {
				HttpActionKey* keyAction = static_cast<HttpActionKey*>(action.get());
				if (keyAction->getState()) {
					hid.pressKey(keyAction->getKey());
				} else {
					hid.releaseKey(keyAction->getKey());
				}
				break;
			}

			case HttpActionType::LoadRom: {
				HttpActionLoadRom* loadRomAction = static_cast<HttpActionLoadRom*>(action.get());
				DeferredHttpResponse& response = loadRomAction->getResponse();
				bool loaded = emulator->loadROM(loadRomAction->getPath());

				response.textBody = loaded ? "ok" : "error";

				std::unique_lock<std::mutex> lock(response.mutex);
				response.ready = true;
				response.cv.notify_one();

				if (loaded) {
					paused = loadRomAction->getPaused();
					framesToRun = 0;

					if (paused) {
						emulator->pause();
					} else {
						emulator->resume();
					}
				}
				break;
			}

			case HttpActionType::TogglePause: {
				if (paused) {
					emulator->resume();
					paused = false;
				} else {
					emulator->pause();
					paused = true;
				}
				break;
			}

			case HttpActionType::Reset: {
				emulator->reset();
				framesToRun = 0;
				paused = false;
				break;
			}

			case HttpActionType::Step: {
				HttpActionStep* stepAction = static_cast<HttpActionStep*>(action.get());
				if (!paused || framesToRun > 0) {
					DeferredHttpResponse& response = stepAction->getResponse();
					response.status = 400;
					response.textBody = "error";
					std::unique_lock<std::mutex> lock(response.mutex);
					response.ready = true;
					response.cv.notify_one();
				} else {
					framesToRun = stepAction->getFrames();
					currentStepAction = std::move(action);
				}
				break;
			}

			default: break;
		}
	}
}

u32 HttpServer::stringToKey(const std::string& key_name) {
	auto iterator = keyMap.find(key_name);
	if (iterator != keyMap.end()) {
		return iterator->second;
	}

	return 0;
}

#endif  // PANDA3DS_ENABLE_HTTP_SERVER
