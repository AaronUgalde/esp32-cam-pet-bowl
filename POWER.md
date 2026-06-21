# Alimentación del robot patrullero

Guía de cableado para alimentar motores y electrónica de forma segura en un robot móvil pequeño.

## Esquema recomendado: dual con GND común

```
Batería motores (2S LiPo 7.4V)
    └──► H-bridge shield (VIN del shield / bornes de motor)

Batería lógica (2S LiPo 7.4V  o  powerbank 5V)
    └──► Buck 5V / 3A
            ├──► ESP32 dev board (5V o VIN según placa)
            ├──► Arduino UNO #1 (jack 5V o pin VIN)
            ├──► ESP32-CAM (5V estable, ≥500 mA)
            └──► DFPlayer Mini (5V)

GND motores ──────┬── GND buck ── GND de todas las placas
                  (solo tierra común; NO unir los + de las baterías)
```

## Reglas críticas

1. **No alimentes los motores desde el pin 5V del Arduino.** Los picos de corriente reinician el ESP32 y corrompen la comunicación WiFi.
2. **GND común obligatorio** entre H-bridge, buck y todas las placas lógicas.
3. **ESP32-CAM** es sensible a caídas de tensión: usa un regulador 5V dedicado con ≥500 mA.
4. **DFPlayer** puede consumir hasta ~500 mA al reproducir audio; inclúyelo en el dimensionamiento del buck (recomendado: **5V / 3A**).
5. **Interruptor maestro** en la batería de lógica; opcional segundo interruptor en la de motores.
6. **Fusible o protección** en cada batería LiPo (5–10 A en motores, 3 A en lógica).

## Consumo estimado (lógica)

| Componente      | Consumo típico |
|-----------------|----------------|
| ESP32 dev       | 150–250 mA (WiFi activo) |
| ESP32-CAM       | 200–400 mA (captura WiFi) |
| Arduino UNO #1  | 50 mA            |
| DFPlayer (pico) | hasta 500 mA     |
| **Total pico**  | **~800 mA – 1.2 A** |

El buck debe ser **5V / 3A** para margen.

## Opción simplificada (una batería)

Un solo pack **2S 7.4V ≥ 2000 mAh**:

- H-bridge conectado directo al pack.
- Buck 5V/3A al pack para toda la lógica.

Funciona bien si el pack puede entregar picos de ~2 A (motores + lógica no simultáneos al máximo).

## Checklist de prueba de carga

Antes de montar sobre el robot:

- [ ] Medir 5V en el buck con multímetro (bajo carga: ESP32 + UNO encendidos).
- [ ] Encender ESP32-CAM y verificar que no hay reinicios al arrancar WiFi.
- [ ] Mover motores a PWM máximo mientras la lógica está encendida; comprobar que no hay brownout.
- [ ] Reproducir audio en el DFPlayer con motores detenidos y en movimiento.
- [ ] Verificar que GND común está continuo entre todas las placas.
- [ ] Confirmar que los positivos de las dos baterías **no** están unidos entre sí.

## Montaje físico

- Coloca las baterías **abajo** del chasis (centro de gravedad bajo).
- Separa el pack de motores del buck con espaciadores; evita que la vibración afloje conexiones.
- Usa cables cortos y bridas; deja holgura en los cables que unen plataformas móviles.
- Mantén antenas WiFi (ESP32 dev y ESP32-CAM) sin metal encima.

## Señales de problema

| Síntoma | Causa probable |
|---------|----------------|
| ESP32 reinicia al mover motores | Motores alimentados desde 5V del Arduino o GND deficiente |
| ESP32-CAM no arranca | Regulador 5V insuficiente (<500 mA) |
| Audio distorsionado | Caída de tensión en el buck durante reproducción |
| Lecturas erráticas MQ135/mic | GND ruidoso; acerca el GND del sensor al ESP32 dev |
