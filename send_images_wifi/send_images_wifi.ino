// =============================================================================
// ESP32-CAM - Servidor TCP de Streaming (v6 - Raw Sockets + FreeRTOS)
// Proyecto: Inspección Visual de Bowl de Mascota
//
// POR QUÉ v5 seguía lento (0.3–4 FPS):
//   WiFiServer/WiFiClient es un wrapper de Arduino que corre en el loop()
//   principal (Core 1, prioridad 1). El driver de la cámara también usa
//   Core 0. Todo compite: captura, red y loop() en un solo hilo secuencial.
//
// QUÉ hace CameraWebServer diferente:
//   Usa esp_http_server que internamente llama send() de lwIP y lanza una
//   tarea FreeRTOS en Core 0 a prioridad 5. El streaming corre completamente
//   separado del loop() de Arduino.
//
// Cambios v6:
//   - Raw POSIX sockets (socket/bind/listen/accept/send) en lugar de
//     WiFiServer/WiFiClient. Es la misma API que usa httpd internamente.
//   - Tarea FreeRTOS "stream_tcp" anclada a Core 0, prioridad 5.
//     El loop() de Arduino queda vacío (igual que en CameraWebServer).
//   - send() con errno EAGAIN → vTaskDelay(1) en lugar de busy-wait.
//   - SO_REUSEADDR + SO_SNDBUF configurados via setsockopt correcto.
// =============================================================================

#include "esp_camera.h"
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

// -----------------------------------------------------------------------------
// CONFIGURACIÓN DE RED
// -----------------------------------------------------------------------------
const char* SSID     = "IZZI-4E6E";
const char* PASSWORD = "509551214E6E";
const int   PUERTO   = 8888;

// -----------------------------------------------------------------------------
// Pines AI-Thinker ESP32-CAM
// -----------------------------------------------------------------------------
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

const uint8_t MAGIC[4] = {0xA5, 0x5A, 0xA5, 0x5A};

// Buffer estático TX: cabecera (8 bytes) + JPEG QQVGA (~2–8 KB) caben de sobra.
static uint8_t TX_BUF[32 * 1024];

// FPS máximo que enviamos al cliente.
// Con GRAB_LATEST el ESP32 puede generar 25+ FPS, pero Python solo consume
// 5-10 FPS. Si enviamos más rápido de lo que Python lee, el buffer TCP se
// llena, send() se bloquea indefinidamente (SO_SNDTIMEO no funciona en lwIP)
// y la cámara queda paralizada → 0 FPS. Cap en 10 FPS = buffer nunca se llena.
const uint32_t FPS_CAP          = 10;
const uint32_t FRAME_MS         = 1000 / FPS_CAP;   // 100 ms por frame

// =============================================================================
// enviarTodo() — socket no-bloqueante con timeout de pared real
//
// SO_SNDTIMEO no funciona en lwIP del ESP-IDF (bug conocido).
// Solución: O_NONBLOCK + millis() para timeout real de 400 ms.
// Si Python no lee en 400 ms, desconectamos limpiamente.
// =============================================================================
static bool enviarTodo(int fd, const uint8_t* datos, size_t total) {
  size_t   enviado  = 0;
  uint32_t tInicio  = millis();

  while (enviado < total) {

    int escritos = send(fd, datos + enviado, total - enviado, 0);

    if (escritos < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        // Buffer lleno pero no permanentemente: ceder CPU 2 ms y reintentar
        vTaskDelay(2 / portTICK_PERIOD_MS);
        continue;
      }
      return false;   // error real: ECONNRESET, EPIPE, etc.
    }

    enviado += (size_t)escritos;
  }
  return true;
}

// =============================================================================
// streamTask — corre en Core 0, prioridad 5 (igual que esp_http_server)
//
// Al anclar esta tarea a Core 0 conseguimos:
//   1. No competir con el loop() de Arduino (Core 1).
//   2. Compartir core con el driver de la cámara → menos latencia de IPC.
//   3. lwIP también corre en Core 0: send() no necesita cruzar cores.
// =============================================================================
static void streamTask(void* arg) {
  // --- Crear socket servidor ---
  int server_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd < 0) {
    Serial.println("[ERROR] No se pudo crear socket.");
    vTaskDelete(NULL);
    return;
  }

  // Reutilizar puerto inmediatamente tras reset (evita "Address already in use")
  int opt = 1;
  setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family      = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port        = htons(PUERTO);

  bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
  listen(server_fd, 1);

  Serial.printf("[TCP] Escuchando en %s:%d  (Core %d)\n",
                WiFi.localIP().toString().c_str(), PUERTO, xPortGetCoreID());

  // --- Loop de aceptar clientes ---
  while (true) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
      continue;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // TCP_NODELAY: sin Nagle, cada frame sale inmediatamente
    int nodelay = 1;
    setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));

    // Buffer de envío del kernel: 32 KB = varios frames QQVGA en vuelo
    int sndbuf = 32768;
    setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

    // SO_SNDTIMEO — CRÍTICO: timeout de 1.5 s para send().
    // Sin esto, send() es bloqueante indefinidamente. Cuando Python frena
    // (GC, GIL, pausa de recv), el buffer TCP se llena y send() se congela.
    // fb_get() nunca se llama mientras send() esté bloqueado → 0 FPS.
    // Con este timeout, send() retorna ETIMEDOUT tras 1.5 s, desconectamos
    // limpiamente y Python reconecta en <100 ms.
    struct timeval sndtv = { .tv_sec = 1, .tv_usec = 500000 };
    setsockopt(client_fd, SOL_SOCKET, SO_SNDTIMEO, &sndtv, sizeof(sndtv));

    Serial.println("[TCP] Cliente conectado.");

    uint32_t frameCount = 0;
    uint32_t tInicio    = millis();

    // --- Loop de streaming ---
    while (true) {
      camera_fb_t* fb = esp_camera_fb_get();
      if (!fb) {
        vTaskDelay(5 / portTICK_PERIOD_MS);
        continue;
      }

      uint32_t longitud   = (uint32_t)fb->len;
      size_t   totalBytes = 4 + 4 + fb->len;
      bool     ok;

      if (totalBytes <= sizeof(TX_BUF)) {
        // Un solo send() = un solo paquete TCP
        memcpy(TX_BUF,     MAGIC,     4);
        memcpy(TX_BUF + 4, &longitud, 4);
        memcpy(TX_BUF + 8, fb->buf,   fb->len);
        ok = enviarTodo(client_fd, TX_BUF, totalBytes);
      } else {
        // Fallback (no debería pasar con QQVGA)
        ok = enviarTodo(client_fd, MAGIC,               4)
          && enviarTodo(client_fd, (uint8_t*)&longitud, 4)
          && enviarTodo(client_fd, fb->buf,         fb->len);
      }

      esp_camera_fb_return(fb);

      if (!ok) {
        Serial.println("[WARN] Error de envío. Cliente desconectado.");
        break;
      }

      // Reportar FPS cada 5 s
      frameCount++;
      uint32_t elapsed = millis() - tInicio;
      if (elapsed >= 5000) {
        Serial.printf("[FPS] %.1f fps | IP: %s\n",
                      frameCount / (elapsed / 1000.0f),
                      WiFi.localIP().toString().c_str());
        frameCount = 0;
        tInicio    = millis();
      }

      vTaskDelay(FRAME_MS / portTICK_PERIOD_MS);
    }

    close(client_fd);
    Serial.println("[TCP] Cliente desconectado.");
  }

  close(server_fd);
  vTaskDelete(NULL);
}

// =============================================================================
// setup()
// =============================================================================
void setup() {
  Serial.begin(115200);
  delay(500);

  // --- Detectar PSRAM ---
  bool tienePSRAM = psramInit() && (ESP.getFreePsram() > 0);
  if (tienePSRAM) {
    Serial.printf("[OK] PSRAM detectada: %d bytes libres\n", ESP.getFreePsram());
  } else {
    Serial.println("[INFO] Sin PSRAM. Modo bajo FPS (fb_count=1).");
  }

  Serial.println("=== ESP32-CAM Servidor TCP v6 ===");

  // --- Configurar cámara ---
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.jpeg_quality = 12;

  // Inicializar siempre a resolución mayor que la de streaming.
  // Quirk del OV2640: inicializar directamente a QQVGA crea búferes DMA
  // inestables que causan fb_get() lento o bloqueado → 0 FPS esporádico.
  // CameraWebServer hace lo mismo: init a UXGA/SVGA, baja a QVGA post-init.
  if (tienePSRAM) {
    config.frame_size  = FRAMESIZE_SVGA;   // init estable; baja a QQVGA después
    config.fb_count    = 2;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode   = CAMERA_GRAB_LATEST;
    Serial.println("[CAM] Modo alto FPS (PSRAM, 2 buffers, GRAB_LATEST)");
  } else {
    config.frame_size  = FRAMESIZE_QVGA;   // menor que cabe en DRAM; baja a QQVGA
    config.fb_count    = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode   = CAMERA_GRAB_WHEN_EMPTY;
    Serial.println("[CAM] Modo estable (DRAM, 1 buffer, GRAB_WHEN_EMPTY)");
  }

  esp_log_level_set("cam_hal", ESP_LOG_NONE);
  esp_log_level_set("cam_dvp", ESP_LOG_NONE);
  esp_log_level_set("camera",  ESP_LOG_NONE);

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[ERROR] Cámara no inicializada: 0x%x\n", err);
    while (true) delay(1000);
  }
  Serial.println("[OK] Cámara lista.");

  // Ajuste del sensor post-init
  sensor_t* s = esp_camera_sensor_get();
  if (s) {
    // Bajar a resolución de streaming (los búferes DMA son más grandes, OK)
    s->set_framesize(s, FRAMESIZE_QQVGA);
    s->set_brightness(s, 0);
    s->set_saturation(s, 0);
    s->set_contrast(s, 0);

    // Deshabilitar AEC y AGC — CRÍTICO para FPS estable.
    // Con AEC activo, el OV2640 ajusta exposición cada frame. Cuando la
    // iluminación cambia, el tiempo de captura sube de 33 ms a 300–2000 ms.
    // Con GRAB_WHEN_EMPTY esto bloquea fb_get() → caída a 0 FPS.
    // Fijamos exposición y ganancia en valores medios y lo dejamos fijo.
    s->set_exposure_ctrl(s, 0);   // AEC off
    s->set_gain_ctrl(s, 0);       // AGC off
    s->set_aec_value(s, 300);     // exposición fija (rango 0–1200; ajustar si muy oscuro/claro)
    s->set_agc_gain(s, 5);        // ganancia fija (rango 0–30)
    s->set_aec2(s, 0);            // AEC de noche off
    Serial.println("[CAM] Sensor listo (AEC/AGC desactivados).");
  }

  // --- Conectar WiFi ---
  Serial.printf("[WiFi] Conectando a '%s'", SSID);
  WiFi.begin(SSID, PASSWORD);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println();
  Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());

  // --- Lanzar tarea de streaming ---
  // Core 0, prioridad 5: igual que esp_http_server (HTTPD_DEFAULT_CONFIG)
  // Stack 4096 bytes es suficiente para send() + camera_fb_get()
  xTaskCreatePinnedToCore(
    streamTask,   // función
    "stream_tcp", // nombre (visible en top de FreeRTOS)
    4096,         // stack bytes
    NULL,         // arg
    5,            // prioridad (misma que HTTP server de Espressif)
    NULL,         // handle (no necesario)
    0             // Core 0
  );
}

// =============================================================================
// loop() — vacío, igual que en CameraWebServer
// Todo el trabajo ocurre en streamTask (Core 0).
// =============================================================================
void loop() {
  delay(10000);
}
