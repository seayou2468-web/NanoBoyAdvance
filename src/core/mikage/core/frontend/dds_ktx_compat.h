#pragma once

// Minimal dds-ktx compatibility declarations used by the core.
enum ddsktx_format {
    DDSKTX_FORMAT_RGBA8 = 0,
    DDSKTX_FORMAT_BC1,
    DDSKTX_FORMAT_BC3,
    DDSKTX_FORMAT_BC5,
    DDSKTX_FORMAT_BC7,
    DDSKTX_FORMAT_ASTC4x4,
    DDSKTX_FORMAT_ASTC6x6,
    DDSKTX_FORMAT_ASTC8x6,
};
