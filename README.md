# ESP32 LoRa32 T3 Multi-Task Demo  
> FreeRTOS, LoRa SX1276, SSD1306 OLED & WiFi/HTTP server on LilyGO LoRa32 T3 v1.6.1

## Overview

This project turns the **LilyGO LoRa32 T3 v1.6.1** (ESP32 + SX1276) into a multi-tasked sensor node and dashboard:

- 🛰️ **LoRa**: send/receive messages via the SX1276 radio  
- 📺 **OLED Display**: scrolls uptime, IP address, and recent Rx packets  
- 🌐 **WiFi + HTTP**: hosts a simple form to change the next LoRa-TX message  
- 🛠️ **FreeRTOS**: isolated tasks with mutex-protected SPI access  

It will be devloped over time, so stay tuned!

## Features

- **FreeRTOS tasks** pinned across both ESP32 cores  
- **LoRa health check**: auto-reset on SPI timeouts or bad version reads  
- **HTTP form** to send custom messages via `/setMsg`  
- **OLED GUI** with graceful text wrapping & limited buffer size  
- **Configurable region**: 915 MHz (US) vs 868 MHz (EU)  
- **Mutex-protected SPI** for thread-safe LoRa & display operations  

## Hardware
- **Board**: LilyGO LoRa32 T3 v1.6.1 (ESP32 + SX1276)  
- **Display**: SSD1306 OLED (I²C, 128×64)

**Default Connections**  
- OLED → ESP32 I²C (SDA/SCL)  
- LoRa SX1276 → SPI:  
  - SCK → GPIO 5  
  - MISO → GPIO 19  
  - MOSI → GPIO 27  
  - SS  → GPIO 18  
  - RST → GPIO 14  
  - DIO0→ GPIO 26  
<p align="center">
  <img
    src="https://github.com/user-attachments/assets/ae2f6752-aafb-44e8-8195-7d9c03055b27"
    alt="LilyGO LoRa32 T3 v1.6.1 pinmap"
    width="300"
  />
</p>  

## Software Dependencies

Install these via the Arduino Library Manager:

- **WiFi**  
- **WebServer**  
- **Adafruit_GFX**  
- **Adafruit_SSD1306**  
- **LoRa** by Sandeep Mistry  
- **SPI** (builtin)  

## Installation & Upload

1. **Clone this repo**  
   ```bash
   git clone https://github.com/Ali-Haghayegh-Jahromi/LoRa32-FreeRTOS-Demo.git
   cd LoRa32-FreeRTOS-Demo

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

