# Sound Implementation Research for CrowPanel Advance 7"

## Current State

**Status:** Audio is currently disabled/stubbed out for size optimization and BLE compatibility.

**Stub Implementation:**
- `src/i2s_player.h` contains empty `play_wav()` stub
- `src/main.c` has commented-out sound playback code (lines 227-234)
- `playbackSound()` function exists but does nothing
- Sound-related calls throughout codebase (`ui_events.c`, `actions.c`, `configuation.c`) are present but no audio output

**Reason for Removal:**
- Commit `9ca1097`: "Add ESP-IDF BLE build support with successful compilation"
- Commit `57fc34f`: "Remove unnecessary files and directories for size optimization"
- Audio was removed to reduce firmware size and focus on BLE functionality

## Hardware Capabilities

### CrowPanel Advance 7" Audio Hardware

According to [Elecrow Wiki](https://www.elecrow.com/pub/wiki/esp32-display-702727-intelligent-touch-screen-wi-fi26ble-800480-hmi-display.html):

- **Speaker Connector:** SPK (PH2.0-2P type)
- **Built-in Amplifier:** Power amplifier chip circuit on motherboard
- **Audio Protocol:** I2S (Inter-IC Sound)
- **I2S Pins (from Basic 7" wiki):**
  - LRCLK (Left/Right Clock): GPIO 18
  - BCLK (Bit Clock): GPIO 42
  - SDIN (Serial Data Input): GPIO 17

### ESP32-S3 Specifications
- **Module:** ESP32-S3-WROOM-1-N16R8 (Advance) / N4R8 (Basic)
- **CPU:** Dual-core 32-bit LX7 @ 240MHz
- **Flash:** 16MB (Advance) / 4MB (Basic)
- **RAM:** 8MB PSRAM + 512KB SRAM
- **I2S Support:** Native I2S hardware peripheral (I2S0, I2S1)

## Pin Conflict Analysis - RESOLVED ✅

### CrowPanel Advance Confirmed I2S Pins (from Factory Code)
```c
#define AUDIO_I2S_DOUT      12  // Data Out to speaker amplifier
#define AUDIO_I2S_BCK_IO    13  // Bit Clock
#define AUDIO_I2S_LRCK_IO   11  // Left/Right Clock (Word Select)
#define AUDIO_I2S_MCK_IO    -1  // Master Clock (not used)
```

**✅ NO CONFLICTS!** These pins are separate from display and do not overlap.

### CrowPanel Advance Display Pins (Current Configuration)
```c
#define PIN_NUM_HSYNC       40
#define PIN_NUM_VSYNC       41
#define PIN_NUM_DE          42
#define PIN_NUM_PCLK        39
// RGB data pins: 21, 47, 48, 45, 38, 9-10, 12-14, 7, 17, 18, 3, 46
```

### SD Card Pins (SPI Mode) - Confirmed Available
```c
#define SD_MOSI             6   // Master Out Slave In
#define SD_MISO             4   // Master In Slave Out
#define SD_SCK              5   // Serial Clock
#define SD_CS               0   // Chip Select (or GPIO 7)
```

**✅ NO CONFLICTS!** SD card pins are also separate from display and I2S.

### Microphone Pins (Optional - for future use)
```c
#define MIC_CLK             19  // Microphone Clock
#define MIC_WS              2   // Microphone Word Select
#define MIC_SD              20  // Microphone Serial Data
```

### Complete Pin Allocation Summary

| GPIO | Function | Conflicts? |
|------|----------|-----------|
| **I2S Audio** | | |
| 11 | I2S_LRCK (WS) | ✅ Clear |
| 12 | I2S_DOUT | ✅ Clear |
| 13 | I2S_BCLK | ✅ Clear |
| **SD Card** | | |
| 4 | SD_MISO | ✅ Clear |
| 5 | SD_SCK | ✅ Clear |
| 6 | SD_MOSI | ✅ Clear |
| 0 or 7 | SD_CS | ✅ Clear |
| **Display** | | |
| 39-42, 45-48 | Display control | N/A |
| 3, 7-10, 13-14, 17-18, 21, 38, 46 | RGB data | Some overlap with DATA pins |
| **Touch** | | |
| 15 | Touch SDA | ✅ Clear |
| 16 | Touch SCL | ✅ Clear |
| **Backlight** | | |
| I2C 0x30 | STC8H1K28 | ✅ Clear |

**IMPORTANT:** GPIO 12 and 13 are used by BOTH display (as DATA pins) AND I2S audio. However, the factory code shows they prioritize I2S on these pins. This suggests:
1. The display might use a different pin configuration than documented
2. These pins might be multiplexed
3. Need to verify actual display pin usage in working factory code

**⚠️ INVESTIGATION NEEDED:** Verify GPIO 12, 13 usage in our current working display configuration vs I2S requirements.

## Upstream Implementation (JC3248W535 - 480x320 Board)

The upstream repository has full working audio implementation:

### Files Structure
```
libraries/audio_player/
├── i2s_configuration.h   # Pin definitions and system includes
├── i2s_player.h          # API declarations
├── i2s_player.c          # WAV playback implementation
└── i2s_sdcard.h          # SD card mounting for audio files

sdcard/assets/sound/      # WAV audio files
├── intro.wav
├── _sel.wav              # Selection sound
├── _des.wav              # Deselection sound
├── _rst.wav              # Reset sound
├── _swp.wav              # Swipe sound
├── _swt.wav              # Switch sound
└── [stratagem sounds]    # bkpk.wav, bot.wav, eagrel.wav, etc.
```

### Upstream Pin Configuration (JC3248W535)
```c
// libraries/audio_player/i2s_configuration.h
#define AUDIO_I2S_PORT I2S_NUM_0
#define AUDIO_I2S_MCK_IO -1      // MCK (not used)
#define AUDIO_I2S_BCK_IO 42      // Bit Clock
#define AUDIO_I2S_LRCK_IO 2      // Left/Right Clock
#define AUDIO_I2S_DO_IO 41       // Data Out

#define SD_MMC_D0 13
#define SD_MMC_CLK 12
#define SD_MMC_CMD 11
```

### Audio Playback Flow
1. **Initialization** (`i2s_setup()`):
   - Creates I2S channel with standard Philips config
   - 44.1kHz sample rate, 16-bit mono
   - Configures GPIO pins for I2S output

2. **Playback** (`play_wav(char *fp)`):
   - Opens WAV file from SD card via LVGL filesystem
   - Skips 44-byte WAV header
   - Reads 2048-byte chunks into buffer
   - Writes buffer to I2S channel
   - Continues until EOF
   - Cleans up I2S channel and closes file

3. **Integration**:
   - `playbackSound(char *path)` in `main.c` sets flags
   - Main loop checks `soundPlayback` flag and calls `play_wav()`
   - UI actions call `playbackSound()` for button clicks, stratagems, etc.

## Investigation Completed ✅

### 1. Correct I2S Pins for CrowPanel Advance - FOUND ✅

**Confirmed from Elecrow factory code:**
- I2S_DOUT: GPIO 12
- I2S_BCLK: GPIO 13
- I2S_LRCK: GPIO 11

**Source:** [Elecrow Official Repository](https://github.com/Elecrow-RD/CrowPanel-Advance-7-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-800x480)

### 2. SD Card Availability - CONFIRMED ✅

**SD Card Slot Available:**
- Interface: SPI mode (not SDMMC like upstream)
- Pins: MOSI=6, MISO=4, SCK=5, CS=0 or 7
- TF card slot present on hardware

**Note:** Upstream uses SDMMC mode with different pins (11, 12, 13). CrowPanel Advance uses SPI mode, so we need to adapt the SD card initialization code.

### 3. Critical Pin Overlap Issue ⚠️

**PROBLEM:** GPIO 12 and 13 appear in both configurations:
- **Our display code:** Uses GPIO 12-13 for RGB data (G3, G4)
- **Factory I2S code:** Uses GPIO 12-13 for audio (DOUT, BCLK)

**RESOLUTION NEEDED:**
1. Check our actual `esp_bsp.h` to see if we're using GPIO 12-13 for display
2. If yes, determine which is correct (display or I2S)
3. Factory code might use different display pins than we do
4. May need to reconfigure display OR use different I2S pins

### 3. Flash Size Budget

**Current Usage:**
- Flash: 53.3% used (plenty of room for audio files)
- RAM: 15.7% used (could fit small audio buffers)

**Audio File Sizes** (estimated):
- Upstream uses 44.1kHz 16-bit mono WAV files
- Typical stratagem sound: ~5-10 seconds = 400-900 KB/file
- All sounds (~30 files): ~15-25 MB uncompressed
- **Problem:** 16MB flash total, firmware already uses 8.5MB
- **Solution:** Lower sample rate (22.05kHz or 16kHz) or compress to MP3/ADPCM

## Recommended Implementation Path

### Phase 0: Pin Conflict Resolution (CRITICAL) 🔴
**Must complete before any audio work!**

1. **Test display quality at 14-bit RGB:**
   ```c
   // Temporarily modify esp_bsp.h for CrowPanel Advance:
   // Comment out:
   // #define PIN_NUM_DATA8       12  // G3
   // #define PIN_NUM_DATA9       13  // G4

   // Rebuild and test display for color banding
   ```

2. **If 14-bit quality acceptable:**
   - Proceed with audio implementation
   - Free GPIO 12-13 for I2S

3. **If 14-bit quality unacceptable:**
   - Investigate Option 2 (alternative I2S pins)
   - Contact Elecrow support about speaker amplifier wiring
   - Consider Option 3 (no audio on this board)

### Phase 1: Research & Planning ✅ COMPLETE
1. ✅ Research hardware capabilities
2. ✅ Identify correct I2S pins (11, 12, 13)
3. ✅ Confirm SD card availability (SPI mode, GPIO 4-6)
4. ✅ Identify pin conflict (GPIO 12-13)
5. ⏳ **USER DECISION:** Accept 14-bit display for audio?

### Phase 2: SD Card Setup (Before Audio)
**Upstream uses SDMMC, we need SPI mode:**

1. Copy SD card library from upstream
2. Modify `i2s_sdcard.h` for SPI mode:
   ```c
   // Replace SDMMC with SPI configuration
   #define SD_MOSI_PIN 6
   #define SD_MISO_PIN 4
   #define SD_SCK_PIN  5
   #define SD_CS_PIN   0  // or 7
   ```
3. Test SD card mount/read before audio
4. Create `/sdcard/assets/sound/` directory structure

### Phase 3: Port Audio Libraries
1. Copy `libraries/audio_player/` from upstream
2. Update `i2s_configuration.h`:
   ```c
   #define AUDIO_I2S_PORT I2S_NUM_0
   #define AUDIO_I2S_MCK_IO -1
   #define AUDIO_I2S_BCK_IO 13   // Changed from 42
   #define AUDIO_I2S_LRCK_IO 11  // Changed from 2
   #define AUDIO_I2S_DO_IO 12    // Changed from 41
   ```
3. Add to CMakeLists.txt or platformio build
4. Test basic I2S initialization

### Phase 4: Test with Single Audio File
1. Copy one WAV file to SD card: `/sdcard/assets/sound/intro.wav`
2. Uncomment audio code in `src/main.c` (lines 227-234)
3. Re-enable `#include "i2s_player.h"`
4. Test playback on startup
5. Verify no crackling/distortion

### Phase 5: Full Integration
1. Copy all sound files from upstream
2. Test stratagem sounds work correctly
3. Test UI feedback sounds (_sel.wav, _des.wav, etc.)
4. Verify flash usage acceptable (currently 53.3%, should stay <80%)

### Phase 6: Optimization (if needed)
1. **If flash space limited:**
   - Reduce sample rate: 44.1kHz → 22.05kHz
   - Convert to mono if not already
   - Reduce bit depth: 16-bit → 8-bit
   - Use ADPCM or IMA compression

2. **If SD card unreliable:**
   - Store sounds in SPIFFS/LittleFS on flash
   - Preload critical sounds to RAM

3. **If audio quality issues:**
   - Adjust I2S buffer size (currently 2048)
   - Implement DMA-based streaming
   - Add error recovery for failed playback

## Pin Conflict Resolution - CRITICAL FINDING ⚠️

After checking our current `esp_bsp.h` configuration:

```c
// Our CrowPanel Advance display configuration
#define PIN_NUM_DATA8       12  // G3 (Green channel bit 3)
#define PIN_NUM_DATA9       13  // G4 (Green channel bit 4)
```

**CONFLICT CONFIRMED:**
- **Display:** GPIO 12 = G3 (DATA8), GPIO 13 = G4 (DATA9)
- **I2S Audio:** GPIO 12 = DOUT, GPIO 13 = BCLK

**WE CANNOT USE BOTH SIMULTANEOUSLY!**

### Options to Resolve:

#### Option 1: Sacrifice Display Quality (16-bit → 14-bit RGB)
- Remove G3 and G4 from display (lose 2 green bits)
- Free up GPIO 12-13 for I2S audio
- Color depth: 16-bit (RGB565) → 14-bit (RGB454)
- **Impact:** Slightly reduced green channel resolution (visible banding possible)

#### Option 2: Use Alternative I2S Pins
- ESP32-S3 allows I2S on many GPIO pins via GPIO matrix
- Try different I2S configuration (if hardware supports it)
- **Risk:** Speaker amplifier might be hardwired to GPIO 11-13
- **Test needed:** Check if amplifier accepts I2S on different pins

#### Option 3: No Audio on CrowPanel Advance
- Accept hardware limitation
- Keep full 16-bit display quality
- Focus on Bluetooth/USB functionality only

#### Option 4: External Audio Module
- Add external I2S DAC/amplifier on unused GPIO pins
- Requires hardware modification
- Not practical for this project

### Recommendation: Option 1 (Sacrifice 2 Green Bits)

**Rationale:**
- Human eye is most sensitive to green, but 2-bit loss unlikely to be noticeable
- Audio functionality adds significant value (stratagem sounds, UI feedback)
- 14-bit color (RGB454) still provides 16,384 colors vs 65,536
- Modern displays dither well enough to hide banding

**Implementation:**
1. Modify display configuration to 14-bit RGB (remove DATA8, DATA9)
2. Configure GPIO 12-13 for I2S audio
3. Test display quality for acceptable color reproduction
4. If banding is visible, implement software dithering

## Open Questions

1. ✅ **What are the actual I2S pins for CrowPanel Advance?**
   - **ANSWERED:** GPIO 11 (LRCK), 12 (DOUT), 13 (BCLK)
   - **CONFLICT:** GPIO 12-13 also used by display

2. ✅ **Does CrowPanel Advance have SD card slot?**
   - **ANSWERED:** Yes, SPI mode on GPIO 4-6 (CS on 0 or 7)
   - Need to adapt upstream SDMMC code to SPI mode

3. ⏳ **What's the priority?**
   - Full audio with all sounds? **Feasible with 16MB flash**
   - Limited audio (just UI feedback)? **Easier, smaller files**
   - Optional feature (user can disable)? **Recommended approach**

4. ⏳ **Should we implement sound before or after upstream merge?**
   - Upstream has full audio working on 480x320 board
   - Could merge audio libraries first, then adapt pins
   - **Blocked by pin conflict resolution**

5. **NEW: Can we accept 14-bit color for audio functionality?**
   - User decision required
   - Test display quality impact first

## References

- [CrowPanel Basic 7" Wiki](https://www.elecrow.com/pub/wiki/esp32-display-702727-intelligent-touch-screen-wi-fi26ble-800480-hmi-display.html) - Basic board I2S pins
- [Upstream Audio Implementation](https://github.com/unic8s/hd2_macropad/tree/main/libraries/audio_player)
- [ESP-IDF I2S Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/peripherals/i2s.html)
- [ESP32-S3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-s3_datasheet_en.pdf)

## Quick Reference: CrowPanel Advance 7" Pin Configuration

```c
// === AUDIO (I2S) ===
#define I2S_LRCK_PIN  11  // Word Select
#define I2S_DOUT_PIN  12  // Data Out (⚠️ conflicts with display G3)
#define I2S_BCLK_PIN  13  // Bit Clock (⚠️ conflicts with display G4)

// === SD CARD (SPI) ===
#define SD_MOSI_PIN   6   // Master Out Slave In
#define SD_MISO_PIN   4   // Master In Slave Out
#define SD_SCK_PIN    5   // Serial Clock
#define SD_CS_PIN     0   // Chip Select (or GPIO 7)

// === DISPLAY (RGB Parallel) ===
#define HSYNC_PIN     40
#define VSYNC_PIN     41
#define DE_PIN        42  // Data Enable
#define PCLK_PIN      39  // Pixel Clock
// Data pins: 3, 7, 9-18, 21, 38, 45-48
// CONFLICT: GPIO 12 (G3), GPIO 13 (G4) also needed for I2S

// === TOUCH (I2C) ===
#define TOUCH_SDA_PIN 15
#define TOUCH_SCL_PIN 16

// === MICROPHONE (Optional) ===
#define MIC_CLK_PIN   19
#define MIC_WS_PIN    2
#define MIC_SD_PIN    20
```

## Final Recommendation

### For Full Audio Support:

**Hardware Trade-off Required:**
- **Give up:** 2 bits of green channel (GPIO 12-13)
- **Get:** Full I2S audio with all stratagem sounds
- **Impact:** 16-bit RGB → 14-bit RGB (16,384 colors vs 65,536)
- **Visibility:** Likely imperceptible with dithering

**Implementation Effort:** Medium
- Port upstream audio libraries ✅ Code exists
- Adapt SD card to SPI mode ✅ Straightforward
- Modify display configuration ✅ Change 2 lines
- Test audio quality ✅ Can validate quickly

**Benefits:**
- Full stratagem audio feedback (immersive!)
- UI sound effects (button clicks, swipes)
- Matches upstream functionality
- Enhances user experience significantly

### Alternative: No Audio

**Keep Current Configuration:**
- **Maintain:** Full 16-bit RGB565 display
- **Sacrifice:** Audio feedback
- **Approach:** Focus on visual-only interface

**Benefits:**
- No hardware compromise
- Simpler codebase
- Lower flash usage
- Current implementation already works

---

## Next Steps

**🎯 Immediate Decision Needed:**

1. **User choice:** Accept 14-bit display for audio?
   - Test 14-bit RGB quality first (5 minute change)
   - If acceptable → proceed with audio implementation
   - If unacceptable → document "no audio" limitation

2. **If proceeding with audio:**
   - Follow Phase 0-6 implementation path above
   - Estimated time: 2-4 hours work
   - High chance of success (upstream code proven)

3. **If skipping audio:**
   - Document in README: "CrowPanel Advance version does not support audio due to GPIO pin constraints"
   - Keep stub `i2s_player.h` for compatibility
   - Focus development on other features

**My Recommendation:** Try 14-bit display first. If quality is good (likely), audio adds significant value to the macropad experience. Helldivers 2 stratagem sounds are iconic and would enhance immersion!
