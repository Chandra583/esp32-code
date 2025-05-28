#include "reset_utils.h"

Preferences preferences_reset_util; // Use a distinct name to avoid conflicts if Preferences is used elsewhere

void setupSerialAndWelcome() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("🔄 ESP32 SPIFFS & Preferences Reset Tool");
  Serial.println("========================================");
}

void performSPIFFSFormat() {
  Serial.println("📁 Step 1: Formatting SPIFFS...");
  if (SPIFFS.begin(true)) {
    Serial.println("✅ SPIFFS mounted successfully");
    
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
}

void performPreferencesClear() {
  Serial.println("\n🔧 Step 2: Clearing Preferences...");
  
  preferences_reset_util.begin("vehicle_config", false);
  preferences_reset_util.clear();
  preferences_reset_util.end();
  Serial.println("✅ Vehicle config preferences cleared");
  
  // Add more if you have other preference namespaces
  // For example:
  // preferences_reset_util.begin("another_namespace", false);
  // preferences_reset_util.clear();
  // preferences_reset_util.end();
  // Serial.println("✅ Another namespace preferences cleared");
}

void verifySystemReset() {
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
  
  preferences_reset_util.begin("vehicle_config", true); // Read-only
  // To make verification more robust, we should check if any key exists, 
  // rather than a specific key like "ngrokHost" if that might not always be present.
  // However, Preferences library doesn't have a direct way to check if a namespace is empty 
  // or list all keys. We'll stick to checking a known key or assuming if getBytesLength is 0 for one, it's likely cleared.
  // A more thorough check would involve iterating known keys if they are predefined.
  bool isVehicleConfigEmpty = true;
  String key;
  // Attempt to get the first key, if Preferences supports it in your version
  // key = preferences_reset_util.getString("dummyKey", ""); // This is a conceptual check
  // For now, we will check a specific key that you were checking previously.
  // If 'ngrokHost' is not a guaranteed key, this check is not foolproof for all scenarios.
  if (preferences_reset_util.getBytesLength("ngrokHost") > 0) { 
      isVehicleConfigEmpty = false;
  }
  preferences_reset_util.end();
  
  if (isVehicleConfigEmpty) {
    Serial.println("✅ Preferences (vehicle_config) appear cleared");
  } else {
    Serial.println("⚠️  Warning: Some preferences may still exist in 'vehicle_config'");
  }
}

void printResetCompletionMessages() {
  Serial.println("\n🎉 ===== RESET COMPLETE =====");
  Serial.println("✅ SPIFFS memory cleared");
  Serial.println("✅ Preferences cleared");
  Serial.println("✅ System ready for fresh start");
  Serial.println("\n💡 You can now upload your main code");
  Serial.println("🔄 Or reset this ESP32 to start fresh");
  Serial.println("================================");
} 