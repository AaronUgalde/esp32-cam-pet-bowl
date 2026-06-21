# Sensores: Arduino UNO vs ESP32 dev

Comparación entre [`examples/senores_example.ino`](../examples/senores_example.ino) (Arduino UNO) y el firmware del robot en ESP32 (`robot_hub/`, `debug/esp32/`).

---

## Resumen rápido

| Parte | ¿Igual en ESP32? | Notas |
|-------|------------------|-------|
| Mic DO + AO | **Sí** (sin librería) | `digitalRead` / `analogRead` nativos |
| MQ135 AO | **Sí** (sin librería) | Cuidado con voltaje máximo 3.3 V |
| Umbral `mic_ao > 200` | **Casi** | Usar `analogReadResolution(10)` en ESP32 |
| DFPlayer Mini | **Misma librería**, distinto UART | Preferir `HardwareSerial`, no `SoftwareSerial` |
| DS3231 RTC | No usado en robot | Librería `DS3231.h` del example es para UNO |
| SD + SPI | No usado en robot | La SD del example es aparte; el MP3 lleva su propia micro-SD |
| WiFi / WebSocket | Solo ESP32 | No existen en el example de Arduino |

**Los sensores (mic y MQ135) no necesitan librerías especiales** en ninguno de los dos. Solo el MP3 usa `DFRobotDFPlayerMini`, que **sí funciona en ESP32**.

---

## 1. Micrófono y MQ135 — sin librerías

En `senores_example.ino` la lectura es directa:

```cpp
int mic_ao   = analogRead(MIC_AO);
int mic_do   = digitalRead(MIC_DO);
int mq135_ao = analogRead(MQ135_AO);
bool pico = (mic_do == LOW || mic_ao > UMBRAL_PICO);
```

En ESP32 usamos **exactamente la misma lógica**. No hace falta portar `DS3231`, `SD` ni `SPI` para el robot.

### Pines: Arduino vs ESP32

| Señal | Arduino UNO (`senores_example`) | ESP32 dev (`robot_hub`) |
|-------|----------------------------------|-------------------------|
| Mic DO | D2 | GPIO 34 |
| Mic AO | A0 | GPIO 35 |
| MQ135 | A1 | GPIO 32 |

Los números de pin **cambian** porque el cableado pasa del UNO al ESP32; la lógica no.

### ADC: el detalle importante del umbral 200

| Placa | Resolución por defecto | Rango |
|-------|------------------------|-------|
| Arduino UNO | 10 bits | 0–1023 |
| ESP32 | 12 bits | 0–4095 |

El umbral `UMBRAL_PICO = 200` se calibró pensando en UNO (10 bits). En ESP32, con 12 bits, el **mismo voltaje** da un número ~4× mayor.

**Solución aplicada en nuestro firmware ESP32:**

```cpp
void setup() {
  analogReadResolution(10);  // Misma escala 0–1023 que Arduino UNO
  ...
}
```

Así puedes reutilizar el 200 (y ajustarlo con `debug/esp32/test_sensors` igual que en el example).

### Voltaje en entradas analógicas

- **ESP32: máximo 3.3 V en ADC** (más voltaje daña el pin).
- **Arduino UNO: hasta 5 V** en A0/A1.

Si tus módulos de mic o MQ135 están alimentados a **5 V** y su salida analógica llega a 5 V, en ESP32 necesitas:

- Divisor de tensión (ej. 10 kΩ + 20 kΩ), o
- Alimentar el módulo a 3.3 V si el fabricante lo permite.

En UNO no era problema; en ESP32 **sí conviene verificar** con multímetro en AO en silencio y con ruido.

### WiFi y ADC en ESP32

GPIO 32, 34 y 35 pertenecen a **ADC1** y funcionan bien **con WiFi activo**.  
Evita usar pines ADC2 (0, 2, 4, 12–15, 25–27) para sensores si el hub usa WiFi.

---

## 2. DFPlayer Mini — misma librería, distinto enlace serial

### Arduino (`senores_example.ino`)

```cpp
#include <SoftwareSerial.h>
#include <DFRobotDFPlayerMini.h>

SoftwareSerial mp3Serial(8, 9);  // RX=D8, TX=D9
DFRobotDFPlayerMini mp3Player;

mp3Serial.begin(9600);
mp3Player.begin(mp3Serial);
```

- Librería: **DFRobotDFPlayerMini** (instalar desde Library Manager).
- `SoftwareSerial` en UNO es la opción habitual.
- Cableado: MP3 TX → D8, MP3 RX → D9 (con resistencia 1 kΩ en TX del Arduino hacia RX del MP3).

### ESP32 (`robot_hub.ino`)

- **Misma librería** `DFRobotDFPlayerMini`.
- En ESP32 es **más fiable** usar `HardwareSerial` (UART2) que `SoftwareSerial`.
- Cableado cruzado: ESP32 TX (19) → MP3 RX, ESP32 RX (18) ← MP3 TX; 1 kΩ entre TX del ESP32 y RX del MP3 (recomendación DFRobot).
- El MP3 sigue alimentándose a **5 V**; la lógica UART del ESP32 es 3.3 V y el DFPlayer la acepta.

| | Arduino example | ESP32 robot_hub |
|--|-----------------|-----------------|
| UART | SoftwareSerial 8/9 | HardwareSerial(2) pines 18/19 |
| Baud | 9600 | 9600 |
| Librería | DFRobotDFPlayerMini | Igual |
| SD del MP3 | micro-SD dentro del módulo MP3 | Igual (0001.mp3) |

**Conflicto de pines en nuestro diseño:** Serial1 (16/17) va al Arduino UNO de motores; el MP3 usa Serial2 (18/19). No comparten UART.

---

## 3. Lo que NO se porta al ESP32 (y no hace falta)

| Componente en `senores_example` | Librería | ¿En robot ESP32? |
|---------------------------------|----------|------------------|
| DS3231 RTC | `DS3231.h` | No — usamos `millis()` / timestamps de la PC |
| SD card (logging) | `SD.h`, `SPI.h` | No — logs por Serial / WebSocket |
| LED status D13 | — | Opcional; no incluido en hub |

Si quisieras RTC o SD en ESP32, existen portaciones (`Wire` + RTClib, SD_MMC/SPI), pero **no son necesarias** para el patrullero.

---

## 4. Orden de prueba recomendado (solo ESP32 dev)

1. **`debug/esp32/test_sensors`** — mic + MQ135, sin librerías extra.
2. **`debug/esp32/hub_simulated`** — flujo completo; MP3 simulado por log.
3. **`debug/esp32/test_mp3`** (opcional) — solo DFPlayer + misma librería que el example.
4. **`robot_hub/robot_hub.ino`** — firmware final con WiFi + MP3 real.

---

## 5. Checklist de cableado desde el example Arduino

Si ya probaste sensores en UNO con `senores_example.ino`:

- [ ] Mic DO → GPIO 34 (no D2)
- [ ] Mic AO → GPIO 35 (no A0)
- [ ] MQ135 → GPIO 32 (no A1)
- [ ] AO de sensores ≤ 3.3 V en el pin ESP32
- [ ] Flashear firmware ESP32 con `analogReadResolution(10)` (ya en nuestros sketches)
- [ ] MP3: pines 18/19, no 8/9 del UNO
- [ ] MP3 al 5 V, GND común con ESP32

---

## Referencias

- [DFRobot DFPlayer Mini wiki](https://wiki.dfrobot.com/dfr0299/) — compatible Arduino y ESP32
- Example original: [`examples/senores_example.ino`](../examples/senores_example.ino)
