// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

namespace SRV {

// Legacy shim for old srv: interface code path.
class Interface {
public:
    Interface();
    ~Interface();

    /**
     * Gets the string name used by CTROS for the service
     * @return Port name of service
     */
    const char *GetPortName() const {
        return "srv:";
    }
};

} // namespace
