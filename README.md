# Robot patrullero para monitoreo de perro

Robot móvil que patrulla de forma autónoma, detecta ladridos, inspecciona el plato de comida con visión por computadora y envía notificaciones por Telegram.

El código de referencia para la cámara y el modelo CNN está en [`examples/`](examples/) (ESP32-CAM, entrenamiento, monitor standalone).

Para probar sensores y el flujo con solo la ESP32 dev: [`debug/`](debug/).

**Cableado completo** (sensores, baterías, resistencias): [`WIRING.md`](WIRING.md).

---

## Arquitectura

```
[ PC: robot_controller.py ] ◄──WebSocket──► [ ESP32 dev: robot_hub ]
        │ HTTP /cam.jpg                              │ UART
        ▼                                            ▼
 [ ESP32-CAM ]                              [ Arduino UNO #1: patrol_motors ]
                                                    │
                                              [ Motores + HC-SR04 ]
```

| Dispositivo | Firmware / script | Rol |
|---|---|---|
| PC | `robot_controller.py` | Estados, ML, Telegram, WebSocket server |
| ESP32 dev | `robot_hub/robot_hub.ino` | Mic, MQ135, audio, puente WiFi |
| Arduino UNO #1 | `patrol_motors/patrol_motors.ino` | Patrulla + parada por UART |
| ESP32-CAM | `examples/send_images_wifi/` | Foto del plato (sin cambios) |

---

## Estructura del proyecto

```
esp32-cam-pet-bowl/
├── robot_controller.py
├── config.example.py         # Plantilla → copiar a config.py (único config Python)
├── secrets.h.example         # Plantilla → copiar a secrets.h (único secrets Arduino)
├── requirements.txt
├── robot_hub/robot_hub.ino
├── patrol_motors/patrol_motors.ino
└── examples/                 # Scripts y firmware ESP32-CAM
```

---

## Configuración (un solo archivo de cada uno)

Todos los scripts Python y todos los firmwares ESP32 comparten **los mismos archivos en la raíz**:

```bash
cp config.example.py config.py
cp secrets.h.example secrets.h
```

| Archivo | Qué contiene | Quién lo usa |
|---------|--------------|--------------|
| **`config.py`** | Telegram, URL cámara, WebSocket, MQ135, serial, modelo | `robot_controller.py`, `mock_controller.py`, `examples/*` |
| **`secrets.h`** | WiFi (`WIFI_SSID`, `WIFI_PASS`) + IP PC (`WS_HOST`, `WS_PORT`) | `robot_hub`, `hub_simulated`, `send_images_wifi`, `ver_ip` |

**`config.py`:** `URL_CAM`, `TG_TOKEN`, `TG_CHAT_ID`, `WS_BIND_HOST`, `AIR_THRESHOLD`, `SERIAL_PORT`, etc.

**`secrets.h`:** misma red WiFi para **todos** los ESP32; `WS_HOST` = IP de tu PC (`ipconfig`), sin espacios.

Los scripts en `examples/` importan el `config.py` de la raíz automáticamente (no hace falta otro `config.py` ahí).

**Modelo CNN:** entrena con `examples/train_model.py` y coloca `modelo_bowl_perro.keras` en la raíz.

---

## Flashear firmware

1. `patrol_motors/patrol_motors.ino` → Arduino UNO #1
2. `robot_hub/robot_hub.ino` → ESP32 dev board  
   Librerías: WebSockets, ArduinoJson, DFRobotDFPlayerMini
3. `examples/send_images_wifi/send_images_wifi.ino` → ESP32-CAM

## Alimentación y cableado

- Resumen de baterías: [POWER.md](POWER.md)
- **Guía detallada de sensores, pilas y resistencias:** [WIRING.md](WIRING.md)

---

## Ejecutar

```bash
python robot_controller.py
```

Flujo: patrulla → ladrido → STOP → Telegram → foto del plato → CNN → Telegram → audio (si plato lleno y aire OK) → sesión termina. **No reanuda la patrulla** hasta reiniciar robot y script.

---

## Seguridad

`config.py` y `secrets.h` están en `.gitignore` (solo en la raíz del repo).
