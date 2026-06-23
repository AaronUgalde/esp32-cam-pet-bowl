/*
 * test_uart_esp32.ino — Prueba UART ESP32 ↔ Arduino UNO #1
 *
 * Verifica que el cableado serial funciona antes de robot_hub.ino.
 *
 * Cableado (NO uses pines 12/13 del UNO — esos son HC-SR04):
 *   GPIO 17 (TX) → UNO pin 8 (RX)
 *   GPIO 16 (RX) ← UNO pin 9 (TX)
 *   GND común
 *
 * Flashea también debug/arduino/test_uart_uno/test_uart_uno.ino en el UNO.
 * Monitor Serial USB del ESP32: 115200 baud.
 * Placa Arduino IDE: ESP32 Dev Module (no Arduino UNO).
 *
 * Comandos por serial USB:
 *   p = ping manual
 *   s = enviar {"cmd":"stop"} (como robot_hub al detectar ladrido)
 */

#if !defined(ESP32)
#error "Selecciona placa ESP32 Dev Module en Arduino IDE (Herramientas > Placa)"
#endif

#include <HardwareSerial.h>

const int UNO_RX = 16; //ROJO
const int UNO_TX = 17;

HardwareSerial unoSerial(1);

String rxBuffer;
unsigned long lastPingMs = 0;
const unsigned long PING_INTERVAL = 3000;

void setup() {
  Serial.begin(115200);
  unoSerial.begin(115200, SERIAL_8N1, UNO_RX, UNO_TX);

  Serial.println(F("\n=== DEBUG: test_uart_esp32 ==="));
  Serial.println(F("UART: GPIO 16=RX, GPIO 17=TX → UNO D8/D9"));
  Serial.println(F("Teclas: p=ping  s=stop"));
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 's' || c == 'S') {
      unoSerial.println("{\"cmd\":\"stop\"}");
      Serial.println(F("[TX UNO] stop enviado"));
    } else if (c == 'p' || c == 'P') {
      unoSerial.println("{\"from\":\"esp32\",\"event\":\"ping\"}");
      Serial.println(F("[TX UNO] ping manual"));
    }
  }

  while (unoSerial.available()) {
    char c = unoSerial.read();
    if (c == '\n' || c == '\r') {
      if (rxBuffer.length() > 0) {
        Serial.print(F("[RX UNO] "));
        Serial.println(rxBuffer);
        rxBuffer = "";
      }
    } else {
      rxBuffer += c;
      if (rxBuffer.length() > 80) rxBuffer = "";
    }
  }

  if (millis() - lastPingMs >= PING_INTERVAL) {
    lastPingMs = millis();
    unoSerial.println("{\"from\":\"esp32\",\"event\":\"ping\"}");
    Serial.println(F("[TX UNO] ping automatico"));
  }
}
