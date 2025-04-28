# ESP32 LoRa32 T3 Multi-Task Demo  
> FreeRTOS, LoRa SX1276, SSD1306 OLED & WiFi/HTTP server on LilyGO LoRa32 T3 v1.6.1

## Overview

This project turns the **LilyGO LoRa32 T3 v1.6.1** (ESP32 + SX1276) into a multi-tasked sensor node and dashboard:

- 🛰️ **LoRa**: send/receive messages via the SX1276 radio  
- 📺 **OLED Display**: scrolls uptime, IP address, and recent Rx packets  
- 🌐 **WiFi + HTTP**: hosts a simple form to change the next LoRa-TX message  
- 🛠️ **FreeRTOS**: isolated tasks with mutex-protected SPI access  

## Features

- **FreeRTOS tasks** pinned across both ESP32 cores  
- **LoRa health check**: auto-reset on SPI timeouts or bad version reads  
- **HTTP form** to send custom messages via `/setMsg`  
- **OLED GUI** with graceful text wrapping & limited buffer size  
- **Configurable region**: 915 MHz (US) vs 868 MHz (EU)  
- **Mutex-protected SPI** for thread-safe LoRa & display operations  

## Hardware

<img src="https://github.com/user-attachments/assets/ae2f6752-aafb-44e8-8195-7d9c03055b27" alt="LilyGO LoRa32 T3 v1.6.1 pinmap" width="300"/>

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
