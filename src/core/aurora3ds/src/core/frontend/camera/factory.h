#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace Service::CAM {
struct Resolution;
enum class Flip : unsigned char;
enum class Effect : unsigned char;
enum class OutputFormat : unsigned char;
enum class FrameRate : unsigned char;
} // namespace Service::CAM

namespace Camera {

class CameraInterface {
public:
    virtual ~CameraInterface() = default;
    virtual void StartCapture() {}
    virtual void StopCapture() {}
    virtual std::vector<unsigned char> ReceiveFrame() { return {}; }
    virtual void SetFlip(Service::CAM::Flip) {}
    virtual void SetResolution(const Service::CAM::Resolution&) {}
    virtual void SetFrameRate(Service::CAM::FrameRate) {}
    virtual void SetEffect(Service::CAM::Effect) {}
    virtual void SetFormat(Service::CAM::OutputFormat) {}
};

inline std::unique_ptr<CameraInterface> CreateCamera(const std::string&, int) {
    return std::make_unique<CameraInterface>();
}

} // namespace Camera
