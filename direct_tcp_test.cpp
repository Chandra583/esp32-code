#include <WiFi.h>

// WiFi credentials
const char* ssid = "Airtel_chet_5279";         // 📝 Replace with your WiFi SSID
const char* password = "Air@54394"; // 🔐 Replace with your WiFi password

const char* serverIP = "192.168.1.4";   // 🔁 Change to your PC's IP address
const int serverPort = 3000;

// WiFiClient for direct TCP connection
WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n=== ESP32 Direct TCP Test ===");
  
  // Connect to WiFi
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // Test direct TCP connection and HTTP request
  testDirectConnection();
}

void testDirectConnection() {
  Serial.println("\n--- Testing direct TCP connection ---");
  Serial.print("Connecting to ");
  Serial.print(serverIP);
  Serial.print(":");
  Serial.println(serverPort);
  
  // Attempt to connect via TCP
  if (client.connect(serverIP, serverPort)) {
    Serial.println("✅ TCP CONNECTION SUCCESSFUL!");
    
    // Create HTTP POST request manually
    String jsonPayload = "{\"status\":\"connected\",\"device\":\"ESP32\"}";
    String postRequest = String("POST /esp32-status HTTP/1.1\r\n") +
                         "Host: " + serverIP + "\r\n" +
                         "Content-Type: application/json\r\n" +
                         "Content-Length: " + jsonPayload.length() + "\r\n" +
                         "Connection: close\r\n" +
                         "\r\n" +
                         jsonPayload;
    
    // Send the raw HTTP request
    Serial.println("Sending HTTP POST request...");
    Serial.println("--- Request start ---");
    Serial.println(postRequest);
    Serial.println("--- Request end ---");
    
    client.print(postRequest);
    
    // Wait for server response
    unsigned long timeout = millis();
    while (client.available() == 0) {
      if (millis() - timeout > 5000) {
        Serial.println("⚠️ Server response timeout!");
        client.stop();
        return;
      }
    }
    
    // Read and print server response
    Serial.println("\n--- Server Response ---");
    while (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
    Serial.println("\n--- End Response ---");
    
    // Close the connection
    client.stop();
    Serial.println("\nConnection closed.");
    
  } else {
    Serial.println("❌ TCP CONNECTION FAILED!");
    Serial.println("\nTROUBLESHOOTING STEPS:");
    Serial.println("1. Double-check the server IP: " + String(serverIP));
    Serial.println("2. Confirm your server is running on port " + String(serverPort));
    Serial.println("3. Check your computer's firewall settings");
    Serial.println("4. Try disabling any VPN on your computer");
    Serial.println("5. Make sure both devices are on the same network/subnet");
    Serial.println("6. Try restarting your server and router");
  }
}

void loop() {
  // Try again every 30 seconds
  delay(30000);
  
  Serial.println("\n--- Testing connection again ---");
  testDirectConnection();
} 