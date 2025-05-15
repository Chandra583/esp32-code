const express = require("express");
const cors = require("cors");
const app = express();
const port = 3000;

// Enable CORS for all routes
app.use(cors());

// Middleware to parse JSON bodies
app.use(express.json());
app.use(express.urlencoded({ extended: true }));

// Store the latest ESP32 status
let latestStatus = {
  connected: false,
  lastUpdate: null,
  device: null,
  details: {},
};

// POST endpoint for ESP32 status
app.post("/esp32-status", (req, res) => {
  console.log("Request received on /esp32-status endpoint");

  // Get the payload from the request body
  const payload = req.body;

  // Get current timestamp
  const timestamp = new Date().toISOString();

  // Update the latest status
  latestStatus = {
    connected: payload.status === "connected",
    lastUpdate: timestamp,
    device: payload.device,
    details: payload,
  };

  // Print confirmation to terminal with emoji
  console.log("✅ ESP32 status update received");

  // Log the payload with timestamp
  console.log(`[${timestamp}] Payload:`, JSON.stringify(payload, null, 2));

  // Create enhanced response with connection details
  const responseData = {
    message: `Hello ${payload.device}! You're successfully connected to the server.`,
    status: "connected",
    receivedFrom: {
      device: payload.device || "Unknown device",
      ip: payload.ip || req.ip || "Unknown IP",
      wifi: payload.network || "Unknown network",
    },
    serverInfo: {
      time: timestamp,
      uptime: process.uptime().toFixed(0) + " seconds",
    },
    connectionQuality: payload.rssi
      ? payload.rssi > -50
        ? "Excellent"
        : payload.rssi > -60
        ? "Very Good"
        : payload.rssi > -70
        ? "Good"
        : payload.rssi > -80
        ? "Fair"
        : "Poor"
      : "Unknown",
  };

  // Send enhanced response
  res.status(200).json(responseData);
});

// GET endpoint to check WiFi status
app.get("/wifi-status", (req, res) => {
  if (latestStatus.lastUpdate) {
    const lastUpdateTime = new Date(latestStatus.lastUpdate);
    const currentTime = new Date();
    const timeDiff = Math.floor((currentTime - lastUpdateTime) / 1000);

    res.json({
      connected: latestStatus.connected,
      device: latestStatus.device,
      lastUpdate: latestStatus.lastUpdate,
      secondsSinceLastUpdate: timeDiff,
      details: latestStatus.details,
    });
  } else {
    res.json({
      connected: false,
      message: "No ESP32 device has reported status yet",
    });
  }
});

// Add a basic GET endpoint that ESP32 can use to test connectivity
app.get("/", (req, res) => {
  console.log("Basic connection test from client");
  res.send("ESP32 Server is running!");
});

// Add more detailed error handling
app.use((err, req, res, next) => {
  console.error("Server error:", err);
  res.status(500).send("Server error occurred");
});

// Start the server
app.listen(port, "0.0.0.0", () => {
  console.log(
    `ESP32 status server listening on ALL interfaces at port ${port}`
  );
  console.log(`Your server IP addresses:`);
  const { networkInterfaces } = require("os");
  const nets = networkInterfaces();
  for (const name of Object.keys(nets)) {
    for (const net of nets[name]) {
      // Skip over non-IPv4 and internal (loopback) addresses
      if (net.family === "IPv4" && !net.internal) {
        console.log(`  - ${name}: ${net.address}`);
      }
    }
  }

  console.log("\nIMPORTANT: Use one of these IP addresses in your ESP32 code");
  console.log("Ready to receive ESP32 connections! 🚀");
});
