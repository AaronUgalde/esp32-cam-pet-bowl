# =============================================================================
# inferencia_tcp.py - Cliente TCP de Inferencia Visual (v4 - Ultra Optimizado)
# Proyecto: Inspección Visual de Bowl de Mascota
#
# Mejoras v4:
#   - Decodificación Perezosa (Lazy Decoding): El hilo de red no decodifica
#     los JPEGs. Pasa los bytes crudos a la cola, ahorrando hasta un 70% de CPU.
#   - El hilo principal decodifica únicamente el frame que va a mostrar.
#   - Al presionar 'p', se envía el frame ya decodificado (matriz OpenCV)
#     al hilo de inferencia sin interferir con el flujo de red.
# =============================================================================

import socket
import threading
import queue
import time
import numpy as np
import cv2
import tensorflow as tf
import os
import sys

# -----------------------------------------------------------------------------
# CONFIGURACIÓN
# -----------------------------------------------------------------------------
ESP32_IP    = "192.168.0.14"   # <-- IP del ESP32-CAM
ESP32_PORT  = 8888
MODELO_PATH = "modelo_bowl_perro.keras"
MAGIC_BYTES = b'\xa5\x5a\xa5\x5a'

# =============================================================================
# Cargar modelo antes de conectar
# =============================================================================
print("=" * 55)
print("  ESP32-CAM - Cliente TCP de Inferencia v4")
print("=" * 55)
print(f"\n[Modelo] Cargando '{MODELO_PATH}'...")

if not os.path.exists(MODELO_PATH):
    print(f"\n[ERROR FATAL] No se encontró '{MODELO_PATH}'.")
    sys.exit(1)

model = tf.keras.models.load_model(MODELO_PATH)
print("[Modelo] Cargado exitosamente.\n")

# =============================================================================
# Colas de comunicación inter-hilos (Tamaño 1 para evitar lag)
# =============================================================================
# frame_queue almacena BYTES crudos del JPEG enviados por la red
frame_queue = queue.Queue(maxsize=1)
# inferencia_queue almacena la MATRIZ BGR (OpenCV) lista para TensorFlow
inferencia_queue = queue.Queue(maxsize=1)

# Evento para señalar cierre limpio a todos los hilos
stop_event = threading.Event()

# Resultado de la última inferencia (compartido entre hilos)
ultimo_resultado = {"texto": "", "tiempo": 0}
resultado_lock   = threading.Lock()

# =============================================================================
# Función de preprocesamiento + inferencia
# =============================================================================
def predecir(frame_bgr):
    img_rgb    = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
    img_resize = cv2.resize(img_rgb, (160, 120))
    img_batch  = np.expand_dims(img_resize, axis=0)
    raw        = model.predict(img_batch, verbose=0)[0][0]

    if raw >= 0.5:
        etiqueta   = "LLENO"
        porcentaje = raw * 100
    else:
        etiqueta   = "VACÍO"
        porcentaje = (1 - raw) * 100

    return etiqueta, porcentaje, raw

# =============================================================================
# HILO DE RED: Recibe bytes del ESP32 y los encola SIN DECODIFICAR
# =============================================================================
def hilo_red(sock):
    buffer = bytearray()

    while not stop_event.is_set():
        try:
            fragmento = sock.recv(32768)  # Buffer de lectura de 32 KB
        except socket.timeout:
            continue
        except OSError:
            break

        if not fragmento:
            print("[Red] Conexión cerrada por el ESP32-CAM.")
            break

        buffer.extend(fragmento)

        # Máquina de análisis del protocolo
        while True:
            inicio = buffer.find(MAGIC_BYTES)

            if inicio == -1:
                if len(buffer) > 3:
                    buffer = buffer[-3:]
                break

            if len(buffer) < inicio + 8:
                break  # Cabecera incompleta

            frame_len = int.from_bytes(buffer[inicio + 4:inicio + 8], 'little')

            if frame_len <= 0 or frame_len > 500_000:
                buffer = buffer[inicio + 1:]
                continue

            fin = inicio + 8 + frame_len
            if len(buffer) < fin:
                break  # Frame incompleto

            # Extraer JPEG crudo en bytes y avanzar el buffer de red
            jpeg_bytes = bytes(buffer[inicio + 8:fin])
            buffer     = buffer[fin:]

            # ENCOLA BYTES CRUDOS: Operación ultrarrápida en memoria
            if frame_queue.full():
                try:
                    frame_queue.get_nowait()  # Descartar paquete viejo sin decodificar
                except queue.Empty:
                    pass
            frame_queue.put_nowait(jpeg_bytes)

    stop_event.set()

# =============================================================================
# HILO DE INFERENCIA: Despierta sólo cuando se solicita una predicción ('p')
# =============================================================================
def hilo_inferencia():
    while not stop_event.is_set():
        try:
            # Recibe el frame ya decodificado (matriz NumPy) desde el hilo principal
            frame = inferencia_queue.get(timeout=0.5)
        except queue.Empty:
            continue

        print("\n[Inferencia] Procesando...")
        t0 = time.time()
        etiqueta, porcentaje, raw = predecir(frame)
        elapsed = time.time() - t0

        with resultado_lock:
            ultimo_resultado["texto"]  = f"{etiqueta}  {porcentaje:.1f}%"
            ultimo_resultado["tiempo"] = elapsed

        print(f"  [RESULTADO]: {etiqueta}")
        print(f"  Certeza   : {porcentaje:.2f}%")
        print(f"  Sigmoide  : {raw:.4f}  ({elapsed*1000:.0f} ms)\n")

# =============================================================================
# Configurar y conectar socket TCP
# =============================================================================
print(f"[Red] Conectando a {ESP32_IP}:{ESP32_PORT}...")

sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)

try:
    sock.settimeout(10)
    sock.connect((ESP32_IP, ESP32_PORT))
    sock.settimeout(0.5)
    print("[Red] ¡Conectado!\n")
    print("[Controles]  'p' = Predecir  |  'q' = Salir\n")

    # Arrancar hilos secundarios
    t_red = threading.Thread(target=hilo_red, args=(sock,), daemon=True)
    t_inf = threading.Thread(target=hilo_inferencia, daemon=True)
    t_red.start()
    t_inf.start()

    # =========================================================================
    # HILO PRINCIPAL: Visualización, Decodificación bajo demanda y Teclado
    # =========================================================================
    ultimo_frame  = None
    fps_contador  = 0
    fps_valor     = 0.0
    fps_tiempo    = time.time()
    VENTANA       = "ESP32-CAM  |  p=Predecir  q=Salir"

    while not stop_event.is_set():
        # Intentar obtener los bytes crudos del frame más reciente
        try:
            ultimo_jpeg = frame_queue.get_nowait()
            
            # DECODIFICACIÓN PEREZOSA: Decodificamos solo el frame extraído
            arr = np.frombuffer(ultimo_jpeg, dtype=np.uint8)
            frame_decodificado = cv2.imdecode(arr, cv2.IMREAD_COLOR)
            
            if frame_decodificado is not None:
                ultimo_frame = frame_decodificado
                fps_contador += 1
                
        except queue.Empty:
            pass  # Si no hay bytes nuevos, se redibuja el último 'ultimo_frame' exitoso

        # Calcular FPS de renderizado real cada segundo
        ahora = time.time()
        if ahora - fps_tiempo >= 1.0:
            fps_valor   = fps_contador / (ahora - fps_tiempo)
            fps_contador = 0
            fps_tiempo   = ahora

        # Dibujar en pantalla
        if ultimo_frame is not None:
            display = cv2.resize(ultimo_frame, (0, 0),
                                 fx=3.0, fy=3.0,
                                 interpolation=cv2.INTER_LINEAR)

            # Overlay: FPS de la aplicación
            cv2.putText(display, f"FPS: {fps_valor:.1f}",
                        (8, 24), cv2.FONT_HERSHEY_SIMPLEX,
                        0.6, (0, 255, 0), 2, cv2.LINE_AA)

            # Overlay: Resultado de la última inferencia
            with resultado_lock:
                texto_resultado = ultimo_resultado["texto"]
            if texto_resultado:
                cv2.putText(display, texto_resultado,
                            (8, display.shape[0] - 12),
                            cv2.FONT_HERSHEY_SIMPLEX,
                            0.7, (0, 200, 255), 2, cv2.LINE_AA)

            cv2.imshow(VENTANA, display)

        # Mantener la ventana viva (~60 Hz de refresco de interfaz)
        tecla = cv2.waitKey(16) & 0xFF

        if tecla == ord('q'):
            print("\n[Sistema] Saliendo...")
            break

        elif tecla == ord('p'):
            if ultimo_frame is not None:
                # Limpiar la cola de inferencia para evitar solicitudes acumuladas
                if inferencia_queue.full():
                    try:
                        inferencia_queue.get_nowait()
                    except queue.Empty:
                        pass
                
                # Enviamos una copia de la matriz decodificada actual al hilo de inferencia
                inferencia_queue.put_nowait(ultimo_frame.copy())
                print("[Sistema] Frame enviado a inferencia.")
            else:
                print("[WARN] Aún no se recibió ningún frame válido para predecir.")

except ConnectionRefusedError:
    print(f"\n[ERROR] No se pudo conectar a {ESP32_IP}:{ESP32_PORT}.")
    print("  Verifica que el ESP32-CAM esté encendido y en la misma red.")

except Exception as e:
    print(f"\n[ERROR] {type(e).__name__}: {e}")

finally:
    stop_event.set()
    sock.close()
    cv2.destroyAllWindows()
    print("[Sistema] Cerrado correctamente.")