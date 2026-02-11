## HD2 Macropad Installation Guide
# CrowPanel 7" Advance (ESP32-S3)

This guide explains how to flash pre-built firmware to your CrowPanel 7" Advance board without compiling from source.

---

## Prerequisites

### 1. Install Python
Download and install Python 3.8+ from [python.org](https://www.python.org/downloads/)

During installation:
- **Windows**: Check "Add Python to PATH"
- **macOS/Linux**: Python is often pre-installed. Verify with `python3 --version`

### 2. Install esptool
Open a terminal (Command Prompt on Windows, Terminal on macOS/Linux) and run:

```bash
pip install esptool
```

Or on some systems:
```bash
pip3 install esptool
```

### 3. Install USB Driver (Windows Only)
Windows may need the CP210x or CH340 USB driver depending on your board:
- [CP210x Driver](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers)
- [CH340 Driver](http://www.wch-ic.com/downloads/CH341SER_ZIP.html)

macOS and Linux typically don't need additional drivers.

---

## Download Firmware

Go to the [Releases page](https://github.com/wokka1/hd2_macropad/releases) and download:

| File | Use Case |
|------|----------|
| `hd2_macropad_install_vX.X.X.zip` | **New boards** - contains everything needed |
| `firmware.bin` | **Updates only** - if you already have HD2 Macropad installed |

---

## Find Your Serial Port

Connect your CrowPanel 7" Advance board via USB, then find the port:

### Windows
Open Device Manager and look under "Ports (COM & LPT)" for something like `COM3` or `COM4`

### macOS
```bash
ls /dev/cu.usb*
```
Look for something like `/dev/cu.usbserial-0001` or `/dev/cu.SLAB_USBtoUART`

### Linux
```bash
ls /dev/ttyUSB* /dev/ttyACM*
```
Look for something like `/dev/ttyUSB0` or `/dev/ttyACM0`

---

## New Board Installation (Recommended)

### Option A: Use the Install Package (Easiest)

1. Download and extract `hd2_macropad_install_vX.X.X.zip`
2. Open a terminal in the extracted folder
3. Run the flash script:

**Linux/macOS:**
```bash
./flash.sh              # Auto-detect port
./flash.sh /dev/ttyUSB0 # Or specify port
```

**Windows:**
```cmd
flash.bat COM3
```

### Option B: Manual Flash

Extract the zip and run:

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0xd000 ota_data_initial.bin \
    0x10000 firmware.bin
```

Replace `PORT` with your actual port (e.g., `COM3`, `/dev/cu.usbserial-0001`, `/dev/ttyUSB0`)

---

## Update Existing Installation

If you already have HD2 Macropad installed, you can update via:

### Option A: OTA Update (Easiest)
1. Connect the device to WiFi (Setup > WiFi)
2. Go to Config > Update tab
3. Press "Check for Updates"
4. If an update is available, press "Install Update"

### Option B: USB Flash
Download just `firmware.bin` and flash:

```bash
esptool.py --chip esp32s3 --port PORT --baud 921600 write_flash 0x10000 firmware.bin
```

---

## Complete Manual Examples

### Windows - New Install
```cmd
esptool.py --chip esp32s3 --port COM3 --baud 921600 write_flash 0x0 bootloader.bin 0x8000 partitions.bin 0xd000 ota_data_initial.bin 0x10000 firmware.bin
```

### macOS - New Install
```bash
esptool.py --chip esp32s3 --port /dev/cu.usbserial-0001 --baud 921600 write_flash \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0xd000 ota_data_initial.bin \
    0x10000 firmware.bin
```

### Linux - New Install
```bash
esptool.py --chip esp32s3 --port /dev/ttyUSB0 --baud 921600 write_flash \
    0x0 bootloader.bin \
    0x8000 partitions.bin \
    0xd000 ota_data_initial.bin \
    0x10000 firmware.bin
```

---

## Troubleshooting

### "Permission denied" (Linux/macOS)
Add your user to the dialout group:
```bash
sudo usermod -a -G dialout $USER
```
Then log out and back in.

Or use sudo (not recommended long-term):
```bash
sudo ./flash.sh /dev/ttyUSB0
```

### "Failed to connect" or "Timed out"
1. Disconnect and reconnect the USB cable
2. Try a different USB cable (some are charge-only)
3. Try a different USB port
4. Hold the **BOOT** button while connecting, then release after a few seconds
5. Try a lower baud rate: `--baud 115200`

### "Port not found"
1. Ensure the board is connected
2. Check Device Manager (Windows) or `ls /dev/` (macOS/Linux)
3. Install the appropriate USB driver (see Prerequisites)

### Board doesn't boot after flash
Try erasing flash completely first:
```bash
esptool.py --chip esp32s3 --port PORT erase_flash
```
Then re-flash using the full install package.

---

## Building from Source (Optional)

If you want to compile the firmware yourself, see the main [README.md](README.md) for build instructions using PlatformIO.

---

## Support

For issues or questions:
- [GitHub Issues](https://github.com/wokka1/hd2_macropad/issues)
