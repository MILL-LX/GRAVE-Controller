# GRAVE RTC Audio & Relay Controller 🎵⏱️
M5Stack ATOM S3 Lite · RTC Unit · Relay Unit · AudioPlayer Unit · Web Interface

---

## Overview

GRAVE is an embedded controller designed to activate an amplifier and play audio automatically during scheduled time periods. Configuration is done entirely through a browser-connected to the device's own Wi-Fi access point — no router, no internet, no app required.

The system runs a dual-core architecture on the ESP32: the web server runs on Core 0 while all hardware (RTC, audio, relay, LED, button) is owned exclusively by Core 1. All communication between cores is done through volatile flags protected by a FreeRTOS spinlock, eliminating I2C and UART race conditions.

---

## Hardware

| Component | Model | Connection |
|---|---|---|
| Microcontroller | M5Stack ATOM S3 Lite | — |
| Base | M5Stack Base AtomPortABC | — |
| RTC | M5Stack RTC Unit (BM8563) | Port A — I2C (pins 38/39) |
| Relay | M5Stack Relay Unit | Port B — GPIO (pin 7) |
| Audio Player | M5Stack AudioPlayer Unit | Port C — UART (pins 5/6) |
| Storage | microSD card (in AudioPlayer) | — |

### Wiring Summary

```
ATOM S3 Lite
├── Port A (I2C  — pins 38/39) ──► RTC Unit
├── Port B (GPIO — pin 7)      ──► Relay Unit  [HIGH = ON, LOW = OFF]
└── Port C (UART — pins 5/6)   ──► AudioPlayer Unit
```

---

## Features

- **Scheduled activation** — up to 3 independent time periods per day
- **Overnight periods** — periods that cross midnight (e.g. 23:00 → 01:00) are supported
- **Manual override** — button on the device toggles audio/relay independently of schedule
- **Volume control** — adjustable 0–255 via web slider, persisted to EEPROM
- **RTC time set** — set date and time from the web interface
- **EEPROM persistence** — all settings survive power cycles
- **Hardware test** — 10-second relay + audio test on every boot
- **LED status indicator** — visual feedback for all system states

---

## LED Status

| Colour | Meaning |
|---|---|
| White | Booting |
| Orange (blinking) | Connecting to AudioPlayer |
| Blue (solid, boot) | Hardware test running |
| Red | Standby — no active period, manual OFF |
| Green | Active — triggered by RTC schedule |
| Blue | Active — triggered by manual button only |

---

## Web Interface

Connect to the device Wi-Fi and open a browser:

| Parameter | Value |
|---|---|
| SSID | `GRAVE` |
| Password | `grave123` |
| URL | `http://192.168.4.1` |

The interface is mobile-friendly and works on any browser without installing anything.

### Sections

**Estado Atual**  
Shows the current RTC time, date, system state (ON/OFF, Auto/Manual) and current volume.

**Volume do MP3**  
Slider from 0 to 255. Press *Salvar Volume* to apply and persist.

**Períodos de Activação**  
Define up to 3 start/end time pairs. Periods where start equals end are ignored. Periods that cross midnight work correctly.

**Ajustar Hora RTC**  
Set hours, minutes, seconds, day, month and year. Applied immediately to the hardware RTC.

---

## Button

| Action | Result |
|---|---|
| Short press | Toggle manual mode ON/OFF |

When manual mode is ON and no RTC period is active, the LED turns **blue**. When an RTC period is also active, the LED turns **green** and the source shown in the web interface is *Auto*.

Manual mode is not persisted — it resets on power cycle.

---

## Audio Setup

1. Insert a microSD card into the AudioPlayer Unit.
2. Place audio files in the root folder or in a subfolder named `01`.
3. The system plays in **single loop** mode when active (repeats the same track continuously).
4. On boot, the system initialises with **folder once** mode; loop mode is set at activation.

> **Note:** The AudioPlayer Unit emits a spontaneous status frame during boot. This causes the message `Invalid frame received: header/footer mismatch` to appear once in the Serial log on first connection. This is expected behaviour specific to Port C (pins 5/6) and the device recovers automatically on the second attempt. It does not affect operation.

---

## EEPROM

Settings are stored at address `0` with signature `0xBBCCDD02`.

| Setting | Default |
|---|---|
| Number of periods | 0 |
| Volume | 127 |

If the signature does not match on boot (e.g. after a firmware change that alters the data layout), all settings are reset to defaults automatically. To force a reset, increment the last byte of `EEPROM_SIGNATURE` in the source code.

---

## Architecture

```
┌─────────────────────────────────┐   ┌─────────────────────────────────┐
│           CORE 0                │   │           CORE 1                │
│        WebServerTask            │   │         setup() / loop()        │
│                                 │   │                                 │
│  WiFi AP                        │   │  RTC (I2C)                      │
│  HTTP handlers                  │   │  AudioPlayer (UART)             │
│  handleRoot()                   │   │  Relay (GPIO)                   │
│  handleSet()                    │   │  LED                            │
│  handleSetVolume()              │   │  Button                         │
│  handleSetTime()                │   │  checkAlarmState()              │
│                                 │   │                                 │
│  NEVER touches I2C or UART      │   │  NEVER called from Core 0       │
└──────────────┬──────────────────┘   └──────────────┬──────────────────┘
               │                                     │
               └──────────── configMux ──────────────┘
                         (portMUX spinlock)

Shared state:
  alarmConfig          — read/write both cores, always under configMux
  manual_play          — volatile + configMux
  is_alarm_active      — volatile + configMux
  sharedH/M/S/D/Mon/Y  — written Core 1 under configMux, read Core 0 under configMux
  pendingVolumeFlag    — set Core 0, consumed Core 1, under configMux
  pendingTimeFlag      — set Core 0, consumed Core 1, under configMux
```

**Rule:** Core 0 never calls any function that touches I2C or UART. Volume and time changes from the web interface are stored as pending requests and applied by Core 1 on its next 1-second tick.

---

## Dependencies

Install via Arduino Library Manager or PlatformIO:

| Library | Source |
|---|---|
| `M5AtomS3` | M5Stack official |
| `M5Unified` | M5Stack official |
| `unit_audioplayer` | M5Stack official (`Unit_AudioPlayer`) |
| `WiFi` | Built-in (ESP32 Arduino core) |
| `WebServer` | Built-in (ESP32 Arduino core) |
| `EEPROM` | Built-in (ESP32 Arduino core) |

**Board:** M5Stack ATOM S3 (ESP32-S3) — select in Arduino IDE under *M5Stack* boards.

---

## Build & Flash

1. Install [Arduino IDE](https://www.arduino.cc/en/software) 2.x
2. Add ESP32 board support: `https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json`
3. Install M5Stack board package and the libraries listed above
4. Select board: **M5Stack AtomS3**
5. Select the correct COM port
6. Upload `grave_controller_v6.7-final.ino`

---

## Changelog

| Version | Changes |
|---|---|
| v5.0–v5.8 | Manual override, period validation, mp3.begin() timeout, EEPROM versioning, button state machine, HTML encoding fixes |
| v5.9 | **Dual-core architecture** — Web Server on Core 0, all hardware on Core 1. Eliminated I2C corruption from TCP/IP stack interference |
| v6.0 | FIX: `mp3.setVolume()` called from Core 0 (UART race condition) — replaced with pending flag. FIX: `RTC.setDateTime()` called from Core 0 (I2C race condition) — replaced with pending struct. FIX: `str_buffer` made local to `handleRoot()` |
| v6.6-final | FIX: `WebServerTask` was launched before the hardware test, allowing HTTP requests to interfere with hardware initialisation |
| v6.7-final | FIX: shared time variables written under `configMux` in `loop()` to prevent torn reads. FIX: shared time variables initialised in `setup()` to prevent 00:00 bug on early button press. FIX: `configMux` protection added to `is_alarm_active` state changes in `checkAlarmState()` |

---

## License

MIT — free to use, modify and distribute.

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

