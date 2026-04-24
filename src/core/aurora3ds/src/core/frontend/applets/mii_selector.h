#pragma once

#include <memory>
#include <string>
#include "core/hle/mii.h"

namespace Frontend {

struct MiiSelectorConfig {
    bool enable_cancel_button{};
    std::string title;
    unsigned int initially_selected_mii_index{};
};

struct MiiSelectorData {
    unsigned int return_code{};
    Mii::MiiData mii{};
};

class MiiSelector {
public:
    virtual ~MiiSelector() = default;
    virtual void Setup(const MiiSelectorConfig&) {}
    virtual MiiSelectorData ReceiveData() { return {}; }
};

} // namespace Frontend
