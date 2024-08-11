#include <MycilaESPConnect.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);
uint32_t lastLog = 0;
uint32_t lastChange = 0;
String hostname = "arduino-1";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  server.on("/clear", HTTP_GET, [&](AsyncWebServerRequest* request) {
    Serial.println("Clearing configuration...");
    espConnect.clearConfiguration();
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

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  espConnect.begin(hostname.c_str(), "Captive Portal SSID");

  Serial.println("====> setup() completed...");
}

void loop() {
  espConnect.loop();

  uint32_t now = millis();

  if (now - lastLog > 3000) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJson(doc, Serial);
    Serial.println();
    lastLog = millis();
  }

  if (now - lastChange > 10000) {
    if (espConnect.getIPConfig().ip == INADDR_NONE) {
      Mycila::ESPConnect::IPConfig ipConfig;
      ipConfig.ip.fromString("192.168.125.99");
      ipConfig.gateway.fromString("192.168.125.1");
      ipConfig.subnet.fromString("255.255.255.0");
      ipConfig.dns.fromString("192.168.125.1");
      espConnect.setIPConfig(ipConfig);
    } else {
      espConnect.setIPConfig(Mycila::ESPConnect::IPConfig());
    }
    lastChange = millis();
  }
}
