#include <AsyncTCP.h>
#include <Arduino_JSON.h>

namespace asyncweb {
    #include <ESPAsyncWebServer.h>
}


asyncweb::AsyncWebServer server(80);
asyncweb::AsyncWebSocket ws("/ws");

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  asyncweb::AwsFrameInfo *info = (asyncweb::AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    String message = (char*)data;
    // Check if the message is "getReadings"
    if (strcmp((char*)data, "getReadings") == 0) {
      //if it is, send current sensor readings
    //   readings["pir"] = pin15.buttonPressed();
    //   String sensorReadings = JSON.stringify(readings);
    //   Serial.print(sensorReadings);
    //   ws.textAll(sensorReadings);
      Serial.println("get readings");
    }
    if (strcmp((char*)data, "toggleBlueLight") == 0) {
    //   pin17.toggleLight();
      Serial.print("toggle blue light");
    }
  }
}

void onEvent(asyncweb::AsyncWebSocket *server, asyncweb::AsyncWebSocketClient *client, asyncweb::AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case asyncweb::WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case asyncweb::WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case asyncweb::WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case asyncweb::WS_EVT_PONG:
    case asyncweb::WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}