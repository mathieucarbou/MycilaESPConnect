#include <MycilaESPConnect.h>
#include <Preferences.h>

AsyncWebServer server(80);

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
    Preferences preferences;
    preferences.begin("app", false);
    preferences.clear();
    preferences.end();
    request->send(200);
    ESP.restart();
  });

  // network state listener is required here in async mode
  ESPConnect.listen([](__unused ESPConnectState previous, ESPConnectState state) {
    JsonDocument doc;
    ESPConnect.toJson(doc.to<JsonObject>());
    serializeJsonPretty(doc, Serial);
    Serial.println();

    switch (state) {
      case ESPConnectState::NETWORK_CONNECTED:
      case ESPConnectState::AP_STARTED:
        server.begin();
        break;

      case ESPConnectState::NETWORK_DISCONNECTED:
        server.end();
        break;

      case ESPConnectState::PORTAL_COMPLETE: {
        Serial.println("====> Captive Portal has ended, save the configuration...");
        bool apMode = ESPConnect.hasConfiguredAPMode();
        Preferences preferences;
        preferences.begin("app", false);
        preferences.putBool("ap", apMode);
        if (!apMode) {
          preferences.putString("ssid", ESPConnect.getConfiguredWiFiSSID().c_str());
          preferences.putString("password", ESPConnect.getConfiguredWiFiPassword().c_str());
        }
        preferences.end();
        break;
      }

      default:
        break;
    }
  });

  ESPConnect.setAutoRestart(true);
  ESPConnect.setBlocking(false);
  ESPConnect.setCaptivePortalTimeout(180);
  ESPConnect.setConnectTimeout(20);
  
  Serial.println("====> Load config from elsewhere...");
  Preferences preferences;
  preferences.begin("app", true);
  ESPConnectConfig config = {
    .wifiSSID = preferences.isKey("ssid") ? preferences.getString("ssid", emptyString) : emptyString,
    .wifiPassword = preferences.isKey("password") ? preferences.getString("password", emptyString) : emptyString,
    .apMode = preferences.isKey("ap") ? preferences.getBool("ap", false) : false};
  preferences.end();

  Serial.println("====> Trying to connect to saved WiFi or will start captive portal in the background...");
  Serial.printf("ap: %d\n", config.apMode);
  Serial.printf("wifiSSID: %s\n", config.wifiSSID.c_str());
  Serial.printf("wifiPassword: %s\n", config.wifiPassword.c_str());

  ESPConnect.begin(server, "arduino", "Captive Portal SSID", "", config);

  Serial.println("====> setup() completed...");
}

void loop() {
  ESPConnect.loop();
  delay(100);
}
