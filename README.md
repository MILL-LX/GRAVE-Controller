# GRAVE RTC Audio & Relay Controller 🎵⏱️

An ultra-stable, dual-core automated audio and relay controller built on the **M5Stack Atom S3 Lite** ecosystem.

This device acts as a standalone, offline scheduler. At predefined times, it automatically activates a relay (powering an external amplifier/siren) and triggers an audio file playback in a continuous loop. It features a built-in Web Server for configuration, EEPROM persistence, and a physical override button.

---

## ✨ Key Features
* **Dual-Core Architecture:** Web server runs on Core 0 while hardware polling (I2C/UART) runs on Core 1, completely eliminating bus collisions and ensuring real-time responsiveness.
* **RTC Scheduled Alarms:** Define up to 3 independent activation periods (supports overnight scheduling).
* **Native Audio Control:** Direct UART communication with the M5Stack AudioPlayer Unit (N9301 DAC), featuring safe volume constraint mapping (0-30).
* **Physical Override:** Press the Atom S3 Lite's built-in button to instantly toggle the relay and audio on/off outside of scheduled hours.
* **Web Configuration Panel:** Access Point (AP) mode to configure schedules, set the RTC time, and adjust the volume from any smartphone or PC.
* **Persistent Memory:** Settings are saved to EEPROM and survive power outages.
* **Automated Hardware Test:** Runs a 10-second diagnostic routine (Relay ON + Audio Playback) on every boot.

---

## 🛠️ Hardware Requirements
This project is strictly designed for the following M5Stack hardware stack:
1. **[M5Stack Atom S3 Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite)** (ESP32-S3 brain).
2. **[Base AtomPortABC](https://docs.m5stack.com/en/atom/AtomPortABC)** (Expansion base to break out pins).
3. **[RTC Unit](https://docs.m5stack.com/en/unit/rtc)** (BM8563 I2C Real-Time Clock).
4. **[Relay Unit](https://docs.m5stack.com/en/unit/relay)** (Active-HIGH logic).
5. **[AudioPlayer Unit](https://docs.m5stack.com/en/unit/audioplayer)** (Serial MP3/WAV player).
6. **MicroSD Card** (Formatted to FAT32).

### 🔌 Pinout & Connections (via PortABC)
| PortABC Base | Color | Interface | M5 Module | Atom S3 Lite Pins |
| :--- | :--- | :--- | :--- | :--- |
| **Port P.A** | Red | I2C | RTC Unit | SDA: 38 / SCL: 39 |
| **Port P.B** | Black | GPIO | Relay Unit | Signal: 7 |
| **Port P.C** | Blue | UART | AudioPlayer Unit | RX: 6 / TX: 5 |

---

## 💻 Software & Libraries
To compile this project in the Arduino IDE, you will need the ESP32 board definitions installed, along with the following libraries:
* **M5AtomS3** (by M5Stack) - *Handles board initialization, M5Unified backend, and the WS2812 LED.*
* **Unit_AudioPlayer** (by M5Stack) - *Handles UART communication with the N9301 chip.*

*(Note: The legacy `Unit_RTC` library is **NOT** required. This project uses the modern `M5.Rtc` native driver included in M5Unified for bulletproof I2C communication).*

---

## 🚀 Setup & Installation

### 1. Preparing the SD Card & Audio File (⚠️ IMPORTANT)
The AudioPlayer unit reads the **physical index** of the files, not the alphabetical name.
1. Format your MicroSD card to **FAT32**.
2. Copy your desired audio file (`.mp3` or `.wav`) to the empty SD card. **It must be the first file copied.**
3. **macOS Users Warning:** macOS creates hidden `._` files when copying data to FAT32 drives. The AudioPlayer will try to play this hidden file and output silence. Before ejecting the SD card on a Mac, open the Terminal and run:

```bash
dot_clean -m /Volumes/YOUR_SD_CARD_NAME
```

4. Insert the SD card into the AudioPlayer Unit.

### 2. Flashing the Code
1. Open the project `.ino` file in Arduino IDE.
2. Select **M5AtomS3** as the target board.
3. Compile and upload.

---

## 📱 Usage Guide

### Boot Sequence & LED Status
When powered via USB-C, the Atom S3 Lite LED indicates the system status:
* ⚪ **White:** System is booting.
* 🟠 **Orange (Blinking):** Establishing UART handshake with AudioPlayer.
* 🔵 **Blue (10 seconds):** Hardware Diagnostic Test (Relay clicks ON, audio plays).
* 🔴 **Red:** System Ready / Standby mode (Inactive).
* 🟢 **Green:** Alarm is ACTIVE (Triggered by RTC schedule).
* 🔵 **Blue:** Alarm is ACTIVE (Triggered manually via the physical button).

### Web Configuration
1. Connect your smartphone or computer to the device's Wi-Fi network:
   * **SSID:** `GRAVE`
   * **Password:** `grave123`
2. Open a web browser and navigate to: **`http://192.168.4.1`**
3. **From the Dashboard you can:**
   * View the current RTC time and amplifier status.
   * Adjust the playback volume (0 to 30).
   * Set up to 3 operation periods (Start/End times).
   * Sync the hardware RTC with your local time.

---

## 🧠 Engineering Notes
* **I2C Bus Stability:** Older iterations of M5Stack libraries conflicted with ESP32-S3 internal bus polling. By migrating fully to `M5.Rtc` and disabling internal IMU checks, `unexpected NACK` errors have been entirely eliminated.
* **Thread Safety (FreeRTOS):** Web server variables (Core 0) and physical hardware state (Core 1) are strictly isolated. Data is passed between cores using volatile flags protected by `portMUX_TYPE` spinlocks to prevent torn reads and race conditions.
* **Audio DAC Limits:** The N9301 chip crashes/mutes if sent volume commands >30. The code strictly constrains volume data before writing to UART to prevent hardware muting.

---

## 📄 License
This project is open source.
