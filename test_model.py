import os
import serial
import time
import numpy as np
import cv2
import tensorflow as tf

from config import SERIAL_PORT as PORT, SERIAL_BAUD as BAUD

# Cargar el modelo entrenado
print("Cargando modelo neuronal...")
if os.path.exists('modelo_bowl_perro.keras'):
    model = tf.keras.models.load_model('modelo_bowl_perro.keras')
    print("Modelo cargado exitosamente.")
else:
    print("ERROR: No se encontró el archivo 'modelo_bowl_perro.keras'. Entrena la red primero.")
    exit()

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
print("Teclas: e = guardar empty, f = guardar full, p = PREDECIR, q = salir")

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
                if len(buffer) > 3:
                    buffer = buffer[-3:]
                break

            # 2. Verificar tamaño mínimo de cabecera
            if len(buffer) < start_idx + 8:
                break 

            # 3. Extraer la longitud de la imagen
            length_bytes = buffer[start_idx + 4 : start_idx + 8]
            frame_len = int.from_bytes(length_bytes, byteorder='little')

            if frame_len > 500000 or frame_len <= 0:
                print("Error de sincronización, descartando buffer")
                buffer = buffer[start_idx + 1:] 
                continue

            # 4. Verificar si ya llegó toda la imagen
            if len(buffer) < start_idx + 8 + frame_len:
                break 

            # 5. Extraer bytes del JPEG y limpiar buffer
            jpg = bytes(buffer[start_idx + 8 : start_idx + 8 + frame_len])
            buffer = buffer[start_idx + 8 + frame_len:]

            # Decodificar
            img_array = np.frombuffer(jpg, dtype=np.uint8)
            img = cv2.imdecode(img_array, cv2.IMREAD_COLOR)

            if img is None:
                print("Frame no decodificable (ruido en serie)")
                continue

            latest_img = img

            # Mostrar la imagen en pantalla
            img_display = cv2.resize(img, (0, 0), fx=3.0, fy=3.0, interpolation=cv2.INTER_LINEAR)
            cv2.imshow("ESP32-CAM", img_display)

            key = cv2.waitKey(1) & 0xFF
            if key == ord('q'):
                raise KeyboardInterrupt
            elif key == ord('e') and latest_img is not None:
                save_image(latest_img, "empty")
            elif key == ord('f') and latest_img is not None:
                save_image(latest_img, "full")
            
            # --- NUEVA ACCIÓN: INFERENCIA CON LA TECLA 'P' ---
            elif key == ord('p') and latest_img is not None:
                print("\n[Procesando Inferencia...]")
                
                # Paso A: Corregir el espacio de color (De BGR a RGB)
                img_rgb = cv2.cvtColor(latest_img, cv2.COLOR_BGR2RGB)
                
                # Paso B: Asegurar que las dimensiones coincidan con la red (120, 160, 3)
                img_resized = cv2.resize(img_rgb, (160, 120))
                
                # Paso C: Expandir dimensiones para simular un lote/batch -> (1, 120, 160, 3)
                img_batch = np.expand_dims(img_resized, axis=0)
                
                # Paso D: Correr el modelo (verbose=0 oculta la barra de carga estorbosa)
                prediction = model.predict(img_batch, verbose=0)[0][0]
                
                # Paso E: Interpretar la salida Sigmoide (0 = empty, 1 = full)
                if prediction >= 0.5:
                    clase_detectada = "FULL (Lleno)"
                    porcentaje_certeza = prediction * 100
                else:
                    clase_detectada = "EMPTY (Vacío)"
                    porcentaje_certeza = (1 - prediction) * 100
                
                print(f"-> RESULTADO: {clase_detectada}")
                print(f"-> Certeza: {porcentaje_certeza:.2f}%")
                print(f"-> Valor crudo del Sigmoide: {prediction:.4f}")

except KeyboardInterrupt:
    pass
finally:
    ser.close()
    cv2.destroyAllWindows()