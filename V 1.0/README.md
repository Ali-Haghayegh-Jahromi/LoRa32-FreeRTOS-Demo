# V 1.0

A complete **Wi-Fi + HTTP server + OLED + LoRa** example, built on FreeRTOS tasks and using dual SPI buses (VSPI for LoRa, HSPI for SD-card). Features:

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
4. **Select** board: *Lilygo T-display*.  
5. **Upload** sketch.

---

## Output Explanation

- **Serial Monitor**  
  - On startup you’ll see connection logs, LoRa init success, and the HTTP server start message.  
  - Every second, the uptime counter increments and the next LoRa send message is shown in Serial.

- **OLED Display**  
  - **Uptime**: shows days, hours, minutes, seconds since boot (`00D, 00:01:23` in the example).  
  - **IP Address**: your ESP32’s WiFi address (`192.168.1.123`).  
  - **LoRa RX**: the most recent packet received, limited to fit on the screen.

- **HTTP Dashboard**  
  - **Uptime:** mirrors the OLED’s uptime in plain seconds.  
  - **Message to send:** the current `loraSendMsg` that will be broadcast next.  
  - **LoRa Receive Terminal:** a live text area of the last Rx packet(s).  
  - Submitting a new message via the form immediately updates what the LoRa send task will broadcast on its next cycle.

This section clarifies what you’ll actually see on each interface and how they relate to the FreeRTOS tasks running under the hood.
## Output Screenshots

<p align="center">
  <img
    src="https://github.com/user-attachments/assets/71f0c266-c70e-4c85-bdbe-3602d2ba4dc2"
    alt="Screenshot 2025-04-28 010415"
    width="300"
  />
</p>

<p align="center">
  <img
    src="https://github.com/user-attachments/assets/2ce20e21-6d04-435e-9eba-cfac481a9cb5"
    alt="photo_2025-04-28_01-02-09"
    width="300"
  />
</p>


-----------------------------------------------------------
MIT © Ali Haghayegh Jahromi
