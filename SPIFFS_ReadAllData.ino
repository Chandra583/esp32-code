#include "FS.h"
#include "SPIFFS.h"

void setup() {
  Serial.begin(115200);

  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    return;
  }

  Serial.println("SPIFFS mounted successfully.");

  File root = SPIFFS.open("/");
  if (!root) {
    Serial.println("- failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    Serial.println(" - not a directory");
    return;
  }

  Serial.println("Listing files in SPIFFS:");
  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      Serial.print("  DIR : ");
      Serial.println(file.name());
    } else {
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("	SIZE: ");
      Serial.println(file.size());

      // Print file content
      Serial.println("  CONTENT:");
      while (file.available()) {
        Serial.write(file.read());
      }
      Serial.println(); // Add a newline after file content
    }
    file.close();
    file = root.openNextFile();
  }
  Serial.println("Finished listing files.");
}

void loop() {
  // Nothing to do here
} 