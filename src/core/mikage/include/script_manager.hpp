#pragma once
#include <string>

#include "./helpers.hpp"

// The kinds of events that can cause a script callback.
// Frame: Call program on frame end
// TODO: Add more
enum class ScriptEvent {
	Frame,
};

class Emulator;

class ScriptManager {
  public:
	ScriptManager(Emulator& emulator) {}

	void close() {}
	void initialize() {}
	void loadFile(const char* path) {}
	void loadString(const std::string& code) {}

	void reset() {}
	void signalEvent(ScriptEvent e) {}

	bool signalInterceptedService(const std::string& service, u32 function, u32 messagePointer, int callbackRef) { return false; }
	void removeInterceptedService(const std::string& service, u32 function, int callbackRef) {}
};
