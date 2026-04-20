#pragma once

#include <string>

namespace jscore_runtime {

// Evaluates a JavaScript snippet and returns its string representation.
// Returns false and sets error_message if JavaScriptCore is unavailable or evaluation fails.
bool EvaluateScriptToString(const std::string& script, std::string& out_string, std::string& error_message);

}  // namespace jscore_runtime
