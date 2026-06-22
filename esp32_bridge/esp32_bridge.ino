/*
 * esp32_bridge.ino — ESP32 dev: solo puente WiFi (sin sensores en GPIO)
 *
 * Usa cuando mic/MQ135/MP3 están en un Arduino UNO #2 (sensor_board.ino).
 *
 * UART:
 *   Serial1 GPIO 16/17 → Arduino UNO #1 motores (patrol_motors.ino)
 *   Serial2 GPIO 18/19 → Arduino UNO #2 sensores (sensor_board.ino)
 *
 * WebSocket → PC (robot_controller.py / mock_controller.py)
 * Mismo secrets.h en la raíz del repo.
 *
 * Librerías: WebSockets, ArduinoJson
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include "../secrets.h"

// Serial1 → motores
const int MOTOR_RX = 16;
const int MOTOR_TX = 17;

// Serial2 → sensor board
const int SENSOR_RX = 18;
const int SENSOR_TX = 19;

WebSocketsClient webSocket;
HardwareSerial sensorSerial(2);

bool wsConnected = false;
bool wsEverConnected = false;
int lastAir = 0;

String sensorRxBuffer;

void sendToMotor(const char *json) {
  Serial1.println(json);
}

void sendStopToMotor() {
  sendToMotor("{\"cmd\":\"stop\"}");
}

void sendToSensor(const char *json) {
  sensorSerial.println(json);
}

void forwardToPc(const char *json) {
  if (wsConnected) {
    webSocket.sendTXT(json);
  }
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      if (wsEverConnected) {
        Serial.println(F("[WS] Desconectado (reintento…)"));
      } else {
        Serial.printf("[WS] No conecta a %s:%d — ¿mock/robot_controller activo?\n", WS_HOST, WS_PORT);
      }
      break;

    case WStype_CONNECTED:
      wsConnected = true;
      wsEverConnected = true;
      Serial.println(F("[WS] Conectado a PC"));
      break;

    case WStype_ERROR:
      Serial.println(F("[WS] Error"));
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
        sendStopToMotor();
        sendToSensor("{\"cmd\":\"stop\"}");
      } else if (strcmp(cmd, "play_audio") == 0) {
        String out;
        serializeJson(doc, out);
        sendToSensor(out.c_str());
      } else if (strcmp(cmd, "ping") == 0) {
        sendToSensor("{\"cmd\":\"ping\"}");
      }
      break;
    }

    default:
      break;
  }
}

void connectWebSocket() {
  webSocket.begin(WS_HOST, WS_PORT, "/");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(30000, 5000, 2);
}

void handleSensorLine(const String &line) {
  Serial.print(F("[SENSOR] "));
  Serial.println(line);

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, line)) {
    Serial.println(F("[SENSOR] JSON invalido"));
    return;
  }

  const char *event = doc["event"];
  if (!event) return;

  if (doc.containsKey("air")) {
    lastAir = doc["air"];
  }

  if (strcmp(event, "bark") == 0) {
    sendStopToMotor();
    Serial.println(F("[BRIDGE] Ladrido → STOP motores"));
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
  Serial1.begin(115200, SERIAL_8N1, MOTOR_RX, MOTOR_TX);
  sensorSerial.begin(115200, SERIAL_8N1, SENSOR_RX, SENSOR_TX);

  Serial.println(F("\n=== esp32_bridge (solo WiFi) ==="));
  Serial.println(F("Sensores en Arduino UNO #2 (sensor_board)"));
  Serial.printf("  Motores UART1 RX=%d TX=%d\n", MOTOR_RX, MOTOR_TX);
  Serial.printf("  Sensores UART2 RX=%d TX=%d\n", SENSOR_RX, SENSOR_TX);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] IP ESP32: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] PC ws://%s:%d\n", WS_HOST, WS_PORT);

  connectWebSocket();
  Serial.println(F("[BRIDGE] Listo"));
}

void loop() {
  webSocket.loop();
  readSensorUart();
  delay(5);
}
