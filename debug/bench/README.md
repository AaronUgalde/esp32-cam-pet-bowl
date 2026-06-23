# Pruebas en banco (sin carrito)

Tienes **Arduino UNO #2 + sensores**, **ESP32 dev** y **ESP32-CAM**.  
No hace falta el UNO de motores ni el chasis.

Usa el mismo [`config.py`](../../config.py) y [`secrets.h`](../../secrets.h) de la raíz.

---

## Orden recomendado

| Paso | Qué pruebas | Firmware / script |
|------|-------------|-------------------|
| **1** | Sensores + MP3 solos (USB) | `arduino/sensor_usb/sensor_usb.ino` |
| **2** | WiFi ESP32 dev | `esp32/wifi_check/wifi_check.ino` |
| **3** | ESP32-CAM → PC | `examples/send_images_wifi` + `test_cam.py` |
| **4** | UNO sensores ↔ ESP32 ↔ PC | `sensor_board` + `bench_bridge` + `mock_controller.py` |

---

## Paso 1 — Arduino UNO #2 solo (USB)

1. Flashea [`arduino/sensor_usb/sensor_usb.ino`](arduino/sensor_usb/sensor_usb.ino)
2. Monitor Serial **115200**
3. Verifica CSV: `mic_ao,mic_do,mq135,trigger`
4. Haz ruido → `trigger=1`
5. Tecla **`a`** → reproduce `0001.mp3` (SD en el DFPlayer)
6. Tecla **`b`** → simula JSON de ladrido en consola

**Pines:** iguales a [`examples/senores_example.ino`](../../examples/senores_example.ino) (D2, A0, A1, D8, D9).

---

## Paso 2 — ESP32 dev: WiFi

1. Asegura [`secrets.h`](../../secrets.h) con tu WiFi y `WS_HOST` = IP de la PC (`ipconfig`)
2. Flashea [`esp32/wifi_check/wifi_check.ino`](esp32/wifi_check/wifi_check.ino)
3. Debe imprimir **IP ESP32** y **WS_HOST** sin errores

---

## Paso 3 — ESP32-CAM

1. Copia WiFi: `secrets.h` ya sirve (misma red)
2. Flashea [`examples/send_images_wifi/send_images_wifi.ino`](../../examples/send_images_wifi/send_images_wifi.ino)  
   (incluye `../../secrets.h`)
3. Opcional: [`examples/ver_ip/ver_ip.ino`](../../examples/ver_ip/ver_ip.ino) para ver la IP
4. En `config.py`: `URL_CAM = "http://<IP_CAM>/cam.jpg"`
5. En la PC:

```bash
python debug/bench/test_cam.py
python debug/bench/test_cam.py --save prueba.jpg
```

---

## Paso 4 — Integración UNO #2 + ESP32 dev + PC (sin carrito)

Cableado UART (GND común):

| ESP32 dev | Arduino UNO #2 |
|-----------|----------------|
| GPIO **19** (TX) | Pin **6** (RX) |
| GPIO **18** (RX) | Pin **7** (TX) |
| GND | GND |

**PC primero:**

```bash
python debug/mock_controller.py --bowl random
```

**Flashear:**

1. UNO #2 → [`sensor_board/sensor_board.ino`](../../sensor_board/sensor_board.ino) (producción)  
   *o el mismo cableado con `sensor_board` — no uses `sensor_usb` en este paso*
2. ESP32 dev → [`esp32/bench_bridge/bench_bridge.ino`](esp32/bench_bridge/bench_bridge.ino)

**Esperado:**

- ESP32: `[WS] PC conectada`, `[SENSOR] {"event":"heartbeat",...}`
- PC: heartbeats; al ladrido (o ruido fuerte) → flujo mock completo + `[BENCH] STOP simulado`
- Si plato simulado lleno + aire OK → PC manda `play_audio` → UNO reproduce MP3

Monitor USB del UNO #2 (115200): líneas `[TX]` / `[RX]`.

---

## Qué NO necesitas ahora

| Componente | Sketch |
|------------|--------|
| Carrito / motores | `patrol_motors` — omitir |
| Sensores en GPIO ESP32 | `robot_hub` — omitir |
| Modelo CNN / Telegram | opcional; `mock_controller` basta |

Cuando tengas el carrito: cambia `bench_bridge` por [`esp32_bridge/esp32_bridge.ino`](../../esp32_bridge/esp32_bridge.ino) y añade `patrol_motors` en el UNO #1.

---

## Resumen de sketches bench

```
debug/bench/
├── README.md                 ← este archivo
├── test_cam.py               ← prueba ESP32-CAM desde PC
├── arduino/sensor_usb/       ← UNO #2 solo USB
└── esp32/
    ├── wifi_check/           ← ESP32 WiFi
    └── bench_bridge/         ← ESP32 + UNO sensores + PC
```

Producción (con carrito): [`SPLIT_ARCHITECTURE.md`](../../SPLIT_ARCHITECTURE.md).
