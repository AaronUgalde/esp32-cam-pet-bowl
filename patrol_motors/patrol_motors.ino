/*
 * patrol_motors.ino — Arduino UNO #1: patrulla autónoma + parada por UART
 *
 * Conexiones motores (H-bridge shield, sin cambios):
 *   ENA=5, IN1=2, IN2=3, ENB=6, IN3=4, IN4=7
 *   HC-SR04: TRIG=12, ECHO=13
 *
 * UART hacia ESP32 dev board (SoftwareSerial):
 *   RX=8  ← ESP32 TX (GPIO 17)
 *   TX=9  → ESP32 RX (GPIO 16)
 *   GND común
 *
 * Comandos UART (JSON por línea):
 *   {"cmd":"stop"}   → detiene motores permanentemente
 *   {"cmd":"status"} → responde {"state":"patrolling|stopped"}
 */

#include <SoftwareSerial.h>

#define ENA 5
#define IN1 2
#define IN2 3
#define ENB 6
#define IN3 4
#define IN4 7

#define TRIG_FRONT 12
#define ECHO_FRONT 13

SoftwareSerial linkEsp(8, 9);

const int PATROL_PWM_FWD  = 120;
const int PATROL_PWM_TURN = 150;
const unsigned long FWD_MS  = 3000;
const unsigned long TURN_MS = 800;
const float DIST_MIN_CM = 15.0;

enum RobotState { PATROLLING, STOPPED };
RobotState state = PATROLLING;

String rxBuffer;

void detener() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void avanzar(int velocidad) {
  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarIzq(int velocidad) {
  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void girarDer(int velocidad) {
  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

float medirDistancia(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);
  if (duracion == 0) return 250.0;
  return duracion * 0.0343 / 2.0;
}

void processCommand(const String &line) {
  if (line.indexOf("\"stop\"") >= 0) {
    detener();
    state = STOPPED;
    linkEsp.println("{\"state\":\"stopped\"}");
    Serial.println(F("[CMD] STOP — sesión terminada"));
  } else if (line.indexOf("\"status\"") >= 0) {
    if (state == PATROLLING)
      linkEsp.println("{\"state\":\"patrolling\"}");
    else
      linkEsp.println("{\"state\":\"stopped\"}");
  }
}

void readUart() {
  while (linkEsp.available()) {
    char c = linkEsp.read();
    if (c == '\n' || c == '\r') {
      if (rxBuffer.length() > 0) {
        processCommand(rxBuffer);
        rxBuffer = "";
      }
    } else {
      rxBuffer += c;
      if (rxBuffer.length() > 64) rxBuffer = "";
    }
  }
}

bool checkStopDuringMove() {
  readUart();
  if (state == STOPPED) {
    detener();
    return true;
  }
  float d = medirDistancia(TRIG_FRONT, ECHO_FRONT);
  if (d < DIST_MIN_CM) {
    detener();
    delay(200);
    return false;
  }
  return false;
}

void moverDuracion(void (*mover)(int), int pwm, unsigned long ms) {
  unsigned long inicio = millis();
  mover(pwm);
  while (millis() - inicio < ms) {
    if (checkStopDuringMove()) return;
    delay(20);
  }
  detener();
}

void ejecutarPatrulla() {
  moverDuracion(avanzar,   PATROL_PWM_FWD,  FWD_MS);
  if (state == STOPPED) return;

  moverDuracion(girarIzq,  PATROL_PWM_TURN, TURN_MS);
  if (state == STOPPED) return;

  moverDuracion(avanzar,   PATROL_PWM_FWD,  FWD_MS);
  if (state == STOPPED) return;

  moverDuracion(girarDer,  PATROL_PWM_TURN, TURN_MS);
}

void setup() {
  Serial.begin(115200);
  linkEsp.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);

  detener();
  Serial.println(F("[PATROL] Iniciando patrulla autónoma"));
}

void loop() {
  readUart();

  if (state == STOPPED) {
    detener();
    return;
  }

  ejecutarPatrulla();
}
