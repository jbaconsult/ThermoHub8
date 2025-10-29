# Thermohub8 - ESP32 PT1000 Sensor Monitoring System

Ein ESP32-basiertes System zum Auslesen von PT1000-Temperatursensoren über Modbus mit LCD-Display und REST-API.

## Features

- ✅ Auslesen von 8 PT1000-Sensoren über Modbus (konfigurierbar)
- ✅ LCD-Display (16x4) mit Joystick-Steuerung zum Scrollen
- ✅ REST-API für Sensordaten und Konfiguration
- ✅ Persistente Speicherung der Sensornamen im ESP32-Flash
- ✅ Web-Interface zur Statusanzeige
- ✅ Automatische Aktualisierung alle 1000ms

## Hardware-Anforderungen

### Komponenten
- ESP32 Development Board
- MAX485 I2C Modbus-Modul
- PT1000-Sensoren mit Modbus-Konverter
- LCD 16x4 mit I2C-Interface (PCF8574)
- Analog-Joystick (2-Achsen + Taster)

### Pin-Belegung

#### Modbus (RS485)
- TX: GPIO 17
- RX: GPIO 16
- DE/RE: GPIO 4

#### I2C (LCD)
- SDA: GPIO 21
- SCL: GPIO 22

#### Joystick
- X-Achse: GPIO 34 (ADC)
- Y-Achse: GPIO 35 (ADC)
- Taster: GPIO 32

## Software-Anforderungen

### Arduino IDE
- Arduino IDE 1.8.x oder 2.x
- ESP32 Board Support Package

### Erforderliche Bibliotheken

Installieren Sie folgende Bibliotheken über den Arduino Library Manager:

```
- ESP32 by Espressif Systems
- ESPAsyncWebServer by me-no-dev
- AsyncTCP by me-no-dev
- ArduinoJson by Benoit Blanchon (Version 6.x)
- ModbusMaster by Doc Walker
- LiquidCrystal_I2C by Frank de Brabander
```

## Installation

1. **Arduino IDE vorbereiten**
   - Installieren Sie die ESP32 Board-Unterstützung
   - Installieren Sie alle erforderlichen Bibliotheken

2. **Projekt herunterladen**
   ```bash
   git clone <repository-url>
   cd Thermohub8
   ```

3. **WLAN-Konfiguration anpassen**
   
   Öffnen Sie `Thermohub8.ino` und passen Sie folgende Zeilen an:
   ```cpp
   const char* WIFI_SSID = "IhrWLAN-Name";
   const char* WIFI_PASSWORD = "IhrWLAN-Passwort";
   ```

4. **Hochladen**
   - Wählen Sie das richtige ESP32-Board aus
   - Wählen Sie den richtigen COM-Port
   - Klicken Sie auf "Upload"

5. **Serielle Konsole öffnen**
   - Öffnen Sie den Serial Monitor (115200 Baud)
   - Notieren Sie die IP-Adresse des ESP32

## Konfiguration

### Anzahl der Sensoren ändern

In `Thermohub8.ino`:
```cpp
#define NUM_SENSORS 8  // Ändern Sie diesen Wert
```

### Modbus-Einstellungen

```cpp
#define MODBUS_SLAVE_ID 1           // Slave-ID des Modbus-Konverters
#define MODBUS_START_REGISTER 0x48  // Startregister (72 dezimal)
#define MODBUS_BAUDRATE 9600        // Baudrate
```

### Joystick-Kalibrierung

Falls der Joystick nicht korrekt reagiert, passen Sie die Schwellwerte an:

```cpp
#define JOY_MIN_VAL 1200      // Minimalwert
#define JOY_MAX_VAL 4095      // Maximalwert
#define JOY_CENTER_VAL 2559   // Mittelwert
#define JOY_DEADZONE 300      // Totbereich
```

Optional können Sie die Achsen invertieren:
```cpp
void initJoystick() {
    // ...
    joystick.setInvertX(true);  // X-Achse invertieren
    joystick.setInvertY(true);  // Y-Achse invertieren
}
```

**Debug-Modus aktivieren:**

Für die Fehlersuche können Sie den Joystick Debug-Modus aktivieren:
```cpp
void initJoystick() {
    // ...
    joystick.setDebugMode(true);  // Debug-Ausgaben aktivieren
}
```

Im Debug-Modus werden folgende Informationen über die serielle Konsole ausgegeben:
- Rohwerte von X- und Y-Achse (alle 500ms)
- Aktuelle Position des Joysticks
- Delta-Werte vom Zentrum (alle 1000ms)
- Positions-Änderungen in Echtzeit
- Switch-Ereignisse

Beispiel-Ausgabe:
```
=== Joystick Debug-Modus aktiviert ===
Joystick Raw: X=2559 Y=2559 | Position: CENTER
Calculate: deltaX=0 deltaY=0 | abs(deltaX)=0 abs(deltaY)=0
>>> Position geändert: CENTER -> UP
>>> Joystick Switch gedrückt
```

## Bedienung

### LCD-Display

- **Joystick hoch**: Eine Zeile nach oben scrollen
- **Joystick runter**: Eine Zeile nach unten scrollen
- **Joystick-Taster**: Reserviert für zukünftige Funktionen

Das Display zeigt:
```
Sensorname  XX.X°C
```

### Web-Interface

Öffnen Sie im Browser:
```
http://thermohub8.local/
```
oder
```
http://<ESP32-IP-Adresse>/
```

Die Status-Seite zeigt alle Sensoren mit aktuellen Werten und aktualisiert sich automatisch alle 5 Sekunden.

## REST-API

### Sensordaten abrufen

**Endpunkt:** `GET /api/v1/sensordata`

**Antwort:**
```json
{
  "sensors": [
    {
      "id": 0,
      "name": "Sensor 1",
      "temperature": 23.5,
      "unit": "°C"
    },
    {
      "id": 1,
      "name": "Vorlauf",
      "temperature": 45.2,
      "unit": "°C"
    }
  ]
}
```

**Beispiel mit curl:**
```bash
curl http://thermohub8.local/api/v1/sensordata
```

**Beispiel mit Python:**
```python
import requests

response = requests.get('http://thermohub8.local/api/v1/sensordata')
data = response.json()

for sensor in data['sensors']:
    print(f"{sensor['name']}: {sensor['temperature']}°C")
```

### Sensorname ändern

**Endpunkt:** `POST /api/v1/sensor`

**Request Body:**
{
  "id": 0,
  "name": "Vorlauf Heizung"
}
```json
```

**Antwort:**
```json
{
  "success": true,
  "id": 0,
  "name": "Vorlauf Heizung"
}
```

**Beispiel mit curl:**
```bash
curl -X POST http://thermohub8.local/api/v1/sensor \
  -H "Content-Type: application/json" \
  -d '{"id": 0, "name": "Vorlauf Heizung"}'
```

**Beispiel mit Python:**
```python
import requests

data = {
    "id": 0,
    "name": "Vorlauf Heizung"
}

response = requests.post('http://thermohub8.local/api/v1/sensor', json=data)
print(response.json())
```

**Beispiel mit JavaScript:**
```javascript
fetch('http://thermohub8.local/api/v1/sensor', {
    method: 'POST',
    headers: {
        'Content-Type': 'application/json'
    },
    body: JSON.stringify({
        id: 0,
        name: 'Vorlauf Heizung'
    })
})
.then(response => response.json())
.then(data => console.log(data));
```

## Modbus-Register

Das System liest die Temperaturdaten wie folgt:

| Sensor | Register (Hex) | Register (Dez) | Datentyp |
|--------|----------------|----------------|----------|
| 0      | 0x48-0x49      | 72-73          | Float32  |
| 1      | 0x4A-0x4B      | 74-75          | Float32  |
| 2      | 0x4C-0x4D      | 76-77          | Float32  |
| 3      | 0x4E-0x4F      | 78-79          | Float32  |
| 4      | 0x50-0x51      | 80-81          | Float32  |
| 5      | 0x52-0x53      | 82-83          | Float32  |
| 6      | 0x54-0x55      | 84-85          | Float32  |
| 7      | 0x56-0x57      | 86-87          | Float32  |

Jeder Sensor verwendet 2 Modbus-Register (32-bit Float, Big Endian).
Zwischen den Sensoren wird ein Register übersprungen.

## Fehlersuche

### LCD zeigt nichts an
- Überprüfen Sie die I2C-Adresse (Standard: 0x27)
- Testen Sie mit einem I2C-Scanner
- Überprüfen Sie die Verkabelung (SDA/SCL)

### Keine Modbus-Daten
- Überprüfen Sie die RS485-Verkabelung
- Prüfen Sie Slave-ID und Baudrate
- Überprüfen Sie das Startregister
- Messen Sie die Spannung am DE/RE-Pin während der Übertragung

### WLAN-Verbindung schlägt fehl
- Überprüfen Sie SSID und Passwort
- Stellen Sie sicher, dass 2.4 GHz WLAN verwendet wird (ESP32 unterstützt kein 5 GHz)
- Überprüfen Sie die Signalstärke

### Joystick reagiert nicht korrekt
- Öffnen Sie den Serial Monitor und beobachten Sie die Rohwerte
- Kalibrieren Sie die Schwellwerte entsprechend
- Invertieren Sie ggf. die Achsen

## Lizenz

Dieses Projekt steht unter der MIT-Lizenz.

## Autor

Erstellt für das Thermohub8-Projekt

## Version

1.0.0 - Erste Version