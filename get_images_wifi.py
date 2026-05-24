import cv2
import numpy as np
import requests
import tensorflow as tf
import threading
import argparse
import time

from config import URL_CAM, TG_TOKEN, TG_CHAT_ID, PREDICT_INTERVAL

# ── Argumentos ────────────────────────────────────────────────────────────────
parser = argparse.ArgumentParser(description="ESP32-CAM Pet Bowl Monitor")
parser.add_argument("--headless", action="store_true",
                    help="Sin ventana: predice automáticamente cada PREDICT_INTERVAL segundos")
args = parser.parse_args()

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

if args.headless:
    print(f"Modo headless — predicción automática cada {PREDICT_INTERVAL}s  |  Ctrl+C para salir")
else:
    print("Modo display  — [ESPACIO] predecir  |  [Q] salir")

# ── Estado compartido ─────────────────────────────────────────────────────────
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
    emoji  = "🟢" if label == "full" else "🔴"
    estado = "lleno" if label == "full" else "vacío"
    msg    = f"{emoji} *Plato de la mascota:* {estado}\nConfianza: {prob*100:.1f}%"
    try:
        requests.post(TG_URL, json={
            "chat_id": TG_CHAT_ID, "text": msg, "parse_mode": "Markdown"
        }, timeout=5)
        print("[Telegram] Mensaje enviado.")
    except Exception as e:
        print(f"[Telegram] Error: {e}")

def notify(label, prob):
    threading.Thread(target=send_telegram, args=(label, prob), daemon=True).start()

# ── Inferencia ────────────────────────────────────────────────────────────────
def get_frame():
    """Devuelve el frame más reciente decodificado, o None si no hay."""
    with _lock:
        raw = _latest_raw
    if raw is None:
        return None
    img = cv2.imdecode(np.frombuffer(raw, dtype=np.uint8), cv2.IMREAD_COLOR)
    return img

def predict(img_bgr):
    tensor = tf.expand_dims(tf.cast(img_bgr, tf.float32), axis=0)
    prob   = float(model(tensor, training=False)[0, 0])
    label  = CLASS_NAMES[int(prob >= THRESHOLD)]
    return label, prob

def run_prediction(img):
    label, prob = predict(img)
    ts = time.strftime("%H:%M:%S")
    print(f"[{ts}] Predicción: {label.upper()}  —  confianza: {prob*100:.1f}%")
    notify(label, prob)
    return label, prob

# ── Banner (solo modo display) ────────────────────────────────────────────────
_banner, _banner_pred = None, None

def apply_banner(img, label, prob):
    global _banner, _banner_pred
    if _banner_pred != (label, prob):
        color  = (0, 200, 0) if label == "full" else (0, 0, 220)
        franja = np.zeros((38, img.shape[1], 3), dtype=np.uint8)
        cv2.putText(franja, f"{label.upper()}  {prob*100:.1f}%",
                    (8, 28), cv2.FONT_HERSHEY_SIMPLEX, 0.9, color, 2, cv2.LINE_AA)
        _banner, _banner_pred = franja, (label, prob)
    out = img.copy()
    out[0:38, :] = cv2.addWeighted(out[0:38, :], 0.4, _banner, 1.0, 0)
    return out


# ── Bucle headless ────────────────────────────────────────────────────────────
def loop_headless():
    print("Esperando primer frame…")
    while True:
        img = get_frame()
        if img is not None:
            break
        time.sleep(0.2)

    print("Primer frame recibido. Iniciando ciclo de predicción.")
    try:
        while True:
            img = get_frame()
            if img is not None:
                run_prediction(img)
            time.sleep(PREDICT_INTERVAL)
    except KeyboardInterrupt:
        pass

# ── Bucle display ─────────────────────────────────────────────────────────────
def loop_display():
    last_prediction = None
    while True:
        img = get_frame()
        if img is None:
            if cv2.waitKey(1) & 0xFF == ord("q"):
                break
            continue

        display = apply_banner(img, *last_prediction) if last_prediction else img
        cv2.imshow("ESP32-CAM Pet Bowl", display)

        key = cv2.waitKey(1) & 0xFF
        if key == ord("q"):
            break
        elif key == ord(" "):
            last_prediction = run_prediction(img)

    cv2.destroyAllWindows()

# ── Entry point ───────────────────────────────────────────────────────────────
if args.headless:
    loop_headless()
else:
    loop_display()

_stop = True
