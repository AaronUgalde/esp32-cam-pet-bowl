/*
 * hub_simulated.ino — robot_hub con UNO, MP3 y patrulla simulados
 *
 * Mismos pines y protocolo JSON que robot_hub/ pero:
 *   - UNO: imprime JSON en Serial USB en lugar de Serial1 real
 *   - MP3: solo log, no requiere DFPlayer conectado
 *   - Patrulla: siempre "active" hasta un bark
 *
 * DEBUG_SERIAL_ONLY = 1  →  sin WiFi, eventos JSON por USB
 * DEBUG_SERIAL_ONLY = 0  →  WebSocket hacia debug/mock_controller.py
 *
 * Comandos USB serial:
 *   b = simular ladrido    r = reset a monitoreo
 *   p = ping WebSocket     s = estado
 */

#define DEBUG_SERIAL_ONLY 0

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

#if !DEBUG_SERIAL_ONLY
#include "../../../secrets.h"
#endif

// ── Pines (identicos a robot_hub/) ──────────────────────────────
const int MIC_DO   = 34;
const int MIC_AO   = 35;
const int MQ135_AO = 32;

const int UMBRAL_PICO  = 200;
const int MIC_DO_MIN_AO  = 30;   // DO solo cuenta si hay señal analógica mínima
const int COOLDOWN     = 2000;
const long HEARTBEAT_MS = 5000;

enum HubMode { MONITORING, BRIDGE_MODE };
HubMode hubMode = MONITORING;

#if !DEBUG_SERIAL_ONLY
WebSocketsClient webSocket;
bool wsConnected = false;
bool wsEverConnected = false;
#endif

unsigned long ultimoPico      = 0;
unsigned long ultimoHeartbeat = 0;
int ultimoAire = 0;

// ── Simulaciones ────────────────────────────────────────────────
void simUnoSend(const char *json) {
  Serial.print(F("[SIM UNO] UART TX: "));
  Serial.println(json);
}

void simUnoStop() {
  simUnoSend("{\"cmd\":\"stop\"}");
  Serial.println(F("[SIM UNO] Motores detenidos (simulado)"));
}

void simMp3Play(int track) {
  Serial.printf("[SIM MP3] Reproducir track %d (000%d.mp3)\n", track, track);
  Serial.println(F("[SIM MP3] Bocina: simulado OK"));
}

void emitJson(const char *json) {
  Serial.print(F("[EVENT] "));
  Serial.println(json);
}

void buildAndEmit(const char *event, int air, const char *extraKey = nullptr, const char *extraVal = nullptr) {
  StaticJsonDocument<160> doc;
  doc["event"] = event;
  if (air >= 0) doc["air"] = air;
  doc["ts"] = millis();
  if (extraKey && extraVal) doc[extraKey] = extraVal;

  String out;
  serializeJson(doc, out);
  emitJson(out.c_str());

#if !DEBUG_SERIAL_ONLY
  if (wsConnected) webSocket.sendTXT(out);
#endif
}

#if !DEBUG_SERIAL_ONLY
void webSocketEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      wsConnected = false;
      if (wsEverConnected) {
        Serial.println(F("[WS] Desconectado (se reintentara)"));
      } else {
        Serial.printf("[WS] No se pudo conectar a %s:%d — ¿mock_controller.py activo? ¿IP correcta?\n",
                      WS_HOST, WS_PORT);
      }
      break;
    case WStype_CONNECTED:
      wsConnected = true;
      wsEverConnected = true;
      Serial.println(F("[WS] Conectado al mock en PC"));
      buildAndEmit("hello", ultimoAire);
      break;
    case WStype_ERROR:
      Serial.println(F("[WS] Error de conexion"));
      break;
    case WStype_TEXT: {
      Serial.print(F("[WS] RX: "));
      Serial.write(payload, length);
      Serial.println();

      StaticJsonDocument<128> doc;
      if (deserializeJson(doc, payload, length)) break;

      const char *cmd = doc["cmd"];
      if (!cmd) break;

      if (strcmp(cmd, "stop") == 0) {
        simUnoStop();
        buildAndEmit("stopped", ultimoAire, "reason", "remote");
      } else if (strcmp(cmd, "play_audio") == 0) {
        int track = doc["track"] | 1;
        simMp3Play(track);
        buildAndEmit("audio_played", ultimoAire);
      } else if (strcmp(cmd, "ping") == 0) {
        buildAndEmit("pong", ultimoAire);
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
#endif

bool sensoresConectados(int micAo, int mq135) {
  // Sin cable, ADC en ESP32 suele quedar en ~0 en ambos canales
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
  simUnoStop();
  buildAndEmit("bark", air);
  buildAndEmit("stopped", air, "reason", "bark");
  Serial.printf(F("[BARK] Detectado — air=%d — sesion simulada detenida\n"), air);
}

void simularBarkManual() {
  int air = analogRead(MQ135_AO);
  Serial.println(F("[TEST] Ladrido manual (tecla b)"));
  onBarkDetected(air);
}

void resetMonitoreo() {
  hubMode = MONITORING;
  ultimoPico = 0;
  buildAndEmit("reset", ultimoAire);
  Serial.println(F("[TEST] Reset → MONITORING (patrulla simulada activa)"));
}

void imprimirEstado() {
  Serial.println(F("--- Estado ---"));
  Serial.printf("  Modo hub: %s\n", hubMode == MONITORING ? "MONITORING" : "BRIDGE_MODE");
  Serial.printf("  Ultimo aire MQ135: %d\n", ultimoAire);
#if !DEBUG_SERIAL_ONLY
  Serial.printf("  WebSocket: %s\n", wsConnected ? "conectado" : "desconectado");
#else
  Serial.println(F("  WebSocket: deshabilitado (SERIAL_ONLY)"));
#endif
  Serial.println(F("  Patrulla UNO: simulada (active/stopped via heartbeat)"));
}

void leerComandosSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    switch (c) {
      case 'b': case 'B': simularBarkManual(); break;
      case 'r': case 'R': resetMonitoreo(); break;
      case 'p': case 'P':
        Serial.println(F("[TEST] Simulando respuesta a ping de la PC"));
        buildAndEmit("pong", ultimoAire);
        break;
      case 's': case 'S': imprimirEstado(); break;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(10);
  pinMode(MIC_DO, INPUT);

  Serial.println(F("\n=== DEBUG: hub_simulated ==="));
  Serial.println(F("UNO + MP3 + patrulla = SIMULADOS"));
  Serial.printf("  Mic DO=%d  Mic AO=%d  MQ135=%d\n", MIC_DO, MIC_AO, MQ135_AO);
  Serial.println(F("Teclas: b=ladrido  r=reset  p=ping  s=estado"));

#if DEBUG_SERIAL_ONLY
  Serial.println(F("Modo: SERIAL_ONLY (sin WiFi)"));
#else
  Serial.println(F("Modo: WebSocket → mock_controller.py"));
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[WiFi] Conectando a %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.printf("\n[WiFi] IP ESP32: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("[WiFi] Mock en ws://%s:%d\n", WS_HOST, WS_PORT);
  connectWebSocket();
#endif

  ultimoHeartbeat = millis();
  Serial.println(F("[HUB] Listo — monitoreando sensores…"));
  Serial.println(F("[HUB] Sin sensores cableados: usa tecla 'b' para simular ladrido"));
}

void loop() {
#if !DEBUG_SERIAL_ONLY
  webSocket.loop();
#endif

  leerComandosSerial();

  int micAo   = analogRead(MIC_AO);
  int micDo   = digitalRead(MIC_DO);
  int mq135Ao = analogRead(MQ135_AO);
  ultimoAire  = mq135Ao;

  unsigned long ahora = millis();

  if (hubMode == MONITORING && leerPico(micAo, micDo, mq135Ao) &&
      (ahora - ultimoPico) >= (unsigned long)COOLDOWN) {
    ultimoPico = ahora;
    onBarkDetected(mq135Ao);
  }

  if ((ahora - ultimoHeartbeat) >= (unsigned long)HEARTBEAT_MS) {
    ultimoHeartbeat = ahora;
    StaticJsonDocument<128> doc;
    doc["event"]  = "heartbeat";
    doc["air"]    = mq135Ao;
    doc["patrol"] = (hubMode == MONITORING) ? "active" : "stopped";
    doc["mic_ao"] = micAo;
    doc["mic_do"] = micDo;
    doc["ts"]     = ahora;
    String out;
    serializeJson(doc, out);
    emitJson(out.c_str());
#if !DEBUG_SERIAL_ONLY
    if (wsConnected) webSocket.sendTXT(out);
#endif
  }

  delay(50);
}
