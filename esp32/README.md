# ThermoHub8 – ESP32 Firmware (RS-485/Modbus → REST API → Home Assistant)

Dieses Projekt liest bis zu **8 Sensorwerte** per **Modbus RTU (RS-485/MAX485)** mit einem **ESP32**, zeigt sie auf einem **I²C-LCD (16×4)** an, stellt sie über eine **REST-API** unter `/api/v1/readings` bereit und bietet eine **Weboberfläche** zum **Benennen** der Sensoren (Persistenz im Flash/NVS).  
Die API ist kompatibel zur Home-Assistant-Custom-Integration „ThermoHub8“.

---

## Inhaltsverzeichnis

- [Funktionen](#funktionen)  
- [Hardware](#hardware)  
- [Pinbelegung & Verdrahtung](#pinbelegung--verdrahtung)  
- [Bibliotheken](#bibliotheken)  
- [Konfiguration](#konfiguration)  
- [Build & Flash](#build--flash)  
- [REST API](#rest-api)  
- [Web UI](#web-ui)  
- [Persistenz (NVS)](#persistenz-nvs)  
- [LCD & Tasten](#lcd--tasten)  
- [mDNS](#mdns)  
- [Beispiel-JSON](#beispiel-json)  
- [Sicherheit (optional)](#sicherheit-optional)  
- [Troubleshooting](#troubleshooting)  
- [Lizenz](#lizenz)

---

## Funktionen

- **Modbus RTU über RS-485** (MAX485) – liest bis zu 8 Sensoren (ID 1…8; Register konfigurierbar).  
- **REST-API** `/api/v1/readings` liefert JSON für Home Assistant.  
- **Weboberfläche** `/` zum Umbenennen der Sensoren; **POST** `/api/v1/names` speichert Namen.  
- **LCD 16×4** zeigt Sensorwerte an, **Tasten (UP/DOWN)** blättern.  
- **Persistenz** der Sensornamen via NVS (Flash).  
- **mDNS** (z. B. `http://thermohub.local/`) für bequemen Zugriff.  

> Hinweis zu Pins: Ursprünglich war GPIO4 doppelt verplant (RS-485 & I²C). In dieser Firmware liegt **RS-485: TX=18, RX=19, DE/RE=23**, **I²C: SDA=4, SCL=5** – somit keine Kollision.

---

## Hardware

- **ESP32 DevKit** (z. B. ESP32-WROOM-32)  
- **MAX485** RS-485-Transceiver  
- **I²C-LCD 16×4** (z. B. PCF8574-Backpack, Adresse meist `0x27` oder `0x3F`)  
- **2 Taster** (UP/DOWN), intern Pull-Ups genutzt  
- Verkabelung für RS-485 (A/B), I²C (SDA/SCL), Stromversorgung

---

## Pinbelegung & Verdrahtung

| Funktion            | Pin ESP32 | Hinweis                                   |
|---------------------|-----------|-------------------------------------------|
| RS-485 TX           | GPIO **18** | UART1 TX                                  |
| RS-485 RX           | GPIO **19** | UART1 RX                                  |
| RS-485 DE/RE        | GPIO **23** | HIGH = Senden, LOW = Empfangen             |
| I²C SDA (LCD)       | GPIO **4**  | I²C Daten                                  |
| I²C SCL (LCD)       | GPIO **5**  | I²C Takt                                   |
| Button UP           | GPIO **25** | `INPUT_PULLUP`, aktiv **LOW**              |
| Button DOWN         | GPIO **26** | `INPUT_PULLUP`, aktiv **LOW**              |

**RS-485 A/B** mit dem Sensor-Bus verbinden.  
**I²C-LCD** an 3V3/GND/SDA/SCL.  
**Taster** zwischen Pin und GND (Pull-Ups sind intern aktiv).

---

## Bibliotheken

Bitte im Arduino-IDE Bibliotheksverwalter oder via Git installieren:

- **ESPAsyncWebServer** und **AsyncTCP**  
- **LiquidCrystal_I2C** (oder kompatibel)  
- **ModbusMaster**  
- **ArduinoJson**

> Alternativ mit **PlatformIO** (siehe unten), dort werden Libs via `platformio.ini` verwaltet.

---

## Konfiguration

Öffne die Sketch-Datei und passe die Konstanten an:

```cpp
#define WIFI_SSID       "DEINE_SSID"
#define WIFI_PASS       "DEIN_PASSWORT"
#define MDNS_NAME       "thermohub"    // -> http://thermohub.local/

// Modbus
#define MODBUS_BAUD     9600
#define MODBUS_CONFIG   SERIAL_8N1
#define MODBUS_START_REG  0x0000        // Startregister
#define MODBUS_READ_COUNT 1             // Anzahl Register
// In readSensorValue(): ggf. Skalierung anpassen (z.B. raw/10.0f)

// LCD
#define LCD_I2C_ADDR    0x27            // ggf. 0x3F
#define LCD_COLS        16
#define LCD_ROWS        4

// Polling
#define POLL_INTERVAL_MS 1000           // 1s
```

---

## Build & Flash

### Arduino IDE

1. **Board**: *ESP32 Dev Module*  
2. **Port**: seriellen Port des ESP32 wählen  
3. **PSRAM**: aus (nicht erforderlich)  
4. **Sketch kompilieren & hochladen**

### PlatformIO (empfohlen)

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200

lib_deps =
  me-no-dev/ESP Async WebServer @ ^1.2.3
  me-no-dev/AsyncTCP @ ^1.1.1
  marcoschwartz/LiquidCrystal_I2C @ ^1.1.4
  4-20ma/ModbusMaster @ ^2.0.1
  bblanchon/ArduinoJson @ ^7
```

---

## REST API

### `GET /api/v1/readings`

```json
{
  "sensors": [
    {"id":1,"name":"Wohnzimmer","value":21.3,"unit":"°C"}
  ],
  "ts": "1970-01-01T00:00:00Z"
}
```

### `POST /api/v1/names`

```json
{ "1": "Wohnzimmer", "2": "Küche" }
```

---

## Web UI

- **`GET /`** liefert HTML-Seite zum Umbenennen der Sensoren.  
- Speichern über **POST** `/api/v1/names`.  

---

## Persistenz (NVS)

- Namen mit **Preferences** in `thermohub/name1`…`name8`.  
- Boot lädt diese, sonst Defaults `Sensor 1`…`8`.

---

## LCD & Tasten

- LCD zeigt 3 Sensoren gleichzeitig an.  
- UP/DOWN-Buttons scrollen.

---

## mDNS

- Zugriff via `http://thermohub.local/`  
- Falls `.local` nicht geht: IP-Adresse verwenden.

---

## Beispiel-JSON

```json
{
  "sensors": [
    { "id": 1, "name": "Wohnzimmer", "value": 21.3, "unit": "°C" },
    { "id": 2, "name": "Küche", "value": 22.5, "unit": "°C" }
  ],
  "ts": "2025-09-15T12:34:56Z"
}
```

---

## Sicherheit (optional)

- API-Key über `Authorization: Bearer <KEY>` prüfen.  

---

## Troubleshooting

- **Keine Werte:** Modbus-Parameter prüfen (Baudrate, Slave-ID, Register).  
- **LCD leer:** Adresse (0x27/0x3F) oder SDA/SCL prüfen.  
- **.local nicht erreichbar:** IP nutzen.  
- **Zeit „1970…“:** NTP in Firmware ergänzen.  

---

## Lizenz

Lizensiert unter Apache2
