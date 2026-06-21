/*
 * test_mp3.ino — Probar DFPlayer Mini en ESP32 (misma librería que senores_example.ino)
 *
 * Arduino UNO example usa SoftwareSerial(8,9).
 * En ESP32 usamos HardwareSerial(2) — más estable.
 *
 * Cableado (MP3-TF-16P v3.0):
 *   VCC  → 5V
 *   GND  → GND
 *   MP3 RX ← GPIO 19 (ESP32 TX)  [opcional: resistencia 1kΩ en serie]
 *   MP3 TX → GPIO 18 (ESP32 RX)
 *   SPK_1/SPK_2 → bocina
 *
 * micro-SD dentro del MP3 con 0001.mp3
 *
 * Librería: DFRobotDFPlayerMini
 *
 * Comandos serial:
 *   1 = play track 1    v = volume 20    s = stop
 */

#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

const int MP3_RX = 18;
const int MP3_TX = 19;

HardwareSerial mp3Link(2);
DFRobotDFPlayerMini player;

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("=== DEBUG: test_mp3 (ESP32) ==="));
  Serial.println(F("Misma libreria que examples/senores_example.ino"));
  Serial.printf("  UART2 RX=%d TX=%d @ 9600 baud\n", MP3_RX, MP3_TX);

  mp3Link.begin(9600, SERIAL_8N1, MP3_RX, MP3_TX);
  delay(1500);

  if (!player.begin(mp3Link)) {
    Serial.println(F("ERROR: DFPlayer no detectado"));
    Serial.println(F("  - SD insertada en el modulo MP3?"));
    Serial.println(F("  - Existe 0001.mp3 en la SD?"));
    Serial.println(F("  - TX/RX cruzados?"));
    return;
  }

  Serial.println(F("DFPlayer OK. Teclas: 1=play  v=vol20  s=stop"));
  player.volume(20);
}

void loop() {
  if (player.available()) {
    uint8_t t = player.readType();
    int v = player.read();
    if (t == DFPlayerPlayFinished) {
      Serial.printf("Pista %d terminada.\n", v);
    } else if (t == DFPlayerError) {
      Serial.printf("Error MP3 codigo: %d\n", v);
    }
  }

  if (Serial.available()) {
    char c = Serial.read();
    if (c == '1') {
      player.play(1);
      Serial.println(F("Play 0001.mp3"));
    } else if (c == 'v' || c == 'V') {
      player.volume(20);
      Serial.println(F("Volumen 20"));
    } else if (c == 's' || c == 'S') {
      player.stop();
      Serial.println(F("Stop"));
    }
  }
}
