# 🐾 ESP32-CAM Pet Bowl Monitor

Sistema de visión por computadora que detecta si el plato de comida de tu mascota está **lleno o vacío** usando una ESP32-CAM y una red neuronal convolucional (CNN). Envía notificaciones automáticas a **Telegram**.

> El robot patrullero vive en la raíz del repositorio. Ver [`../README.md`](../README.md).

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

## 📂 Estructura

```
examples/
├── get_images_wifi.py          # Stream + inferencia + Telegram
├── train_model.py              # Entrenamiento de la CNN
├── test_model.py               # Captura serial + inferencia
├── test_serial.py              # Captura de dataset serial
├── example_use_of_motors.ino   # Referencia: control de motores
├── senores_example.ino         # Referencia: mic, MQ135, MP3
├── config.example.py           # Plantilla → config.py
├── secrets.h.example           # Plantilla → secrets.h
├── requirements.txt
├── send_images_wifi/           # Firmware ESP32-CAM (HTTP)
├── send_images_serial/         # Firmware ESP32-CAM (serial)
└── ver_ip/                     # Utilidad: IP del ESP32-CAM
```

---

## ⚙️ Instalación

```bash
pip install -r requirements.txt
cp ../config.example.py ../config.py   # un solo config en la raíz
cp ../secrets.h.example ../secrets.h   # un solo secrets en la raíz
```

---

## 🚀 Uso

1. Flashea `send_images_wifi/send_images_wifi.ino` en la ESP32-CAM.
2. (Opcional) Recolecta imágenes con `python test_serial.py`.
3. Entrena con `python train_model.py` → genera `modelo_bowl_perro.keras`.
4. Monitor en tiempo real:

```bash
python get_images_wifi.py
# [ESPACIO] predecir + Telegram  |  [Q] salir
```

---

## 🤖 Modelo CNN

| Parámetro       | Valor                  |
|-----------------|------------------------|
| Resolución      | 160 × 120 px (QQVGA)   |
| Clases          | `empty` / `full`       |
| Arquitectura    | 3× Conv2D + MaxPooling |
| Salida          | Sigmoide (binaria)     |

---

## 📦 Hardware

- ESP32-CAM (Ai-Thinker)
- Programador FTDI
- Red WiFi 2.4 GHz
