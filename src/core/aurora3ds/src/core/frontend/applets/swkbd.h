#pragma once

#include <memory>
#include <string>
#include <vector>

namespace Frontend {

enum class ButtonConfig { SingleButton = 0, DualButton = 1, TripleButton = 2, NoButton = 3 };
enum class AcceptedInput { Anything = 0, NotEmpty = 1, NotEmptyNotBlank = 2, NotBlank = 3, FixedLen = 4 };

struct KeyboardFilters {
    bool prevent_digit{};
    bool prevent_at{};
    bool prevent_percent{};
    bool prevent_backslash{};
    bool prevent_profanity{};
    bool enable_callback{};
};

struct KeyboardConfig {
    ButtonConfig button_config{ButtonConfig::SingleButton};
    AcceptedInput accept_mode{AcceptedInput::Anything};
    bool multiline_mode{};
    int max_text_length{};
    int max_digits{};
    std::string hint_text;
    std::vector<std::string> button_text;
    KeyboardFilters filters{};
};

struct KeyboardData {
    std::string text;
    int button{};
};

class SoftwareKeyboard {
public:
    virtual ~SoftwareKeyboard() = default;
    virtual void Execute(const KeyboardConfig&) {}
    virtual bool DataReady() const { return false; }
    virtual KeyboardData ReceiveData() { return {}; }
    virtual void ShowError(const std::string&) {}
};

} // namespace Frontend
