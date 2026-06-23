/*
 * wifi_check.ino — ESP32 dev: solo comprueba WiFi y secrets.h
 *
 * Flashea, abre Monitor Serial 115200.
 * Debe mostrar IP local y el WS_HOST configurado (IP de tu PC).
 * No requiere Arduino sensores ni mock en PC.
 */

#include <WiFi.h>
#include "../../../../secrets.h"

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println(F("\n=== BENCH: wifi_check ==="));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("SSID: %s\n", WIFI_SSID);
  Serial.print("Conectando");
  unsigned long t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 30000) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("OK — IP ESP32: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("PC esperada (WS_HOST): %s:%d\n", WS_HOST, WS_PORT);
    Serial.println(F("Siguiente paso: bench_bridge + sensor_board + mock_controller"));
  } else {
    Serial.println(F("FALLO WiFi — revisa secrets.h (SSID/PASS)"));
  }
}

void loop() {
  delay(10000);
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println(F("[WiFi] Perdida — reconectando…"));
    WiFi.reconnect();
  }
}
