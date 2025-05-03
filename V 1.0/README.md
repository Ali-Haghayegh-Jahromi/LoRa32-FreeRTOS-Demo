# V 1.0

A complete **Wi-Fi + HTTP server + OLED + LoRa** example for the [TTGO LoRa32 T3](https://github.com/LilyGO/TTGO-LoRa32) board, built on FreeRTOS tasks and using dual SPI buses (VSPI for LoRa, HSPI for SD-card). Features:

- **Concurrent FreeRTOS tasks** for display, LoRa TX/RX, HTTP server, uptime counter, and LoRa health checks  
- **SPI mutex** to guarantee safe transactions on both buses  
- **OLED (SSD1306)** shows uptime, IP address, and recent LoRa messages  
- **HTTP web UI** to view status and update outgoing LoRa message  
- **LoRa (SX1276)** on VSPI with hardware reset and register health checks  
- **Auto-generated uptime messages** when no manual override is pending  

---

## Hardware Setup

| Peripheral     | Pins                          | Notes                       |
| -------------- | ----------------------------- | --------------------------- |
| **OLED (I²C)** | SDA: 21, SCL: 22, VCC, GND    | SSD1306 @ 0x3C              |
| **LoRa (VSPI)**| SCK: 5, MISO: 19, MOSI: 27, CS: 18, RST: 14, DIO0: 26 | SX1276 module |
| **(Optional) SD** | SCK: 14, MISO: 2, MOSI: 15, CS: 13 | HSPI for logging |

---

## Software Architecture

**FreeRTOS Tasks & Priorities**

| Task              | Priority | Core | Purpose                              |
| ----------------- | -------- | ---- | ------------------------------------ |
| `displayTask`     | 3        | 1    | Refresh OLED (uptime, IP, RX log)    |
| `loraSendTask`    | 3        | 0    | Send LoRa packet (auto + manual)     |
| `loraReceiveTask` | 2        | 0    | Listen for incoming LoRa packets     |
| `httpServerTask`  | 2        | 1    | Serve web UI & handle form posts     |
| `loraCheckTask`   | 4        | 1    | LoRa health-check via register read  |
| `uptimeTask`      | 1        | 0    | Increment uptime & build auto-message |

**Dual SPI Buses + Mutex**  
- **VSPI** exclusively for LoRa  
- **HSPI** (optional) for SD-card  
- **`spiMutex`** ensures one SPI user at a time  

---

## Installation

1. **Clone** this repo.  
2. **Open** in Arduino IDE or PlatformIO.  
3. **Install** libraries:  
   - Adafruit SSD1306 & GFX  
   - LoRa (Sandeep Mistry)  
4. **Select** board: *TTGO LoRa32 T3*.  
5. **Upload** sketch.

-----------------------------------------------------------
MIT © Ali Haghayegh Jahromi
