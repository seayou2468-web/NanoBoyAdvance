#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>

namespace {

constexpr std::size_t kWidth = 240;
constexpr std::size_t kHeight = 160;
constexpr std::size_t kBytesPerPixel = sizeof(std::uint32_t);
constexpr std::size_t kTightBytesPerRow = kWidth * kBytesPerPixel; // 960
constexpr std::size_t kMetalBlitRowAlignment = 256;

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

} // namespace

int main() {
  std::printf("=== iOS framebuffer upload repro (Linux) ===\n");
  std::printf("frame: %zux%zu, pixel=%zu bytes, tight row=%zu bytes\n",
              kWidth, kHeight, kBytesPerPixel, kTightBytesPerRow);

  const bool tight_is_aligned_256 = (kTightBytesPerRow % kMetalBlitRowAlignment) == 0;
  std::printf("tight row %% 256 = %zu -> %s\n",
              kTightBytesPerRow % kMetalBlitRowAlignment,
              tight_is_aligned_256 ? "aligned" : "NOT aligned");

  const std::size_t padded_bytes_per_row = AlignUp(kTightBytesPerRow, kMetalBlitRowAlignment);
  std::printf("recommended padded row = %zu bytes\n", padded_bytes_per_row);

  // Synthetic framebuffer with deterministic per-pixel pattern.
  std::vector<std::uint32_t> src(kWidth * kHeight);
  for (std::size_t y = 0; y < kHeight; ++y) {
    for (std::size_t x = 0; x < kWidth; ++x) {
      src[y * kWidth + x] = 0xFF000000u | ((x & 0xFFu) << 16) | ((y & 0xFFu) << 8) | ((x ^ y) & 0xFFu);
    }
  }

  // Simulate Metal staging buffer with padded row copy (what iOS side now does).
  std::vector<std::uint8_t> staging(padded_bytes_per_row * kHeight, 0xCD);
  for (std::size_t y = 0; y < kHeight; ++y) {
    const auto* row_src = reinterpret_cast<const std::uint8_t*>(src.data()) + y * kTightBytesPerRow;
    auto* row_dst = staging.data() + y * padded_bytes_per_row;
    std::memcpy(row_dst, row_src, kTightBytesPerRow);
  }

  // Read back only tight row payload and verify lossless transfer.
  std::vector<std::uint32_t> roundtrip(kWidth * kHeight);
  for (std::size_t y = 0; y < kHeight; ++y) {
    const auto* row_src = staging.data() + y * padded_bytes_per_row;
    auto* row_dst = reinterpret_cast<std::uint8_t*>(roundtrip.data()) + y * kTightBytesPerRow;
    std::memcpy(row_dst, row_src, kTightBytesPerRow);
  }

  std::size_t mismatches = 0;
  for (std::size_t i = 0; i < src.size(); ++i) {
    if (src[i] != roundtrip[i]) {
      ++mismatches;
      if (mismatches < 5) {
        std::printf("mismatch at pixel %zu: %08X != %08X\n", i, src[i], roundtrip[i]);
      }
    }
  }

  if (mismatches == 0) {
    std::printf("roundtrip check: PASS\n");
  } else {
    std::printf("roundtrip check: FAIL (%zu mismatches)\n", mismatches);
    return 1;
  }

  std::printf("\nConclusion: 240x160x4 uses 960-byte rows; padded staging is required to satisfy 256-byte row alignment.\n");
  return 0;
}
