/*
 * test_sensors.ino — Lectura cruda de mic + MQ135 (ESP32 dev)
 *
 * Sin WiFi. Solo verifica cableado y umbrales en Monitor Serial (115200).
 *
 * Pines (mismos que robot_hub/):
 *   Mic DO  → GPIO 34
 *   Mic AO  → GPIO 35
 *   MQ135   → GPIO 32
 *
 * Comandos serial:
 *   t = mostrar umbrales
 *   + = subir UMBRAL_PICO (+10)
 *   - = bajar UMBRAL_PICO (-10)
 */

const int PIN_MIC_DO   = 34;
const int PIN_MIC_AO   = 35;
const int PIN_MQ135    = 32;

int umbralPico = 200;
const int INTERVALO_MS = 200;

void imprimirBanner() {
  Serial.println();
  Serial.println(F("=== DEBUG: test_sensors ==="));
  Serial.println(F("Pines:"));
  Serial.printf("  Mic DO  -> GPIO %d\n", PIN_MIC_DO);
  Serial.printf("  Mic AO  -> GPIO %d (ADC)\n", PIN_MIC_AO);
  Serial.printf("  MQ135   -> GPIO %d (ADC)\n", PIN_MQ135);
  Serial.printf("  Umbral mic AO: %d  (escala 10-bit, igual que senores_example.ino)\n", umbralPico);
  Serial.println(F("Salida CSV: mic_ao,mic_do,mq135,trigger"));
  Serial.println(F("Comandos: t=umbrales  +=subir  -=bajar umbral"));
  Serial.println();
}

bool esPico(int micAo, int micDo, int mq135) {
  if (micAo <= 5 && mq135 <= 5) return false;
  if (micAo > umbralPico) return true;
  if (micAo > 30 && micDo == LOW) return true;
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  analogReadResolution(10);  // Igual escala que senores_example.ino en Arduino UNO
  pinMode(PIN_MIC_DO, INPUT);
  imprimirBanner();
}

void loop() {
  if (Serial.available()) {
    char c = Serial.read();
    if (c == 't' || c == 'T') {
      imprimirBanner();
    } else if (c == '+') {
      umbralPico += 10;
      Serial.printf("[CFG] Umbral mic AO = %d\n", umbralPico);
    } else if (c == '-') {
      umbralPico = max(0, umbralPico - 10);
      Serial.printf("[CFG] Umbral mic AO = %d\n", umbralPico);
    }
  }

  int micAo   = analogRead(PIN_MIC_AO);
  int micDo   = digitalRead(PIN_MIC_DO);
  int mq135   = analogRead(PIN_MQ135);
  int trigger = esPico(micAo, micDo, mq135) ? 1 : 0;

  Serial.printf("%d,%d,%d,%d\n", micAo, micDo, mq135, trigger);

  if (trigger) {
    Serial.println(F("  ^^^ PICO detectado (seria ladrido en hub)"));
  }

  delay(INTERVALO_MS);
}
