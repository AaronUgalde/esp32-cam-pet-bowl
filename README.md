# 🐾 ESP32-CAM Pet Bowl Monitor

Sistema de visión por computadora que detecta si el plato de comida de tu mascota está **lleno o vacío** usando una ESP32-CAM y una red neuronal convolucional (CNN). Envía notificaciones automáticas a **Telegram**.

---

## 🏗️ Arquitectura

```
[ ESP32-CAM ] ──(WiFi / HTTP)──► [ Python + OpenCV ]
                                          │
                          ┌───────────────┴───────────────┐
                          ▼                               ▼
                 [ CNN (TensorFlow) ]            [ Telegram Bot ]
                  empty / full + %            🔴 Vacío / 🟢 Lleno
```

---

## 📂 Estructura del proyecto

```
esp32-cam-pet-bowl/
│
├── get_images_wifi.py          # Script principal: stream + inferencia + Telegram
├── train_model.py              # Entrenamiento de la CNN
├── test_model.py               # Captura de dataset por puerto serial + inferencia
├── test_serial.py              # Solo captura de dataset por puerto serial
│
├── config.example.py           # Plantilla de credenciales Python → copiar a config.py
├── secrets.h.example           # Plantilla de credenciales Arduino → copiar a secrets.h
├── requirements.txt            # Dependencias Python
│
├── send_images_wifi/           # Firmware ESP32: streaming por WiFi (HTTP)
│   └── send_images_wifi.ino
├── send_images_serial/         # Firmware ESP32: streaming por Serial (UART)
│   └── send_images_serial.ino
├── ver_ip/                     # Utilidad: muestra la IP del ESP32 en Serial Monitor
│   └── ver_ip.ino
│
└── dataset/                    # Imágenes de entrenamiento (ignorado en git)
    ├── empty/
    └── full/
```

---

## ⚙️ Instalación

### 1. Clonar el repositorio

```bash
git clone https://github.com/tu-usuario/esp32-cam-pet-bowl.git
cd esp32-cam-pet-bowl
```

### 2. Instalar dependencias Python

```bash
pip install -r requirements.txt
```

### 3. Configurar credenciales

**Python** — copia y edita:
```bash
cp config.example.py config.py
```
Rellena en `config.py`:
- `URL_CAM` → IP de tu ESP32 (usa `ver_ip.ino` para encontrarla)
- `TG_TOKEN` → token de tu bot (obtenido con @BotFather)
- `TG_CHAT_ID` → tu chat ID (obtenido con `/getUpdates`)

**Arduino** — copia y edita:
```bash
cp secrets.h.example secrets.h
```
Rellena en `secrets.h`:
- `WIFI_SSID` → nombre de tu red WiFi
- `WIFI_PASS` → contraseña de tu red WiFi

---

## 🚀 Uso

### Paso 1 — Flashear el ESP32

Abre `send_images_wifi/send_images_wifi.ino` en el Arduino IDE y flashea tu ESP32-CAM.  
Usa `ver_ip/ver_ip.ino` para confirmar la IP asignada y actualiza `config.py`.

### Paso 2 — Recolectar imágenes de entrenamiento (opcional si ya tienes modelo)

```bash
python test_serial.py    # Conecta el ESP32 por USB serial
# Teclas: E = guardar empty | F = guardar full | Q = salir
```

### Paso 3 — Entrenar el modelo

```bash
python train_model.py
# Genera modelo_bowl_perro.keras al terminar
```

### Paso 4 — Ejecutar el monitor en tiempo real

```bash
python get_images_wifi.py
# Teclas: ESPACIO = predecir y notificar por Telegram | Q = salir
```

---

## 🤖 Modelo CNN

| Parámetro       | Valor                  |
|-----------------|------------------------|
| Resolución      | 160 × 120 px (QQVGA)   |
| Clases          | `empty` / `full`       |
| Arquitectura    | 3× Conv2D + MaxPooling |
| Salida          | Sigmoide (binaria)     |
| Data Augmentation | Flip, Rotación, Zoom, Brillo |

---

## 📦 Hardware necesario

- ESP32-CAM (Ai-Thinker)
- Programador FTDI (para flashear)
- Red WiFi 2.4 GHz
- Plato de comida para mascota 🐶

---

## 🔒 Seguridad

Los archivos `config.py` y `secrets.h` están en `.gitignore` y **nunca se suben al repositorio**.  
Usa siempre los archivos `.example` como referencia.
