# ESP32 LoRa32 T3 Multi-Task Demo  
> FreeRTOS, LoRa SX1276, SSD1306 OLED & WiFi/HTTP server on LilyGO LoRa32 T3 v1.6.1

## Overview

This project turns the **LilyGO LoRa32 T3 v1.6.1** (ESP32 + SX1276) into a multi-tasked sensor node and dashboard.
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
  (In Arduino IDE **Select** board: *Lilygo T-display*.)
  
- **Display**: SSD1306 OLED (I²C, 128×64)

**Default Connections**  
- OLED         → ESP32 I²C (SDA/SCL)  
- LoRa SX1276  → VSPI
- SD           → HSPI
<p align="center">
  <img
    src="https://github.com/user-attachments/assets/ae2f6752-aafb-44e8-8195-7d9c03055b27"
    alt="LilyGO LoRa32 T3 v1.6.1 pinmap"
    width="300"
  />
</p>  

## Software Dependencies

Install these via the Arduino Library Manager:

- **WiFi**             (builtin)
- **WebServer**        (builtin)
- **SPI**              (builtin)
- **FS**               (builtin)
- **SD.h**             (builtin)
- **Adafruit_GFX**     by Adafruit
- **Adafruit_SSD1306** by Adafruit
- **LoRa**             by Sandeep Mistry  

## Installation & Upload

1. **Clone this repo**
   Instance:
   ```bash 
   git clone https://github.com/Ali-Haghayegh-Jahromi/LoRa32-FreeRTOS-Demo.git
   cd LoRa32-FreeRTOS-Demo
