/*
 * sensor_usb.ino — Prueba UNO #2 solo por USB (sin ESP32, sin carrito)
 *
 * Mismos pines que examples/senores_example.ino y sensor_board.ino:
 *   Mic DO  → D2    Mic AO  → A0    MQ135 → A1
 *   MP3     → D8 RX, D9 TX (1 kΩ en TX hacia MP3 RX)
 *   LED     → D13
 *
 * Monitor Serial 115200 baud.
 *
 * Salida CSV cada 200 ms: mic_ao,mic_do,mq135,trigger
 *
 * Teclas:
 *   a = reproducir 0001.mp3
 *   b = simular evento ladrido (mensaje en serial)
 *   t = mostrar ayuda
 *   + / - = subir/bajar umbral mic AO
 *
 * Librería: DFRobotDFPlayerMini
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

const int MIC_DO   = 2;
const int MIC_AO   = A0;
const int MQ135_AO = A1;
const int LED_PIN  = 13;

SoftwareSerial mp3Serial(8, 9);
DFRobotDFPlayerMini mp3Player;

int umbralPico = 350;
const int MIC_DO_MIN_AO = 30;
const int INTERVALO_MS = 200;

bool mp3Ready = false;

void banner() {
  Serial.println();
  Serial.println(F("=== BENCH: sensor_usb (UNO #2) ==="));
  Serial.println(F("Pines: Mic D2/A0  MQ135 A1  MP3 D8/D9"));
  Serial.println(F("CSV: mic_ao,mic_do,mq135,trigger"));
  Serial.print(F("Umbral mic AO: "));
  Serial.println(umbralPico);
  Serial.println(F("Teclas: a=MP3  b=simular ladrido  t=ayuda  +/-=umbral"));
  Serial.println();
}

bool esPico(int micAo, int micDo) {
  if (micAo > umbralPico) return true;
  if (micAo > MIC_DO_MIN_AO && micDo == LOW) return true;
  return false;
}

void simularLadrido(int micAo, int micDo, int mq135) {
  digitalWrite(LED_PIN, HIGH);
  Serial.println(F("--- EVENTO SIMULADO (como JSON al ESP32) ---"));
  Serial.print(F("{\"event\":\"bark\",\"air\":"));
  Serial.print(mq135);
  Serial.print(F(",\"mic_ao\":"));
  Serial.print(micAo);
  Serial.print(F(",\"mic_do\":"));
  Serial.print(micDo);
  Serial.println('}');
  Serial.println(F("{\"event\":\"stopped\",\"reason\":\"bark\"}"));
  delay(500);
  digitalWrite(LED_PIN, LOW);
}

void setup() {
  Serial.begin(115200);
  pinMode(MIC_DO, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  mp3Serial.begin(9600);
  delay(1500);
  if (mp3Player.begin(mp3Serial)) {
    mp3Ready = true;
    Serial.println(F("[MP3] OK"));
  } else {
    Serial.println(F("[MP3] No detectado (sensores siguen funcionando)"));
  }

  banner();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') banner();
    else if (c == '+') { umbralPico += 10; Serial.print(F("Umbral=")); Serial.println(umbralPico); }
    else if (c == '-') { umbralPico = max(0, umbralPico - 10); Serial.print(F("Umbral=")); Serial.println(umbralPico); }
    else if (c == 'a' || c == 'A') {
      if (mp3Ready) { mp3Player.volume(20); delay(150); mp3Player.play(1); Serial.println(F("Play 0001.mp3")); }
      else Serial.println(F("MP3 no disponible"));
    }
    else if (c == 'b' || c == 'B') {
      simularLadrido(analogRead(MIC_AO), digitalRead(MIC_DO), analogRead(MQ135_AO));
    }
  }

  if (mp3Ready && mp3Player.available()) {
    if (mp3Player.readType() == DFPlayerPlayFinished) {
      Serial.println(F("[MP3] Pista terminada"));
      mp3Player.read();
    }
  }

  int micAo = analogRead(MIC_AO);
  int micDo = digitalRead(MIC_DO);
  int mq135 = analogRead(MQ135_AO);
  int trigger = esPico(micAo, micDo) ? 1 : 0;

  Serial.print(micAo);
  Serial.print(',');
  Serial.print(micDo);
  Serial.print(',');
  Serial.print(mq135);
  Serial.print(',');
  Serial.println(trigger);
  if (trigger) Serial.println(F("  ^^^ pico (ladrido)"));

  delay(INTERVALO_MS);
}
