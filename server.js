const express = require("express");
const app = express();
const port = 3000;

// Support both JSON and raw body formats
app.use(express.json());
app.use(express.raw({ type: "*/*" }));
app.use(express.text({ type: "*/*" }));
app.use(express.urlencoded({ extended: true }));

app.post("/esp32-status", (req, res) => {
  let payload;
  const timestamp = new Date().toISOString();

  console.log("✅ ESP32 status update received");
  console.log(`[${timestamp}] Content-Type:`, req.headers["content-type"]);

  // Try to parse payload based on what we receive
  try {
    if (typeof req.body === "string") {
      payload = JSON.parse(req.body);
    } else if (Buffer.isBuffer(req.body)) {
      payload = JSON.parse(req.body.toString());
    } else {
      payload = req.body;
    }
    console.log(`[${timestamp}] Payload:`, JSON.stringify(payload, null, 2));
  } catch (err) {
    console.log(`[${timestamp}] Raw body:`, req.body);
    console.log(`[${timestamp}] Parse error:`, err.message);
  }

  res.status(200).send("Status received");
});

app.listen(port, "0.0.0.0", () => {
  console.log(`🚀 Listening on http://localhost:${port}`);
});
