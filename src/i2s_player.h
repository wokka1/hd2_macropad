/* I2S player stub for Arduino compatibility */
#pragma once

#ifdef ESP32_8048S070
// Arduino build - stub (audio not implemented yet)
#ifdef __cplusplus
extern "C" {
#endif

inline void play_wav(const char *path) {
    // Stub - audio playback not implemented in Arduino version yet
}

#ifdef __cplusplus
}
#endif

#else
// ESP-IDF build - use real implementation
#include "../libraries/audio_player/i2s_player.h"
#endif
