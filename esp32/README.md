# Thermohub8 - ESP32 PT1000 Temperature Monitoring System

A professional ESP32-based system for reading PT1000 temperature sensors via Modbus RTU with LCD display and REST API.

## Features

- Read up to 8 PT1000 sensors via Modbus RTU (configurable)
- 16x4 LCD display with joystick navigation
- REST API for sensor data and configuration
- Persistent sensor name storage in ESP32 flash
- Web interface with auto-refresh
- Status LED for Modbus activity indication

## Hardware Requirements

### Components

- ESP32 Development Board
- MAX485 RS485 to TTL Module
- PT1000 sensors with Modbus converter
- 16x4 LCD with I2C interface (PCF8574)
- Analog joystick (2-axis + button)
- Status LED with 220-330Ω resistor

### Wiring Diagram

```
ESP32 Pin          →  Component
═══════════════════════════════════════════════════════════

Modbus (MAX485):
GPIO 16 (TX2)      →  MAX485 DI
GPIO 17 (RX2)      →  MAX485 RO
GPIO 4             →  MAX485 DE & RE (connected together)
3.3V/5V            →  MAX485 VCC
GND                →  MAX485 GND
                      MAX485 A  →  Modbus Device A
                      MAX485 B  →  Modbus Device B

LCD Display (I2C):
GPIO 21 (SDA)      →  LCD SDA
GPIO 22 (SCL)      →  LCD SCL
5V                 →  LCD VCC
GND                →  LCD GND

Joystick:
GPIO 34            →  Joystick VRx (X-axis)
GPIO 35            →  Joystick VRy (Y-axis)
GPIO 32            →  Joystick SW (Button)
3.3V               →  Joystick +5V
GND                →  Joystick GND

Status LED:
GPIO 2             →  LED Anode (+)
                      LED Cathode (-) → [330Ω] → GND
```

**Important Notes:**
- MAX485 DE and RE pins must be connected together
- Use twisted pair cable for Modbus A/B connections
- Joystick works with 3.3V, no level shifting needed
- Status LED resistor: 220-330Ω

## Software Requirements

### Arduino IDE

- Arduino IDE 1.8.13 or newer (2.x recommended)
- ESP32 Board Support: Add to Board Manager URLs:
  ```
  https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
  ```

### Required Libraries

Install via Arduino Library Manager:

| Library | Author | Version |
|---------|--------|---------|
| ESPAsyncWebServer | me-no-dev | 1.2.3+ |
| AsyncTCP | me-no-dev | 1.1.1+ |
| ArduinoJson | Benoit Blanchon | 6.21.0+ |
| ModbusMaster | Doc Walker | 2.0.1+ |
| LiquidCrystal_I2C | Frank de Brabander | 1.1.2+ |

## Installation

1. **Install Arduino IDE and ESP32 support**
2. **Install all required libraries**
3. **Download Thermohub8 project**
4. **Configure WiFi credentials** (lines 49-50 in Thermohub8.ino):
   ```cpp
   const char *WIFI_SSID = "YourWiFiName";
   const char *WIFI_PASSWORD = "YourWiFiPassword";
   ```
5. **Wire hardware** according to diagram above
6. **Select board**: Tools → Board → ESP32 Dev Module
7. **Select port**: Tools → Port → (your COM port)
8. **Upload**: Click upload button

## Configuration

### Basic Settings

Edit these values in `Thermohub8.ino`:

```cpp
// Number of sensors (line 42)
#define NUM_SENSORS 8

// Modbus configuration (lines 60-63)
#define MODBUS_SLAVE_ID 1              // Your device ID
#define MODBUS_START_REGISTER 0x30     // Start register (48 decimal)
#define MODBUS_BAUDRATE 9600           // Communication speed

// LCD I2C address (line 69)
#define LCD_I2C_ADDR 0x27              // Try 0x3F if 0x27 doesn't work

// Joystick calibration (lines 81-84)
#define JOY_MIN_VAL 0
#define JOY_MAX_VAL 4095
#define JOY_CENTER_VAL 2000
#define JOY_DEADZONE 500
```

### Joystick Calibration

If joystick doesn't respond correctly:

1. Enable debug mode in `initJoystick()`:
   ```cpp
   joystick.setDebugMode(true);
   ```
2. Open Serial Monitor (115200 baud)
3. Move joystick to all positions and note values
4. Update configuration accordingly

To invert axes:
```cpp
// In initJoystick() function:
joystick.setInvertY(true);  // Already enabled by default
```

## Usage

### LCD Display

**Navigation:**
- Push joystick UP: Scroll up
- Push joystick DOWN: Scroll down

**Display Format:**
```
SensName  23.5°C
Sensor 2  45.2°C
Outside   -5.3°C
Boiler   105.7°C
```

After scrolling through all sensors, system information is shown:
```
================
IP-Address:
192.168.1.100
Version:     1.0
```

### Web Interface

Access via browser:
- `http://thermohub8.local/` (mDNS)
- `http://[IP-ADDRESS]/` (direct IP)

The status page shows all sensors with auto-refresh every 5 seconds.

### REST API

#### Get Sensor Data

```bash
GET /api/v1/sensordata
```

**Response:**
```json
{
  "sensors": [
    {
      "id": 0,
      "name": "Sensor 1",
      "value": 23.5,
      "unit": "°C"
    }
  ]
}
```

**Example:**
```bash
curl http://thermohub8.local/api/v1/sensordata
```

#### Update Sensor Name

```bash
POST /api/v1/sensor
Content-Type: application/json

{
  "id": 0,
  "name": "Living Room"
}
```

**Example:**
```bash
curl -X POST http://thermohub8.local/api/v1/sensor \
  -H "Content-Type: application/json" \
  -d '{"id": 0, "name": "Living Room"}'
```

**Response:**
```json
{
  "success": true,
  "id": 0,
  "name": "Living Room"
}
```

## Modbus Register Map

The system reads 32-bit float values (Big Endian) from Modbus registers:

| Sensor | Registers | Hex | Decimal |
|--------|-----------|-----|---------|
| 0 | 2 registers | 0x30-0x31 | 48-49 |
| 1 | 2 registers | 0x32-0x33 | 50-51 |
| 2 | 2 registers | 0x34-0x35 | 52-53 |
| 3 | 2 registers | 0x36-0x37 | 54-55 |
| 4 | 2 registers | 0x38-0x39 | 56-57 |
| 5 | 2 registers | 0x3A-0x3B | 58-59 |
| 6 | 2 registers | 0x3C-0x3D | 60-61 |
| 7 | 2 registers | 0x3E-0x3F | 62-63 |

**Note:** Registers are spaced 2 apart (every other register is skipped).

## Troubleshooting

### LCD Shows Nothing

- Check I2C wiring (SDA to GPIO 21, SCL to GPIO 22)
- Try different I2C address (0x3F instead of 0x27)
- Adjust contrast potentiometer on LCD module
- Verify 5V power to LCD

### No Modbus Communication

- Check status LED: Should blink every second
- Verify RS485 wiring: A to A, B to B
- Try swapping A and B connections
- Check slave ID matches device
- Verify baud rate matches device (9600)
- Ensure start register is correct (0x30)

### WiFi Connection Failed

- Verify SSID and password (case-sensitive!)
- Ensure 2.4 GHz WiFi (ESP32 doesn't support 5 GHz)
- Check WiFi signal strength
- Try increasing connection timeout in code

### Joystick Not Responding

- Enable debug mode and check Serial Monitor
- Verify wiring: VRx→GPIO34, VRy→GPIO35, SW→GPIO32
- Adjust deadzone if drifting
- Calibrate min/max/center values

### Web Interface Not Loading

- Verify ESP32 is connected to WiFi (check LCD)
- Try direct IP address instead of hostname
- Check firewall settings
- Ensure same network as computer/phone

## Technical Specifications

### Performance
- Main loop: 100 Hz (10ms cycle)
- Modbus update: 1 Hz (1000ms interval)
- Sensor read time: ~80ms per sensor
- Web response: <50ms

### Memory Usage
- Flash: ~800 KB
- RAM: ~75 KB
- Available Flash: ~3.2 MB
- Available RAM: ~445 KB

### Power Consumption
- ESP32 (WiFi active): 80 mA
- LCD with backlight: 20-30 mA
- MAX485: 5 mA
- Total: ~110 mA @ 5V

### Communication
- Modbus: RTU, 9600 baud, 8N1
- I2C: 100 kHz standard mode
- WiFi: 802.11 b/g/n (2.4 GHz only)
- HTTP: Port 80, JSON format

## API Integration Examples

### Python
```python
import requests

# Get sensor data
response = requests.get('http://thermohub8.local/api/v1/sensordata')
data = response.json()

for sensor in data['sensors']:
    print(f"{sensor['name']}: {sensor['value']}°C")

# Update sensor name
requests.post('http://thermohub8.local/api/v1/sensor',
              json={'id': 0, 'name': 'Living Room'})
```

### JavaScript
```javascript
// Get sensor data
fetch('http://thermohub8.local/api/v1/sensordata')
  .then(response => response.json())
  .then(data => {
    data.sensors.forEach(sensor => {
      console.log(`${sensor.name}: ${sensor.value}°C`);
    });
  });

// Update sensor name
fetch('http://thermohub8.local/api/v1/sensor', {
  method: 'POST',
  headers: { 'Content-Type': 'application/json' },
  body: JSON.stringify({ id: 0, name: 'Living Room' })
});
```

### Home Assistant
```yaml
sensor:
  - platform: rest
    name: "Living Room Temperature"
    resource: "http://thermohub8.local/api/v1/sensordata"
    value_template: "{{ value_json.sensors[0].value }}"
    unit_of_measurement: "°C"
    scan_interval: 30
```

## Safety Notes

⚠️ **Important:**
- This system is for monitoring only, not safety-critical applications
- Use certified 5V power supplies
- PT1000 sensors should be installed by qualified personnel
- Verify readings against calibrated instruments
- Not suitable for life-safety or fire detection

## License

MIT License - See LICENSE file for details.

## Author

**Johannes**  
Version 1.0 - 2025

## Support

For issues or questions:
1. Check this documentation
2. Review troubleshooting section
3. Open GitHub issue with detailed information

---

**Quick Start:** Wire hardware → Install libraries → Configure WiFi → Upload → Access web interface at http://thermohub8.local/