/*
 * ================================================================
 *  MODO A → envía 'A' por serial → reproduce audio (0001.mp3)
 *  MODO B → envía 'B' por serial → graba sensores en dataset.csv
 * ================================================================
 *  Conexiones MP3-TF-16P v3.0:
 *    VCC   → 5V
 *    GND   → GND (ambos pines GND del módulo)
 *    TX    → Arduino D8
 *    RX    → Arduino D9 (a través de resistencia 1kΩ)
 *    SPK_1 → Bocina (+)
 *    SPK_2 → Bocina (-)
 *
 *  Módulo SD (SPI):
 *    CS    → D10 | MOSI → D11 | MISO → D12 | SCK → D13
 * ================================================================
 */

#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>
#include <SPI.h>
#include <SD.h>
#include <DS3231.h>

// Init the DS3231 using the hardware interface
DS3231  rtc(SDA, SCL);

// ── MP3 ───────────────────────────────────────────────────────────
SoftwareSerial mp3Serial(8, 9);   // RX = D8, TX = D9
DFRobotDFPlayerMini mp3Player;

// ── Sensores / SD ─────────────────────────────────────────────────
const int MIC_DO   = 2;
const int MIC_AO   = A0;
const int MQ135_AO = A1;
const int CS_PIN   = 10;
const int LED_PIN  = 13;

// ── Parámetros logging ────────────────────────────────────────────
const int  UMBRAL_PICO = 200;
const int  COOLDOWN    = 2000;  // ms entre registros del mismo pico
const int  LED_TIEMPO  = 1000;  // ms que permanece encendido el LED

// ── Estado global ─────────────────────────────────────────────────
char modo = ' ';                // 'A' = audio  |  'B' = logging
bool sdLista = false;
unsigned long ultimoRegistro = 0;
unsigned long ultimoLED      = 0;
unsigned long arranque       = 0;
File archivo;

// ─────────────────────────────────────────────────────────────────
String tiempoTranscurrido() {
  unsigned long seg = (millis() - arranque) / 1000;
  unsigned long min = seg / 60;  seg %= 60;
  unsigned long hr  = min / 60;  min %= 60;
  String t = "";
  if (hr  < 10) t += "0";  t += hr;  t += ":";
  if (min < 10) t += "0";  t += min; t += ":";
  if (seg < 10) t += "0";  t += seg;
  return t;
}

// ─────────────────────────────────────────────────────────────────
void activarModoA() {
  Serial.println(F("── MODO A: Reproduciendo audio ──"));
  mp3Player.volume(20);
  delay(150);  // el v3.0 necesita pausa entre comandos
  mp3Player.play(1);  // reproduce 0001.mp3
}

// ─────────────────────────────────────────────────────────────────
void activarModoB() {
  Serial.println(F("── MODO B: Iniciando grabación ──"));

  // Detener audio si estaba en modo A
  mp3Player.stop();
  delay(150);

  // Inicializar SD (solo la primera vez)
  if (!sdLista) {
    if (!SD.begin(CS_PIN)) {
      Serial.println(F("ERROR: No se pudo inicializar la SD."));
      modo = ' ';  // cancelar cambio de modo
      return;
    }
    sdLista = true;

    // Crear encabezado si el archivo no existe
    if (!SD.exists("dataset.csv")) {
      archivo = SD.open("dataset.csv", FILE_WRITE);
      if (archivo) {
        archivo.println("tiempo,mic_ao,mic_do,mq135_ao,evento");
        archivo.close();
      }
    }
  }

  arranque = millis();  // reiniciar cronómetro relativo
  ultimoRegistro = 0;
  ultimoLED      = 0;
  digitalWrite(LED_PIN, LOW);

  Serial.println(F("SD lista. Grabando datos..."));
  Serial.println(F("tiempo,mic_ao,mic_do,mq135_ao,evento"));
}

// ─────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);

  rtc.begin();

  pinMode(MIC_DO, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // El MP3-TF-16P v3.0 necesita ~1.5 s para arrancar
  delay(1500);

  if (!mp3Player.begin(mp3Serial)) {
    Serial.println(F("ADVERTENCIA: Módulo MP3 no detectado."));
    Serial.println(F("  → Verifica conexiones y que la SD del MP3 esté insertada."));
    Serial.println(F("  → El modo B seguirá funcionando con normalidad."));
  } else {
    Serial.println(F("Módulo MP3 listo."));
  }

  Serial.println(F("Envía 'A' para reproducir audio, 'B' para grabar sensores."));
}

// ─────────────────────────────────────────────────────────────────
void loop() {

  // ── Leer comando serial ──────────────────────────────────────
  if (Serial.available()) {
    char cmd = toupper((char)Serial.read());

    if (cmd == 'A' && modo != 'A') {
      modo = 'A';
      activarModoA();

    } else if (cmd == 'B' && modo != 'B') {
      modo = 'B';
      activarModoB();
    }
  }

  // ── MODO A: monitorear estado del reproductor ────────────────
  if (modo == 'A') {
    if (mp3Player.available()) {
      uint8_t tipo  = mp3Player.readType();
      int     valor = mp3Player.read();

      if (tipo == DFPlayerPlayFinished) {
        Serial.print(F("Pista "));
        Serial.print(valor);
        Serial.println(F(" terminada."));
      } else if (tipo == DFPlayerError) {
        Serial.print(F("Error MP3: "));
        switch (valor) {
          case Busy:         Serial.println(F("SD no detectada"));       break;
          case FileIndexOut: Serial.println(F("Índice fuera de rango")); break;
          case FileMismatch: Serial.println(F("Archivo no encontrado")); break;
          default:           Serial.println(valor);                      break;
        }
      }
    }
  }

  // ── MODO B: lectura y grabación de sensores ──────────────────
  if (modo == 'B') {
    unsigned long ahora = millis();

    int mic_ao   = analogRead(MIC_AO);
    int mic_do   = digitalRead(MIC_DO);
    int mq135_ao = analogRead(MQ135_AO);

    // Apagado automático del LED
    if (digitalRead(LED_PIN) == HIGH && (ahora - ultimoLED) >= (unsigned long)LED_TIEMPO) {
      digitalWrite(LED_PIN, LOW);
    }

    // Detectar pico de sonido
    bool pico = (mic_do == LOW || mic_ao > UMBRAL_PICO);

    if (pico && (ahora - ultimoRegistro) >= (unsigned long)COOLDOWN) {
      ultimoRegistro = ahora;
      ultimoLED      = ahora;
      digitalWrite(LED_PIN, HIGH);

      String dow   = String(rtc.getDOWStr());
      String fecha = String(rtc.getDateStr());
      String hora  = String(rtc.getTimeStr());

      String dia_fecha_hora = dow + " " + fecha + " " + hora;

      String linea = dia_fecha_hora + "," +
                     String(mic_ao) + "," +
                     String(mic_do) + "," +
                     String(mq135_ao) + "," +
                     "pico";

      // Guardar en SD
      archivo = SD.open("dataset.csv", FILE_WRITE);
      if (archivo) {
        archivo.println(linea);
        archivo.close();
      }

      // Imprimir en monitor serial
      Serial.println(linea);
    }

    delay(50);
  }
}
