#!/usr/bin/env python3
"""
Prueba rápida ESP32-CAM: descarga una foto y opcionalmente la guarda.

Uso (desde la raíz del repo):
  python debug/bench/test_cam.py
  python debug/bench/test_cam.py --save foto.jpg
  python debug/bench/test_cam.py --url http://192.168.0.50/cam.jpg
"""

import argparse
import sys
from pathlib import Path

import requests

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT))

try:
    from config import URL_CAM
except ImportError:
    URL_CAM = "http://<IP_ESP32_CAM>/cam.jpg"


def main() -> None:
    parser = argparse.ArgumentParser(description="Prueba HTTP ESP32-CAM")
    parser.add_argument("--url", default=URL_CAM, help="URL /cam.jpg")
    parser.add_argument("--save", metavar="FILE", help="Guardar JPEG en disco")
    args = parser.parse_args()

    if "<IP" in args.url:
        print("Configura URL_CAM en config.py o usa --url http://IP/cam.jpg")
        sys.exit(1)

    print(f"GET {args.url}")
    try:
        r = requests.get(args.url, timeout=10)
        r.raise_for_status()
    except requests.RequestException as e:
        print(f"Error: {e}")
        print("Flashea examples/send_images_wifi y verifica IP con examples/ver_ip")
        sys.exit(1)

    data = r.content
    print(f"OK — {len(data)} bytes, Content-Type: {r.headers.get('Content-Type', '?')}")

    if args.save:
        path = Path(args.save)
        path.write_bytes(data)
        print(f"Guardado: {path.resolve()}")

    if data[:2] == b"\xff\xd8":
        print("JPEG valido (magic FF D8)")
    else:
        print("Advertencia: no parece JPEG")


if __name__ == "__main__":
    main()
