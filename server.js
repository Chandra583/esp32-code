const express = require( express");
const app = express();
const port = 3000;

// Middleware to capture raw body for debugging and parse JSON manually if needed
app.use("/esp32-status", (req, res, next) => {
  let rawBody = "";
  req.on("data", (chunk) => {
    rawBody += chunk.toString();
  });
  req.on("end", () => {
    console.log("📡 RAW DATA FROM ESP32:", rawBody);
    req.rawBody = rawBody;

    // If Content-Type is text/plain but data looks like JSON, parse it manually
    if (
      req.get("Content-Type") === "text/plain" &&
      rawBody.trim().startsWith("{")
    ) {
      try {
        req.body = JSON.parse(rawBody);
        console.log("✅ Manually parsed JSON from text/plain");
      } catch (e) {
        console.log("❌ Failed to parse JSON manually:", e.message);
      }
    }
    next();
  });
});

// Middleware to parse JSON (for proper application/json requests)
app.use(express.json());

// Middleware to log all requests
app.use((req, res, next) => {
  const timestamp = new Date().toISOString();
  console.log(`[${timestamp}] ${req.method} ${req.url}`);
  console.log(`🔍 Content-Type: ${req.get("Content-Type")}`);
  console.log(`📏 Content-Length: ${req.get("Content-Length")}`);

  if (req.body && Object.keys(req.body).length > 0) {
    console.log(
      "✅ Request Body (parsed JSON):",
      JSON.stringify(req.body, null, 2)
    );
  } else {
    console.log("❌ Request Body is empty or not parsed");
  }
  next();
});

// Main endpoint to receive ESP32 data
app.post("/esp32-status", (req, res) => {
  const timestamp = new Date().toISOString();

  console.log("\n🚗 ===== ESP32 DATA RECEIVED =====");
  console.log(`⏰ Time: ${timestamp}`);

  // Check if req.body exists and has data
  if (!req.body || Object.keys(req.body).length === 0) {
    console.log("❌ ERROR: No JSON data received or failed to parse");
    console.log("📄 Raw request body:", req.body);
    console.log("🔍 Content-Type:", req.get("Content-Type"));

    res.status(400).json({
      success: false,
      message: "No valid JSON data received",
      timestamp: timestamp,
    });
    return;
  }

  console.log(`📱 Device: ${req.body.deviceID || "Unknown"}`);
  console.log(`📊 Data Source: ${req.body.dataSource || "Unknown"}`);

  // Handle different types of data
  if (req.body.dataSource === "veepeak_obd") {
    console.log("🔧 OBD Vehicle Data:");
    console.log(`  VIN: ${req.body.vin}`);
    console.log(`  Mileage: ${req.body.mileage} km`);
    console.log(`  RPM: ${req.body.rpm}`);
    console.log(`  Speed: ${req.body.speed} km/h`);
    console.log(`  Engine Temp: ${req.body.engineTemp}°C`);
    console.log(`  Fuel Level: ${req.body.fuelLevel}%`);
    console.log(`  Battery: ${req.body.batteryVoltage}V`);
    console.log(`  Data Quality: ${req.body.dataQuality}%`);
    console.log(`  Odometer PID: ${req.body.odometerPID || "N/A"}`);
  } else if (req.body.dataSource === "connection_status") {
    console.log("🔌 Connection Status:");
    console.log(`  Status: ${req.body.status}`);
    console.log(`  Veepeak Connected: ${req.body.veepeakConnected}`);
    console.log(`  Battery: ${req.body.batteryVoltage}V`);
    console.log(`  Boot Count: ${req.body.bootCount}`);
    if (req.body.errorMessage) {
      console.log(`  ❌ Error: ${req.body.errorMessage}`);
    }
    if (req.body.troubleshooting) {
      console.log(`  💡 Troubleshooting: ${req.body.troubleshooting}`);
    }
  } else if (req.body.dataSource === "network_diagnostics") {
    console.log("📡 Network Diagnostics:");
    console.log(`  Operator: ${req.body.operator}`);
    console.log(`  Signal: ${req.body.signal}`);
    console.log(`  SIM: ${req.body.sim}`);
    console.log(`  APN: ${req.body.apn}`);
    console.log(`  IP Address: ${req.body.ipAddress}`);
    console.log(`  Connected: ${req.body.isConnected}`);
  } else if (req.body.dataSource === "device_status") {
    console.log("📱 Device Status Update:");
    console.log(`  Status: ${req.body.status}`);
    console.log(`  Message: ${req.body.message}`);
    console.log(`  Battery: ${req.body.batteryVoltage}V`);
    console.log(`  Boot Count: ${req.body.bootCount}`);

    if (req.body.status === "device_not_connected") {
      console.log("  ❌ Device Issue: OBD device not connected");
      console.log("  💡 Check: Veepeak power and WiFi broadcast");
    }
  } else if (req.body.dataSource === "dummy_data") {
    console.log("🎭 Dummy Vehicle Data (Veepeak Failed):");
    console.log(`  VIN: ${req.body.vin}`);
    console.log(`  Mileage: ${req.body.mileage} km`);
    console.log(`  RPM: ${req.body.rpm}`);
    console.log(`  Speed: ${req.body.speed} km/h`);
    console.log(`  Engine Temp: ${req.body.engineTemp}°C`);
    console.log(`  Fuel Level: ${req.body.fuelLevel}%`);
    console.log(`  Battery: ${req.body.batteryVoltage}V`);
    console.log(`  Data Quality: ${req.body.dataQuality}%`);
    console.log(
      `  ⚠️  Note: This is dummy data - Veepeak device not connected`
    );
  } else {
    console.log("📄 Raw Data:");
    console.log(JSON.stringify(req.body, null, 2));
  }

  console.log("================================\n");

  // Send success response
  res.status(200).json({
    success: true,
    message: "Data received successfully",
    timestamp: timestamp,
    receivedData: {
      deviceID: req.body.deviceID,
      dataSource: req.body.dataSource,
      timestamp: req.body.timestamp,
    },
  });
});

// Health check endpoint
app.get("/health", (req, res) => {
  res.status(200).json({
    status: "OK",
    message: "ESP32 Data Receiver Server is running",
    timestamp: new Date().toISOString(),
  });
});

// Root endpoint
app.get("/", (req, res) => {
  res.send(`
    <h1>🚗 ESP32 Vehicle Telematics Server</h1>
    <p>Server is running and ready to receive data from ESP32</p>
    <p><strong>Endpoints:</strong></p>
    <ul>
      <li><code>POST /esp32-status</code> - Receive ESP32 data</li>
      <li><code>GET /health</code> - Health check</li>
    </ul>
    <p><strong>Server Time:</strong> ${new Date().toISOString()}</p>
  `);
});

// Error handling middleware
app.use((err, req, res, next) => {
  console.error("❌ Server Error:", err);
  res.status(500).json({
    success: false,
    message: "Internal server error",
    error: err.message,
  });
});

// Start server
app.listen(port, () => {
  console.log("🚀 ===== ESP32 DATA RECEIVER SERVER =====");
  console.log(`📡 Server running on port ${port}`);
  console.log(`🌐 Local URL: http://localhost:${port}`);
  console.log(`🔗 Ngrok URL: https://loved-adapting-vulture.ngrok-free.app`);
  console.log("⏳ Waiting for ESP32 data...\n");
});

// Graceful shutdown
process.on("SIGINT", () => {
  console.log("\n🛑 Server shutting down gracefully...");
  process.exit(0);
});
