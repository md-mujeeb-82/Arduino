extern "C" {
  #include "user_interface.h"
}
#include <ESP8266WiFi.h>

int8_t buffer[100];

void setup() {
  WiFi.setOutputPower(20.5);
  wifi_set_opmode(STATION_MODE);
}

void loop() {

  for (int i = 0; i < 100; i++) {
    buffer[i] = random(0, 254);
  }

  for (int ch = 1; ch <= 13; ch++) {
    wifi_set_channel(ch);
    wifi_send_pkt_freedom((uint8*)buffer, sizeof(buffer), 0);
    delay(5);
  }
}
