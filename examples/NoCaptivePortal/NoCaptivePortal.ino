#include <MycilaESPConnect.h>

// Compile with -D ESPCONNECT_NO_CAPTIVE_PORTAL

Mycila::ESPConnect espConnect;
uint32_t lastLog = 0;
uint32_t lastChange = 0;
const char* hostname = "arduino-1";

void setup() {
  Serial.begin(115200);
  while (!Serial)
    continue;

  // network state listener is required here in async mode
  espConnect.listen([](__unused Mycila::ESPConnect::State previous, Mycila::ESPConnect::State state) {
    if (state == Mycila::ESPConnect::State::NETWORK_TIMEOUT) {
      // failed to connect to saved WiFi
      Serial.println("====> Failed to connect to saved WiFi, starting AP mode...");
      espConnect.getConfig().apMode = true;
      return;
    }
  });

  espConnect.setBlocking(true);

  Serial.println("====> Trying to connect to saved WiFi or will start portal in the background...");

  espConnect.begin(hostname, "AP SSID");

  Serial.println("====> setup() completed...");
}

void loop() {
  delay(100);
}
