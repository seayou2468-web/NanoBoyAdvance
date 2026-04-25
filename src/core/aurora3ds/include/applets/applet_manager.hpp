#pragma once

class Memory;

namespace Applets {

// Transitional compatibility shim:
// The legacy include-tree services interfaces still expect Applets::AppletManager
// to exist in this location. The modern aurora3ds HLE implementation keeps the
// actual manager under src/core/.../hle/service/apt. For now we provide a thin
// placeholder so CPU/HLE include chains can be connected during build integration.
class AppletManager {
public:
    explicit AppletManager(Memory&) {}
};

} // namespace Applets
