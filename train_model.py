import tensorflow as tf
from tensorflow.keras import layers, models
import matplotlib.pyplot as plt

# 1. Configuración inicial
# QQVGA es 160x120. En Keras, el formato de entrada es (alto, ancho)
IMG_HEIGHT = 120
IMG_WIDTH = 160
BATCH_SIZE = 16 # Lote pequeño porque tienes pocas imágenes
EPOCHS = 30
DATASET_DIR = 'dataset'

# 2. Carga del Dataset
print("Cargando dataset de entrenamiento...")
train_dataset = tf.keras.utils.image_dataset_from_directory(
    DATASET_DIR,
    validation_split=0.2,
    subset="training",
    seed=123,
    image_size=(IMG_HEIGHT, IMG_WIDTH),
    batch_size=BATCH_SIZE
)

# === RESPALDA LOS NOMBRES AQUÍ ANTES DE OPTIMIZAR ===
class_names = train_dataset.class_names 
print("Clases detectadas:", class_names) # Esto imprimirá ['empty', 'full']
# ====================================================

print("Cargando dataset de validación...")
val_dataset = tf.keras.utils.image_dataset_from_directory(
    DATASET_DIR,
    validation_split=0.2,
    subset="validation",
    seed=123,
    image_size=(IMG_HEIGHT, IMG_WIDTH),
    batch_size=BATCH_SIZE
)

# Aquí es donde se "perdía" el atributo al sobreescribir la variable
AUTOTUNE = tf.data.AUTOTUNE
train_dataset = train_dataset.cache().shuffle(1000).prefetch(buffer_size=AUTOTUNE)
val_dataset = val_dataset.cache().prefetch(buffer_size=AUTOTUNE)

# 3. Data Augmentation (Integrado en el modelo)
data_augmentation = tf.keras.Sequential([
    layers.RandomFlip("horizontal_and_vertical", seed=123),
    layers.RandomRotation(0.2, seed=123), # Rota la imagen hasta 20%
    layers.RandomZoom(0.1, seed=123),     # Hace un ligero zoom in/out
    layers.RandomBrightness(factor=0.2, seed=123) # Simula cambios de luz
])

# 4. Arquitectura de la Red Neuronal (CNN)
model = models.Sequential([
    # Capa de entrada explícita
    layers.Input(shape=(IMG_HEIGHT, IMG_WIDTH, 3)),
    
    # Bloque de Aumento de Datos y Normalización
    data_augmentation,
    layers.Rescaling(1./255), # Normalizamos los píxeles de 0-255 a 0-1
    
    # Bloque Convolucional 1
    layers.Conv2D(16, (3, 3), padding='same', activation='relu'),
    layers.MaxPooling2D(pool_size=(2, 2)),
    
    # Bloque Convolucional 2
    layers.Conv2D(32, (3, 3), padding='same', activation='relu'),
    layers.MaxPooling2D(pool_size=(2, 2)),
    
    # Bloque Convolucional 3 (Opcional, ayuda a extraer texturas más finas)
    layers.Conv2D(64, (3, 3), padding='same', activation='relu'),
    layers.MaxPooling2D(pool_size=(2, 2)),
    
    # Aplanamiento y Red Densa
    layers.Flatten(),
    layers.Dropout(0.5), # Apagamos el 50% de las neuronas para evitar sobreajuste
    layers.Dense(64, activation='relu'),
    
    # Capa de Salida (1 neurona con sigmoide para clasificación binaria)
    layers.Dense(1, activation='sigmoid')
])

# 5. Compilación del Modelo
model.compile(optimizer='adam',
              loss=tf.keras.losses.BinaryCrossentropy(),
              metrics=['accuracy'])

model.summary()

# 6. Entrenamiento
history = model.fit(
    train_dataset,
    validation_data=val_dataset,
    epochs=EPOCHS
)

# 7. Guardar el modelo para usarlo después con los datos del puerto serial
model.save('modelo_bowl_perro.keras')
print("Modelo guardado exitosamente.")

# ==========================================
# 8. EVALUACIÓN DETALLADA DEL MODELO
# ==========================================
import numpy as np
from sklearn.metrics import classification_report, confusion_matrix

# --- PARTE A: Gráficas de Precisión y Pérdida ---
acc = history.history['accuracy']
val_acc = history.history['val_accuracy']
loss = history.history['loss']
val_loss = history.history['val_loss']
epochs_range = range(EPOCHS)

plt.figure(figsize=(14, 5))

# Gráfica de Accuracy
plt.subplot(1, 2, 1)
plt.plot(epochs_range, acc, label='Entrenamiento (Train Acc)', color='blue', linewidth=2)
plt.plot(epochs_range, val_acc, label='Validación (Val Acc)', color='orange', linewidth=2)
plt.title('Evolución de la Precisión (Accuracy)')
plt.xlabel('Época')
plt.ylabel('Precisión')
plt.legend(loc='lower right')
plt.grid(True, linestyle='--', alpha=0.6)

# Gráfica de Loss
plt.subplot(1, 2, 2)
plt.plot(epochs_range, loss, label='Entrenamiento (Train Loss)', color='blue', linewidth=2)
plt.plot(epochs_range, val_loss, label='Validación (Val Loss)', color='orange', linewidth=2)
plt.title('Evolución de la Pérdida (Loss)')
plt.xlabel('Época')
plt.ylabel('Pérdida')
plt.legend(loc='upper right')
plt.grid(True, linestyle='--', alpha=0.6)

plt.tight_layout()
plt.show()

# --- PARTE B: Matriz de Confusión y Métricas en Validación ---
print("\n" + "="*50)
print("EXTRAYENDO MÉTRICAS DETALLADAS DE VALIDACIÓN...")
print("="*50)

y_true = []
y_pred = []

# Iterar sobre el dataset de validación para recolectar etiquetas reales y predicciones
for images, labels in val_dataset:
    preds = model.predict(images, verbose=0)
    # Convertir las salidas del sigmoide (0.0 a 1.0) en valores binarios (0 o 1)
    preds_binary = (preds >= 0.5).astype(int).flatten()
    
    y_true.extend(labels.numpy())
    y_pred.extend(preds_binary)

y_true = np.array(y_true)
y_pred = np.array(y_pred)

# Generar reporte de clasificación (Precision, Recall, F1-Score)
# train_dataset.class_names mapea automáticamente 0 -> 'empty' y 1 -> 'full'
print("\n[Reporte de Clasificación]")
print(classification_report(y_true, y_pred, target_names=class_names))

# Generar Matriz de Confusión
print("[Matriz de Confusión]")
cm = confusion_matrix(y_true, y_pred)
print(f"               Predicho EMPTY   Predicho FULL")
print(f"Real EMPTY:        {cm[0][0]}                {cm[0][1]}")
print(f"Real FULL:         {cm[1][0]}                {cm[1][1]}")