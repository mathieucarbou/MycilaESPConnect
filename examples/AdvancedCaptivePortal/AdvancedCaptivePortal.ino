#include <MycilaESPConnect.h>
#include <Preferences.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);

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
    Preferences preferences;
    preferences.begin("app", false);
    preferences.clear();
    preferences.end();
    request->send(200);
    ESP.restart();
  });

  // network state listener is required here in async mode
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    JsonDocument doc;
    espConnect.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();

    switch (state) {
      case Mycila::ESPConnect::State::NETWORK_CONNECTED:
      case Mycila::ESPConnect::State::AP_STARTED:
        // serve your home page here
        server.on("/", HTTP_GET, [&](AsyncWebServerRequest* request) {
          return request->send(200, "text/plain", "Hello World!");
        }).setFilter([](__unused AsyncWebServerRequest* request) { return espConnect.getState() != Mycila::ESPConnect::State::PORTAL_STARTED; });
        server.begin();
        break;

      case Mycila::ESPConnect::State::NETWORK_DISCONNECTED:
        server.end();
        break;

      case Mycila::ESPConnect::State::PORTAL_COMPLETE: {
        Serial.println("====> Captive Portal has ended, save the configuration...");
        bool apMode = espConnect.hasConfiguredAPMode();
        Preferences preferences;
        preferences.begin("app", false);
        preferences.putBool("ap", apMode);
        if (!apMode) {
          preferences.putString("ssid", espConnect.getConfiguredWiFiSSID().c_str());
          preferences.putString("password", espConnect.getConfiguredWiFiPassword().c_str());
        }
        preferences.end();
        break;
      }

      default:
        break;
    }
  });

  espConnect.setAutoRestart(true);
  espConnect.setBlocking(false);
  espConnect.setCaptivePortalTimeout(180);
  espConnect.setConnectTimeout(20);
  
  Serial.println("====> Load config from elsewhere...");
  Preferences preferences;
  preferences.begin("app", true);
  Mycila::ESPConnect::Config config = {
    .hostname = "arduino",
    .wifiSSID = (preferences.isKey("ssid") ? preferences.getString("ssid", emptyString) : emptyString).c_str(),
    .wifiPassword = (preferences.isKey("password") ? preferences.getString("password", emptyString) : emptyString).c_str(),
    .apMode = preferences.isKey("ap") ? preferences.getBool("ap", false) : false};
  preferences.end();

  Serial.println("====> Trying to connect to saved WiFi or will start captive portal in the background...");
  Serial.printf("ap: %d\n", config.apMode);
  Serial.printf("wifiSSID: %s\n", config.wifiSSID.c_str());
  Serial.printf("wifiPassword: %s\n", config.wifiPassword.c_str());

  espConnect.begin("Captive Portal SSID", "", config);

  Serial.println("====> setup() completed...");
}

void loop() {
  espConnect.loop();
}
