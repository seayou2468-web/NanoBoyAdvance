#ifndef AUDIO_DSP_H
#define AUDIO_DSP_H

#include "common.h"

#define FRAME_SAMPLES 160
#define SAMPLE_RATE 32728
#define DSPRAM_VBASE 0x1FF00000
#define DSPRAM_DATA_OFF 0x2000

typedef struct {
    u8 _opaque;
} DSP;

static inline void dsp_process_frame(DSP* dsp) { (void)dsp; }
static inline void dsp_write_audio_pipe(DSP* dsp, void* buf, u32 size) { (void)dsp; (void)buf; (void)size; }
static inline void dsp_write_binary_pipe(DSP* dsp, void* buf, u32 size) { (void)dsp; (void)buf; (void)size; }
static inline void dsp_read_audio_pipe(DSP* dsp, void* buf, u32 size) { (void)dsp; (void)buf; (void)size; }
static inline void dsp_read_binary_pipe(DSP* dsp, void* buf, u32 size) { (void)dsp; (void)buf; (void)size; }
static inline void dsp_reset(DSP* dsp) { (void)dsp; }

#endif
