## Overview

This project turns the TTGO LoRa32 T-3 (ESP32 + SX1276) into a multi-tasked sensor node and dashboard:

- 🛰️ **LoRa**: send/receive messages every second  
- 📺 **OLED Display**: scrolls uptime, IP address, and recent Rx packets  
- 🌐 **WiFi + HTTP**: hosts a simple form to change the next LoRa-Tx message  
- **FreeRTOS**: isolated tasks with mutex-protected SPI access
- **SDIO**: under construction!🛠️
