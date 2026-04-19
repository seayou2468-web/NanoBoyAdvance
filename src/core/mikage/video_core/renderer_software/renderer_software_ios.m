#include <stddef.h>
#include <stdint.h>

void MikageConvertBGR24ToRGBA8888Flipped(const uint8_t* src,
                                         uint8_t* dst,
                                         size_t width,
                                         size_t height,
                                         size_t dst_stride_pixels) {
    if (!src || !dst || width == 0 || height == 0 || dst_stride_pixels == 0) {
        return;
    }

    size_t in_coord = 0;
    for (size_t x = 0; x < width; ++x) {
        for (size_t y = height; y > 0; --y) {
            size_t out_y = y - 1;
            size_t out_coord = (out_y * dst_stride_pixels + x) * 4;
            dst[out_coord + 0] = src[in_coord + 2];
            dst[out_coord + 1] = src[in_coord + 1];
            dst[out_coord + 2] = src[in_coord + 0];
            dst[out_coord + 3] = 0xFF;
            in_coord += 3;
        }
    }
}

