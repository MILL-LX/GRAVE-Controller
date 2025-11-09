# **Amplifier Activation and MP3 Controller** **(GRAVE Controller)**

This project implements a programmable time controller, based on the **M5Stack** ecosystem, to manage the activation of an amplifier (via relay) and audio playback via an MP3 module. The configuration of time periods and audio volume is managed through a simple Web interface.

This project was developed with the help of Gemini AI under the guidance of Mauricio Martins. @the-spaceinvader

## **Key Features**

* **Self-Test at Boot:** Upon power-up, the device initiates a 10-second self-test. The amplifier relay is activated (LOW state on Pin 7), Track 1 audio plays in a loop, and the ATOM S3 LED turns Blue, confirming the functionality of the audio and amplification system.
* **Precise Time Control:** Uses a **Real-Time Clock (RTC)** for precise system activation during programmed periods.  
* **Access Point (AP) Mode:** Creates a fixed local Wi-Fi network for direct access to the controller.  
* **Web Interface (HTTP Server):** Allows remote configuration of:  
  * Three daily activation periods.  
  * Manual adjustment of the RTC time and date.  
  * **MP3 Volume Control** in real-time (scale 0 to 30).  
* **Persistence:** Alarm and volume settings are saved in **EEPROM** memory, remaining preserved after power cycling.  
* **Audio Control:** Starts and stops *loop* playback of the audio file on the MP3 module's SD card.

## **🛠️ Hardware Used**

The project uses the ATOM S3 module as the central processing unit, interconnected with various M5Stack units and an MP3 module via the **Atomic Port ABC** base.

**M5Stack Hardware** (More information at: [M5Stack Documentation](https://m5stack.com/))

| Component | Function | Documentation |
| :---- | :---- | :---- |
| **ATOM S3 Lite** | Main board (ESP32) | [Docs ATOM S3 Lite](https://docs.m5stack.com/en/core/AtomS3%20Lite) |
| **ATOM RTC UNIT** | Real-Time Clock (I2C) | [Docs ATOM RTC UNIT](https://docs.m5stack.com/en/unit/UNIT%20RTC) |
| **ATOM RELAY UNIT** | Amplifier Activation | [Docs ATOM RELAY UNIT](https://docs.m5stack.com/en/unit/relay) |
| **Atomic Port ABC Base** | Connection Base | [Docs Atomic Port ABC](https://www.google.com/search?q=https://docs.m5stack.com/en/atom/AtomPortABC) |

**MP3 Module**

| Component | Function | Purchase/Reference Link |
| :---- | :---- | :---- |
| **MP3 Player Module (YX5300)** | UART Audio Playback | [Mini MP3 Module YX5300](https://www.tinytronics.nl/en/audio/audio-sources/mini-mp3-module-yx5300-with-audio-jack) |

### **Connection Mapping (Atomic Port ABC)**

| Port ABC | Connected Module | Protocol | Used Pins (GPIO) |
| :---- | :---- | :---- | :---- |
| **Port A** | ATOM RTC UNIT | I2C | SDA (38), SCL (39) |
| **Port B** | ATOM RELAY UNIT | GPIO | Signal Pin (**7**) |
| **Port C** | MP3 Player Module | UART | RX (**5**), TX (**6**) |

## **💻 Required Libraries**

This project requires the following libraries in your Arduino development environment, installable via the Library Manager:

| Library | Dependency | Reference Link (GitHub/Docs) |
| :---- | :---- | :---- |
| **Unit\_RTC** | M5Stack | [M5Stack Unit RTC Library](https://docs.m5stack.com/en/unit/UNIT%20RTC) |
| **M5AtomS3** | M5Stack | [M5Stack ATOM S3 Core Library](https://docs.m5stack.com/en/core/AtomS3%20Lite) |
| **YX5300\_ESP32** | MP3 Player | [JRodrigoTech/YX5300\_ESP32](https://www.google.com/search?q=https://github.com/JRodrigoTech/YX5300_ESP32) |
| **WebServer** | ESP32 Standard | (Integrated into ESP32 Core) |
| **EEPROM** | Arduino Standard | (Integrated into Arduino Core) |

