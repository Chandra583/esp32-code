#define SerialAT Serial1
#define SerialMon Serial

#define RXD1 40
#define TXD1 41
#define powerPin 42

const char* HOST = "loved-adapting-vulture.ngrok-free.app";  // <-- Your ngrok domain
const int PORT = 80;
const char* ENDPOINT = "/esp32-status";

String jsonPayload = "{\"status\":\"connected\",\"device\":\"EC200U\"}";
String responseBuffer = "";

// Add these near the top with other globals
const char* VIN = "1HGCM82633A123456";  // Replace with your actual VIN
float totalMileage = 12345.6;           // Replace with actual mileage tracking
unsigned long engineHours = 1234;       // Replace with actual engine hours

void sendAT(const char* cmd, int timeout = 2000);
bool sendHttpPost();

// Create a simple connection payload
String createConnectionPayload() {
  String json = "{";
  json += "\"status\":\"connecting\",";
  json += "\"device\":\"EC200U\"";
  json += "}";
  
  return json;
}

// Create a more detailed JSON payload
String createDetailedPayload() {
  // Get the current time (if you have RTC or network time)
  // This is a placeholder - in reality you would get time from GPS, RTC or network
  unsigned long uptime = millis() / 1000; // seconds since boot
  
  // Create a more detailed JSON payload
  String json = "{";
  json += "\"status\":\"data_update\",";
  json += "\"device\":\"EC200U\",";
  json += "\"vin\":\"" + String(VIN) + "\",";
  json += "\"mileage\":" + String(totalMileage) + ",";
  json += "\"engineHours\":" + String(engineHours) + ",";
  json += "\"uptime\":" + String(uptime) + ",";
  json += "\"timestamp\":" + String(millis());
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

  SerialMon.println("📡 Starting EC200U HTTP POST");

  sendAT("AT");
  sendAT("ATE0");
  sendAT("AT+CPIN?");
  sendAT("AT+CSQ");

  sendAT("AT+QICSGP=1,1,\"airtelgprs.com\",\"\",\"\",1");
  sendAT("AT+CGDCONT=1,\"IP\",\"airtelgprs.com\"");
  sendAT("AT+QIACT=1", 6000);
  sendAT("AT+QIACT?", 3000);
  sendAT("AT+QICLOSE=0", 1000);

  // STEP 1: Send initial connection message
  SerialMon.println("\n---- STEP 1: Sending connection status ----");
  String connectionPayload = createConnectionPayload();
  
  if (sendHttpPostWithPayload(connectionPayload)) {
    SerialMon.println("✅ Connection status sent successfully!");
    
    // STEP 2: Send detailed vehicle data after successful connection
    SerialMon.println("\n---- STEP 2: Sending detailed vehicle data ----");
    
    // Simulate changes to values you would track
    // In a real application, you would read these from sensors
    totalMileage += random(1, 10) / 10.0;  // Add a random distance
    engineHours += random(0, 2);           // Add some engine hours
    
    String detailedPayload = createDetailedPayload();
    
    if (sendHttpPostWithPayload(detailedPayload)) {
      SerialMon.println("✅ Vehicle data sent successfully!");
    } else {
      SerialMon.println("❌ Failed to send vehicle data");
    }
  } else {
    SerialMon.println("❌ Failed to send connection status");
  }
  
  // Add this to properly end communications
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
