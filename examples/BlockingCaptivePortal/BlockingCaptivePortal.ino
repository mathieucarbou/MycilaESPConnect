#include <MycilaESPConnect.h>

AsyncWebServer server(80);
uint32_t lastLog = 0;

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
    request->send(200, "text/plain", "Hello World!");
  }).setFilter([](__unused AsyncWebServerRequest* request) { return ESPConnect.getState() != ESPConnectState::PORTAL_STARTED; });

  // clear persisted config
  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    ESPConnect.clearConfiguration();
    request->send(200);
    ESP.restart();
  });

  // network state listener
  ESPConnect.listen([](__unused ESPConnectState previous, __unused ESPConnectState state) {
    JsonDocument doc;
    ESPConnect.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  });

  ESPConnect.setAutoRestart(true);
  ESPConnect.setBlocking(true);

  Serial.println("Trying to connect to saved WiFi or will start portal...");

  ESPConnect.begin(server, "arduino", "Captive Portal SSID");

  Serial.println("ESPConnect completed, continuing setup()...");

  server.begin();
}

void loop() {
  ESPConnect.loop();

  if (millis() - lastLog > 5000) {
    JsonDocument doc;
    ESPConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
    lastLog = millis();
  }
}
