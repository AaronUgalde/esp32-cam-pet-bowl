"""
mock_controller.py — Simula robot_controller.py en la PC (sin cámara ni TensorFlow).

Recibe eventos WebSocket del ESP32 (hub_simulated.ino) y ejecuta el mismo flujo
de estados, imprimiendo todo en consola.

Uso:
  python debug/mock_controller.py
  python debug/mock_controller.py --bowl full
  python debug/mock_controller.py --bowl empty --air 900
"""

import argparse
import asyncio
import importlib.util
import json
import logging
import random
import sys
from enum import Enum, auto
from pathlib import Path

import websockets

_DEBUG_DIR = Path(__file__).resolve().parent
_ROOT_CONFIG = _DEBUG_DIR.parent / "config.py"
_DEFAULT_BIND = "0.0.0.0"
_DEFAULT_PORT = 8765


def _load_settings() -> tuple[str, int, int, int]:
    """Carga el config.py unificado de la raíz del repo."""
    bind = _DEFAULT_BIND
    port = _DEFAULT_PORT
    air_threshold = 600
    audio_track = 1

    cfg_path = _ROOT_CONFIG
    if not cfg_path.is_file():
        return bind, port, air_threshold, audio_track

    spec = importlib.util.spec_from_file_location("robot_cfg", cfg_path)
    if spec is None or spec.loader is None:
        return bind, port, air_threshold, audio_track
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    bind = str(getattr(mod, "WS_BIND_HOST", getattr(mod, "WS_HOST", bind))).strip()
    port = int(getattr(mod, "WS_PORT", port))
    air_threshold = int(getattr(mod, "AIR_THRESHOLD", air_threshold))
    audio_track = int(getattr(mod, "AUDIO_TRACK", audio_track))
    return bind, port, air_threshold, audio_track


def _normalize_bind_host(host: str) -> str:
    host = (host or "").strip()
    if not host or host.startswith("<") or " " in host:
        return _DEFAULT_BIND
    return host


WS_BIND_HOST, WS_PORT, AIR_THRESHOLD, AUDIO_TRACK = _load_settings()
WS_BIND_HOST = _normalize_bind_host(WS_BIND_HOST)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%H:%M:%S",
)
log = logging.getLogger("mock")


class State(Enum):
    PATROLLING = auto()
    ALERT = auto()
    INSPECTING = auto()
    NOTIFYING_BOWL = auto()
    PLAYING_AUDIO = auto()
    SESSION_END = auto()


class MockController:
    def __init__(self, bowl: str, air_override: int | None, bind_host: str, port: int):
        self.state = State.PATROLLING
        self.esp_socket = None
        self.bark_air = 0
        self.bowl_mode = bowl
        self.air_override = air_override
        self.bind_host = bind_host
        self.port = port

    def console_notify(self, title: str, body: str) -> None:
        log.info("[Telegram SIM] %s — %s", title, body.replace("\n", " | "))

    async def send_esp(self, payload: dict) -> None:
        if self.esp_socket is None:
            log.warning("ESP32 no conectado: %s", payload)
            return
        msg = json.dumps(payload)
        log.info("[WS TX] %s", msg)
        await self.esp_socket.send(msg)

    def simulate_camera_and_cnn(self) -> tuple[str, float]:
        log.info("[SIM CAM] GET /cam.jpg … esperando 1 s")
        if self.bowl_mode == "random":
            label = random.choice(["empty", "full"])
        else:
            label = self.bowl_mode
        prob = random.uniform(0.82, 0.98)
        log.info("[SIM CNN] Prediccion: %s (%.1f%% confianza simulada)", label, prob * 100)
        return label, prob

    async def handle_bark(self, air: int) -> None:
        if self.state != State.PATROLLING:
            log.warning("Ignorando bark en estado %s", self.state.name)
            return

        self.state = State.ALERT
        self.bark_air = self.air_override if self.air_override is not None else air

        log.warning(">>> LADRIDO — aire=%d — estado=%s", self.bark_air, self.state.name)
        self.console_notify(
            "Ladrido",
            f"Ladrido detectado. Aire MQ135: {self.bark_air}. Deteniendo robot…",
        )

        await self.send_esp({"cmd": "stop"})

        self.state = State.INSPECTING
        await asyncio.sleep(1.0)

        label, prob = self.simulate_camera_and_cnn()
        self.state = State.NOTIFYING_BOWL

        estado = "lleno" if label == "full" else "vacio"
        self.console_notify("Plato", f"Plato {estado} — confianza {prob * 100:.1f}%")

        air_ok = self.bark_air <= AIR_THRESHOLD
        if label == "full" and air_ok:
            self.state = State.PLAYING_AUDIO
            log.info(">>> AUDIO: plato lleno + aire OK (%d <= %d)", self.bark_air, AIR_THRESHOLD)
            await self.send_esp({"cmd": "play_audio", "track": AUDIO_TRACK})
            self.console_notify("Audio", "Reproduciendo voz del dueno (simulado en ESP32)")
        elif label == "full":
            log.info(">>> AUDIO omitido: aire %d > umbral %d", self.bark_air, AIR_THRESHOLD)
            self.console_notify("Audio", f"No reproducido — aire {self.bark_air} > {AIR_THRESHOLD}")
        else:
            log.info(">>> AUDIO omitido: plato vacio")

        self.state = State.SESSION_END
        log.info(">>> SESION TERMINADA — pulsa 'r' en ESP32 para otra prueba")

    async def handle_message(self, raw: str) -> None:
        log.debug("[WS RX] %s", raw)
        try:
            data = json.loads(raw)
        except json.JSONDecodeError:
            log.warning("JSON invalido: %s", raw[:80])
            return

        event = data.get("event")
        if event == "heartbeat":
            log.info(
                "heartbeat | air=%s patrol=%s mic_ao=%s mic_do=%s",
                data.get("air"),
                data.get("patrol"),
                data.get("mic_ao", "-"),
                data.get("mic_do", "-"),
            )
        elif event == "bark":
            await self.handle_bark(data.get("air", 0))
        elif event == "hello":
            log.info("ESP32 conectado — aire inicial=%s", data.get("air"))
            if self.state == State.SESSION_END:
                self.state = State.PATROLLING
        elif event == "reset":
            self.state = State.PATROLLING
            log.info("Mock reseteado → PATROLLING (tecla r en ESP32)")
        elif event == "stopped":
            log.info("Robot detenido — razon=%s", data.get("reason"))
        elif event == "audio_played":
            log.info("ESP32 confirmo reproduccion MP3 simulada")
        elif event in ("pong", "audio_error"):
            log.info("evento: %s", event)

    async def client_handler(self, websocket):
        addr = websocket.remote_address
        log.info("ESP32 conectado desde %s", addr)
        self.esp_socket = websocket
        if self.state == State.SESSION_END:
            self.state = State.PATROLLING
            log.info("Estado reseteado a PATROLLING (nueva conexion)")
        try:
            async for message in websocket:
                await self.handle_message(message)
        except websockets.ConnectionClosed:
            log.info("ESP32 desconectado")
        finally:
            if self.esp_socket is websocket:
                self.esp_socket = None

    async def run(self) -> None:
        log.info("=== Mock controller (sin camara/ML/Telegram) ===")
        log.info("WebSocket escuchando en ws://%s:%d", self.bind_host, self.port)
        log.info(
            "En secrets.h del ESP32 pon WS_HOST = IP de esta PC (ipconfig), no %s",
            self.bind_host,
        )
        log.info("Bowl simulado: %s | Umbral aire: %d", self.bowl_mode, AIR_THRESHOLD)
        if self.air_override is not None:
            log.info("Aire forzado en bark: %d", self.air_override)
        log.info("Esperando hub_simulated.ino …")

        async with websockets.serve(
            self.client_handler,
            self.bind_host,
            self.port,
            ping_interval=None,
            ping_timeout=None,
        ):
            await asyncio.Future()


def main() -> None:
    parser = argparse.ArgumentParser(description="Mock del robot_controller para debug")
    parser.add_argument(
        "--bowl",
        choices=["full", "empty", "random"],
        default="random",
        help="Resultado simulado de la CNN (default: random)",
    )
    parser.add_argument(
        "--air",
        type=int,
        default=None,
        help="Forzar valor de aire en evento bark (ej. 900 para probar omision de audio)",
    )
    parser.add_argument(
        "--host",
        default=None,
        help=f"Direccion de escucha del servidor (default: {WS_BIND_HOST})",
    )
    parser.add_argument(
        "--port",
        type=int,
        default=None,
        help=f"Puerto WebSocket (default: {WS_PORT})",
    )
    args = parser.parse_args()

    bind = _normalize_bind_host(args.host or WS_BIND_HOST)
    port = args.port if args.port is not None else WS_PORT

    try:
        ctrl = MockController(
            bowl=args.bowl, air_override=args.air, bind_host=bind, port=port
        )
        asyncio.run(ctrl.run())
    except KeyboardInterrupt:
        log.info("Salida.")


if __name__ == "__main__":
    main()
