/* I2S player stub - audio removed for BLE-only build */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Stub for BLE-only build - audio playback not needed
inline void play_wav(const char *path) {
    // Stub - audio playback removed for size optimization
}

#ifdef __cplusplus
}
#endif
