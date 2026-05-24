/*
  ver_ip.ino - Solo muestra la IP del ESP32 en el Monitor Serial.
  Abre este sketch, flashea, abre Monitor Serial a 115200 y listo.
*/

#include <WiFi.h>
#include "../secrets.h"

void setup() {
  Serial.begin(115200);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nIP: " + WiFi.localIP().toString());
}

void loop() {
  delay(3000);
  Serial.println("IP: " + WiFi.localIP().toString());
}
