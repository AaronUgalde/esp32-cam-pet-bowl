
```text
📦 proyecto-plato-esp32cam
 ┣ 📂 dataset
 ┃ ┣ 📂 empty                  # Imágenes del plato vacío
 ┃ ┗ 📂 full                   # Imágenes del plato lleno
 ┣ 📜 send_images_serial\send_images_serial.ino     # Código para el ESP32-CAM (Arduino IDE)
 ┣ 📜 train_model.py             # Script de entrenamiento de la red neuronal
 ┣ 📜 test_model.py      # Script de visualización, captura e inferencia en tiempo real
 ┣ 📜 modelo_bowl_perro.keras  # Modelo entrenado generado automáticamente
 ┗ 📜 README.md                # Este archivo de documentación

```

---

## 🛠️ Componentes del Código

### 1. Firmware ESP32-CAM (`send_images_serial\send_images_serial.ino`)

Configura el hardware del ESP32-CAM (sensor OV2640) para capturar imágenes en resolución **QQVGA (160x120)** en formato JPEG para optimizar el ancho de banda.

* **Protocolo Custom de Comunicación**: Para evitar la corrupción de datos en ráfagas seriales de alta velocidad, las imágenes se empaquetan con un encabezado de sincronización rígido:
1. `MAGIC_BYTES`: 4 bytes fijos (`0xA5, 0x5A, 0xA5, 0x5A`).
2. `Length`: 4 bytes que representan el tamaño total del JPEG (en formato Little Endian).
3. `Payload`: Los bytes crudos de la imagen JPEG comprimida.


* **Velocidad de Transmisión**: Configurado a **1,500,000 baudios** para garantizar un flujo continuo (baja latencia).

### 2. Script de Adquisición e Inferencia (`test_model.py`)

Controla la comunicación serial bidireccional desde la PC y procesa los bytes entrantes mediante una máquina de estados de búfer para reconstruir las imágenes JPEG.

* **Interfaz de Usuario Básica (OpenCV)**: Abre una ventana interactiva escalando la imagen x3 para facilitar el monitoreo humano.
* **Control por Teclado**:
* `e`: Guarda el cuadro actual dentro de la carpeta `dataset/empty` (etiquetado automático).
* `f`: Guarda el cuadro actual dentro de la carpeta `dataset/full`.
* `p`: Detiene momentáneamente el flujo, transforma el espacio de color (BGR a RGB), redimensiona y ejecuta el modelo cargado (`.keras`) para imprimir una predicción de probabilidad binaria.
* `q`: Cierra los hilos de comunicación de forma segura.



### 3. Pipeline de Entrenamiento de la CNN (`train_model.py`)

Contiene la lógica de entrenamiento usando **TensorFlow 2.x / Keras**.

* **Preprocesamiento Integrado**: Realiza divisiones automatizadas de Entrenamiento/Validación (80/20) y aplica técnicas de *Data Augmentation* (volteos aleatorios, rotación de hasta 20%, zoom y cambios dinámicos de brillo) directamente en capas de GPU para reducir el sobreajuste (*overfitting*).
* **Arquitectura del Modelo**:
* Capa de normalización (escala los píxeles de `[0, 255]` a `[0, 1]`).
* 3 Bloques Convolucionales con filtros progresivos (16, 32, 64) de tamaño 3x3 y activaciones **ReLU**, acompañados de *Max Pooling* de 2x2.
* Bloque clasificador denso con una capa de **Dropout al 50%** y una capa de salida con **función de activación Sigmoide** para una salida probabilística lineal de clase única.


* **Métricas del Negocio**: Genera un reporte detallado con gráficas de evolución temporal de precisión/pérdida, además de una **Matriz de Confusión** y reportes de métricas avanzadas (*Precision, Recall, F1-Score*) usando Scikit-Learn.

---

## 💻 Requisitos e Instalación

### Hardware

* Tarjeta de desarrollo **ESP32-CAM** (Ai-Thinker recomendado).
* Cable / Programador FTDI USB a Serial.
* Plato de comida para mascotas (para el entorno de pruebas).

### Entorno de Software (Python 3.9 - 3.11 Recomendado)

Instala las dependencias necesarias ejecutando en tu terminal:

```bash
pip install tensorflow opencv-python pyserial matplotlib scikit-learn numpy

```

---

## 📋 Instrucciones de Operación paso a paso

### Paso 1: Recolección de Datos

1. Carga el código `send_images_serial\send_images_serial.ino` en tu ESP32-CAM usando el Arduino IDE (asegúrate de seleccionar la velocidad de consola correcta y los pines de tu placa).
2. Verifica a qué puerto COM se conectó tu dispositivo (ej. `COM7` en Windows o `/dev/ttyUSB0` en Linux) y edita la variable `PORT` en tu script de Python (`test_model.py`).
3. Ejecuta el script `test_model.py`.
4. Coloca el plato vacío frente a la cámara en diferentes ángulos y luces, presiona la tecla `e` consecutivas veces para capturar al menos 100-200 imágenes.
5. Llena el plato y repite el proceso presionando la tecla `f` para recolectar muestras llenas.

### Paso 2: Entrenamiento del Modelo

1. Asegúrate de que las carpetas `dataset/empty` y `dataset/full` tengan imágenes válidas.
2. Ejecuta el script de entrenamiento:
```bash
python train_model.py

```


3. Al finalizar, analiza las gráficas de entrenamiento. Si la brecha entre el entrenamiento y la validación es muy amplia, considera recolectar más datos. El archivo `modelo_bowl_perro.keras` se guardará automáticamente en la raíz.

### Paso 3: Inferencia en Tiempo Real

1. Vuelve a abrir tu script interactivo `test_model.py`. El script detectará y cargará automáticamente la red neuronal entrenada.
2. Apunta la cámara al plato y presiona la tecla `p`. La consola imprimirá inmediatamente si el plato está **VACÍO** o **LLENO** junto con el porcentaje de certeza probabilística del modelo.
