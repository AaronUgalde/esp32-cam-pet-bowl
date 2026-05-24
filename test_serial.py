import os
import serial
import time
import numpy as np
import cv2

from config import SERIAL_PORT as PORT, SERIAL_BAUD as BAUD

BASE_DIR = "dataset"
EMPTY_DIR = os.path.join(BASE_DIR, "empty")
FULL_DIR = os.path.join(BASE_DIR, "full")
os.makedirs(EMPTY_DIR, exist_ok=True)
os.makedirs(FULL_DIR, exist_ok=True)

ser = serial.Serial(PORT, BAUD, timeout=0.1)
ser.setDTR(False)
ser.setRTS(False)
time.sleep(2)
ser.reset_input_buffer()

print(f"Conectado a {PORT} @ {BAUD}")
print("Teclas: e = empty, f = full, q = salir")

buffer = bytearray()
latest_img = None
counter_empty = 41
counter_full = 31

def save_image(img, label):
    global counter_empty, counter_full

    if label == "empty":
        path = os.path.join(EMPTY_DIR, f"empty_{counter_empty:05d}.jpg")
        counter_empty += 1
    else:
        path = os.path.join(FULL_DIR, f"full_{counter_full:05d}.jpg")
        counter_full += 1

    cv2.imwrite(path, img)
    print(f"Guardada: {path}")

MAGIC_BYTES = b'\xa5\x5a\xa5\x5a'

try:
    while True:
        chunk = ser.read(4096)
        if chunk:
            buffer.extend(chunk)

        while True:
            # 1. Buscar la secuencia mágica
            start_idx = buffer.find(MAGIC_BYTES)
            
            if start_idx == -1:
                # Si no encontramos los magic bytes, conservamos los últimos 3 bytes 
                # por si la secuencia quedó cortada a la mitad del chunk
                if len(buffer) > 3:
                    buffer = buffer[-3:]
                break

            # 2. Verificar si tenemos suficientes bytes para el Magic + Longitud (4 + 4 = 8 bytes)
            if len(buffer) < start_idx + 8:
                break # Esperar a leer más datos

            # 3. Extraer la longitud de la imagen
            length_bytes = buffer[start_idx + 4 : start_idx + 8]
            frame_len = int.from_bytes(length_bytes, byteorder='little')

            # Prevenir errores absurdos si hubo ruido en el cable
            if frame_len > 500000 or frame_len <= 0:
                print("Error de sincronización, descartando buffer")
                buffer = buffer[start_idx + 1:] 
                continue

            # 4. Verificar si ya llegó toda la imagen a nuestro buffer
            if len(buffer) < start_idx + 8 + frame_len:
                break # Esperar a leer más datos

            # 5. Extraer exactamente los bytes del JPEG
            jpg = bytes(buffer[start_idx + 8 : start_idx + 8 + frame_len])
            
            # Limpiar el buffer quitando la imagen que ya procesamos
            buffer = buffer[start_idx + 8 + frame_len:]

            # Decodificar
            img_array = np.frombuffer(jpg, dtype=np.uint8)
            img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

            if img is None:
                print("Frame no decodificable (ruido en serie)")
                continue

            latest_img = img

            img_display = cv2.resize(img, (0, 0), fx=3.0, fy=3.0, interpolation=cv2.INTER_LINEAR)
            cv2.imshow("ESP32-CAM", img_display)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                raise KeyboardInterrupt
            elif key == ord('e') and latest_img is not None:
                save_image(latest_img, "empty")
            elif key == ord('f') and latest_img is not None:
                save_image(latest_img, "full")

except KeyboardInterrupt:
    pass
finally:
    ser.close()
    cv2.destroyAllWindows()