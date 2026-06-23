"""
robot_controller.py — Cerebro del robot patrullero (PC).

Máquina de estados:
  Patrolling → Alert → Inspecting → NotifyingBowl → PlayingAudio → SessionEnd

Uso:
  python robot_controller.py
  (desde la raíz del repositorio, con config.py y modelo_bowl_perro.keras)
"""

import asyncio
import json
import logging
import sys
from enum import Enum, auto

import cv2
import numpy as np
import requests
import tensorflow as tf
import websockets

from config import (
    AIR_THRESHOLD,
    AUDIO_TRACK,
    MODEL_PATH,
    TG_CHAT_ID,
    TG_TOKEN,
    URL_CAM,
    WS_PORT,
)

try:
    from config import WS_BIND_HOST as WS_HOST
except ImportError:
    from config import WS_HOST  # type: ignore[no-redef]

WS_HOST = (WS_HOST or "0.0.0.0").strip()

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("robot")

CLASS_NAMES = ["empty", "full"]
THRESHOLD = 0.5
TG_URL = f"https://api.telegram.org/bot{TG_TOKEN}/sendMessage"


class State(Enum):
    PATROLLING = auto()
    ALERT = auto()
    INSPECTING = auto()
    NOTIFYING_BOWL = auto()
    PLAYING_AUDIO = auto()
    SESSION_END = auto()


class RobotController:
    def __init__(self):
        self.state = State.PATROLLING
        self.esp_socket = None
        self.last_air = 0
        self.bark_air = 0
        self.http = requests.Session()

        log.info("Cargando modelo %s…", MODEL_PATH)
        self.model = tf.keras.models.load_model(MODEL_PATH)
        _dummy = tf.zeros((1, 120, 160, 3), dtype=tf.float32)
        self.model(_dummy, training=False)
        log.info("Modelo listo.")

    def send_telegram(self, text: str) -> None:
        try:
            requests.post(
                TG_URL,
                json={"chat_id": TG_CHAT_ID, "text": text, "parse_mode": "Markdown"},
                timeout=5,
            )
            log.info("[Telegram] %s", text.replace("\n", " | "))
        except Exception as exc:
            log.error("[Telegram] Error: %s", exc)

    async def send_esp(self, payload: dict) -> None:
        if self.esp_socket is None:
            log.warning("ESP32 no conectado; comando omitido: %s", payload)
            return
        await self.esp_socket.send(json.dumps(payload))

    def fetch_camera_jpeg(self) -> bytes | None:
        try:
            r = self.http.get(URL_CAM, timeout=5)
            r.raise_for_status()
            return r.content
        except requests.RequestException as exc:
            log.error("Error al obtener imagen: %s", exc)
            return None

    def predict_bowl(self, jpeg_bytes: bytes) -> tuple[str, float] | None:
        img_bgr = cv2.imdecode(np.frombuffer(jpeg_bytes, dtype=np.uint8), cv2.IMREAD_COLOR)
        if img_bgr is None:
            return None
        img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
        tensor = tf.expand_dims(tf.cast(img_rgb, tf.float32), axis=0)
        prob = float(self.model(tensor, training=False)[0, 0])
        label = CLASS_NAMES[int(prob >= THRESHOLD)]
        return label, prob

    async def handle_bark(self, air: int) -> None:
        if self.state != State.PATROLLING:
            log.warning(
                "Ladrido ignorado — estado=%s (reinicia robot_controller.py o pulsa reset en UNO #2)",
                self.state.name,
            )
            return

        self.state = State.ALERT
        self.bark_air = air
        log.warning("Ladrido detectado — aire=%d", air)

        self.send_telegram(
            f"🐕 *Ladrido detectado*\nCalidad del aire (MQ135): `{air}`\nDeteniendo robot…"
        )

        await self.send_esp({"cmd": "stop"})

        self.state = State.INSPECTING
        await asyncio.sleep(1.0)

        jpeg = self.fetch_camera_jpeg()
        if jpeg is None:
            self.send_telegram("⚠️ No se pudo capturar imagen del plato.")
            self.state = State.SESSION_END
            return

        result = self.predict_bowl(jpeg)
        self.state = State.NOTIFYING_BOWL

        if result is None:
            self.send_telegram("⚠️ No se pudo analizar la imagen del plato.")
            self.state = State.SESSION_END
            return

        label, prob = result
        emoji = "🟢" if label == "full" else "🔴"
        estado = "lleno" if label == "full" else "vacío"
        self.send_telegram(
            f"{emoji} *Plato de la mascota:* {estado}\nConfianza: {prob * 100:.1f}%"
        )

        air_ok = self.bark_air <= AIR_THRESHOLD
        if label == "full" and air_ok:
            self.state = State.PLAYING_AUDIO
            log.info("Enviando play_audio al ESP32 (plato lleno, aire=%d <= %d)", self.bark_air, AIR_THRESHOLD)
            await self.send_esp({"cmd": "play_audio", "track": AUDIO_TRACK})
            self.send_telegram("🔊 *Audio reproducido* — voz del dueño enviada al robot.")
        elif label == "full" and not air_ok:
            log.info(
                "Audio omitido: aire %d supera umbral %d", self.bark_air, AIR_THRESHOLD
            )
            self.send_telegram(
                f"🔇 Audio *no* reproducido — calidad del aire `{self.bark_air}` "
                f"supera umbral `{AIR_THRESHOLD}`."
            )
        else:
            log.info("Audio omitido: plato vacío")

        self.state = State.SESSION_END
        log.info("Sesión terminada. Reinicia el robot y este script para una nueva patrulla.")

    async def handle_message(self, raw: str) -> None:
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            log.warning("JSON inválido: %s", raw[:80])
            return

        event = data.get("event")
        if event == "heartbeat":
            self.last_air = data.get("air", self.last_air)
            patrol = data.get("patrol", "?")
            log.debug("Heartbeat aire=%s patrol=%s", self.last_air, patrol)
        elif event == "bark":
            air = data.get("air", self.last_air)
            await self.handle_bark(air)
        elif event == "hello":
            log.info("ESP32 hub conectado (aire inicial=%s)", data.get("air"))
            if self.state == State.SESSION_END:
                self.state = State.PATROLLING
                log.info("Estado reseteado → PATROLLING (hello del sensor board)")
        elif event == "audio_played":
            log.info("ESP32 confirmó reproducción de audio")
        elif event == "stopped":
            log.info("Robot detenido (%s)", data.get("reason", ""))
        elif event == "pong":
            log.info("Pong del sensor board (UART ESP32→UNO OK)")
        elif event == "audio_error":
            log.warning("Sensor board: error al reproducir MP3")

    async def client_handler(self, websocket):
        remote = websocket.remote_address
        log.info("Cliente WebSocket conectado: %s", remote)
        self.esp_socket = websocket
        if self.state == State.SESSION_END:
            self.state = State.PATROLLING
            log.info("Estado reseteado → PATROLLING (nueva conexión ESP32)")
        try:
            async for message in websocket:
                await self.handle_message(message)
        except websockets.ConnectionClosed:
            log.info("Cliente WebSocket desconectado: %s", remote)
        finally:
            if self.esp_socket is websocket:
                self.esp_socket = None

    async def run(self) -> None:
        log.info("Servidor WebSocket en ws://%s:%d", WS_HOST, WS_PORT)
        log.info("Umbral aire MQ135: %d (menor = mejor)", AIR_THRESHOLD)
        log.info("Esperando ESP32 hub… (Ctrl+C para salir)")

        async with websockets.serve(
            self.client_handler,
            WS_HOST,
            WS_PORT,
            ping_interval=None,
            ping_timeout=None,
        ):
            await asyncio.Future()


def main() -> None:
    try:
        controller = RobotController()
        asyncio.run(controller.run())
    except KeyboardInterrupt:
        log.info("Interrupción del usuario.")
        sys.exit(0)


if __name__ == "__main__":
    main()
