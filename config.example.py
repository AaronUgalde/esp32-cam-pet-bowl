# ──────────────────────────────────────────────────────────────
#  config.example.py  —  Plantilla de configuración
#  Copia este archivo a config.py y rellena tus valores reales.
# ──────────────────────────────────────────────────────────────

# URL de la cámara ESP32-CAM (ajusta la IP si cambia)
URL_CAM = "http://<IP_DEL_ESP32>/cam.jpg"

# Token del bot de Telegram (obtenido con @BotFather)
TG_TOKEN = "123456789:AAFxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# Tu chat ID de Telegram (usa /getUpdates para obtenerlo)
TG_CHAT_ID = "000000000"

# Puerto serial del ESP32 (para test_serial.py y test_model.py)
SERIAL_PORT = "COM7"      # Windows: COM7 | Linux/Mac: /dev/ttyUSB0
SERIAL_BAUD = 1500000

# Segundos entre predicciones automáticas en modo --headless
PREDICT_INTERVAL = 30
