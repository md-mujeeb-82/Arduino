#include "esp_wifi.h"

static int8_t buffer[100];

void setup() {
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_start();
}

void loop() {
  for(int i=0; i<100; i++) {
    buffer[i] = random(0, 254);
  }
  for (int ch = 1; ch <= 13; ch++) {
      esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
      esp_wifi_80211_tx(WIFI_IF_AP, buffer, sizeof(buffer), true);
      delay(5);
  }
}
