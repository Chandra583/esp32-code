#define SerialAT Serial1
#define SerialMon Serial

#define RXD1 40
#define TXD1 41
#define powerPin 42

const char* HOST = "loved-adapting-vulture.ngrok-free.app";  // <-- Your ngrok domain
const int PORT = 80;
const char* ENDPOINT = "/esp32-status";

String responseBuffer = "";

// Device info
const char* DEVICE_ID = "ESP32_VTS_001";
const char* VIN = "1HGCM82633A123456";
float totalMileage = 12345.6;
unsigned long engineHours = 1234;

void sendAT(const char* cmd, int timeout = 2000);

// Create connection status payload (when Veepeak fails)
String createVeepeakFailedPayload() {
  String json = "{";
  json += "\"deviceID\":\"" + String(DEVICE_ID) + "\",";
  json += "\"status\":\"veepeak_failed\",";
  json += "\"veepeakConnected\":false,";
  json += "\"errorMessage\":\"Failed to connect to Veepeak WiFi network\",";
  json += "\"troubleshooting\":\"Check Veepeak device power and WiFi broadcast\",";
  json += "\"batteryVoltage\":1.19,";
  json += "\"bootCount\":37,";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"dataSource\":\"connection_status\"";
  json += "}";
  
  return json;
}

// Create dummy vehicle data payload
String createDummyVehiclePayload() {
  String json = "{";
  json += "\"deviceID\":\"" + String(DEVICE_ID) + "\",";
  json += "\"vin\":\"" + String(VIN) + "\",";
  json += "\"mileage\":" + String(totalMileage) + ",";
  json += "\"rpm\":0,";
  json += "\"speed\":0,";
  json += "\"engineTemp\":0,";
  json += "\"fuelLevel\":0,";
  json += "\"batteryVoltage\":1.19,";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"dataQuality\":0,";
  json += "\"dataSource\":\"dummy_data\"";
  json += "}";
  
  return json;
}

// Create network diagnostics payload
String createNetworkPayload() {
  String json = "{";
  json += "\"deviceID\":\"" + String(DEVICE_ID) + "\",";
  json += "\"operator\":\"IND airtel\",";
  json += "\"signal\":\"28 (90%)\",";
  json += "\"sim\":\"40445069...\",";
  json += "\"apn\":\"airtelgprs.com\",";
  json += "\"ipAddress\":\"100.65.227.198\",";
  json += "\"isConnected\":true,";
  json += "\"timestamp\":" + String(millis()) + ",";
  json += "\"dataSource\":\"network_diagnostics\"";
  json += "}";
  
  return json;
}

// Function to send a specific JSON payload via HTTP POST
bool sendHttpPostWithPayload(String payload) {
  // Try up to 3 times
  for (int attempt = 1; attempt <= 3; attempt++) {
    SerialMon.print("Attempt ");
    SerialMon.print(attempt);
    SerialMon.println(" of 3");
    
    // Reset HTTP context
    sendAT("AT+QHTTPSTOP", 1000);
    delay(500);
    
    // Configure HTTP session
    sendAT("AT+QHTTPCFG=\"contextid\",1");
    sendAT("AT+QHTTPCFG=\"responseheader\",1");
    
    // CRITICAL: Set content type to JSON
    sendAT("AT+QHTTPCFG=\"contenttype\",1");  // 1 = application/json
    
    // Directly try HTTPS connection
    String url = "https://" + String(HOST) + String(ENDPOINT);
    String urlCmd = "AT+QHTTPURL=" + String(url.length()) + ",80";
    
    // Clear serial buffer before sending URL command
    while (SerialAT.available()) {
      SerialAT.read();
    }
    
    // Send URL command and wait for CONNECT
    SerialMon.println(">> " + urlCmd);
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
      SerialMon.println("❌ No CONNECT prompt for URL");
      continue;
    }
    
    // Send the URL
    SerialMon.println("Sending URL: " + url);
    SerialAT.print(url);
    delay(1000);
    
    // Get all available data
    while (SerialAT.available()) {
      SerialMon.write(SerialAT.read());
    }
    
    // Send HTTP POST with payload directly
    SerialMon.println("Sending POST request...");
    String postCmd = "AT+QHTTPPOST=" + String(payload.length()) + ",80,80";
    
    // Clear buffer again
    while (SerialAT.available()) {
      SerialAT.read();
    }
    
    SerialMon.println(">> " + postCmd);
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
      SerialMon.println("❌ No CONNECT prompt for POST");
      continue;
    }
    
    // SEND JSON PAYLOAD - this is crucial
    SerialMon.println("➡️ Sending JSON payload: " + payload);
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
          
          SerialMon.print("HTTP Status Code: ");
          SerialMon.println(statusCode);
          
          if (statusCode >= 200 && statusCode < 300) {
            // Success! (2xx status codes)
            sendAT("AT+QHTTPREAD=80", 5000);
            SerialMon.println("✅ Server responded with status: " + String(statusCode));
            sendAT("AT+QHTTPSTOP", 1000);
            return true;
          } else {
            SerialMon.println("❌ Server responded with error status: " + String(statusCode));
          }
        }
      }
      
      SerialMon.println("Raw response: " + restOfResponse);
    } else {
      SerialMon.println("❌ Could not find +QHTTPPOST in response");
      SerialMon.println("Full response: " + response);
    }
    
    // Try to read response anyway for debugging
    sendAT("AT+QHTTPREAD=80", 5000);
    
    // Close HTTP session before next attempt
    sendAT("AT+QHTTPSTOP", 1000);
  }
  
  return false;
}

void setup() {
  pinMode(powerPin, OUTPUT);
  digitalWrite(powerPin, LOW);

  SerialMon.begin(115200);
  SerialAT.begin(115200, SERIAL_8N1, RXD1, TXD1);
  delay(3000);

  SerialMon.println("📡 Starting ESP32 Simple HTTP POST Test");
  SerialMon.println("🔌 Simulating Veepeak connection failure scenario");

  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CPIN?");
  sendAT("AT+CSQ");

  sendAT("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1");
  sendAT("AT+CGDCONT=1,\"IP\",\"airtelgprs.com\"");
  sendAT("AT+QIACT=1", 6000);
  sendAT("AT+QIACT?", 3000);
  sendAT("AT+QICLOSE=0", 1000);

  // STEP 1: Send Veepeak connection failure status
  SerialMon.println("\n---- STEP 1: Sending Veepeak failure status ----");
  String veepeakFailedPayload = createVeepeakFailedPayload();
  
  if (sendHttpPostWithPayload(veepeakFailedPayload)) {
    SerialMon.println("✅ Veepeak failure status sent successfully!");
    
    // STEP 2: Send dummy vehicle data
    SerialMon.println("\n---- STEP 2: Sending dummy vehicle data ----");
    String dummyVehiclePayload = createDummyVehiclePayload();
    
    if (sendHttpPostWithPayload(dummyVehiclePayload)) {
      SerialMon.println("✅ Dummy vehicle data sent successfully!");
      
      // STEP 3: Send network diagnostics
      SerialMon.println("\n---- STEP 3: Sending network diagnostics ----");
      String networkPayload = createNetworkPayload();
      
      if (sendHttpPostWithPayload(networkPayload)) {
        SerialMon.println("✅ Network diagnostics sent successfully!");
      } else {
        SerialMon.println("❌ Failed to send network diagnostics");
      }
    } else {
      SerialMon.println("❌ Failed to send dummy vehicle data");
    }
  } else {
    SerialMon.println("❌ Failed to send Veepeak failure status");
  }
  
  // Add this to properly end communications
  SerialMon.println("\n🎉 HTTP POST Test Complete!");
  SerialMon.println("Going to sleep now - will restart to send new data");
  SerialAT.println("AT+QPOWD=1"); // Graceful power-down of the modem
  delay(1000);
}

void loop() {
  // Sleep or do nothing - we don't want to repeat the transmission
  delay(3600000); // Sleep for an hour (adjust as needed)
}

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
  }
  SerialMon.println();
} 