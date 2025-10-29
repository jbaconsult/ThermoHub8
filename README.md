# ThermoHub8 – Open Source Modular Temperature Hub

ThermoHub8 is an open-source hardware and software project designed to connect **up to 8 temperature sensors** (via Modbus/RS-485) to an **ESP32** microcontroller.  
The device publishes all sensor readings through a **REST API** and integrates seamlessly with **Home Assistant** via a custom integration.

The goal of this project is to provide a **reliable, local, and customizable temperature monitoring system** for smart home and industrial environments — with **no cloud dependency**.

---

## 🧩 Project Overview

ThermoHub8 consists of three major parts:

1. **ESP32 Firmware**  
   - Communicates via Modbus (RS-485) with up to 8 PT1000 temperature sensors  
   - Serves a local REST API (`/api/v1/readings`)  
   - Includes a built-in web UI for naming sensors  
   - Displays readings on an LCD (16x4 I²C display)  
   - Supports joystick control for local navigation  

2. **Home Assistant Custom Integration**  
   - Polls the ThermoHub8 REST API  
   - Displays all 8 sensors as entities in Home Assistant  
   - Allows configuration via UI (Config Flow)  
   - Supports custom update intervals, API key, and SSL settings  

3. **Hardware Assembly**  
   - Based on ESP32 + MAX485 converter  
   - Uses Modbus PT1000 sensor modules  
   - LCD display and joystick for local display and control  
   - Powered via DC converter (e.g., 12V → 5V)

---

## 🔧 Components

| Component | Description | Notes / Links (to be added) |
|------------|--------------|-----------------------------|
| **ESP32** | Main controller with Wi-Fi and serial interface | [Link] |
| **MAX485 module** | RS-485 transceiver for Modbus communication | [Link] |
| **PT1000 Modbus modules** | 8 temperature input converters (Modbus RTU) | [Link] |
| **Joystick module** | Analog joystick for navigation (X/Y via ADC) | [Link] |
| **LCD 16x4 I²C display** | Local temperature display | [Link] |
| **DC power converter** | Converts 12V DC to 5V for ESP32 and peripherals | [Link] |

> 💡 Add pictures or wiring diagrams here once available.  
> Suggested folder: `docs/images/`  
> Example: `![ThermoHub8 Overview](docs/images/system_overview.jpg)`

---

## ⚙️ Features Summary

- 8 Modbus-connected temperature sensors (PT1000)
- REST API output for easy integration
- Web configuration UI for sensor naming
- LCD display with joystick navigation
- Full Home Assistant integration
- No cloud dependency
- Open hardware and open firmware
- Logging support via serial and Home Assistant

---

## 🧠 System Architecture

```
+------------------------+
| PT1000 Sensor Modules  |  (Modbus RTU)
+----------+-------------+
           |
           | RS-485 (A/B)
           |
     +-----+-----+
     |   MAX485  |
     +-----+-----+
           |
           | UART1 (RX=19, TX=18)
           |
       +---+---+
       |  ESP32 |
       |         |
       |  - Reads Modbus
       |  - Hosts REST API
       |  - Runs Web UI
       |  - Updates LCD
       +---+---+
           |
           | I²C (SDA=4, SCL=5)
           |
     +-----+------+
     | LCD 16x4   |
     +------------+
           |
           | ADC (X=32, Y=33)
           |
     +-----+------+
     | Joystick   |
     +------------+
```

---

## 🧰 Firmware

The firmware is written in **C++ (Arduino / ESP32)** and uses the following libraries:

- `ESPAsyncWebServer`
- `AsyncTCP`
- `ModbusMaster`
- `LiquidCrystal_I2C`
- `ArduinoJson`

### Features:
- Single Modbus slave (configurable ID)
- Reads 8 float values (32-bit) from 16 registers
- REST API endpoint: `/api/v1/readings`
- Web interface for naming sensors (`/`)
- Persistent storage via `Preferences` (NVS)
- LCD navigation with joystick input
- Logging via Serial output

Firmware source:  
`/firmware/thermohub8_esp32/`

Example JSON output:

```json
{
  "sensors": [
    { "id": 1, "name": "Living Room", "value": 21.3, "unit": "°C" },
    { "id": 2, "name": "Bedroom", "value": 19.8, "unit": "°C" }
  ],
  "ts": "2025-09-15T12:34:56Z"
}
```

---

## 🏠 Home Assistant Integration

A dedicated Home Assistant integration (`custom_components/thermohub8/`) is included.

### Key Features
- Auto-discovery of all 8 sensors  
- Adjustable polling interval (1–60 s)  
- Configurable base URL, SSL verification, and API key  
- Logging and diagnostics support  

Integration documentation:  
[`/homeassistant/README_ThermoHub8.md`](./homeassistant/README_ThermoHub8.md)

---

## ⚡ Power Supply

- Input: 12 V DC (typical)
- Conversion: via DC-DC step-down converter (to 5 V DC)
- Powering ESP32, LCD, MAX485, and sensors
- Typical power draw: 150–250 mA depending on LCD backlight and Modbus load

---

## 🧱 Assembly Notes

- Keep RS-485 wiring twisted and shielded for stability.  
- Terminate RS-485 bus with 120 Ω resistors on both ends.  
- Use short, clean I²C and analog lines for the joystick and display.  
- Mount ESP32 on a perforated PCB or custom carrier board.  

---

## 🔒 License

### 📜 MIT License

```
MIT License

Copyright (c) 2025 [Your Name]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 🤝 Contributing

Pull requests are welcome!  
If you find a bug, have an idea, or want to improve the documentation, feel free to fork the repository and submit a PR.

---

## 🧩 To-Do / Roadmap

- [ ] PCB design for cleaner wiring  
- [ ] Optional OLED support  
- [ ] MQTT output mode  
- [ ] Configurable Modbus ID via web UI  
- [ ] OTA firmware updates  

---

## 📸 Gallery (to be added)

Place your images here, for example:

```
docs/images/
├─ overview.jpg
├─ wiring.jpg
├─ assembled_front.jpg
├─ assembled_back.jpg
```
