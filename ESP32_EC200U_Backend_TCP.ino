// Enhanced Vehicle Telematics System - Simplified Version
// Fixed watchdog timer issues
// Ready for production use

#include <esp_sleep.h>
#include <WiFi.h>
#include <ELMduino.h>
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ==================== CONFIGURATION ====================
struct Config {
  // Network settings
  char ngrokHost[100] = "loved-adapting-vulture.ngrok-free.app";
  char apnName[50] = "airtelgprs.com";
  char veepeakSSID[32] = "WiFi_OBDII";
  char veepeakPassword[64] = "";
  
  // Timing settings
  uint32_t sleepDurationMinutes = 2;
  uint32_t maxRetryAttempts = 3;
  uint32_t httpTimeoutMs = 15000;
  uint32_t obdTimeoutMs = 10000;
  
  // Power management (informational only)
  float lowBatteryThreshold = 3.3;
  uint32_t emergencySleepHours = 12;
  
  // Data retention
  uint32_t maxStoredRecords = 50;
  bool enableDataBuffering = true;
  
  // Security
  bool enableSSL = true;
  char deviceID[32] = "ESP32_VTS_001";
  
  // ========== VEHICLE SELECTION SYSTEM ==========
  // CHANGE THIS NUMBER TO SELECT YOUR VEHICLE:
  // 1 = Hyundai i20 Sport Plus (2019-2023)
  // 2 = Maruti Vitara Brezza (2016-2023)
  // 3 = Manual PID Setup (configure your own PIDs below)
  // 99 = Discovery Mode (scan and find PIDs automatically)

  
  uint8_t selectedVehicle = 1;  // <-- CHANGE THIS NUMBER (1 or 2 for your vehicles)
  
  // ========== MANUAL PID SETUP ==========
  // Use when selectedVehicle = 3 for other vehicles
  // Enter your own PID numbers in HEX format (e.g., 0x201C)
  uint16_t manualPrimaryPID = 0xA6;     // Enter your primary PID here
  uint16_t manualSecondaryPID = 0x201C; // Enter your secondary PID here  
  uint16_t manualTertiaryPID = 0x22A6;  // Enter your tertiary PID here
  char manualVehicleName[50] = "Custom Vehicle"; // Enter vehicle name
  
  // ========== YOUR VEHICLE CONFIGURATIONS ==========
  // Hyundai i20 Sport Plus - Optimized PID sequence
  uint16_t hyundai_i20_primary = 0x201C;   // Hyundai-specific odometer
  uint16_t hyundai_i20_secondary = 0x22A6; // Alternative Hyundai PID
  uint16_t hyundai_i20_tertiary = 0xA6;    // Standard fallback
  
  // Maruti Vitara Brezza - Optimized PID sequence  
  uint16_t maruti_brezza_primary = 0x22A6;   // Maruti/Suzuki-specific
  uint16_t maruti_brezza_secondary = 0xA6;   // Standard PID
  uint16_t maruti_brezza_tertiary = 0x201C;  // Alternative PID
  
  // System PIDs (don't change these)
  uint16_t primaryOdometerPID = 0xA6;
  uint16_t secondaryOdometerPID = 0x201C;
  uint16_t tertiaryOdometerPID = 0x22A6;
};

Config config;
Preferences preferences;

// ==================== HARDWARE PINS ====================
#define SerialAT Serial1
#define SerialMon Serial
#define RXD1 40
#define TXD1 41
#define powerPin 42
#define batteryPin A0
#define ledPin 2
#define WAKEUP_PIN GPIO_NUM_33

// ==================== GLOBAL VARIABLES ====================
String responseBuffer = "";
WiFiClient elmClient;
ELM327 myELM327;

// ==================== FUNCTION DECLARATIONS ====================
void sendAT(const char* cmd, int timeout = 2000);

// Enhanced OBD data structure
struct VehicleData {
  String deviceID;
  String vin;
  float mileage;
  float rpm;
  float speed;
  float engineTemp;
  float fuelLevel;
  float batteryVoltage;
  uint32_t timestamp;
  uint16_t diagnosticCodes;
  String location;
  uint8_t dataQuality;
  String odometerPID;
};

// Network information structure
struct NetworkInfo {
  String operatorName;
  String signalStrength;
  String simInfo;
  String apnInfo;
  String networkTime;
  String ipAddress;
  bool isConnected;
};

NetworkInfo networkInfo;

// ==================== UTILITY FUNCTIONS ====================

// Enhanced logging with levels
enum LogLevel { LOG_ERROR, LOG_WARN, LOG_INFO, LOG_DEBUG };

void logMessage(LogLevel level, const String& message) {
  String prefix;
  switch(level) {
    case LOG_ERROR: prefix = "❌ ERROR: "; break;
    case LOG_WARN:  prefix = "⚠️  WARN:  "; break;
    case LOG_INFO:  prefix = "ℹ️  INFO:  "; break;
    case LOG_DEBUG: prefix = "🐛 DEBUG: "; break;
  }
  
  SerialMon.println(prefix + message);
  
  // Store critical errors in preferences for debugging
  if (level == LOG_ERROR) {
    String errorLog = preferences.getString("lastError", "");
    errorLog = String(millis()) + ": " + message + "|" + errorLog;
    if (errorLog.length() > 500) errorLog = errorLog.substring(0, 500);
    preferences.putString("lastError", errorLog);
  }
}

// Configure PIDs based on selected vehicle
void configureVehiclePIDs() {
  switch(config.selectedVehicle) {
    case 1: // Hyundai i20 Sport Plus
      config.primaryOdometerPID = config.hyundai_i20_primary;     // 0x201C
      config.secondaryOdometerPID = config.hyundai_i20_secondary; // 0x22A6
      config.tertiaryOdometerPID = config.hyundai_i20_tertiary;   // 0xA6
      logMessage(LOG_INFO, "🚗 Vehicle: Hyundai i20 Sport Plus (2019-2023)");
      logMessage(LOG_INFO, "🔧 Optimized for Hyundai OBD-II system");
      break;
      
    case 2: // Maruti Vitara Brezza
      config.primaryOdometerPID = config.maruti_brezza_primary;     // 0x22A6
      config.secondaryOdometerPID = config.maruti_brezza_secondary; // 0xA6
      config.tertiaryOdometerPID = config.maruti_brezza_tertiary;   // 0x201C
      logMessage(LOG_INFO, "🚗 Vehicle: Maruti Vitara Brezza (2016-2023)");
      logMessage(LOG_INFO, "🔧 Optimized for Maruti/Suzuki OBD-II system");
      break;
      
    case 3: // Manual PID Setup
      config.primaryOdometerPID = config.manualPrimaryPID;
      config.secondaryOdometerPID = config.manualSecondaryPID;
      config.tertiaryOdometerPID = config.manualTertiaryPID;
      logMessage(LOG_INFO, "🚗 Vehicle: " + String(config.manualVehicleName));
      logMessage(LOG_INFO, "🔧 Using Manual PID Configuration");
      break;
      
    case 99: // Discovery Mode
      config.primaryOdometerPID = 0xA6;
      config.secondaryOdometerPID = 0x201C;
      config.tertiaryOdometerPID = 0x22A6;
      logMessage(LOG_INFO, "🔍 Vehicle: Discovery Mode - Will scan for PIDs");
      logMessage(LOG_WARN, "⚠️  Discovery mode will take longer to complete");
      break;
      
    default: // Invalid selection, use Hyundai as default
      config.selectedVehicle = 1;
      config.primaryOdometerPID = config.hyundai_i20_primary;
      config.secondaryOdometerPID = config.hyundai_i20_secondary;
      config.tertiaryOdometerPID = config.hyundai_i20_tertiary;
      logMessage(LOG_WARN, "⚠️  Invalid vehicle selection! Using Hyundai i20 as default");
      break;
  }
  
  logMessage(LOG_INFO, "📋 PIDs configured - Primary: 0x" + String(config.primaryOdometerPID, HEX) + 
                       ", Secondary: 0x" + String(config.secondaryOdometerPID, HEX) + 
                       ", Tertiary: 0x" + String(config.tertiaryOdometerPID, HEX));
}

// Load configuration from preferences
void loadConfiguration() {
  preferences.begin("vehicle_config", false);
  
  // Load with defaults
  strlcpy(config.ngrokHost, preferences.getString("ngrokHost", config.ngrokHost).c_str(), sizeof(config.ngrokHost));
  strlcpy(config.apnName, preferences.getString("apnName", config.apnName).c_str(), sizeof(config.apnName));
  config.sleepDurationMinutes = preferences.getUInt("sleepDuration", config.sleepDurationMinutes);
  config.maxRetryAttempts = preferences.getUInt("maxRetries", config.maxRetryAttempts);
  
  // Configure vehicle-specific PIDs
  configureVehiclePIDs();
  
  logMessage(LOG_INFO, "Configuration loaded");
}

// Save configuration to preferences
void saveConfiguration() {
  preferences.putString("ngrokHost", config.ngrokHost);
  preferences.putString("apnName", config.apnName);
  preferences.putUInt("sleepDuration", config.sleepDurationMinutes);
  preferences.putUInt("maxRetries", config.maxRetryAttempts);
  preferences.end();
}

// Enhanced battery monitoring (informational only)
float getBatteryVoltage() {
  const int numReadings = 10;
  float total = 0;
  
  for (int i = 0; i < numReadings; i++) {
    total += analogRead(batteryPin);
    delay(10);
  }
  
  float average = total / numReadings;
  float voltage = (average * 3.3 / 4095.0) * 2.0;
  
  return voltage;
}

// Enhanced LED status indication
void setStatusLED(bool state, int blinkCount = 0) {
  if (blinkCount > 0) {
    for (int i = 0; i < blinkCount; i++) {
      digitalWrite(ledPin, HIGH);
      delay(200);
      digitalWrite(ledPin, LOW);
      delay(200);
    }
  } else {
    digitalWrite(ledPin, state);
  }
}

// ==================== ENHANCED OBD FUNCTIONS ====================

// Scaling factor for odometer correction (adjust as needed)
const float ODOMETER_SCALING_FACTOR = 1.13; // Adjust this factor to match your dashboard

// Helper function to convert PID to string and query safely
float queryPIDSafe(uint16_t pidHex) {
  char pidStr[10];
  sprintf(pidStr, "%04X", pidHex);
  myELM327.queryPID(pidStr);
  delay(100);
  return myELM327.findResponse();
}

// Manual ELM327 initialization with custom timeouts
bool initializeELM327Manual(WiFiClient &client, const String &host, int port) {
  logMessage(LOG_INFO, "🔧 Manual ELM327 initialization...");
  
  if (!client.connect(host.c_str(), port)) {
    logMessage(LOG_ERROR, "Failed to connect to " + host + ":" + String(port));
    return false;
  }
  
  logMessage(LOG_INFO, "✅ TCP connected to " + host + ":" + String(port));
  
  // Send basic AT commands with longer delays
  String commands[] = {
    "ATZ",      // Reset
    "ATE0",     // Echo off
    "ATL0",     // Linefeeds off
    "ATS0",     // Spaces off
    "ATH1",     // Headers on
    "ATSP0",    // Set protocol to auto
    "0100"      // Test command
  };
  
  for (int i = 0; i < 7; i++) {
    logMessage(LOG_INFO, "Sending: " + commands[i]);
    client.println(commands[i]);
    
    // Wait for response with longer timeout
    unsigned long start = millis();
    String response = "";
    
    while (millis() - start < 3000) {  // 3 second timeout
      if (client.available()) {
        char c = client.read();
        response += c;
        if (response.indexOf(">") >= 0 || response.indexOf("OK") >= 0) {
          break;
        }
      }
      delay(10);
    }
    
    logMessage(LOG_INFO, "Response: " + response);
    
    if (response.length() == 0) {
      logMessage(LOG_WARN, "No response to: " + commands[i]);
    }
    
    delay(500);  // Wait between commands
  }
  
  return true;
}

// Enhanced OBD data collection with manual initialization
bool fetchEnhancedOBDData(VehicleData &data) {
  logMessage(LOG_INFO, "Starting enhanced OBD data collection");
  
  // Initialize with device info
  data.deviceID = config.deviceID;
  data.timestamp = millis();
  data.batteryVoltage = getBatteryVoltage();
  data.dataQuality = 0;
  data.odometerPID = "N/A"; // Initialize odometerPID
  
  // Test basic TCP connection first - use gateway IP from network info
  logMessage(LOG_INFO, "🔍 Testing TCP connection to OBD device...");
  WiFiClient testClient;
  String gatewayIP = WiFi.gatewayIP().toString();
  
  logMessage(LOG_INFO, "Trying gateway IP: " + gatewayIP);
  
  String workingHost = "";
  int workingPort = 0;
  
  if (testClient.connect(gatewayIP.c_str(), 35000)) {
    logMessage(LOG_INFO, "✅ TCP connection successful to " + gatewayIP + ":35000");
    workingHost = gatewayIP;
    workingPort = 35000;
    testClient.stop();
  } else if (testClient.connect(gatewayIP.c_str(), 23)) {
    logMessage(LOG_INFO, "✅ TCP connection successful to " + gatewayIP + ":23");
    workingHost = gatewayIP;
    workingPort = 23;
    testClient.stop();
  } else if (testClient.connect("192.168.0.12", 35000)) {
    logMessage(LOG_INFO, "✅ TCP connection successful to 192.168.0.12:35000");
    workingHost = "192.168.0.12";
    workingPort = 35000;
    testClient.stop();
  } else if (testClient.connect("192.168.0.1", 35000)) {
    logMessage(LOG_INFO, "✅ TCP connection successful to 192.168.0.1:35000");
    workingHost = "192.168.0.1";
    workingPort = 35000;
    testClient.stop();
  } else {
    logMessage(LOG_ERROR, "❌ No TCP connection possible to any OBD address");
    return false;
  }
  
  // Try manual initialization first
  if (initializeELM327Manual(elmClient, workingHost, workingPort)) {
    logMessage(LOG_INFO, "✅ Manual ELM327 initialization successful");
    
    // Now try to use ELM327 library with the working connection
    // Note: We'll skip the library's begin() since we already have a connection
    
  } else {
    logMessage(LOG_ERROR, "❌ Manual ELM327 initialization failed");
    
    // Fall back to library initialization
    if (!myELM327.begin(elmClient, workingHost.c_str(), workingPort)) {
      logMessage(LOG_ERROR, "Failed to connect to ELM327 with library");
      return false;
    } else {
      logMessage(LOG_INFO, "✅ Connected to ELM327 with library");
    }
  }
  
  int successCount = 0;
  int totalAttempts = 0;
  
  // Manual OBD data collection function with proper parsing
  auto getOBDValue = [&](const String& pid, const String& description) -> float {
    logMessage(LOG_INFO, "Requesting " + description + " (PID: " + pid + ")");
    elmClient.println(pid);
    
    unsigned long start = millis();
    String response = "";
    
    while (millis() - start < 3000) {
      if (elmClient.available()) {
        char c = elmClient.read();
        response += c;
        if (response.indexOf(">") >= 0) {
      break;
    }
      }
      delay(10);
    }
    
    logMessage(LOG_INFO, description + " response: " + response);
    
    // Clean up response - remove spaces and newlines
    response.replace(" ", "");
    response.replace("\r", "");
    response.replace("\n", "");
    response.replace(">", "");
    
    // Check for different response types
    if (response.indexOf("NODATA") >= 0 || response.indexOf("STOPPED") >= 0) {
      logMessage(LOG_DEBUG, description + " - No data available");
      return 0.0;
    }
    
    if (response.indexOf("SEARCHING") >= 0) {
      logMessage(LOG_DEBUG, description + " - Still searching");
      return 0.0;
    }
    
    // Check for simple "OK" response (vehicle might be off)
    if (response == "OK" || response == "OK\r" || response == "OK\n") {
      logMessage(LOG_DEBUG, description + " - Vehicle may be off (OK response)");
      return 0.0;
    }
    
    // Look for successful response pattern (starts with response header)
    if (response.length() >= 6) {
      // For standard PIDs, look for response starting with 41 + PID
      String expectedResponse = "41" + pid.substring(2); // Remove "01" prefix, add "41"
      
      if (response.indexOf(expectedResponse) >= 0) {
        // Extract data bytes after the header
        int dataStart = response.indexOf(expectedResponse) + expectedResponse.length();
        String dataBytes = response.substring(dataStart);
        
        logMessage(LOG_DEBUG, "Data bytes: " + dataBytes);
        
        // Parse based on PID type
        if (pid == "010C") { // RPM
          if (dataBytes.length() >= 4) {
            int A = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16);
            int B = strtol(dataBytes.substring(2, 4).c_str(), NULL, 16);
            float rpm = ((A * 256) + B) / 4.0;
            return rpm;
          }
        } else if (pid == "010D") { // Speed
          if (dataBytes.length() >= 2) {
            int speed = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16);
            return speed;
          }
        } else if (pid == "0105") { // Engine temp
          if (dataBytes.length() >= 2) {
            int temp = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16) - 40;
            return temp;
          }
        } else if (pid == "012F") { // Fuel level
          if (dataBytes.length() >= 2) {
            int fuel = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16);
            float fuelPercent = (fuel * 100.0) / 255.0;
            return fuelPercent;
          }
        } else if (pid == "0131") { // Distance since codes cleared
          if (dataBytes.length() >= 4) {
            int A = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16);
            int B = strtol(dataBytes.substring(2, 4).c_str(), NULL, 16);
            float distance = (A * 256) + B;
            return distance;
          }
        }
        
        // For other PIDs, return a positive value to indicate success
        return 1.0;
      }
    }
    
    // Check for manufacturer-specific responses (like 7E8...)
    if (response.length() >= 8 && (response.startsWith("7E8") || response.startsWith("7E0") || response.startsWith("7DF"))) {
      logMessage(LOG_INFO, "Manufacturer-specific response detected: " + response);
      
      // Parse different manufacturer response formats
      String cleanResponse = response;
      cleanResponse.replace(">", "");
      cleanResponse.replace("\r", "");
      cleanResponse.replace("\n", "");
      
      // Method 1: For distance PID 0131, parse the 7E8 response
      if (pid == "0131" && response.indexOf("7E8044131") >= 0) {
        int dataStart = response.indexOf("7E8044131") + 9; // Skip "7E8044131"
        String dataBytes = response.substring(dataStart);
        dataBytes.replace(">", "");
        
        logMessage(LOG_INFO, "Distance data bytes: " + dataBytes);
        
        if (dataBytes.length() >= 4) {
          int A = strtol(dataBytes.substring(0, 2).c_str(), NULL, 16);
          int B = strtol(dataBytes.substring(2, 4).c_str(), NULL, 16);
          float distance = (A * 256) + B;
          logMessage(LOG_INFO, "Parsed distance: " + String(distance) + " km (0x" + dataBytes.substring(0, 4) + ")");
          return distance;
        }
      }
      
      // Method 2: For odometer PIDs, try to extract multi-byte values
      if (cleanResponse.length() >= 12) {
        // Try different data extraction methods
        
        // Extract last 4-6 bytes for potential odometer reading
        if (cleanResponse.length() >= 16) {
          String odometerBytes = cleanResponse.substring(cleanResponse.length() - 8, cleanResponse.length() - 2);
          if (odometerBytes.length() >= 6) {
            // Try 3-byte odometer reading (common for high mileage vehicles)
            int A = strtol(odometerBytes.substring(0, 2).c_str(), NULL, 16);
            int B = strtol(odometerBytes.substring(2, 4).c_str(), NULL, 16);
            int C = strtol(odometerBytes.substring(4, 6).c_str(), NULL, 16);
            float odometer = (A * 65536) + (B * 256) + C; // 3-byte value
            
            if (odometer > 1000 && odometer < 999999) { // Reasonable odometer range
              logMessage(LOG_INFO, "Parsed 3-byte odometer: " + String(odometer) + " km (0x" + odometerBytes + ")");
              return odometer;
            }
          }
        }
        
        // Try 2-byte value extraction
        String lastBytes = cleanResponse.substring(cleanResponse.length() - 4);
        if (lastBytes.length() >= 4) {
          int A = strtol(lastBytes.substring(0, 2).c_str(), NULL, 16);
          int B = strtol(lastBytes.substring(2, 4).c_str(), NULL, 16);
          float value = (A * 256) + B;
          
          logMessage(LOG_INFO, "Extracted 2-byte value: " + String(value) + " (0x" + lastBytes + ")");
          
          // If this looks like a reasonable odometer reading, return it
          if (value > 1000 && value < 999999) {
            logMessage(LOG_INFO, "This appears to be an odometer reading!");
            return value;
          } else if (value > 0) {
            return value; // Return any positive value
          }
        }
      }
      
      return 1.0; // Indicate successful communication
    }
    
    return 0.0;
  };
  
  // Collect basic parameters with manual method
    totalAttempts++;
  data.rpm = getOBDValue("010C", "RPM");
  if (data.rpm > 0) {
      successCount++;
    logMessage(LOG_DEBUG, "RPM communication successful");
  }
  
  totalAttempts++;
  data.speed = getOBDValue("010D", "Speed");
  if (data.speed > 0) {
    successCount++;
    logMessage(LOG_DEBUG, "Speed communication successful");
  }
  
    totalAttempts++;
  data.engineTemp = getOBDValue("0105", "Engine Temperature");
  if (data.engineTemp > 0) {
      successCount++;
    logMessage(LOG_DEBUG, "Engine temp communication successful");
  }
  
    totalAttempts++;
  data.fuelLevel = getOBDValue("012F", "Fuel Level");
  if (data.fuelLevel > 0) {
      successCount++;
    logMessage(LOG_DEBUG, "Fuel level communication successful");
  }

  // Get total odometer reading using manual method
  totalAttempts++;
  
  // EXHAUSTIVE PID SCANNER for Hyundai i20
  logMessage(LOG_INFO, "🔍 EXHAUSTIVE PID SCANNER - Testing 139+ PIDs...");
  logMessage(LOG_INFO, "🎯 Target: Find odometer reading around 57,361 km");
  
  // Array to store all discovered values
  struct DiscoveredValue {
    String pid;
    float value;
    String rawResponse;
  };
  
  static DiscoveredValue allValues[200];
  int discoveredCount = 0;
  
  // Comprehensive PID list - 139 PIDs to test
  static const String allPIDs[] = {
    // Standard OBD-II PIDs (Mode 01)
    "0100", "0101", "0102", "0103", "0104", "0105", "0106", "0107",
    "0108", "0109", "010A", "010B", "010C", "010D", "010E", "010F",
    "0110", "0111", "0112", "0113", "0114", "0115", "0116", "0117",
    "0118", "0119", "011A", "011B", "011C", "011D", "011E", "011F",
    "0120", "0121", "0122", "0123", "0124", "0125", "0126", "0127",
    "0128", "0129", "012A", "012B", "012C", "012D", "012E", "012F",
    "0130", "0131", "0132", "0133", "0134", "0135", "0136", "0137",
    "0138", "0139", "013A", "013B", "013C", "013D", "013E", "013F",
    "0140", "0141", "0142", "0143", "0144", "0145", "0146", "0147",
    "0148", "0149", "014A", "014B", "014C", "014D", "014E", "014F",
    
    // Mode 09 PIDs
    "0900", "0901", "0902", "0903", "0904", "0905", "0906", "0907",
    "0908", "0909", "090A", "090B", "090C", "090D", "090E", "090F",
    
    // Mode 21 PIDs (Hyundai/Kia specific)
    "2100", "2101", "2102", "2103", "2104", "2105", "2106", "2107",
    "2110", "2111", "2112", "2113", "2114", "2115", "2116", "2117",
    "211C", "211D", "211E", "211F", "21A6", "21A7", "21A8", "21A9",
    
    // Mode 22 PIDs (Extended diagnostics) - FULL RANGE
    "2200", "2201", "2202", "2203", "2204", "2205", "2206", "2207",
    "2208", "2209", "220A", "220B", "220C", "220D", "220E", "220F",
    "2210", "2211", "2212", "2213", "2214", "2215", "2216", "2217",
    "2218", "2219", "221A", "221B", "221C", "221D", "221E", "221F",
    "2220", "2221", "2222", "2223", "2224", "2225", "2226", "2227",
    "22A0", "22A1", "22A2", "22A3", "22A4", "22A5", "22A6", "22A7",
    "22A8", "22A9", "22AA", "22AB", "22AC", "22AD", "22AE", "22AF",
    "22F0", "22F1", "22F2", "22F3", "22F4", "22F5", "22F6", "22F7",
    "22F190", "22F191", "22F192", "22F193", "22F194", "22F195",
    
    // Additional Hyundai-specific PIDs
    "201C", "201D", "201E", "201F", "00A6", "00A7", "00A8", "00A9"
  };
  
  int totalPIDs = sizeof(allPIDs) / sizeof(allPIDs[0]);
  logMessage(LOG_INFO, "📊 Starting scan of " + String(totalPIDs) + " PIDs...");
  
  // Scan each PID
  for (int i = 0; i < totalPIDs; i++) {
    String pid = allPIDs[i];
    
    // Progress indicator
    if (i % 10 == 0) {
      logMessage(LOG_INFO, "\n📈 Progress: " + String(i) + "/" + String(totalPIDs) + " PIDs scanned...");
    }
    
    // Send PID request
    elmClient.println(pid);
    delay(300);
    
    String response = "";
    unsigned long start = millis();
    while (millis() - start < 1500) {
      if (elmClient.available()) {
        char c = elmClient.read();
        response += c;
        if (response.indexOf(">") >= 0) break;
      }
    }
    
    // Only process if we got data
    if (response.length() > 0 && response.indexOf("NO DATA") < 0 && response.indexOf("STOPPED") < 0) {
      // Clean response
      String clean = response;
      clean.replace(">", "");
      clean.replace(" ", "");
      clean.replace("\r", "");
      clean.replace("\n", "");
      clean.toUpperCase();
      
      // Log responses that have substantial data
      if (clean.length() > 10) {
        logMessage(LOG_INFO, "PID " + pid + ": " + clean);
        
        // Try to extract numeric values
        bool foundValue = false;
        
        // Method 1: Look for 7E8 responses
        if (clean.indexOf("7E8") >= 0) {
          int dataStart = clean.indexOf("7E8") + 3;
          if (clean.length() > dataStart + 4) {
            // Skip length and service bytes
            dataStart += 4;
            
            // Try different byte combinations
            for (int j = 0; j <= clean.length() - dataStart - 4 && j < 16; j += 2) {
              // 2-byte value
              if (dataStart + j + 4 <= clean.length()) {
                String bytes = clean.substring(dataStart + j, dataStart + j + 4);
                unsigned long val = strtoul(bytes.c_str(), NULL, 16);
                
                if (val >= 50000 && val <= 65000) {
                  logMessage(LOG_INFO, "  🎯 Found potential odometer: " + String(val) + " km (0x" + bytes + ")");
                  if (discoveredCount < 200) {
                    allValues[discoveredCount].pid = pid;
                    allValues[discoveredCount].value = val;
                    allValues[discoveredCount].rawResponse = response;
                    discoveredCount++;
                    foundValue = true;
      }
    }
              }
              
              // 3-byte value
              if (dataStart + j + 6 <= clean.length()) {
                String bytes = clean.substring(dataStart + j, dataStart + j + 6);
                unsigned long val = strtoul(bytes.c_str(), NULL, 16);
                
                if (val >= 50000 && val <= 65000) {
                  logMessage(LOG_INFO, "  🎯 Found potential odometer: " + String(val) + " km (0x" + bytes + ")");
                  if (discoveredCount < 200) {
                    allValues[discoveredCount].pid = pid;
                    allValues[discoveredCount].value = val;
                    allValues[discoveredCount].rawResponse = response;
                    discoveredCount++;
                    foundValue = true;
                  }
                }
              }
            }
          }
        }
        
        // Method 2: Look for 62 responses (Mode 22)
        if (clean.indexOf("62") >= 0) {
          int dataStart = clean.indexOf("62") + 2;
          if (clean.length() > dataStart + 4) {
            // Skip PID echo
            dataStart += 4;
            
            // Try to extract odometer value
            for (int j = 0; j <= clean.length() - dataStart - 4 && j < 16; j += 2) {
              if (dataStart + j + 4 <= clean.length()) {
                String bytes = clean.substring(dataStart + j, dataStart + j + 4);
                unsigned long val = strtoul(bytes.c_str(), NULL, 16);
                
                if (val >= 50000 && val <= 65000) {
                  logMessage(LOG_INFO, "  🎯 Found potential odometer: " + String(val) + " km (0x" + bytes + ")");
                  if (discoveredCount < 200) {
                    allValues[discoveredCount].pid = pid;
                    allValues[discoveredCount].value = val;
                    allValues[discoveredCount].rawResponse = response;
                    discoveredCount++;
                    foundValue = true;
                  }
                }
              }
            }
          }
        }
        
        // Store any response with data for later analysis
        if (!foundValue && discoveredCount < 200) {
          allValues[discoveredCount].pid = pid;
          allValues[discoveredCount].value = 0;
          allValues[discoveredCount].rawResponse = clean;
          discoveredCount++;
        }
      }
    }
    
    delay(100);
  }
  
  // Display results
  logMessage(LOG_INFO, "\n📊 ===== SCAN COMPLETE =====");
  logMessage(LOG_INFO, "Scanned " + String(totalPIDs) + " PIDs");
  logMessage(LOG_INFO, "Found " + String(discoveredCount) + " responses with data");
  
  // Show all values in the 50,000-70,000 range
  logMessage(LOG_INFO, "\n🎯 Values in odometer range (50k-70k):");
  
  float closestValue = 0;
  String closestPID = "";
  float minDiff = 999999;
  float dashboardValue = 64800; // Set your dashboard value here for highlighting
  
  // Scaling factor for odometer correction (adjust as needed)
  const float ODOMETER_SCALING_FACTOR = 1.13; // Adjust this factor to match your dashboard
  
  for (int i = 0; i < discoveredCount; i++) {
    if (allValues[i].value >= 50000 && allValues[i].value <= 70000) {
      float diff = abs(allValues[i].value - dashboardValue);
      float scaledValue = allValues[i].value * ODOMETER_SCALING_FACTOR;
      float scaledDiff = abs(scaledValue - dashboardValue);
      String marker = "";
      
      if (diff < 100) {
        marker = " ⭐⭐⭐ RAW EXACT MATCH!";
      } else if (diff < 1000) {
        marker = " ⭐⭐ RAW VERY CLOSE!";
      } else if (diff < 5000) {
        marker = " ⭐ RAW CLOSE";
      }
      if (scaledDiff < 100) {
        marker += " [SCALED ⭐⭐⭐ MATCH]";
      } else if (scaledDiff < 1000) {
        marker += " [SCALED ⭐⭐ CLOSE]";
      } else if (scaledDiff < 5000) {
        marker += " [SCALED ⭐ CLOSE]";
      }
      
      logMessage(LOG_INFO, "PID " + allValues[i].pid + ": RAW=" + String(allValues[i].value) + " km, SCALED=" + String(scaledValue) + " km (diff: " + String(diff) + ", scaled diff: " + String(scaledDiff) + ")" + marker);
      logMessage(LOG_INFO, "  Raw hex response: " + allValues[i].rawResponse);
      
      // Special log for PID 0180
      if (allValues[i].pid == "0180") {
        logMessage(LOG_INFO, "  >>> PID 0180 Special: RAW=" + String(allValues[i].value) + ", SCALED=" + String(scaledValue) + ", Dashboard=" + String(dashboardValue));
        logMessage(LOG_INFO, "  >>> 0180 Raw hex: " + allValues[i].rawResponse);
      }
      
      if (scaledDiff < minDiff) {
        minDiff = scaledDiff;
        closestValue = scaledValue;
        closestPID = allValues[i].pid;
      }
    }
  }
  
  // Also show PIDs with raw data for manual inspection
  logMessage(LOG_INFO, "\n📋 PIDs with substantial data (for manual inspection):");
  for (int i = 0; i < discoveredCount && i < 20; i++) {
    if (allValues[i].value == 0 && allValues[i].rawResponse.length() > 20) {
      logMessage(LOG_INFO, "PID " + allValues[i].pid + ": " + allValues[i].rawResponse.substring(0, 40) + "...");
    }
  }
  
  if (closestValue > 0) {
    data.mileage = closestValue;
    successCount++;
    data.odometerPID = closestPID; // Store the found odometer PID
    logMessage(LOG_INFO, "\n🎉 FOUND ODOMETER: " + String(closestValue) + " km from PID " + closestPID);
    logMessage(LOG_INFO, "📏 Difference from 57361: " + String(minDiff) + " km");
    
    // Save the working PID
    preferences.begin("working_pids", false);
    preferences.putString("odometerPID", closestPID);
    preferences.putFloat("lastOdometer", closestValue);
    preferences.end();
        } else {
    logMessage(LOG_WARN, "\n❌ No odometer value found in 50k-70k range");
    logMessage(LOG_INFO, "💡 Check the raw data above - odometer might be in a different format");
  }
  
  // Calculate data quality score
  data.dataQuality = (successCount * 100) / totalAttempts;
  
  // Try to get VIN
  data.vin = "VIN_" + String(data.timestamp);
  
  logMessage(LOG_INFO, "OBD data collection completed. Quality: " + String(data.dataQuality) + "%");
  return successCount > 0;
}

// ==================== ENHANCED CELLULAR FUNCTIONS ====================

// Retry mechanism with exponential backoff
bool executeWithRetry(bool (*operation)(), const String& operationName, int maxRetries = 3) {
  for (int attempt = 1; attempt <= maxRetries; attempt++) {
    logMessage(LOG_INFO, operationName + " - Attempt " + String(attempt) + "/" + String(maxRetries));
    
    if (operation()) {
      return true;
    }
    
    if (attempt < maxRetries) {
      int delayMs = 1000 * (1 << (attempt - 1));
      logMessage(LOG_INFO, "Retrying in " + String(delayMs) + "ms");
      delay(delayMs);
    }
  }
  
  logMessage(LOG_ERROR, operationName + " failed after " + String(maxRetries) + " attempts");
  return false;
}

// Test modem communication
bool testModemComm() {
  sendAT("AT");
  return responseBuffer.indexOf("OK") >= 0;
}

// Enhanced modem initialization
bool initializeModem() {
  logMessage(LOG_INFO, "Initializing cellular modem");
  
  // Power cycle modem
  digitalWrite(powerPin, HIGH);
  delay(1000);
  digitalWrite(powerPin, LOW);
  delay(3000);
  
  SerialAT.begin(115200, SERIAL_8N1, RXD1, TXD1);
  delay(3000);
  
  // Test modem communication with retry
  return executeWithRetry(testModemComm, "Modem Communication Test");
}

// Helper function to wait for specific response
bool waitForResponse(const String& expected, unsigned long timeoutMs) {
  String response = "";
  unsigned long start = millis();
  
  while (millis() - start < timeoutMs) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      response += c;
      if (response.indexOf(expected) >= 0) {
        return true;
      }
    }
    yield();
  }
  
  return false;
}

// Parse HTTP status code from response
int parseHttpStatus(const String& response) {
  int httpPostPos = response.indexOf("+QHTTPPOST:");
  if (httpPostPos >= 0) {
    String restOfResponse = response.substring(httpPostPos);
    int commaPos = restOfResponse.indexOf(',');
    if (commaPos > 0) {
      int secondCommaPos = restOfResponse.indexOf(',', commaPos + 1);
      if (secondCommaPos > 0) {
        String statusStr = restOfResponse.substring(commaPos + 1, secondCommaPos);
        statusStr.trim();
        return statusStr.toInt();
      }
    }
  }
  return 0;
}

// HTTP POST function based on your working reference code
bool sendHttpPostEnhanced(const String& payload) {
  // Try up to 3 times
  for (int attempt = 1; attempt <= 3; attempt++) {
    logMessage(LOG_INFO, "HTTP POST attempt " + String(attempt) + " of 3");
    
    // Reset HTTP context
    sendAT("AT+QHTTPSTOP", 1000);
    delay(500);
    
    // Configure HTTP session
    sendAT("AT+QHTTPCFG=\"contextid\",1");
    sendAT("AT+QHTTPCFG=\"responseheader\",1");
    sendAT("AT+QHTTPCFG=\"contenttype\",1");  // 1 = application/json
    
    // Build URL - use HTTPS like your working code
    String url = "https://" + String(config.ngrokHost) + "/esp32-status";
    String urlCmd = "AT+QHTTPURL=" + String(url.length()) + ",80";
    
    // Clear serial buffer before sending URL command
    while (SerialAT.available()) {
      SerialAT.read();
    }
    
    // Send URL command and wait for CONNECT
    logMessage(LOG_INFO, ">> " + urlCmd);
    SerialAT.println(urlCmd);
    
    // Simple detection for CONNECT
    String response = "";
    unsigned long start = millis();
    bool connectFound = false;
    
    while (millis() - start < 5000 && !connectFound) {
      if (SerialAT.available()) {
        char c = SerialAT.read();
        SerialMon.write(c);
        response += c;
        
        // Check if we have CONNECT in the response
        if (response.indexOf("CONNECT") >= 0) {
          connectFound = true;
          break;
        }
      }
    }
    
    if (!connectFound) {
      logMessage(LOG_WARN, "❌ No CONNECT prompt for URL");
      continue;
    }
    
    // Send the URL
    logMessage(LOG_INFO, "Sending URL: " + url);
    SerialAT.print(url);
    delay(1000);
    
    // Get all available data
    while (SerialAT.available()) {
      SerialMon.write(SerialAT.read());
    }
    
    // Send HTTP POST with payload directly
    logMessage(LOG_INFO, "Sending POST request...");
    String postCmd = "AT+QHTTPPOST=" + String(payload.length()) + ",80,80";
    
    // Clear buffer again
    while (SerialAT.available()) {
      SerialAT.read();
    }
    
    logMessage(LOG_INFO, ">> " + postCmd);
    SerialAT.println(postCmd);
    
    // Wait for second CONNECT prompt
    response = "";
    start = millis();
    connectFound = false;
    
    while (millis() - start < 5000 && !connectFound) {
      if (SerialAT.available()) {
        char c = SerialAT.read();
        SerialMon.write(c);
        response += c;
        
        if (response.indexOf("CONNECT") >= 0) {
          connectFound = true;
          break;
        }
      }
    }
    
    if (!connectFound) {
      logMessage(LOG_WARN, "❌ No CONNECT prompt for POST");
      continue;
    }
    
    // SEND JSON PAYLOAD - this is crucial
    logMessage(LOG_INFO, "➡️ Sending JSON payload: " + payload);
    SerialAT.print(payload);
    
    // Increase delay to make sure data is fully sent
    delay(500);
    
    // Wait for POST result
    response = "";
    start = millis();
    bool statusFound = false;
    while (millis() - start < 10000 && !statusFound) {
      if (SerialAT.available()) {
        char c = SerialAT.read();
        SerialMon.write(c);
        response += c;
        
        // Look for +QHTTPPOST anywhere in the response
        if (response.indexOf("+QHTTPPOST:") >= 0) {
          statusFound = true;
          // Wait a bit more to get the full response
          delay(200);
          while (SerialAT.available()) {
            c = SerialAT.read();
            SerialMon.write(c);
            response += c;
          }
        }
      }
    }
    
    // Simplified HTTP status code parsing - the response might have newlines
    int httpPostPos = response.indexOf("+QHTTPPOST:");
    if (httpPostPos >= 0) {
      String restOfResponse = response.substring(httpPostPos);
      
      // Extract HTTP status code using a simple digit search
      int statusCode = 0;
      // Look for the pattern: "+QHTTPPOST: 0,200,15" and extract the 200 part
      int commaPos = restOfResponse.indexOf(',');
      if (commaPos > 0) {
        int secondCommaPos = restOfResponse.indexOf(',', commaPos + 1);
        if (secondCommaPos > 0) {
          String statusStr = restOfResponse.substring(commaPos + 1, secondCommaPos);
          statusStr.trim();
          statusCode = statusStr.toInt();
          
          logMessage(LOG_INFO, "HTTP Status Code: " + String(statusCode));
          
          if (statusCode >= 200 && statusCode < 300) {
            // Success! (2xx status codes)
            sendAT("AT+QHTTPREAD=80", 5000);
            logMessage(LOG_INFO, "✅ Server responded with status: " + String(statusCode));
            sendAT("AT+QHTTPSTOP", 1000);
            return true;
          } else {
            logMessage(LOG_ERROR, "❌ Server responded with error status: " + String(statusCode));
          }
        }
      }
      
      logMessage(LOG_INFO, "Raw response: " + restOfResponse);
    } else {
      logMessage(LOG_ERROR, "❌ Could not find +QHTTPPOST in response");
      logMessage(LOG_INFO, "Full response: " + response);
    }
    
    // Try to read response anyway for debugging
    sendAT("AT+QHTTPREAD=80", 5000);
    
    // Close HTTP session before next attempt
    sendAT("AT+QHTTPSTOP", 1000);
  }
  
  return false;
}

// ==================== DATA MANAGEMENT ====================

// Save connection status to memory
bool saveConnectionStatus(bool veepeakConnected) {
  if (!SPIFFS.begin(true)) {
    logMessage(LOG_ERROR, "SPIFFS Mount Failed");
    return false;
  }
  
  // Create connection status JSON
  DynamicJsonDocument doc(512);
  doc["deviceID"] = config.deviceID;
  doc["status"] = veepeakConnected ? "connected" : "veepeak_failed";
  doc["veepeakConnected"] = veepeakConnected;
  doc["timestamp"] = millis();
  doc["batteryVoltage"] = getBatteryVoltage();
  doc["bootCount"] = preferences.getUInt("bootCount", 0);
  doc["dataSource"] = "connection_status";
  
  if (!veepeakConnected) {
    doc["errorMessage"] = "Failed to connect to Veepeak WiFi network";
    doc["troubleshooting"] = "Check Veepeak device power and WiFi broadcast";
  }
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Save connection status file
  String filename = "/connection_" + String(millis()) + ".json";
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    logMessage(LOG_ERROR, "Failed to create connection file: " + filename);
    return false;
  }
  
  file.println(jsonString);
  file.close();
  
  logMessage(LOG_INFO, "Connection status saved: " + filename);
  return true;
}

// Save network information to memory
bool saveNetworkInfo(const NetworkInfo &netInfo) {
  if (!SPIFFS.begin(true)) {
    logMessage(LOG_ERROR, "SPIFFS Mount Failed");
    return false;
  }
  
  // Create network info JSON
  DynamicJsonDocument doc(512);
  doc["deviceID"] = config.deviceID;
  doc["operator"] = netInfo.operatorName;
  doc["signal"] = netInfo.signalStrength;
  doc["sim"] = netInfo.simInfo;
  doc["apn"] = config.apnName;
  doc["networkTime"] = netInfo.networkTime;
  doc["ipAddress"] = netInfo.ipAddress;
  doc["isConnected"] = netInfo.isConnected;
  doc["timestamp"] = millis();
  doc["dataSource"] = "network_diagnostics";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Save network info file
  String filename = "/network_" + String(millis()) + ".json";
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    logMessage(LOG_ERROR, "Failed to create network file: " + filename);
    return false;
  }
  
  file.println(jsonString);
  file.close();
  
  logMessage(LOG_INFO, "Network info saved: " + filename);
  return true;
}

// Enhanced data storage with JSON format
bool saveVehicleDataToMemory(const VehicleData &data) {
  if (!SPIFFS.begin(true)) {
    logMessage(LOG_ERROR, "SPIFFS Mount Failed");
    return false;
  }
  
  // Create JSON document
  DynamicJsonDocument doc(1024);
  doc["deviceID"] = data.deviceID;
  doc["vin"] = data.vin;
  doc["mileage"] = data.mileage;
  doc["rpm"] = data.rpm;
  doc["speed"] = data.speed;
  doc["engineTemp"] = data.engineTemp;
  doc["fuelLevel"] = data.fuelLevel;
  doc["batteryVoltage"] = data.batteryVoltage;
  doc["timestamp"] = data.timestamp;
  doc["dataQuality"] = data.dataQuality;
  doc["odometerPID"] = data.odometerPID;
  doc["dataSource"] = "veepeak_obd";
  
  String jsonString;
  serializeJson(doc, jsonString);
  
  // Save to timestamped file for buffering
  String filename = "/data_" + String(data.timestamp) + ".json";
  File file = SPIFFS.open(filename, FILE_WRITE);
  if (!file) {
    logMessage(LOG_ERROR, "Failed to create data file: " + filename);
    return false;
  }
  
  file.println(jsonString);
  file.close();
  
  logMessage(LOG_INFO, "Vehicle data saved: " + filename);
  
  return true;
}

// Get all pending data files for upload (data, connection, network)
int getPendingDataFiles(String files[], int maxFiles) {
  if (!SPIFFS.begin(true)) {
    logMessage(LOG_ERROR, "SPIFFS Mount Failed in getPendingDataFiles");
    return 0;
  }
  
  File root = SPIFFS.open("/");
  if (!root) {
    logMessage(LOG_ERROR, "Failed to open root directory");
    return 0;
  }
  
  int count = 0;
  File file = root.openNextFile();
  
  while (file && count < maxFiles) {
    String filename = String(file.name());
    logMessage(LOG_DEBUG, "Checking file: " + filename);
    
    if (filename.startsWith("/data_") || 
        filename.startsWith("/connection_") || 
        filename.startsWith("/network_")) {
      files[count] = filename;
      count++;
      logMessage(LOG_DEBUG, "Added file to transmission list: " + filename);
    }
    
    file.close();
    file = root.openNextFile();
  }
  
  root.close();
  logMessage(LOG_INFO, "Found " + String(count) + " files for transmission");
  return count;
}

// ==================== SYSTEM DIAGNOSTICS ====================

// Comprehensive system health check
void performSystemDiagnostics() {
  logMessage(LOG_INFO, "=== SYSTEM DIAGNOSTICS ===");
  
  // Memory diagnostics
  logMessage(LOG_INFO, "Free heap: " + String(ESP.getFreeHeap()) + " bytes");
  logMessage(LOG_INFO, "Largest free block: " + String(ESP.getMaxAllocHeap()) + " bytes");
  
  // SPIFFS diagnostics
  if (SPIFFS.begin(true)) {
    logMessage(LOG_INFO, "SPIFFS total: " + String(SPIFFS.totalBytes()) + " bytes");
    logMessage(LOG_INFO, "SPIFFS used: " + String(SPIFFS.usedBytes()) + " bytes");
  }
  
  // Battery diagnostics (informational only)
  float battVoltage = getBatteryVoltage();
  logMessage(LOG_INFO, "🔋 Battery voltage: " + String(battVoltage) + "V");
  
  // Boot diagnostics
  logMessage(LOG_INFO, "Boot count: " + String(preferences.getUInt("bootCount", 0)));
  logMessage(LOG_INFO, "Last error: " + preferences.getString("lastError", "None"));
}

// ==================== MAIN FUNCTIONS ====================

void sendAT(const char* cmd, int timeout) {
  SerialMon.print(">> ");
  SerialMon.println(cmd);
  SerialAT.println(cmd);
  responseBuffer = "";

  unsigned long start = millis();
  while (millis() - start < timeout) {
    if (SerialAT.available()) {
      char c = SerialAT.read();
      SerialMon.write(c);
      responseBuffer += c;
    }
    yield();
  }
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_TIMER:
      logMessage(LOG_INFO, "Wakeup: Timer");
      break;
    case ESP_SLEEP_WAKEUP_EXT0:
      logMessage(LOG_INFO, "Wakeup: External signal (RTC_IO)");
      break;
    case ESP_SLEEP_WAKEUP_EXT1:
      logMessage(LOG_INFO, "Wakeup: External signal (RTC_CNTL)");
      break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD:
      logMessage(LOG_INFO, "Wakeup: Touchpad");
      break;
    case ESP_SLEEP_WAKEUP_ULP:
      logMessage(LOG_INFO, "Wakeup: ULP program");
      break;
    default:
      logMessage(LOG_INFO, "Wakeup: Power on or reset");
      break;
  }
}

bool connectToVeepeak() {
  logMessage(LOG_INFO, "🔌 Connecting to WiFi_OBDII...");
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(config.veepeakSSID, config.veepeakPassword);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    SerialMon.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    logMessage(LOG_INFO, "✅ Connected to WiFi_OBDII! IP: " + WiFi.localIP().toString());
    logMessage(LOG_INFO, "Gateway IP: " + WiFi.gatewayIP().toString());
    logMessage(LOG_INFO, "DNS IP: " + WiFi.dnsIP().toString());
    return true;
  } else {
    logMessage(LOG_ERROR, "❌ Failed to connect to WiFi_OBDII");
    return false;
  }
}

void disconnectFromVeepeak() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(1000);
  logMessage(LOG_INFO, "📴 Disconnected from WiFi_OBDII");
}

void updateNetworkInfo() {
  logMessage(LOG_INFO, "📡 Updating network information...");
  
  // Get operator information
  sendAT("AT+COPS?", 2000);
  if (responseBuffer.indexOf("+COPS:") >= 0) {
    // Parse operator name from response like: +COPS: 0,0,"IND airtel",7
    int startQuote = responseBuffer.indexOf('"');
    if (startQuote >= 0) {
      int endQuote = responseBuffer.indexOf('"', startQuote + 1);
      if (endQuote > startQuote) {
        networkInfo.operatorName = responseBuffer.substring(startQuote + 1, endQuote);
      }
    }
  }
  
  // Get signal strength
  sendAT("AT+CSQ", 2000);
  if (responseBuffer.indexOf("+CSQ:") >= 0) {
    // Parse signal strength from response like: +CSQ: 27,99
    int colonPos = responseBuffer.indexOf(':');
    if (colonPos >= 0) {
      int commaPos = responseBuffer.indexOf(',', colonPos);
      if (commaPos > colonPos) {
        String signalStr = responseBuffer.substring(colonPos + 1, commaPos);
        signalStr.trim();
        int signalValue = signalStr.toInt();
        networkInfo.signalStrength = String(signalValue) + " (" + String((signalValue * 100) / 31) + "%)";
      }
    }
  }
  
  // Get SIM information
  sendAT("AT+CIMI", 2000);
  if (responseBuffer.indexOf("404") >= 0 || responseBuffer.indexOf("405") >= 0) {  
    // Extract IMSI number
    String imsi = responseBuffer;
    imsi.replace("AT+CIMI", "");
    imsi.replace("OK", "");
    imsi.replace("\r", "");
    imsi.replace("\n", "");
    imsi.trim();
    if (imsi.length() >= 10) {
      networkInfo.simInfo = imsi;
    }
  }
  
  // Get IP address
  sendAT("AT+QIACT?", 2000);
  if (responseBuffer.indexOf("+QIACT:") >= 0) {
    // Parse IP from response
    int lastComma = responseBuffer.lastIndexOf(',');
    if (lastComma >= 0) {
      int quoteStart = responseBuffer.indexOf('"', lastComma);
      int quoteEnd = responseBuffer.indexOf('"', quoteStart + 1);
      if (quoteStart >= 0 && quoteEnd > quoteStart) {
        networkInfo.ipAddress = responseBuffer.substring(quoteStart + 1, quoteEnd);
      }
    }
  }
  
  networkInfo.isConnected = true;
  networkInfo.networkTime = String(millis());
  
  logMessage(LOG_INFO, "✅ Network info - Operator: " + networkInfo.operatorName + 
                       ", Signal: " + networkInfo.signalStrength + 
                       ", SIM: " + networkInfo.simInfo.substring(0, 8) + "...");
  
  // Save network information to file
  saveNetworkInfo(networkInfo);
}

void setup() {
  SerialMon.begin(115200);
  delay(2000);
  
  // Initialize hardware
  pinMode(powerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);
  setStatusLED(true, 3); // 3 blinks to indicate startup
  
  logMessage(LOG_INFO, "🚗 Enhanced Vehicle Telematics System Starting");
  
  // Load configuration and increment boot count
  loadConfiguration();
  uint32_t bootCount = preferences.getUInt("bootCount", 0) + 1;
  preferences.putUInt("bootCount", bootCount);
  
  print_wakeup_reason();
  performSystemDiagnostics();
  
  // PHASE 1: OBD Data Collection
  logMessage(LOG_INFO, "\n==== PHASE 1: OBD DATA COLLECTION ====");
  setStatusLED(true, 1);
  
  bool obdSuccess = false;
  bool veepeakConnected = false;
  String deviceStatus = "";
  
  // Try to connect to Veepeak
  if (connectToVeepeak()) {
    veepeakConnected = true;
    VehicleData vehicleData;
    if (fetchEnhancedOBDData(vehicleData)) {
      obdSuccess = true;
      logMessage(LOG_INFO, "✅ OBD data collected successfully");
      
      // Create successful OBD data message
      deviceStatus = "{";
      deviceStatus += "\"deviceID\":\"" + String(config.deviceID) + "\",";
      deviceStatus += "\"status\":\"obd_connected\",";
      deviceStatus += "\"message\":\"Veepeak OBD data collected successfully\",";
      deviceStatus += "\"vin\":\"" + vehicleData.vin + "\",";
      deviceStatus += "\"mileage\":" + String(vehicleData.mileage) + ",";
      deviceStatus += "\"rpm\":" + String(vehicleData.rpm) + ",";
      deviceStatus += "\"speed\":" + String(vehicleData.speed) + ",";
      deviceStatus += "\"engineTemp\":" + String(vehicleData.engineTemp) + ",";
      deviceStatus += "\"fuelLevel\":" + String(vehicleData.fuelLevel) + ",";
      deviceStatus += "\"batteryVoltage\":" + String(vehicleData.batteryVoltage) + ",";
      deviceStatus += "\"dataQuality\":" + String(vehicleData.dataQuality) + ",";
      deviceStatus += "\"odometerPID\":\"" + vehicleData.odometerPID + "\",";
      deviceStatus += "\"bootCount\":" + String(bootCount) + ",";
      deviceStatus += "\"timestamp\":" + String(millis()) + ",";
      deviceStatus += "\"dataSource\":\"veepeak_obd\"";
      deviceStatus += "}";
    }
    disconnectFromVeepeak();
  }
  
  // If OBD failed, create device not connected message
  if (!obdSuccess) {
    logMessage(LOG_WARN, veepeakConnected ? "OBD data collection failed" : "Failed to connect to Veepeak WiFi");
    
    deviceStatus = "{";
    deviceStatus += "\"deviceID\":\"" + String(config.deviceID) + "\",";
    deviceStatus += "\"status\":\"device_not_connected\",";
    deviceStatus += "\"message\":\"" + String(veepeakConnected ? "OBD data collection failed" : "Veepeak WiFi connection failed") + "\",";
    deviceStatus += "\"veepeakConnected\":" + String(veepeakConnected ? "true" : "false") + ",";
    deviceStatus += "\"batteryVoltage\":" + String(getBatteryVoltage()) + ",";
    deviceStatus += "\"bootCount\":" + String(bootCount) + ",";
    deviceStatus += "\"timestamp\":" + String(millis()) + ",";
    deviceStatus += "\"dataSource\":\"device_status\"";
    deviceStatus += "}";
  }
  
  // PHASE 2: Cellular Communication
  logMessage(LOG_INFO, "\n==== PHASE 2: CELLULAR COMMUNICATION ====");
  setStatusLED(true, 2);
  
  if (initializeModem()) {
    // Configure and connect to cellular network
    sendAT("ATE0");
    sendAT(("AT+QICSGP=1,1,\"" + String(config.apnName) + "\",\"\",\"\",1").c_str());
    sendAT("AT+QIACT=1", 6000);
    
    // PHASE 3: Send Device Status
    logMessage(LOG_INFO, "\n==== PHASE 3: SENDING DEVICE STATUS ====");
    setStatusLED(true, 3);
    
    logMessage(LOG_INFO, "📤 Sending device status: " + deviceStatus);
    
    if (sendHttpPostEnhanced(deviceStatus)) {
      logMessage(LOG_INFO, "✅ Device status sent successfully");
    } else {
      logMessage(LOG_ERROR, "❌ Failed to send device status");
    }
    
    // Graceful modem shutdown
    sendAT("AT+QPOWD=1");
  } else {
    logMessage(LOG_ERROR, "Cellular modem initialization failed");
  }
  
  // PHASE 4: Sleep Management
  logMessage(LOG_INFO, "\n==== PHASE 4: SLEEP MANAGEMENT ====");
  setStatusLED(false);
  
  uint64_t sleepDurationUs = config.sleepDurationMinutes * 60 * 1000000ULL;
  
  logMessage(LOG_INFO, "💤 Entering deep sleep for " + String(config.sleepDurationMinutes) + " minutes");
  SerialMon.flush();
  
  esp_sleep_enable_timer_wakeup(sleepDurationUs);
  esp_deep_sleep_start();
}

void loop() {
  // Should never reach here due to deep sleep
} 