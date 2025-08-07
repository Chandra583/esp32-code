# ESP32 Vehicle Telematics Server

## Quick Setup

```bash
# Install dependencies
npm install

# Start server
npm start
```

## Testing with curl

The server will be running on `http://localhost:3000`

## Server Endpoints

- `POST /esp32-status` - Receive ESP32 data
- `GET /health` - Health check
- `GET /` - Server info page
