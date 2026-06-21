/*
=====================================
LAFVIN
CONTROLADOR PROPORCIONAL (P)
CONTROL DE VELOCIDAD POR DISTANCIA
=====================================
*/

// ============================
// MOTORES
// ============================

#define ENA 5
#define IN1 2
#define IN2 3

#define ENB 6
#define IN3 4
#define IN4 7

// ============================
// ULTRASONICO FRONTAL
// ============================

#define TRIG_FRONT 12
#define ECHO_FRONT 13

// ============================
// CONTROLADOR P
// ============================

const float DISTANCIA_REFERENCIA = 20.0; // cm
const float KP = 6.0;

const int PWM_MAX = 255;

// ============================
// SETUP
// ============================

void setup() {

  Serial.begin(9600);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);

  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(TRIG_FRONT, OUTPUT);
  pinMode(ECHO_FRONT, INPUT);

  detener();

  Serial.println("================================");
  Serial.println(" CONTROLADOR PROPORCIONAL (P)");
  Serial.println("================================");
}

// ============================
// LOOP
// ============================

void loop() {

  float distancia = medirDistancia(TRIG_FRONT, ECHO_FRONT);

  // Error del controlador
  float error = distancia - DISTANCIA_REFERENCIA;

  // Ley de control
  float control = KP * error;

  // Saturación
  int pwm = constrain((int)control, 0, PWM_MAX);

  if (distancia > DISTANCIA_REFERENCIA) {
    avanzar(pwm);
  }
  else {
    detener();
    pwm = 0;
  }

  Serial.print("Distancia: ");
  Serial.print(distancia);

  Serial.print(" cm | Error: ");
  Serial.print(error);

  Serial.print(" | PWM: ");
  Serial.println(pwm);

  delay(50);
}

// ============================
// SENSOR ULTRASONICO
// ============================

float medirDistancia(int trigPin, int echoPin) {

  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);

  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);

  digitalWrite(trigPin, LOW);

  long duracion = pulseIn(echoPin, HIGH, 30000);

  if (duracion == 0)
    return 250;

  return duracion * 0.0343 / 2.0;
}

// ============================
// AVANZAR CON PWM VARIABLE
// ============================

void avanzar(int velocidad) {

  analogWrite(ENA, velocidad);
  analogWrite(ENB, velocidad);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

// ============================
// DETENER
// ============================

void detener() {

  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}