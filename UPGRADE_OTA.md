# HD2 Macropad OTA Upgrade Guide

This guide explains how to update your HD2 Macropad firmware over-the-air (OTA) without connecting to a computer.

---

## Prerequisites

- HD2 Macropad running firmware v1.6.9 or later
- WiFi network with internet access
- Device connected to WiFi

---

## Step 1: Connect to WiFi

If your device isn't already connected to WiFi:

1. Go to **Config** page
2. Tap **WiFi** Tab at the top
3. Flip the slider to Enable Wifi (disabled on boot)
4. Press Scan SSID button or Enter Manual SSID info
5. Enter appropriate password if required
6. Press Connect under Auto or Manual Connect if needed
7. Wait for connection confirmation

> [!NOTE]
> Wifi credentials are stored on the NVS flash for storage, enabling Wifi will attempt to reconnect to the last saved SSID.

<!-- Screenshot: WiFi connection screen -->
<img width="804" height="481" alt="wifi_setup" src="https://github.com/user-attachments/assets/e779285a-72ed-48ff-a4c8-9c7f4c29b862" />

---

## Step 2: Navigate to Update Screen

Tap the **Update** tab

You'll see:
- **Current version** - your installed firmware version
- **Check for Update** button
- **Previous version** (if available) with rollback option

<!-- Screenshot: Update tab initial state -->
<img width="805" height="482" alt="update_screen1" src="https://github.com/user-attachments/assets/d209a31c-6fba-47df-a583-cdd21f72d43b" />

---

## Step 3: Check for Updates

1. Tap **Check for Update**
2. Wait while the device contacts GitHub for the latest release
3. Status will show "Checking..."

<!-- Screenshot: Checking for updates -->
<img width="997" height="601" alt="update_install" src="https://github.com/user-attachments/assets/4ecd57f7-43ac-4662-9112-c84842c21cb1" />

---

## Step 4: Review Available Update

If an update is available:
- **Available version** label appears showing the new version
- **Install Update** button becomes visible
- Status shows "Update available"

If you're already on the latest version:
- Status shows "Up to date"

<!-- Screenshot: Update available -->

---

## Step 5: Install Update

1. Tap **Install Update**
2. Progress bar shows download progress (0-100%)
3. Status updates through each phase:
   - "Downloading..."
   - "Installing..."
   - "Complete, rebooting..."

**Important:** Do not power off the device during this process!

<!-- Screenshot: Download in progress -->

---

## Step 6: Automatic Reboot

Once complete, the device automatically reboots into the new firmware.

After reboot:
- Check the version on the Setup or Update page
- Verify the new version number matches what you installed

<!-- Screenshot: After reboot showing new version -->

---

## Rollback to Previous Version

If you experience issues with a new update, you can rollback to the previous firmware:

1. Go to **Config** > **Update** tab
2. Check the **Previous version** label
3. Tap **Rollback to Previous** button
4. Device reboots into the previous firmware

**Note:** Rollback is only available if a previous firmware exists in the backup partition (after at least one OTA update).

<!-- Screenshot: Rollback option -->

---

## Troubleshooting

### "Check for Updates" fails
- Verify WiFi is connected (check Setup > WiFi)
- Ensure your network has internet access
- Try again in a few minutes (GitHub may be temporarily unavailable)

### Download stalls or fails
- Check WiFi signal strength
- Move closer to your router
- Restart the device and try again

### Device doesn't reboot after update
- Wait at least 60 seconds
- If still unresponsive, power cycle the device
- The device will boot into the new firmware or rollback automatically

### Wrong version after update
- Perform a manual update via USB (see [INSTALL_7IN_ADV.md](INSTALL_7IN_ADV.md))
- Ensure you're downloading from the correct release

---

## Manual USB Update

If OTA isn't working, you can always update via USB. See [INSTALL_7IN_ADV.md](INSTALL_7IN_ADV.md) for instructions.
