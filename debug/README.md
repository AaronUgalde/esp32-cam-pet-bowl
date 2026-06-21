# Debug — pruebas por componente

Scripts y firmware para verificar pines y sensores **antes** de integrar el robot completo.

Diseñado para probar solo con la **ESP32 dev board**; Arduino UNO, DFPlayer y ESP32-CAM se simulan por software.

**¿Arduino vs ESP32?** El example [`examples/senores_example.ino`](../examples/senores_example.ino) es para UNO; lee [`SENSORS.md`](SENSORS.md) para ver qué se reutiliza igual y qué cambia (ADC, pines, UART del MP3).

---

## Pines (igual que `robot_hub/`)

| Señal   | GPIO ESP32 |
|---------|------------|
| Mic DO  | 34         |
| Mic AO  | 35         |
| MQ135   | 32         |
| UNO TX  | 17 → RX Arduino |
| UNO RX  | 16 ← TX Arduino |
| MP3 RX  | 18         |
| MP3 TX  | 19         |

---

## Orden recomendado

### 1. Sensores crudos (sin WiFi)

Flashea `esp32/test_sensors/test_sensors.ino` y abre el Monitor Serial a **115200 baud**.

- Deberías ver CSV: `mic_ao,mic_do,mq135,trigger`
- `trigger=1` cuando se cumple la condición de ladrido (`mic_do==LOW` o `mic_ao>200`)
- Haz ruido cerca del mic y verifica que `mic_ao` sube o `mic_do` cambia
- MQ135 tarda ~1–2 min en estabilizarse

**Comandos por serial:**

| Tecla | Acción |
|-------|--------|
| `t` | Mostrar umbrales actuales |
| `+` / `-` | Subir/bajar umbral del mic analógico |

---

### 2b. DFPlayer solo (cuando conectes el módulo MP3)

Flashea `esp32/test_mp3/test_mp3.ino` — misma librería `DFRobotDFPlayerMini` que `senores_example.ino`, pero con `HardwareSerial` en pines 18/19.

---

### 3. Hub simulado + PC mock (con WiFi)

**PC:**

```bash
pip install websockets
python debug/mock_controller.py
# usa config.py de la raíz del repo
# o: python debug/mock_controller.py --bowl full
```

**ESP32:** flashea `esp32/hub_simulated/hub_simulated.ino`

1. Asegúrate de tener **`secrets.h` en la raíz del repo** (copia de `secrets.h.example`)
2. Pon WiFi y la IP de tu PC en `WS_HOST` (sin espacios)
3. `DEBUG_SERIAL_ONLY` en `0` (WiFi activo)

En el Monitor Serial verás líneas `[SIM UNO]`, `[SIM MP3]`, etc. En la PC, el mock recibe eventos y responde como `robot_controller.py` pero **sin cámara ni TensorFlow**.

**Comandos serial (Monitor ESP32):**

| Tecla | Acción |
|-------|--------|
| `b` | Simular ladrido manualmente (sin mic) |
| `r` | Volver a modo monitoreo (repetir prueba) |
| `p` | Enviar ping al mock por WebSocket |
| `s` | Imprimir estado actual |

---

### 4. Hub simulado solo USB (sin WiFi)

En `hub_simulated.ino` cambia:

```cpp
#define DEBUG_SERIAL_ONLY 1
```

Flashea y usa el Monitor Serial. Los eventos JSON se imprimen en USB; no hace falta PC.

Opcional — lector en PC:

```bash
pip install pyserial
python debug/read_serial_hub.py COM7
```

---

## Qué simula cada capa

| Componente real | En debug |
|-----------------|----------|
| Arduino UNO #1  | `[SIM UNO] UART TX: {"cmd":"stop"}` en Serial |
| DFPlayer        | `[SIM MP3] Reproducir track N` |
| ESP32-CAM       | Mock espera 1 s y usa `--bowl full\|empty\|random` |
| Telegram        | Desactivado en mock (solo consola) |
| Patrulla        | `patrol:"active"` en heartbeat hasta evento bark |

---

## Checklist antes del robot completo

- [ ] `test_sensors`: mic responde al ruido, MQ135 da valores estables
- [ ] `hub_simulated` + `mock_controller`: ladrido real o tecla `b` dispara flujo completo
- [ ] Mock reproduce audio simulado cuando `--bowl full` y aire ≤ umbral
- [ ] Mock omite audio con `--bowl empty` o aire alto (`--air 900`)

Cuando todo pase, flashea `robot_hub/robot_hub.ino` y usa `robot_controller.py` en producción.

---

## Problemas frecuentes

### `[WS] No se pudo conectar a …` en bucle

1. En la PC: `python debug/mock_controller.py` debe estar **corriendo antes** de encender el ESP32.
2. En **`secrets.h` (raíz del repo)**: `WS_HOST` = **IP de tu PC** (`ipconfig` → IPv4), sin espacios. **No** uses `0.0.0.0`.
3. PC y ESP32 en la **misma red WiFi**.
4. Firewall Windows — permitir puerto **8765** TCP entrante, o prueba desactivar firewall un momento.
5. Deberías ver en la PC: `ESP32 conectado desde ('192.168.x.x', …)`.

### `mic_ao=0`, `mic_do=0`, `patrol=stopped`

- **Sensores no cableados:** normal que lea 0. Antes del fix, `mic_do=0` (LOW flotante) disparaba un falso ladrido.
- **Solución:** reflashea `hub_simulated.ino` (versión actual) y pulsa **`r`** en Serial para volver a `patrol:active`.
- Sin sensores, usa tecla **`b`** para simular ladrido.

---

## Dependencias Python (solo debug)

```bash
pip install websockets pyserial
```

El mock no requiere TensorFlow ni OpenCV.
