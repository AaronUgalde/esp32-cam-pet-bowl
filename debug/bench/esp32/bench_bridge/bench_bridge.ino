/*
 * bench_bridge.ino — ESP32 dev: puente WiFi SIN carrito (sin UNO motores)
 *
 * Para probar con:
 *   - Arduino UNO #2 con sensor_board.ino (UART)
 *   - PC con debug/mock_controller.py
 *
 * NO usa Serial1 / motores. Al ladrido solo imprime [BENCH] STOP simulado.
 *
 * UART sensores (Serial2):
 *   ESP32 RX=18 ← UNO TX pin 7
 *   ESP32 TX=19 → UNO RX pin 6
 *   GND común
 *
 * secrets.h en la raíz del repo.
 * Librerías: WebSockets, ArduinoJson
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "../../../../secrets.h"

const int SENSOR_RX = 18;
const int SENSOR_TX = 19;
const long SENSOR_BAUD = 57600;

WebSocketsClient webSocket;
HardwareSerial sensorSerial(2);

bool wsConnected = false;
bool wsEverConnected = false;
String sensorRxBuffer;

void forwardToPc(const char *json) {
  if (wsConnected) webSocket.sendTXT(json);
}

void sendToSensor(const char *json) {
  Serial.print(F("[SENSOR TX] "));
  Serial.println(json);
  sensorSerial.println(json);
}

void onBarkBench() {
  Serial.println(F("[BENCH] STOP motores simulado (no hay carrito)"));
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      if (wsEverConnected)
        Serial.println(F("[WS] Desconectado"));
      else
        Serial.printf("[WS] Sin conexion a %s:%d — ejecuta mock_controller.py\n", WS_HOST, WS_PORT);
      break;
    case WStype_CONNECTED:
      wsConnected = true;
      wsEverConnected = true;
      Serial.println(F("[WS] PC conectada"));
      break;
    case WStype_TEXT: {
      Serial.print(F("[WS RX] "));
      Serial.write(payload, length);
      Serial.println();
      StaticJsonDocument<128> doc;
      if (deserializeJson(doc, payload, length)) break;
      const char *cmd = doc["cmd"];
      if (!cmd) break;
      if (strcmp(cmd, "stop") == 0) {
        onBarkBench();
      } else if (strcmp(cmd, "play_audio") == 0 || strcmp(cmd, "ping") == 0) {
        String out;
        serializeJson(doc, out);
        sendToSensor(out.c_str());
      }
      break;
    }
    default: break;
  }
}

void handleSensorLine(const String &line) {
  Serial.print(F("[SENSOR] "));
  Serial.println(line);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, line)) return;

  const char *event = doc["event"];
  if (event && strcmp(event, "bark") == 0) {
    onBarkBench();
  }
  forwardToPc(line.c_str());
}

void readSensorUart() {
  while (sensorSerial.available()) {
    char c = sensorSerial.read();
    if (c == '\n' || c == '\r') {
      if (sensorRxBuffer.length() > 0) {
        handleSensorLine(sensorRxBuffer);
        sensorRxBuffer = "";
      }
    } else {
      sensorRxBuffer += c;
      if (sensorRxBuffer.length() > 200) sensorRxBuffer = "";
    }
  }
}

void setup() {
  Serial.begin(115200);
  sensorSerial.begin(SENSOR_BAUD, SERIAL_8N1, SENSOR_RX, SENSOR_TX);

  Serial.println(F("\n=== BENCH: bench_bridge (sin motores) ==="));
  Serial.println(F("Conecta UNO #2 con sensor_board.ino en GPIO 18/19"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\n[WiFi] IP ESP32: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] PC: ws://%s:%d\n", WS_HOST, WS_PORT);

  webSocket.begin(WS_HOST, WS_PORT, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(30000, 5000, 2);
}

void loop() {
  webSocket.loop();
  readSensorUart();
  delay(5);
}
