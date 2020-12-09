#include "lib/miniaudio/miniaudio.h"

static const ma_format miniAudioFormatFromLovr[] = {
  [SAMPLE_I16] = ma_format_s16,
  [SAMPLE_F32] = ma_format_f32
};

static const ma_format sampleSizes[] = {
  [SAMPLE_I16] = 2,
  [SAMPLE_F32] = 4
};

static inline size_t bytesPerAudioFrame(int channelCount, SampleFormat fmt) {
    return channelCount * sampleSizes[fmt];
}