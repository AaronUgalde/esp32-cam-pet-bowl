# Calibración del sensor MQ135

El ESP32 dev board (`robot_hub/robot_hub.ino`) lee el MQ135 como valor ADC bruto (0–4095) y lo envía en cada evento `bark` y `heartbeat`. La PC compara ese valor con `AIR_THRESHOLD` en `config.py`: **valores menores o iguales al umbral = aire aceptable**.

## Procedimiento recomendado

### 1. Medir en aire limpio (referencia buena)

1. Coloca el robot en una habitación ventilada, lejos de humo o cocina.
2. Flashea `robot_hub/robot_hub.ino` y abre el Monitor Serial (115200 baud).
3. Espera 2–3 minutos para que el MQ135 se estabilice (es normal que tarde).
4. Anota el valor `air` de varios mensajes `heartbeat` en Serial o en los logs de `robot_controller.py`.
5. Calcula el **promedio en aire limpio** → llámalo `AIR_CLEAN`.

### 2. Medir en condición “mala” (opcional)

Si quieres afinar el umbral, expón el sensor brevemente a una fuente controlada (ej. alcohol en un pañuelo cerca, sin encender fuego) y anota el pico → `AIR_BAD`.

### 3. Elegir `AIR_THRESHOLD`

Regla práctica para empezar:

```
AIR_THRESHOLD = AIR_CLEAN + 150
```

Si mediste también `AIR_BAD`:

```
AIR_THRESHOLD = (AIR_CLEAN + AIR_BAD) / 2
```

### 4. Configurar

En `config.py` (copiado de `config.example.py`):

```python
AIR_THRESHOLD = 600  # ajusta según tus mediciones
```

El valor por defecto `600` es un punto de partida razonable si aún no calibraste.

## Verificación

1. Ejecuta `python robot_controller.py`.
2. Observa los `heartbeat` en los logs (nivel DEBUG) o el Monitor Serial del ESP32.
3. Simula un ladrido (golpe cerca del mic o ladrido real).
4. Comprueba en Telegram:
   - Si el aire reportado es ≤ `AIR_THRESHOLD` y el plato está lleno → debe reproducirse audio.
   - Si supera el umbral → mensaje indicando que el audio se omitió.

## Notas

- El MQ135 no es un sensor de CO₂ calibrado de fábrica; sirve como indicador relativo de “aire raro”.
- Recalibra si cambias la ubicación del robot o la ventilación de la habitación.
- Valores típicos en ESP32 ADC: aire limpio ~200–500, humo cercano >800 (varía por módulo).
