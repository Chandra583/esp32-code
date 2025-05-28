// Simple Blink Sketch to Overwrite Deep Sleep Code
// This will completely replace your current code

void setup() {
  Serial.begin(115200);
  pinMode(2, OUTPUT); // Built-in LED on most ESP32 boards
  
  Serial.println("ESP32 Reset Complete!");
  Serial.println("Deep sleep code has been overwritten");
  Serial.println("You can now upload your vehicle telematics code safely");
}

void loop() {
  digitalWrite(2, HIGH);   // Turn LED on
  Serial.println("LED ON - ESP32 is working normally");
  delay(1000);
  
  digitalWrite(2, LOW);    // Turn LED off
  Serial.println("LED OFF - Ready for new code upload");
  delay(1000);
} 