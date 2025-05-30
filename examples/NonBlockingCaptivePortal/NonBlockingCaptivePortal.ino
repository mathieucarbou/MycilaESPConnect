#include <MycilaESPConnect.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);
uint32_t lastLog = 0;
const char* hostname = "arduino-1";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // this is possible to serve a logo
  // server.on("/logo", HTTP_GET, [&](AsyncWebServerRequest* request) {
  //   AsyncWebServerResponse* response = request->beginResponse(200, "image/png");
  //   response->addHeader("Content-Encoding", "gzip");
  //   response->addHeader("Cache-Control", "public, max-age=900");
  //   request->send(response);
  // });

  // clear persisted config
  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Clearing configuration...");
    espConnect.clearConfiguration();
    request->send(200);
    ESP.restart();
  });

  server.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Restarting...");
    request->send(200);
    ESP.restart();
  });

  // network state listener is required here in async mode
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();

    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        // serve your home page here
        server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
                return request->send(200, "text/plain", "Hello World!");
              })
          .setFilter([](__unused AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
        server.begin();
        break;

      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        server.end();
        break;

      default:
        break;
    }
  });

  espConnect.setConnectTimeout(20);
  espConnect.setCaptivePortalTimeout(40);
  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  espConnect.begin(hostname, "Captive Portal SSID");

  Serial.println("====> setup() completed...");
}

uint32_t last = 0;

void loop() {
  espConnect.loop();

  if (millis() - last > 5000) {
    last = millis();
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  }
}
