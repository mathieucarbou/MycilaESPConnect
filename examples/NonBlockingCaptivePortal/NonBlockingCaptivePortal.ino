#include <MycilaESPConnect.h>

AsyncWebServer server(80);
uint32_t lastLog = 0;
String hostname = "arduino-1";

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

  // serve your home page here
  server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
    return request->send(200, "text/plain", "Hello World!");
  }).setFilter([](__unused AsyncWebServerRequest* request) { return ESPConnect.getState() != ESPConnectState::PORTAL_STARTED; });

  // clear persisted config
  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Clearing configuration...");
    ESPConnect.clearConfiguration();
    request->send(200);
    ESP.restart();
  });

  server.on("/restart", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Restarting...");
    request->send(200);
    ESP.restart();
  });

  // network state listener is required here in async mode
  ESPConnect.listen([](__unused ESPConnectState previous, ESPConnectState state) {
    JsonDocument doc;
    ESPConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();

    switch (state) {
      case ESPConnectState::NETWORK_CONNECTED:
      case ESPConnectState::AP_STARTED:
        server.begin();
        break;

      case ESPConnectState::NETWORK_DISCONNECTED:
        server.end();
      default:
        break;
    }
  });

  ESPConnect.setAutoRestart(true);
  ESPConnect.setBlocking(false);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  ESPConnect.begin(server, hostname.c_str(), "Captive Portal SSID");

  Serial.println("====> setup() completed...");
}

void loop() {
  ESPConnect.loop();
}
