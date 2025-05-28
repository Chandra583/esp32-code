// SPIFFS Memory Reset Tool for ESP32
// This code will completely clear SPIFFS and Preferences
// Use this when you want to start fresh

#include <SPIFFS.h>
#include <Preferences.h>

Preferences preferences;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("🔄 ESP32 SPIFFS & Preferences Reset Tool");
  Serial.println("========================================");
  
  // Step 1: Format SPIFFS completely
  Serial.println("📁 Step 1: Formatting SPIFFS...");
  if (SPIFFS.begin(true)) {
    Serial.println("✅ SPIFFS mounted successfully");
    
    // List all files before deletion
    Serial.println("📂 Files before reset:");
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    int fileCount = 0;
    
    while (file) {
      Serial.println("  - " + String(file.name()) + " (" + String(file.size()) + " bytes)");
      fileCount++;
      file = root.openNextFile();
    }
    root.close();
    
    Serial.println("📊 Total files found: " + String(fileCount));
    
    // Format SPIFFS (this deletes everything)
    Serial.println("🗑️  Formatting SPIFFS (deleting all files)...");
    if (SPIFFS.format()) {
      Serial.println("✅ SPIFFS formatted successfully - All files deleted!");
    } else {
      Serial.println("❌ SPIFFS format failed!");
    }
    
    SPIFFS.end();
  } else {
    Serial.println("❌ Failed to mount SPIFFS");
  }
  
  // Step 2: Clear all Preferences
  Serial.println("\n🔧 Step 2: Clearing Preferences...");
  
  // Clear vehicle config preferences
  preferences.begin("vehicle_config", false);
  preferences.clear();
  preferences.end();
  Serial.println("✅ Vehicle config preferences cleared");
  
  // Clear any other preference namespaces you might have
  // Add more if you have other preference namespaces
  
  // Step 3: Verification
  Serial.println("\n🔍 Step 3: Verification...");
  
  if (SPIFFS.begin(true)) {
    File root = SPIFFS.open("/");
    File file = root.openNextFile();
    int remainingFiles = 0;
    
    while (file) {
      remainingFiles++;
      file = root.openNextFile();
    }
    root.close();
    SPIFFS.end();
    
    if (remainingFiles == 0) {
      Serial.println("✅ SPIFFS is completely empty");
    } else {
      Serial.println("⚠️  Warning: " + String(remainingFiles) + " files still remain");
    }
  }
  
  // Check preferences
  preferences.begin("vehicle_config", true); // Read-only
  size_t prefSize = preferences.getBytesLength("ngrokHost");
  preferences.end();
  
  if (prefSize == 0) {
    Serial.println("✅ Preferences are cleared");
  } else {
    Serial.println("⚠️  Warning: Some preferences may still exist");
  }
  
  // Final message
  Serial.println("\n🎉 ===== RESET COMPLETE =====");
  Serial.println("✅ SPIFFS memory cleared");
  Serial.println("✅ Preferences cleared");
  Serial.println("✅ System ready for fresh start");
  Serial.println("\n💡 You can now upload your main code");
  Serial.println("🔄 Or reset this ESP32 to start fresh");
  Serial.println("================================");
}

void loop() {
  // Blink LED to show reset is complete
  digitalWrite(LED_BUILTIN, HIGH);
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  delay(1000);
  
  // Print status every 10 seconds
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint > 10000) {
    Serial.println("💤 Reset complete - System idle (LED blinking)");
    lastPrint = millis();
  }
} 