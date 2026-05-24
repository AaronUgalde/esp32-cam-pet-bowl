#include <esp32cam.h>
#include <WebServer.h>
#include <WiFi.h>
#include "../secrets.h"
// WIFI_SSID y WIFI_PASS se usan directamente desde secrets.h

WebServer server(80);

static auto loRes = esp32cam::Resolution::find(160, 120);

void handleJpg()
{
  auto frame = esp32cam::capture();
  if (frame == nullptr) {
    Serial.println("CAPTURE FAIL");
    server.send(503, "", "");
    return;
  }

  Serial.printf("CAPTURE OK %dx%d %db\n",
                frame->getWidth(),
                frame->getHeight(),
                static_cast<int>(frame->size()));

  server.setContentLength(frame->size());
  server.send(200, "image/jpeg");
  WiFiClient client = server.client();
  frame->writeTo(client);
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  {
    using namespace esp32cam;
    Config cfg;
    cfg.setPins(pins::AiThinker);
    cfg.setResolution(loRes);
    cfg.setBufferCount(2);
    cfg.setJpeg(80);

    bool ok = Camera.begin(cfg);
    Serial.println(ok ? "CAMERA OK" : "CAMERA FAIL");
  }

  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  Serial.print("http://");
  Serial.println(WiFi.localIP());
  Serial.println("  /cam.jpg");

  server.on("/cam.jpg", handleJpg);
  server.begin();
}

void loop()
{
  server.handleClient();
}