# ──────────────────────────────────────────────────────────────
#  config.example.py  —  ÚNICA plantilla de configuración (raíz del repo)
#  Copia este archivo a config.py y rellena tus valores reales.
# ──────────────────────────────────────────────────────────────

# URL de la cámara ESP32-CAM (ajusta la IP si cambia)
URL_CAM = "http://<IP_DEL_ESP32_CAM>/cam.jpg"

# Token del bot de Telegram (obtenido con @BotFather)
TG_TOKEN = "123456789:AAFxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"

# Tu chat ID de Telegram (usa /getUpdates para obtenerlo)
TG_CHAT_ID = "000000000"

# ── Robot patrullero (robot_controller.py, mock_controller.py) ──

# Dónde escucha el servidor WebSocket en la PC (todas las interfaces)
WS_BIND_HOST = "0.0.0.0"
WS_PORT = 8765
WS_HOST = WS_BIND_HOST  # alias

# Umbral MQ135 (escala 10-bit si usas analogReadResolution(10)). Ver CALIBRATION.md
AIR_THRESHOLD = 600

# Pista DFPlayer (0001.mp3 → track 1)
AUDIO_TRACK = 1

# Modelo CNN (entrenar con examples/train_model.py)
MODEL_PATH = "modelo_bowl_perro.keras"

# ── Herramientas examples/ (serial, monitor plato) ───────────────

SERIAL_PORT = "COM7"      # Windows: COM7 | Linux/Mac: /dev/ttyUSB0
SERIAL_BAUD = 1500000
PREDICT_INTERVAL = 30
