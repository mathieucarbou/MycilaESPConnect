#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>

AsyncWebServer server(80);
Mycila::ESPConnect espConnect(server);
WiFiEventHandler onSoftAPModeStationConnected;
WiFiEventHandler onStationModeConnected;
WiFiEventHandler onStationModeGotIP;

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // this is just an ugly piece of code to wait for serial to connect
  delay(2000);

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
  });

  onSoftAPModeStationConnected = WiFi.onSoftAPModeStationConnected([](__unused const WiFiEventSoftAPModeStationConnected& v) {
    Serial.println("onSoftAPModeStationConnected");
  });

  onStationModeConnected = WiFi.onStationModeConnected([](__unused const WiFiEventStationModeConnected& v) {
    Serial.println("onStationModeConnected");
  });
  onStationModeGotIP = WiFi.onStationModeGotIP([](__unused const WiFiEventStationModeGotIP& v) {
    Serial.println("onStationModeGotIP");
  });

  // WiFi.mode(WiFiMode_t::WIFI_AP);
  // WiFi.softAP("Captive Portal SSID");
  // Serial.println(WiFi.softAPIP().toString());

  WiFi.persistent(false);
  WiFi.mode(WiFiMode_t::WIFI_STA);
  WiFi.begin("www.mathieu.photography", "mathieu&aurelie1205");
  while (WiFi.status() != wl_status_t::WL_CONNECTED) {
    Serial.println("Connecting...");
    delay(1000);
  }
  Serial.println("Connected!");
  Serial.println(WiFi.localIP().toString());

  server.begin();
}

void loop() {
  delay(100);
}
