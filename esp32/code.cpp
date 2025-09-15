/*
 * ThermoHub8 – ESP32 Firmware
 * - Modbus RTU über RS-485 (MAX485)
 * - REST API kompatibel zur Home-Assistant ThermoHub8-Integration
 * - Web UI zum Benennen der 8 Sensoren (Persistenz via NVS/Preferences)
 * - LCD 16x4 I2C Anzeige mit Up/Down Buttons
 *
 * Pins:
 *   RS485: TX=18, RX=19, DE/RE=23  (DE/RE High=Senden, Low=Empfangen)
 *   I2C LCD: SDA=4, SCL=5, Adresse typ. 0x27
 *   Buttons: UP=25, DOWN=26 (internes Pull-up; aktiv LOW)
 *
 * Anpasspunkte:
 *   - WLAN SSID/PASS
 *   - MODBUS_* Konstanten (Slave-IDs, Register, etc.)
 *   - LCD_I2C_ADDR ggf. 0x3F statt 0x27
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <ModbusMaster.h>

// === Web Server (Async) ===
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>

// -------------------- Konfiguration --------------------
#define WIFI_SSID       "DEINE_SSID"
#define WIFI_PASS       "DEIN_PASSWORT"
#define MDNS_NAME       "thermohub"

// RS485 / Modbus
#define RS485_TX_PIN    18
#define RS485_RX_PIN    19
#define RS485_DE_RE_PIN 23

// I2C LCD
#define I2C_SDA_PIN     4
#define I2C_SCL_PIN     5
#define LCD_I2C_ADDR    0x27
#define LCD_COLS        16
#define LCD_ROWS        4

// Buttons (aktiv LOW, interner Pullup)
#define BTN_UP_PIN      25
#define BTN_DOWN_PIN    26

// Sensor-Konfiguration
#define SENSOR_COUNT    8
static const char* DEFAULT_UNIT = "°C";

// Modbus Einstellungen (Beispiel! Anpassen an deine Sensoren)
#define MODBUS_BAUD     9600
#define MODBUS_CONFIG   SERIAL_8N1
// Für dieses Beispiel: Wir gehen von 8 Slaves mit IDs 1..8 aus,
// und lesen je Sensor Holding Register 0x0000 (1 Register, 16-bit).
// Passe das an deine echten Geräte/Adressen an:
#define MODBUS_START_REG  0x0000
#define MODBUS_READ_COUNT 1

// Poll-Intervall
#define POLL_INTERVAL_MS 1000

// ------------------ Globale Objekte --------------------
HardwareSerial RS485Serial(1); // UART1 auf ESP32
ModbusMaster modbus;

// LCD
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

// Web
AsyncWebServer server(80);
Preferences prefs;

// Sensorzustand
struct SensorData {
  uint8_t id;
  String name;
  float value;
  String unit;
  bool valid;
};

SensorData sensors[SENSOR_COUNT];
uint8_t currentIndex = 0; // für LCD Scroll
unsigned long lastPoll = 0;

// ------------------ RS485 Helfer -----------------------
void preTransmission() {
  digitalWrite(RS485_DE_RE_PIN, HIGH); // senden
}

void postTransmission() {
  digitalWrite(RS485_DE_RE_PIN, LOW);  // empfangen
}

// ------------------ Utilities --------------------------
String iso8601NowUTC() {
  // Einfache UTC Zeit aus millis() gibt es nicht zuverlässig -> NTP weglassen,
  // stattdessen rudimentär "Z" Zeit mit Uptime. Für Produktivbetrieb: NTP einbauen!
  // Hier nur Demo:
  unsigned long ms = millis();
  unsigned long sec = ms / 1000UL;
  char buf[32];
  snprintf(buf, sizeof(buf), "1970-01-01T%02lu:%02lu:%02luZ",
           (sec / 3600UL) % 24UL,
           (sec / 60UL) % 60UL,
           sec % 60UL);
  return String(buf);
}

void loadNames() {
  prefs.begin("thermohub", true);
  for (int i = 0; i < SENSOR_COUNT; i++) {
    char key[8];
    snprintf(key, sizeof(key), "name%d", i + 1);
    String defName = String("Sensor ") + String(i + 1);
    sensors[i].name = prefs.getString(key, defName);
  }
  prefs.end();
}

void saveName(uint8_t idx, const String& name) {
  prefs.begin("thermohub", false);
  char key[8];
  snprintf(key, sizeof(key), "name%d", idx + 1);
  prefs.putString(key, name);
  prefs.end();
}

// ------------------ Modbus Lesen -----------------------
bool readSensorValue(uint8_t slaveId, float &outValue) {
  // Setze Slave-ID
  modbus.begin(slaveId, RS485Serial);

  // Lese Holding Register
  preTransmission();
  uint8_t result = modbus.readHoldingRegisters(MODBUS_START_REG, MODBUS_READ_COUNT);
  postTransmission();

  if (result == modbus.ku8MBSuccess) {
    // Beispiel: 16-bit Wert, optional in °C interpretieren (Skalierung anpassen!)
    uint16_t raw = modbus.getResponseBuffer(0);
    // Bei Bedarf skalieren: z.B. 0.1°C Schritte: outValue = raw / 10.0f;
    outValue = (float)raw; // <--- anpassen!
    return true;
  }
  return false;
}

void pollAllSensors() {
  for (int i = 0; i < SENSOR_COUNT; i++) {
    float val = NAN;
    bool ok = readSensorValue(i + 1, val); // Slave IDs 1..8
    sensors[i].valid = ok;
    if (ok) {
      sensors[i].value = val;
      sensors[i].unit = DEFAULT_UNIT;
    }
  }
}

// ------------------ LCD Anzeige ------------------------
void lcdShowCurrentPage() {
  lcd.clear();

  // Zeile 0: Titel
  lcd.setCursor(0, 0);
  lcd.print("ThermoHub8");

  // Zeile 1..3: jeweils 1 Sensor pro Zeile, Start bei currentIndex
  // Wir zeigen bis zu 3 Einträge
  for (int row = 1; row <= 3; row++) {
    int idx = currentIndex + (row - 1);
    if (idx >= SENSOR_COUNT) break;

    lcd.setCursor(0, row);
    // Format: "S1 Wohnz.  21.3C"
    char line[21];
    String nm = sensors[idx].name;
    if (nm.length() > 6) nm = nm.substring(0, 6); // kompakt
    if (sensors[idx].valid) {
      snprintf(line, sizeof(line), "S%d %-6s %5.1fC", idx + 1, nm.c_str(), sensors[idx].value);
    } else {
      snprintf(line, sizeof(line), "S%d %-6s  ----", idx + 1, nm.c_str());
    }
    lcd.print(line);
  }
}

void handleButtons() {
  static uint32_t lastDebounce = 0;
  uint32_t now = millis();
  if (now - lastDebounce < 150) return; // simple debounce

  int up = digitalRead(BTN_UP_PIN);
  int dn = digitalRead(BTN_DOWN_PIN);
  if (up == LOW) {
    if (currentIndex > 0) currentIndex--;
    lastDebounce = now;
    lcdShowCurrentPage();
  } else if (dn == LOW) {
    if (currentIndex < SENSOR_COUNT - 1) currentIndex++;
    lastDebounce = now;
    lcdShowCurrentPage();
  }
}

// ------------------ Web UI / API -----------------------
const char* INDEX_HTML = R"HTML(
<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>ThermoHub8</title>
<style>
body{font-family:system-ui,-apple-system,Segoe UI,Roboto,Ubuntu, Cantarell, Noto Sans, Helvetica, Arial, sans-serif;margin:20px;}
label{display:block;margin:8px 0 4px}
input[type=text]{width:100%;max-width:320px;padding:8px}
button{padding:8px 12px;margin-top:12px}
.card{border:1px solid #ddd;border-radius:8px;padding:12px;margin-bottom:12px}
</style>
</head>
<body>
<h1>ThermoHub8 – Sensor-Namen</h1>
<p>Benenne die Sensoren 1–8 und speichere die Einstellungen.</p>
<form id="f">
  <div id="cards"></div>
  <button type="submit">Speichern</button>
</form>
<script>
async function load(){
  const res = await fetch('/api/v1/readings');
  const js = await res.json();
  const cards = document.getElementById('cards');
  cards.innerHTML = '';
  js.sensors.forEach(s=>{
    const div = document.createElement('div');
    div.className='card';
    div.innerHTML = `
      <label>Sensor ${s.id}</label>
      <input type="text" name="${s.id}" value="${s.name||('Sensor '+s.id)}"/>
    `;
    cards.appendChild(div);
  });
}
document.getElementById('f').addEventListener('submit', async (e)=>{
  e.preventDefault();
  const fd = new FormData(e.target);
  const obj = {};
  for (const [k,v] of fd.entries()) { obj[k]=v; }
  await fetch('/api/v1/names', {method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify(obj)});
  alert('Gespeichert');
  await load();
});
load();
</script>
</body>
</html>
)HTML";

void setupWeb() {
  // Index
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req){
    req->send(200, "text/html; charset=utf-8", INDEX_HTML);
  });

  // Readings JSON
  server.on("/api/v1/readings", HTTP_GET, [](AsyncWebServerRequest* req){
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.createNestedArray("sensors");
    for (int i = 0; i < SENSOR_COUNT; i++) {
      JsonObject o = arr.createNestedObject();
      o["id"] = sensors[i].id;
      o["name"] = sensors[i].name;
      if (sensors[i].valid) {
        o["value"] = sensors[i].value;
      } else {
        o["value"] = nullptr; // keine Daten
      }
      o["unit"] = DEFAULT_UNIT;
    }
    doc["ts"] = iso8601NowUTC();

    String out;
    serializeJson(doc, out);
    req->send(200, "application/json; charset=utf-8", out);
  });

  // Namen speichern
  server.on("/api/v1/names", HTTP_POST, [](AsyncWebServerRequest* req){},
    NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total){
      StaticJsonDocument<1024> doc;
      DeserializationError err = deserializeJson(doc, data, len);
      if (err) {
        req->send(400, "application/json", "{\"error\":\"bad json\"}");
        return;
      }
      // JSON keys "1".."8"
      for (int i = 0; i < SENSOR_COUNT; i++) {
        char key[4]; snprintf(key, sizeof(key), "%d", i + 1);
        if (doc.containsKey(key)) {
          String nm = String((const char*)doc[key]);
          sensors[i].name = nm;
          saveName(i, nm);
        }
      }
      req->send(200, "application/json", "{\"ok\":true}");
    }
  );

  server.begin();
}

// ------------------ Setup & Loop -----------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  // Sensor Defaults
  for (int i = 0; i < SENSOR_COUNT; i++) {
    sensors[i].id = i + 1;
    sensors[i].name = String("Sensor ") + String(i + 1);
    sensors[i].value = NAN;
    sensors[i].unit = DEFAULT_UNIT;
    sensors[i].valid = false;
  }

  // Buttons
  pinMode(BTN_UP_PIN, INPUT_PULLUP);
  pinMode(BTN_DOWN_PIN, INPUT_PULLUP);

  // RS485 DE/RE
  pinMode(RS485_DE_RE_PIN, OUTPUT);
  digitalWrite(RS485_DE_RE_PIN, LOW); // Empfang aktiv

  // UART1 für RS485
  RS485Serial.begin(MODBUS_BAUD, MODBUS_CONFIG, RS485_RX_PIN, RS485_TX_PIN);
  modbus.preTransmission(preTransmission);
  modbus.postTransmission(postTransmission);

  // I2C + LCD
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("ThermoHub8 Boot");

  // Namen aus NVS laden
  loadNames();

  // WLAN verbinden
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  lcd.setCursor(0,1); lcd.print("WiFi connecting...");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  lcd.setCursor(0,2);
  if (WiFi.status() == WL_CONNECTED) {
    lcd.print("WiFi OK: ");
    lcd.setCursor(0,3);
    lcd.print(WiFi.localIP().toString());
  } else {
    lcd.print("WiFi FAILED");
  }

  // mDNS
  if (WiFi.status() == WL_CONNECTED) {
    if (MDNS.begin(MDNS_NAME)) {
      MDNS.addService("http", "tcp", 80);
      Serial.printf("mDNS: http://%s.local/\n", MDNS_NAME);
    }
  }

  // Erste Poll-Runde
  pollAllSensors();
  lcdShowCurrentPage();

  // Webserver
  setupWeb();
}

void loop() {
  // Polling
  if (millis() - lastPoll >= POLL_INTERVAL_MS) {
    lastPoll = millis();
    pollAllSensors();
    lcdShowCurrentPage();
  }

  // Buttons
  handleButtons();

  // (AsyncWebServer benötigt kein loop-handling)
}
