/*
 * robot_hub.ino — ESP32 dev board: hub embarcado del robot patrullero
 *
 * Funciones:
 *   - Lectura mic (DO + AO) y MQ135
 *   - Detección local de ladrido → STOP inmediato a Arduino UNO #1
 *   - WebSocket cliente hacia PC (eventos + comandos)
 *   - DFPlayer Mini para audio de tranquilización
 *
 * Conexiones:
 *   Mic DO  → GPIO 34    Mic AO  → GPIO 35
 *   MQ135   → GPIO 32
 *   UNO #1  → Serial1: RX=16, TX=17  (115200 baud, JSON por línea)
 *   DFPlayer→ HardwareSerial(2): RX=18, TX=19  (9600 baud, misma librería que senores_example)
 *
 * Librerías Arduino IDE:
 *   WebSockets by Markus Sattler
 *   ArduinoJson by Benoit Blanchon
 *   DFRobotDFPlayerMini
 */

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include "../secrets.h"

// ── Pines ───────────────────────────────────────────────────────
const int MIC_DO   = 34;
const int MIC_AO   = 35;
const int MQ135_AO = 32;

const int UNO_RX = 16;
const int UNO_TX = 17;

const int MP3_RX = 18;
const int MP3_TX = 19;

// ── Umbrales (mismos que examples/senores_example.ino) ──────────
const int  UMBRAL_PICO = 200;
const int  MIC_DO_MIN_AO = 30;
const int  COOLDOWN    = 2000;
const long HEARTBEAT_MS = 5000;

// ── Objetos ─────────────────────────────────────────────────────
WebSocketsClient webSocket;
HardwareSerial   mp3Serial(2);
DFRobotDFPlayerMini mp3Player;

// ── Estado ──────────────────────────────────────────────────────
enum HubMode { MONITORING, BRIDGE_MODE };
HubMode hubMode = MONITORING;

bool     wsConnected   = false;
bool     mp3Ready      = false;
unsigned long ultimoPico      = 0;
unsigned long ultimoHeartbeat = 0;
int      ultimoAire    = 0;

// ── UART hacia Arduino UNO #1 ───────────────────────────────────
void sendToUno(const char *json) {
  Serial1.println(json);
}

void sendStopToUno() {
  sendToUno("{\"cmd\":\"stop\"}");
}

// ── WebSocket ───────────────────────────────────────────────────
void sendEvent(const char *event, int air = -1, const char *extraKey = nullptr, const char *extraVal = nullptr) {
  if (!wsConnected) return;

  StaticJsonDocument<128> doc;
  doc["event"] = event;
  if (air >= 0) doc["air"] = air;
  doc["ts"] = millis();
  if (extraKey && extraVal) doc[extraKey] = extraVal;

  String out;
  serializeJson(doc, out);
  webSocket.sendTXT(out);
}

void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      Serial.println("[WS] Desconectado");
      break;

    case WStype_CONNECTED:
      wsConnected = true;
      Serial.println("[WS] Conectado a PC");
      sendEvent("hello", ultimoAire);
      break;

    case WStype_TEXT: {
      StaticJsonDocument<128> doc;
      DeserializationError err = deserializeJson(doc, payload, length);
      if (err) break;

      const char *cmd = doc["cmd"];
      if (!cmd) break;

      if (strcmp(cmd, "stop") == 0) {
        sendStopToUno();
        sendEvent("stopped", ultimoAire, "reason", "remote");
      } else if (strcmp(cmd, "play_audio") == 0) {
        int track = doc["track"] | 1;
        if (mp3Ready) {
          mp3Player.volume(20);
          delay(150);
          mp3Player.play(track);
          sendEvent("audio_played", ultimoAire);
        } else {
          sendEvent("audio_error", ultimoAire);
        }
      } else if (strcmp(cmd, "ping") == 0) {
        sendEvent("pong", ultimoAire);
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
  webSocket.setReconnectInterval(3000);
}

// ── Detección de ladrido ────────────────────────────────────────
bool sensoresConectados(int micAo, int mq135) {
  return !(micAo <= 5 && mq135 <= 5);
}

bool leerPico(int micAo, int micDo, int mq135) {
  if (!sensoresConectados(micAo, mq135)) {
    return false;
  }
  if (micAo > UMBRAL_PICO) {
    return true;
  }
  if (micAo > MIC_DO_MIN_AO && micDo == LOW) {
    return true;
  }
  return false;
}

void onBarkDetected(int air) {
  hubMode = BRIDGE_MODE;

  sendStopToUno();
  sendEvent("bark", air);
  sendEvent("stopped", air, "reason", "bark");

  Serial.printf("[BARK] air=%d — robot detenido\n", air);
}

// ── Setup ───────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, UNO_RX, UNO_TX);

  // Misma escala ADC que Arduino UNO (senores_example usa 0–1023, umbral 200)
  analogReadResolution(10);

  pinMode(MIC_DO, INPUT);

  mp3Serial.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  delay(1500);
  if (mp3Player.begin(mp3Serial)) {
    mp3Ready = true;
    Serial.println("[MP3] Listo");
  } else {
    Serial.println("[MP3] No detectado — audio deshabilitado");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

  connectWebSocket();
  ultimoHeartbeat = millis();

  Serial.println("[HUB] Monitoreando sensores…");
}

// ── Loop ────────────────────────────────────────────────────────
void loop() {
  webSocket.loop();

  int micAo   = analogRead(MIC_AO);
  int micDo   = digitalRead(MIC_DO);
  int mq135Ao = analogRead(MQ135_AO);
  ultimoAire  = mq135Ao;

  unsigned long ahora = millis();

  if (hubMode == MONITORING) {
    if (leerPico(micAo, micDo, mq135Ao) && (ahora - ultimoPico) >= (unsigned long)COOLDOWN) {
      ultimoPico = ahora;
      onBarkDetected(mq135Ao);
    }
  }

  if (wsConnected && (ahora - ultimoHeartbeat) >= (unsigned long)HEARTBEAT_MS) {
    ultimoHeartbeat = ahora;
    StaticJsonDocument<96> doc;
    doc["event"]  = "heartbeat";
    doc["air"]    = mq135Ao;
    doc["patrol"] = (hubMode == MONITORING) ? "active" : "stopped";
    doc["ts"]     = ahora;
    String out;
    serializeJson(doc, out);
    webSocket.sendTXT(out);
  }

  delay(50);
}
