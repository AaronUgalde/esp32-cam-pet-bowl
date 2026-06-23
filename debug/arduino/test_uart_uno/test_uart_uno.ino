/*
 * test_uart_uno.ino — Prueba UART ESP32 ↔ Arduino UNO #1
 *
 * Verifica que el cableado serial funciona antes de patrol_motors.ino.
 *
 * Cableado (NO uses pines 12/13 — esos son HC-SR04):
 *   UNO pin 8 (RX) ← ESP32 GPIO 17 (TX)
 *   UNO pin 9 (TX) → ESP32 GPIO 16 (RX)
 *   GND común
 *
 * Flashea también debug/esp32/test_uart_esp32/test_uart_esp32.ino en el ESP32.
 * Monitor Serial USB del UNO: 115200 baud.
 */

#include <SoftwareSerial.h>

SoftwareSerial linkEsp(8, 9);

String rxBuffer;
unsigned long lastPingMs = 0;
const unsigned long PING_INTERVAL = 2000;

void setup() {
  Serial.begin(115200);
  linkEsp.begin(115200);

  Serial.println(F("\n=== DEBUG: test_uart_uno (UNO #1) ==="));
  Serial.println(F("UART: D8=RX, D9=TX → ESP32 GPIO 17/16"));
  Serial.println(F("Enviara ping cada 2 s; responde pong al ping del ESP32"));
}

void loop() {
  while (linkEsp.available()) {
    char c = linkEsp.read();
    if (c == '\n' || c == '\r') {
      if (rxBuffer.length() > 0) {
        Serial.print(F("[RX ESP32] "));
        Serial.println(rxBuffer);

        if (rxBuffer.indexOf("ping") >= 0) {
          linkEsp.println("{\"from\":\"uno\",\"event\":\"pong\"}");
          Serial.println(F("[TX ESP32] pong enviado"));
        }
        if (rxBuffer.indexOf("\"stop\"") >= 0) {
          linkEsp.println("{\"state\":\"stopped\"}");
          Serial.println(F("[TX ESP32] stopped confirmado"));
        }
        rxBuffer = "";
      }
    } else {
      rxBuffer += c;
      if (rxBuffer.length() > 80) rxBuffer = "";
    }
  }

  if (millis() - lastPingMs >= PING_INTERVAL) {
    lastPingMs = millis();
    linkEsp.println("{\"from\":\"uno\",\"event\":\"ping\"}");
    Serial.println(F("[TX ESP32] ping enviado"));
  }
}
