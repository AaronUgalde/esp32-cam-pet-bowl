/*
 * sensor_board.ino — Arduino UNO #2: sensores + audio (como examples/senores_example.ino)
 *
 * Mic, MQ135 y DFPlayer en los mismos pines que senores_example.ino.
 * Envía eventos JSON al ESP32 puente por UART (no usa WiFi en este Arduino).
 *
 * Pines sensores / MP3 (igual que senores_example.ino):
 *   Mic DO  → D2      Mic AO  → A0
 *   MQ135   → A1
 *   MP3 RX  → D8      MP3 TX  → D9 (1 kΩ hacia RX del MP3)
 *
 * UART hacia ESP32 bridge (SoftwareSerial):
 *   RX = D6  ← ESP32 GPIO 19 (TX)
 *   TX = D7  → ESP32 GPIO 18 (RX)
 *   GND común
 *
 * Librería: DFRobotDFPlayerMini
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

// ── Sensores (mismos pines que senores_example.ino) ─────────────
const int MIC_DO   = 2;
const int MIC_AO   = A0;
const int MQ135_AO = A1;
const int LED_PIN  = 13;

// ── MP3 (mismos pines que senores_example.ino) ───────────────────
SoftwareSerial mp3Serial(8, 9);
DFRobotDFPlayerMini mp3Player;

// ── UART → ESP32 bridge ──────────────────────────────────────────
SoftwareSerial linkEsp(6, 7);
// 57600: SoftwareSerial del UNO pierde bytes a 115200 con MP3 activo
const long LINK_BAUD = 57600;

const int  UMBRAL_PICO    = 370;
const int  MIC_DO_MIN_AO  = 30;
const int  COOLDOWN       = 2000;
const long HEARTBEAT_MS   = 5000;
const int  LED_TIEMPO     = 1000;

enum BoardMode { MONITORING, BRIDGE_MODE };
BoardMode mode = MONITORING;

bool mp3Ready = false;
bool mp3Playing = false;
unsigned long ultimoPico = 0;
unsigned long ultimoHeartbeat = 0;
unsigned long ultimoLED = 0;
String rxBuffer;

// ── UART helpers ────────────────────────────────────────────────
void sendJson(const char *json) {
  linkEsp.println(json);
  Serial.print(F("[TX] "));
  Serial.println(json);
}

void emitEvent(const char *event, int air, int micAo, int micDo) {
  String out = "{\"event\":\"" + String(event) + "\"";
  if (air >= 0) out += ",\"air\":" + String(air);
  if (micAo >= 0) out += ",\"mic_ao\":" + String(micAo);
  if (micDo >= 0) out += ",\"mic_do\":" + String(micDo);
  out += ",\"ts\":" + String(millis());
  if (strcmp(event, "heartbeat") == 0) {
    out += ",\"patrol\":\"" + String(mode == MONITORING ? "active" : "stopped") + "\"";
  }
  out += "}";
  sendJson(out.c_str());
}

void emitStopped(const char *reason) {
  String out = "{\"event\":\"stopped\",\"reason\":\"" + String(reason) + "\",\"ts\":" + String(millis()) + "}";
  sendJson(out.c_str());
}

// ── Detección de ladrido (misma lógica que robot_hub) ───────────
bool leerPico(int micAo, int micDo) {
  if (micAo > UMBRAL_PICO) return true;
  if (micAo > MIC_DO_MIN_AO && micDo == LOW) return true;
  return false;
}

void onBark(int micAo, int micDo, int mq135) {
  mode = BRIDGE_MODE;
  digitalWrite(LED_PIN, HIGH);
  ultimoLED = millis();

  emitEvent("bark", mq135, micAo, micDo);
  emitStopped("bark");
  Serial.print(F("[BARK] air="));
  Serial.print(mq135);
  Serial.print(F(" mic_ao="));
  Serial.println(micAo);
}

int parseTrackFromJson(const String &line) {
  int track = 1;
  int idx = line.indexOf("\"track\"");
  if (idx >= 0) {
    int colon = line.indexOf(':', idx);
    if (colon >= 0) track = line.substring(colon + 1).toInt();
  }
  return track < 1 ? 1 : track;
}

void playTrack(int track) {
  if (!mp3Ready) {
    Serial.println(F("[MP3] No disponible"));
    emitEvent("audio_error", -1, -1, -1);
    return;
  }
  linkEsp.listen();
  mp3Serial.listen();
  mp3Player.volume(20);
  delay(150);
  mp3Player.play(track);
  mp3Playing = true;
  Serial.print(F("[MP3] Play 000"));
  Serial.print(track);
  Serial.println(F(".mp3"));
  linkEsp.listen();
  emitEvent("audio_played", analogRead(MQ135_AO), -1, -1);
}

void processCommand(const String &line) {
  // Acepta {"cmd":"play_audio",...} con o sin espacios
  if (line.indexOf("play_audio") >= 0) {
    playTrack(parseTrackFromJson(line));
  } else if (line.indexOf("\"cmd\":\"ping\"") >= 0 || line.endsWith("ping\"}")) {
    emitEvent("pong", analogRead(MQ135_AO), analogRead(MIC_AO), digitalRead(MIC_DO));
  } else if (line.indexOf("\"cmd\":\"stop\"") >= 0) {
    Serial.println(F("[CMD] stop (ignorado en sensor board)"));
  } else {
    Serial.print(F("[CMD] desconocido: "));
    Serial.println(line);
  }
}

void readUartFromEsp() {
  linkEsp.listen();
  while (linkEsp.available()) {
    char c = linkEsp.read();
    if (c == '\n' || c == '\r') {
      if (rxBuffer.length() > 0) {
        Serial.print(F("[RX] "));
        Serial.println(rxBuffer);
        processCommand(rxBuffer);
        rxBuffer = "";
      }
    } else {
      rxBuffer += c;
      if (rxBuffer.length() > 96) rxBuffer = "";
    }
  }
}

void pollMp3() {
  if (!mp3Ready || !mp3Playing) return;
  mp3Serial.listen();
  if (mp3Player.available() && mp3Player.readType() == DFPlayerPlayFinished) {
    Serial.println(F("[MP3] Pista terminada"));
    mp3Player.read();
    mp3Playing = false;
  }
  linkEsp.listen();
}

// ── Setup / loop ────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(MIC_DO, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inicializar MP3 antes que linkEsp (mismo orden que sensor_usb.ino).
  mp3Serial.begin(9600);
  mp3Serial.listen();
  delay(1500);
  if (mp3Player.begin(mp3Serial)) {
    mp3Ready = true;
    Serial.println(F("[MP3] OK"));
  } else {
    Serial.println(F("[MP3] No detectado (sensores siguen funcionando)"));
  }

  linkEsp.begin(LINK_BAUD);
  linkEsp.listen();

  Serial.println(F("=== sensor_board (UNO #2) ==="));
  Serial.print(F("UART ESP32 @ "));
  Serial.println(LINK_BAUD);
  Serial.println(F("Pines como senores_example.ino + UART D6/D7 → ESP32"));
  Serial.println(F("Tecla USB 'a' = probar MP3 sin PC"));
  delay(500);
  emitEvent("hello", analogRead(MQ135_AO), analogRead(MIC_AO), digitalRead(MIC_DO));
  ultimoHeartbeat = millis();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 'a' || c == 'A') {
      playTrack(1);
    }
  }

  readUartFromEsp();
  pollMp3();

  int micAo   = analogRead(MIC_AO);
  int micDo   = digitalRead(MIC_DO);
  int mq135   = analogRead(MQ135_AO);
  unsigned long ahora = millis();

  if (digitalRead(LED_PIN) == HIGH && (ahora - ultimoLED) >= (unsigned long)LED_TIEMPO) {
    digitalWrite(LED_PIN, LOW);
  }

  if (mode == MONITORING && leerPico(micAo, micDo) &&
      (ahora - ultimoPico) >= (unsigned long)COOLDOWN) {
    ultimoPico = ahora;
    onBark(micAo, micDo, mq135);
  }

  if ((ahora - ultimoHeartbeat) >= (unsigned long)HEARTBEAT_MS) {
    ultimoHeartbeat = ahora;
    emitEvent("heartbeat", mq135, micAo, micDo);
  }

  readUartFromEsp();
  linkEsp.listen();
  delay(10);
}
