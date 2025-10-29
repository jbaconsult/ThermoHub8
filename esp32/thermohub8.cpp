/*
 * Thermohub8 - ESP32 PT1000 Sensor Monitoring System
 *
 * Description:
 *   Reads PT1000 sensor data via Modbus and provides it through
 *   LCD display and REST API interface.
 *
 * Features:
 *   - Reads up to 8 PT1000 sensors via Modbus RTU
 *   - 16x4 LCD display with joystick navigation
 *   - REST API for sensor data and configuration
 *   - Persistent sensor name storage in ESP32 flash
 *   - Web interface for status monitoring
 *
 * Hardware Requirements:
 *   - ESP32 Development Board
 *   - MAX485 RS485 to TTL Module
 *   - PT1000 sensors with Modbus converter
 *   - 16x4 LCD with I2C interface (PCF8574)
 *   - Analog joystick (2-axis + button)
 *
 * Author: Johannes
 * Version: 1.0
 * Date: 2025
 */

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ModbusMaster.h>
#include <LiquidCrystal_I2C.h>
#include "Joystick.h"

// ============================================================================
// CONFIGURATION SECTION
// ============================================================================

// Number of sensors to read (configurable, default: 8)
#define NUM_SENSORS 8

// Status LED Configuration
#define STATUS_LED 2 // GPIO pin for Modbus activity LED indicator

// WiFi Configuration
// NOTE: Update these credentials for your network
const char *WIFI_SSID = "ADD YOUR SSID HERE";             // WiFi network name
const char *WIFI_PASSWORD = "ADD YOUR WIFI PW HERE"; // WiFi password
const char *HOSTNAME = "thermohub8";           // mDNS hostname (access via thermohub8.local)

// RS485/Modbus Pin Configuration
// MAX485 module connections to ESP32
#define RS485_TX_PIN 16   // UART TX pin for Modbus communication
#define RS485_RX_PIN 17   // UART RX pin for Modbus communication
#define RS485_DE_RE_PIN 4 // Driver Enable / Receiver Enable control pin

// Modbus Protocol Configuration
#define MODBUS_SLAVE_ID 1           // Modbus slave device ID
#define MODBUS_START_REGISTER 0x30  // Starting register address (48 decimal)
#define MODBUS_BAUDRATE 9600        // Communication speed (9600 baud)
#define MODBUS_UPDATE_INTERVAL 1000 // Sensor read interval in milliseconds

// I2C LCD Display Pin Configuration
// Standard ESP32 I2C pins for LCD communication
#define I2C_SDA_PIN 21    // I2C data line
#define I2C_SCL_PIN 22    // I2C clock line
#define LCD_I2C_ADDR 0x27 // I2C address of LCD (default for PCF8574)
#define LCD_COLS 16       // Number of columns on LCD
#define LCD_ROWS 4        // Number of rows on LCD

// Joystick Pin Configuration
// Analog joystick connected to ADC pins
#define JOY_X_PIN 34  // X-axis analog input (ADC1_CH6)
#define JOY_Y_PIN 35  // Y-axis analog input (ADC1_CH7)
#define JOY_SW_PIN 32 // Button/switch digital input

// Joystick Calibration Values
// Adjust these values based on your specific joystick hardware
#define JOY_MIN_VAL 0       // Minimum ADC value (fully left/down)
#define JOY_MAX_VAL 4095    // Maximum ADC value (fully right/up)
#define JOY_CENTER_VAL 2000 // Center position value (neutral)
#define JOY_DEADZONE 500    // Deadzone radius to prevent drift

// Sensor Display Configuration
#define MAX_SENSOR_NAME_LENGTH 16 // Maximum characters for sensor name in storage
#define DISPLAY_NAME_LENGTH 8     // Maximum characters displayed on LCD (to fit temperature)

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

// Modbus master instance for RTU communication
ModbusMaster modbus;

// LCD display object (16x4 with I2C interface)
LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);

// Joystick controller instance
Joystick joystick(JOY_X_PIN, JOY_Y_PIN, JOY_SW_PIN);

// Async web server on port 80
AsyncWebServer server(80);

// Non-volatile storage for sensor names
Preferences preferences;

// ============================================================================
// GLOBAL VARIABLES
// ============================================================================

// Sensor data arrays
float sensorTemperatures[NUM_SENSORS]; // Current temperature readings in °C
String sensorNames[NUM_SENSORS];       // User-defined sensor names

// Display navigation state
int displayOffset = 0;    // Current scroll position (first visible row)
int maxDisplayOffset = 0; // Maximum scroll position

// Timing control
unsigned long lastModbusUpdate = 0; // Timestamp of last Modbus read

// ============================================================================
// RS485 CONTROL FUNCTIONS
// ============================================================================

/**
 * @brief Enable RS485 transmitter mode
 *
 * Called before sending data via Modbus. Sets DE (Driver Enable) and
 * RE (Receiver Enable) pins HIGH to enable transmission.
 */
void preTransmission()
{
    digitalWrite(RS485_DE_RE_PIN, HIGH);
}

/**
 * @brief Enable RS485 receiver mode
 *
 * Called after sending data via Modbus. Sets DE and RE pins LOW
 * to enable reception of responses.
 */
void postTransmission()
{
    digitalWrite(RS485_DE_RE_PIN, LOW);
}

// ============================================================================
// MODBUS COMMUNICATION FUNCTIONS
// ============================================================================

/**
 * @brief Initialize Modbus communication interface
 *
 * Configures Serial2 for Modbus RTU communication and sets up
 * the MAX485 control pins.
 */
void initModbus()
{
    Serial.println("Initializing Modbus...");

    // Configure RS485 control pin
    pinMode(RS485_DE_RE_PIN, OUTPUT);
    digitalWrite(RS485_DE_RE_PIN, LOW); // Default to receive mode

    // Initialize Serial2 for Modbus communication
    // 8 data bits, No parity, 1 stop bit (8N1)
    Serial2.begin(MODBUS_BAUDRATE, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);

    // Configure Modbus master
    modbus.begin(MODBUS_SLAVE_ID, Serial2);
    modbus.preTransmission(preTransmission);
    modbus.postTransmission(postTransmission);

    Serial.println("Modbus initialized");
}

/**
 * @brief Read a 32-bit float value from two consecutive Modbus registers
 *
 * Reads two 16-bit holding registers and combines them into a single
 * 32-bit float value using Big Endian byte order.
 *
 * @param registerAddress Starting register address
 * @return float Temperature value in °C, or -999.9 on error
 */
float readModbusFloat(uint16_t registerAddress)
{
    uint8_t result;
    uint16_t data[2];

    // Read 2 consecutive registers (32-bit float = 2x 16-bit registers)
    result = modbus.readHoldingRegisters(registerAddress, 2);

    if (result == modbus.ku8MBSuccess)
    {
        // Get register values from response buffer
        data[0] = modbus.getResponseBuffer(0); // High word
        data[1] = modbus.getResponseBuffer(1); // Low word

        // Convert Big Endian bytes to float
        // First register contains high word, second contains low word
        union
        {
            uint32_t i;
            float f;
        } converter;

        converter.i = ((uint32_t)data[0] << 16) | data[1];
        return converter.f;
    }
    else
    {
        Serial.print("Modbus error reading register ");
        Serial.println(registerAddress);
        return -999.9; // Error indicator value
    }
}

/**
 * @brief Update all sensor temperature readings via Modbus
 *
 * Periodically reads temperature values from all configured sensors.
 * Updates occur at intervals defined by MODBUS_UPDATE_INTERVAL.
 * Status LED blinks during Modbus communication.
 *
 * Note: Register addresses skip by 2 (every other register)
 * Example: Register 0x30, 0x32, 0x34, 0x36, etc.
 */
void updateSensorData()
{
    unsigned long currentTime = millis();

    // Check if update interval has elapsed
    if (currentTime - lastModbusUpdate >= MODBUS_UPDATE_INTERVAL)
    {
        lastModbusUpdate = currentTime;

        // Read all configured sensors
        for (int i = 0; i < NUM_SENSORS; i++)
        {
            // Calculate register address: start + (sensor_index * 2)
            // Registers are spaced 2 apart
            uint16_t registerAddr = MODBUS_START_REGISTER + (i * 2);

            // Indicate Modbus activity with LED
            digitalWrite(STATUS_LED, HIGH);
            float temp = readModbusFloat(registerAddr);
            digitalWrite(STATUS_LED, LOW);

            // Update sensor value if read was successful
            if (temp != -999.9)
            {
                sensorTemperatures[i] = temp;
            }
        }
    }
}

// ============================================================================
// NON-VOLATILE STORAGE FUNCTIONS
// ============================================================================

/**
 * @brief Initialize preferences (non-volatile storage) and load sensor names
 *
 * Opens the "thermohub8" namespace in ESP32 flash memory and loads
 * all sensor names. If a name doesn't exist, creates a default name.
 */
void initPreferences()
{
    Serial.println("Initializing Preferences...");
    preferences.begin("thermohub8", false); // false = read/write mode

    // Load or create default sensor names
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        String key = "sensor" + String(i);
        String defaultName = "Sensor " + String(i + 1);
        sensorNames[i] = preferences.getString(key.c_str(), defaultName);

        Serial.print("Sensor ");
        Serial.print(i);
        Serial.print(": ");
        Serial.println(sensorNames[i]);
    }
}

/**
 * @brief Save a sensor name to non-volatile storage
 *
 * Stores the sensor name in ESP32 flash memory so it persists
 * across power cycles.
 *
 * @param sensorIndex Index of the sensor (0 to NUM_SENSORS-1)
 * @param name New name for the sensor (max MAX_SENSOR_NAME_LENGTH chars)
 */
void saveSensorName(int sensorIndex, String name)
{
    // Validate sensor index
    if (sensorIndex >= 0 && sensorIndex < NUM_SENSORS)
    {
        // Truncate name if too long
        if (name.length() > MAX_SENSOR_NAME_LENGTH)
        {
            name = name.substring(0, MAX_SENSOR_NAME_LENGTH);
        }

        // Save to flash memory
        String key = "sensor" + String(sensorIndex);
        preferences.putString(key.c_str(), name);
        sensorNames[sensorIndex] = name;

        Serial.print("Sensor name saved: ");
        Serial.print(sensorIndex);
        Serial.print(" = ");
        Serial.println(name);
    }
}

// ============================================================================
// LCD DISPLAY FUNCTIONS
// ============================================================================

/**
 * @brief Initialize the LCD display
 *
 * Sets up I2C communication, initializes the LCD, displays a welcome
 * message, and calculates the maximum scroll offset based on the
 * number of sensors.
 */
void initDisplay()
{
    Serial.println("Initializing LCD...");

    // Initialize I2C bus
    Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

    // Initialize LCD display
    lcd.init();
    lcd.backlight();
    lcd.clear();

    // Display welcome message
    lcd.setCursor(1, 0);
    lcd.print("Thermohub8");
    lcd.setCursor(-4, 2); // Using offset workaround for row 2
    lcd.print("Developed by");
    lcd.setCursor(-4, 3); // Using offset workaround for row 3
    lcd.print("Johannes    v1.0");

    delay(2000);
    lcd.clear();

    // Calculate maximum scroll position
    // Total scrollable items = sensors + menu items (separator + IP + IP value + version)
    maxDisplayOffset = max(0, NUM_SENSORS - LCD_ROWS);

    Serial.println("LCD initialized");
}

/**
 * @brief Print sensor data to LCD at current cursor position
 *
 * Displays sensor name (truncated to DISPLAY_NAME_LENGTH) followed by
 * temperature value with one decimal place and °C symbol.
 *
 * Format: "SensName  XX.X°C"
 *
 * @param sensorIndex Index of the sensor to display
 */
void print_sensordata(int sensorIndex)
{
    // Get and truncate sensor name if needed
    String displayName = sensorNames[sensorIndex];
    if (displayName.length() > DISPLAY_NAME_LENGTH)
    {
        displayName = displayName.substring(0, DISPLAY_NAME_LENGTH);
    }
    lcd.print(displayName);

    // Pad name with spaces to align temperature (right-aligned)
    for (int i = displayName.length(); i < DISPLAY_NAME_LENGTH; i++)
    {
        lcd.print(" ");
    }
    lcd.print(" ");

    // Display temperature value
    float temp = sensorTemperatures[sensorIndex];
    if (temp > -99.0)
    {
        // Add leading space for positive temperatures
        if (temp > 0)
            lcd.print(" ");
        // Add leading space for single-digit temperatures
        if (temp > -10.0 && temp < 10.0)
            lcd.print(" ");

        // Print temperature with 1 decimal place
        lcd.print(temp, 1);
        lcd.print((char)223); // Degree symbol '°'
        lcd.print("C");
    }
    else
    {
        // Display error indicator
        lcd.print(" --.-");
        lcd.print((char)223);
        lcd.print("C");
    }
}

/**
 * @brief Print menu/info items on LCD
 *
 * Displays system information after the sensor list:
 * - Separator line
 * - IP address label
 * - IP address value
 * - Version information
 *
 * @param sensorIndex Current item index (sensor count + menu offset)
 * @param correctXOffset X-axis correction for display bug on rows 3-4
 * @param row Current LCD row (0-3)
 */
void print_menu(int sensorIndex, int correctXOffset, int row)
{
    if (sensorIndex == NUM_SENSORS)
    {
        // Separator line after sensors
        lcd.setCursor(correctXOffset, row);
        lcd.print("================");
    }
    if (sensorIndex == NUM_SENSORS + 1)
    {
        // IP address label
        lcd.setCursor(correctXOffset, row);
        lcd.print("IP-Address:");
    }
    if (sensorIndex == NUM_SENSORS + 2)
    {
        // IP address value
        lcd.setCursor(correctXOffset, row);
        lcd.print(WiFi.localIP());
    }
    if (sensorIndex == NUM_SENSORS + 3)
    {
        // Version information
        lcd.setCursor(correctXOffset, row);
        lcd.print("Version:     1.0");
    }
}

/**
 * @brief Update LCD display with current data
 *
 * Renders 4 rows of information starting from the current scroll position.
 * Displays either sensor data or menu items depending on scroll offset.
 *
 * Note: Uses X-axis correction for rows 3-4 due to LCD library bug
 */
void updateDisplay()
{
    // Display 4 rows starting from displayOffset
    for (int row = 0; row < LCD_ROWS; row++)
    {
        int sensorIndex = displayOffset + row;
        int correctXOffset = 0;

        // Apply X-axis correction for rows 3 and 4
        // This is a workaround for a bug in the LCD library
        if (row > 1)
            correctXOffset = -4;

        // Display sensor data if within sensor range
        if (sensorIndex < NUM_SENSORS)
        {
            lcd.setCursor(correctXOffset, row);
            print_sensordata(sensorIndex);
        }

        // Display menu items after sensors
        print_menu(sensorIndex, correctXOffset, row);
    }
}

/**
 * @brief Scroll display up by one row
 *
 * Decrements the display offset if not already at the top,
 * then refreshes the display.
 */
void scrollUp()
{
    lcd.clear();
    if (displayOffset > 0)
    {
        displayOffset--;
        updateDisplay();
    }
}

/**
 * @brief Scroll display down by one row
 *
 * Increments the display offset if not already at the bottom,
 * then refreshes the display. Maximum offset includes menu items (+4).
 */
void scrollDown()
{
    lcd.clear();
    if (displayOffset < maxDisplayOffset + 4) // +4 for menu items
    {
        displayOffset++;
        updateDisplay();
    }
}

// ============================================================================
// JOYSTICK CALLBACK FUNCTIONS
// ============================================================================

/**
 * @brief Callback for joystick up movement
 * Scrolls the display up by one row
 */
void onJoystickUp()
{
    Serial.println("Joystick: Up");
    scrollUp();
}

/**
 * @brief Callback for joystick down movement
 * Scrolls the display down by one row
 */
void onJoystickDown()
{
    Serial.println("Joystick: Down");
    scrollDown();
}

/**
 * @brief Callback for joystick left movement
 * Currently unused - reserved for future features
 */
void onJoystickLeft()
{
    Serial.println("Joystick: Left");
}

/**
 * @brief Callback for joystick right movement
 * Currently unused - reserved for future features
 */
void onJoystickRight()
{
    Serial.println("Joystick: Right");
}

/**
 * @brief Callback for joystick center position
 * Currently unused - reserved for future features
 */
void onJoystickCenter()
{
    Serial.println("Joystick: Center");
}

/**
 * @brief Callback for joystick button press
 * Currently unused - reserved for future menu navigation
 */
void onJoystickSwitch()
{
    Serial.println("Joystick: Switch pressed");
    // Reserved for menu system implementation
}

/**
 * @brief Initialize joystick controller
 *
 * Configures the joystick with calibration values, axis inversion,
 * and registers all callback functions for user interaction.
 */
void initJoystick()
{
    Serial.println("Initializing Joystick...");

    joystick.begin();
    joystick.setThresholds(JOY_MIN_VAL, JOY_MAX_VAL, JOY_CENTER_VAL, JOY_DEADZONE);

    // Invert Y-axis for intuitive up/down navigation
    // Uncomment to invert X-axis if needed
    // joystick.setInvertX(true);
    joystick.setInvertY(true);

    // Enable debug mode for troubleshooting (outputs to serial)
    // Uncomment to see raw joystick values and events
    // joystick.setDebugMode(true);

    // Register callback functions for joystick events
    joystick.onUp(onJoystickUp);
    joystick.onDown(onJoystickDown);

    // These callbacks are currently unused but available for future features
    joystick.onLeft(onJoystickLeft);
    joystick.onRight(onJoystickRight);
    joystick.onCenter(onJoystickCenter);
    joystick.onSwitch(onJoystickSwitch);

    Serial.println("Joystick initialized");
}

// ============================================================================
// WIFI FUNCTIONS
// ============================================================================

/**
 * @brief Initialize WiFi connection
 *
 * Connects to the configured WiFi network and displays connection
 * status on the LCD. Times out after 20 attempts (10 seconds).
 */
void initWiFi()
{
    lcd.print("Connecting Wifi");

    // Set hostname and begin connection
    WiFi.setHostname(HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    // Wait for connection (max 20 attempts = 10 seconds)
    int attempts = 0;
    lcd.setCursor(0, 1);
    while (WiFi.status() != WL_CONNECTED && attempts < 20)
    {
        delay(500);
        lcd.print(".");
        attempts++;
    }

    delay(100);

    // Display connection result
    if (WiFi.status() == WL_CONNECTED)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Wifi Connected");
        lcd.setCursor(0, 1);
        lcd.print("IP: ");
        lcd.setCursor(-4, 2); // Workaround for display bug
        lcd.print(WiFi.localIP());
    }
    else
    {
        lcd.clear();
        lcd.print("WIFI connection failed!");
    }

    delay(3500);
}

// ============================================================================
// WEB SERVER / REST API FUNCTIONS
// ============================================================================

/**
 * @brief Generate HTML status page
 *
 * Creates a responsive HTML page displaying all sensor readings.
 * Includes auto-refresh every 5 seconds and links to API endpoints.
 *
 * @return String containing complete HTML document
 */
String generateStatusHTML()
{
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<title>Thermohub8 Status</title>";
    html += "<style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
    html += "h1 { color: #333; }";
    html += ".container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 5px rgba(0,0,0,0.1); }";
    html += ".sensor { display: flex; justify-content: space-between; padding: 10px; margin: 5px 0; background: #f9f9f9; border-radius: 5px; }";
    html += ".sensor-name { font-weight: bold; }";
    html += ".sensor-temp { color: #0066cc; }";
    html += ".refresh-btn { background: #0066cc; color: white; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; margin-top: 20px; }";
    html += ".refresh-btn:hover { background: #0052a3; }";
    html += "</style>";
    html += "<script>";
    html += "function refreshData() { location.reload(); }";
    html += "setTimeout(refreshData, 5000);"; // Auto-refresh after 5 seconds
    html += "</script>";
    html += "</head><body>";
    html += "<div class='container'>";
    html += "<h1>Thermohub8 - Sensor Status</h1>";

    // Generate sensor list
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        html += "<div class='sensor'>";
        html += "<span class='sensor-name'>" + sensorNames[i] + "</span>";
        html += "<span class='sensor-temp'>";
        if (sensorTemperatures[i] > -999.0)
        {
            html += String(sensorTemperatures[i], 1) + " °C";
        }
        else
        {
            html += "Error";
        }
        html += "</span>";
        html += "</div>";
    }

    html += "<button class='refresh-btn' onclick='refreshData()'>Refresh</button>";
    html += "<p style='margin-top: 20px; color: #666; font-size: 12px;'>";
    html += "API Endpoint: <a href='/api/v1/sensordata'>/api/v1/sensordata</a><br>";
    html += "Auto-refresh every 5 seconds";
    html += "</p>";
    html += "</div>";
    html += "</body></html>";

    return html;
}

/**
 * @brief Initialize web server and REST API endpoints
 *
 * Sets up HTTP routes for:
 * - GET  /                    - HTML status page
 * - GET  /api/v1/sensordata   - JSON sensor data
 * - POST /api/v1/sensor       - Update sensor name
 */
void initWebServer()
{
    Serial.println("Initializing Web Server...");

    // Route: Root page - HTML status display
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(200, "text/html", generateStatusHTML()); });

    // Route: API endpoint - Get all sensor data as JSON
    // Returns: {"sensors":[{"id":0,"name":"Sensor 1","value":23.5,"unit":"°C"},...]}
    server.on("/api/v1/sensordata", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        StaticJsonDocument<1024> doc;
        JsonArray sensors = doc.createNestedArray("sensors");
        
        // Build JSON array with all sensor data
        for (int i = 0; i < NUM_SENSORS; i++) {
            JsonObject sensor = sensors.createNestedObject();
            sensor["id"] = i;
            sensor["name"] = sensorNames[i];
            sensor["value"] = round(sensorTemperatures[i] * 10) / 10.0;  // 1 decimal place
            sensor["unit"] = "°C";
        }
        
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response); });

    // Route: API endpoint - Update sensor name
    // POST body: {"id": 0, "name": "New Name"}
    // Returns: {"success": true, "id": 0, "name": "New Name"}
    server.on("/api/v1/sensor", HTTP_POST, [](AsyncWebServerRequest *request) {}, // Request handler (unused for POST with body)
              NULL,                                                               // Upload handler (unused)
              [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
              {
            // Parse JSON request body
            StaticJsonDocument<256> doc;
            DeserializationError error = deserializeJson(doc, (const char*)data);
            
            if (error) {
                request->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
                return;
            }
            
            // Extract and validate sensor ID
            int sensorId = doc["id"];
            const char* newName = doc["name"];
            
            if (sensorId < 0 || sensorId >= NUM_SENSORS) {
                request->send(400, "application/json", "{\"error\":\"Invalid sensor ID\"}");
                return;
            }
            
            if (newName == nullptr || strlen(newName) == 0) {
                request->send(400, "application/json", "{\"error\":\"Name is required\"}");
                return;
            }
            
            // Save new name and update display
            saveSensorName(sensorId, String(newName));
            updateDisplay();
            
            // Build success response
            StaticJsonDocument<128> responseDoc;
            responseDoc["success"] = true;
            responseDoc["id"] = sensorId;
            responseDoc["name"] = sensorNames[sensorId];
            
            String response;
            serializeJson(responseDoc, response);
            request->send(200, "application/json", response); });

    // 404 handler for unknown routes
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "application/json", "{\"error\":\"Not Found\"}"); });

    server.begin();
    Serial.println("Web Server started");
}

// ============================================================================
// SETUP FUNCTION
// ============================================================================

/**
 * @brief Arduino setup function - runs once at startup
 *
 * Initializes all system components in the following order:
 * 1. Serial communication for debugging
 * 2. Status LED
 * 3. Non-volatile storage (sensor names)
 * 4. LCD display
 * 5. Modbus communication
 * 6. Joystick controller
 * 7. WiFi connection
 * 8. Web server
 *
 * After initialization, displays a ready message and starts
 * the main sensor display.
 */
void setup()
{
    // Initialize serial console for debugging
    Serial.begin(115200);

    // Configure status LED
    pinMode(STATUS_LED, OUTPUT);
    digitalWrite(STATUS_LED, LOW);

    delay(1000);
    Serial.println("=== Thermohub8 Starting ===");

    // Initialize all temperature readings with error value
    for (int i = 0; i < NUM_SENSORS; i++)
    {
        sensorTemperatures[i] = -999.9;
    }

    // Initialize all system components
    initPreferences(); // Load sensor names from flash
    initDisplay();     // Setup LCD and show welcome message
    initModbus();      // Configure Modbus communication
    initJoystick();    // Setup joystick with callbacks
    initWiFi();        // Connect to WiFi network
    initWebServer();   // Start HTTP server and API

    // Display ready message
    lcd.clear();
    lcd.print("Thermohub8 Ready");
    delay(1000);
    lcd.clear();

    // Show initial sensor display
    updateDisplay();
}

// ============================================================================
// MAIN LOOP FUNCTION
// ============================================================================

/**
 * @brief Arduino main loop - runs continuously
 *
 * Main program loop that performs the following tasks:
 * 1. Updates sensor readings via Modbus (every MODBUS_UPDATE_INTERVAL)
 * 2. Refreshes LCD display with current data
 * 3. Polls joystick for user input
 *
 * Loop delay: 10ms (100 Hz update rate)
 */
void loop()
{
    // Update sensor data from Modbus (time-controlled)
    updateSensorData();

    // Refresh LCD display with current values
    updateDisplay();

    // Process joystick input and trigger callbacks
    joystick.update();

    // Small delay to prevent excessive CPU usage
    delay(10);
}
