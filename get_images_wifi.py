import cv2
import numpy as np
import requests
import tensorflow as tf
import threading

from config import URL_CAM, TG_TOKEN, TG_CHAT_ID

# ── Configuración ──────────────────────────────────────────────────────────────
MODEL_PATH  = "modelo_bowl_perro.keras"
CLASS_NAMES = ["empty", "full"]
THRESHOLD   = 0.5
TG_URL      = f"https://api.telegram.org/bot{TG_TOKEN}/sendMessage"

# ── Carga del modelo ───────────────────────────────────────────────────────────
print("Cargando modelo…")
model = tf.keras.models.load_model(MODEL_PATH)
_dummy = tf.zeros((1, 120, 160, 3), dtype=tf.float32)
model(_dummy, training=False)
print("Modelo listo.")
print("Presiona  [ESPACIO]  para predecir  |  [Q]  para salir")

# ── Estado compartido entre threads ───────────────────────────────────────────
_lock       = threading.Lock()
_latest_raw = None
_stop       = False

# ── Thread de captura ─────────────────────────────────────────────────────────
def fetch_loop():
    global _latest_raw, _stop
    session = requests.Session()
    while not _stop:
        try:
            r = session.get(URL_CAM, timeout=2)
            with _lock:
                _latest_raw = r.content
        except requests.RequestException:
            pass

fetch_thread = threading.Thread(target=fetch_loop, daemon=True)
fetch_thread.start()

# ── Telegram ──────────────────────────────────────────────────────────────────
def send_telegram(label, prob):
    emoji   = "🟢" if label == "full" else "🔴"
    estado  = "lleno" if label == "full" else "vacío"
    mensaje = f"{emoji} *Plato de la mascota:* {estado}\nConfianza: {prob*100:.1f}%"
    try:
        requests.post(TG_URL, json={
            "chat_id":    TG_CHAT_ID,
            "text":       mensaje,
            "parse_mode": "Markdown"
        }, timeout=5)
        print("[Telegram] Mensaje enviado.")
    except Exception as e:
        print(f"[Telegram] Error al enviar: {e}")

def notify(label, prob):
    t = threading.Thread(target=send_telegram, args=(label, prob), daemon=True)
    t.start()

# ── Inferencia ────────────────────────────────────────────────────────────────
def predict(img_bgr):
    tensor = tf.expand_dims(tf.cast(img_bgr, tf.float32), axis=0)
    prob   = float(model(tensor, training=False)[0, 0])
    label  = CLASS_NAMES[int(prob >= THRESHOLD)]
    return label, prob

# ── Banner cacheado ───────────────────────────────────────────────────────────
_banner      = None
_banner_pred = None

def get_banner(width, label, prob):
    global _banner, _banner_pred
    if _banner_pred == (label, prob) and _banner is not None:
        return _banner
    color  = (0, 200, 0) if label == "full" else (0, 0, 220)
    text   = f"{label.upper()}  {prob*100:.1f}%"
    franja = np.zeros((38, width, 3), dtype=np.uint8)
    cv2.putText(franja, text, (8, 28), cv2.FONT_HERSHEY_SIMPLEX,
                0.9, color, 2, cv2.LINE_AA)
    _banner      = franja
    _banner_pred = (label, prob)
    return _banner

def apply_banner(img, label, prob):
    out    = img.copy()
    banner = get_banner(img.shape[1], label, prob)
    out[0:38, :] = cv2.addWeighted(out[0:38, :], 0.4, banner, 1.0, 0)
    return out

# ── Bucle principal ───────────────────────────────────────────────────────────
last_prediction = None

while True:
    with _lock:
        raw = _latest_raw

    if raw is None:
        if cv2.waitKey(1) & 0xFF == ord("q"):
            break
        continue

    img_np = np.frombuffer(raw, dtype=np.uint8)
    img    = cv2.imdecode(img_np, cv2.IMREAD_COLOR)

    if img is None:
        continue

    display = apply_banner(img, *last_prediction) if last_prediction else img
    cv2.imshow("ESP32-CAM Pet Bowl", display)

    key = cv2.waitKey(1) & 0xFF

    if key == ord("q"):
        break
    elif key == ord(" "):
        label, prob = predict(img)
        last_prediction = (label, prob)
        print(f"[Predicción]  {label.upper()}  —  confianza: {prob*100:.1f}%")
        notify(label, prob)

_stop = True
cv2.destroyAllWindows()
