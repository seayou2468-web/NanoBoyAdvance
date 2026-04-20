#include "./include/jscore_runtime.hpp"

#if defined(__APPLE__) && __has_include(<JavaScriptCore/JavaScript.h>)
#define NBA_HAS_JSCORE 1
#include <JavaScriptCore/JavaScript.h>
#else
#define NBA_HAS_JSCORE 0
#endif

namespace jscore_runtime {

#if NBA_HAS_JSCORE
namespace {

std::string JSStringToUtf8(JSStringRef value) {
  if (!value) {
    return {};
  }

  const size_t max_size = JSStringGetMaximumUTF8CStringSize(value);
  std::string buffer(max_size, '\0');
  const size_t written = JSStringGetUTF8CString(value, buffer.data(), max_size);
  if (written == 0) {
    return {};
  }
  buffer.resize(written - 1);
  return buffer;
}

}  // namespace
#endif

bool EvaluateScriptToString(const std::string& script, std::string& out_string, std::string& error_message) {
#if NBA_HAS_JSCORE
  JSGlobalContextRef context = JSGlobalContextCreate(nullptr);
  if (!context) {
    error_message = "JavaScriptCore context creation failed";
    return false;
  }

  JSStringRef source = JSStringCreateWithUTF8CString(script.c_str());
  JSValueRef exception = nullptr;
  JSValueRef result = JSEvaluateScript(context, source, nullptr, nullptr, 1, &exception);
  JSStringRelease(source);

  if (exception != nullptr) {
    JSStringRef exception_string = JSValueToStringCopy(context, exception, nullptr);
    error_message = JSStringToUtf8(exception_string);
    JSStringRelease(exception_string);
    JSGlobalContextRelease(context);
    return false;
  }

  JSStringRef result_string = JSValueToStringCopy(context, result, nullptr);
  out_string = JSStringToUtf8(result_string);
  JSStringRelease(result_string);
  JSGlobalContextRelease(context);
  return true;
#else
  (void)script;
  (void)out_string;
  error_message = "JavaScriptCore is unavailable on this build target";
  return false;
#endif
}

}  // namespace jscore_runtime
