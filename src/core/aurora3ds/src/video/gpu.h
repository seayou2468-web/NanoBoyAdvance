#ifndef VIDEO_GPU_H
#define VIDEO_GPU_H

#include "common.h"

typedef struct {
    u8 _opaque;
} GPU;

static inline void gpu_invalidate_range(GPU* gpu, u32 paddr, u32 size) { (void)gpu; (void)paddr; (void)size; }
static inline void gpu_render_lcd_fb(GPU* gpu, u32 paddr, u32 fmt, u32 screen) { (void)gpu; (void)paddr; (void)fmt; (void)screen; }
static inline void gpu_reset_needs_rehesh(GPU* gpu) { (void)gpu; }
static inline void gpu_run_command_list(GPU* gpu, u32 paddr, u32 size) { (void)gpu; (void)paddr; (void)size; }
static inline void gpu_clear_fb(GPU* gpu, u32 start, u32 value, u32 end, u32 ctl) { (void)gpu; (void)start; (void)value; (void)end; (void)ctl; }
static inline void gpu_display_transfer(GPU* gpu, u32 in, u32 out, u32 inDim, u32 outDim, u32 flags) { (void)gpu; (void)in; (void)out; (void)inDim; (void)outDim; (void)flags; }
static inline void gpu_texture_copy(GPU* gpu, u32 in, u32 out, u32 size, u32 inGap, u32 outGap) { (void)gpu; (void)in; (void)out; (void)size; (void)inGap; (void)outGap; }

#endif
