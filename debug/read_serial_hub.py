"""
read_serial_hub.py — Lee eventos [EVENT] del hub en modo SERIAL_ONLY.

Uso:
  python debug/read_serial_hub.py COM7
  python debug/read_serial_hub.py COM7 --raw
"""

import argparse
import json
import sys
import time

import serial


def main() -> None:
    parser = argparse.ArgumentParser(description="Monitor serial del hub_simulated")
    parser.add_argument("port", help="Puerto COM (ej. COM7)")
    parser.add_argument("--baud", type=int, default=115200)
    parser.add_argument("--raw", action="store_true", help="Imprimir todas las lineas")
    args = parser.parse_args()

    ser = serial.Serial(args.port, args.baud, timeout=0.1)
    ser.setDTR(False)
    ser.setRTS(False)
    time.sleep(2)
    ser.reset_input_buffer()

    print(f"Escuchando {args.port} @ {args.baud}  (Ctrl+C salir)\n")

    try:
        while True:
            line = ser.readline().decode("utf-8", errors="replace").strip()
            if not line:
                continue
            if args.raw:
                print(line)
                continue
            if line.startswith("[EVENT]"):
                payload = line[len("[EVENT] "):]
                try:
                    data = json.loads(payload)
                    event = data.get("event", "?")
                    extra = {k: v for k, v in data.items() if k not in ("event", "ts")}
                    print(f"  {event:12} {extra}")
                except json.JSONDecodeError:
                    print(line)
            elif any(
                line.startswith(p)
                for p in ("[SIM", "[BARK", "[TEST", "[WS", "[WiFi", "===")
            ):
                print(line)
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()


if __name__ == "__main__":
    main()
