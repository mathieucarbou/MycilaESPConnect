#include <MycilaESPConnect.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);
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

  // clear persisted config
  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    espConnect.clearConfiguration();
    request->send(200);
    ESP.restart();
  });

  // network state listener
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, __unused Mycila::ESPConnect::State state) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(true);

  Serial.println("Trying to connect to saved WiFi or will start portal...");

  espConnect.begin("arduino", "Captive Portal SSID");

  Serial.println("ESPConnect completed, continuing setup()...");

  // serve your home page here
  server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
    return request->send(200, "text/plain", "Hello World!");
  }).setFilter([](__unused AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });

  server.begin();
}

void loop() {
  espConnect.loop();

  if (millis() - lastLog > 5000) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
    lastLog = millis();
  }
}
