
#include <WiFi.h>
#include <ArduinoOTA.h>


#define STASSID "Ahmads"
#define STAPSK  "Xg#609590"

#define LED_PIN    4

long lastTimeBlink = -1;

void setup() {
  
  pinMode(LED_PIN, OUTPUT);

  WiFi.mode(WIFI_STA);
//  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(STASSID, STAPSK);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);                             // wait for 0.2 second
  }

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname("BarebonesESP32");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });

  ArduinoOTA.begin();
  lastTimeBlink = millis();
}

// the loop function runs over and over again forever
void loop() {
  if(millis() - lastTimeBlink > 1000) {

    if(digitalRead(LED_PIN) == LOW) {
      digitalWrite(LED_PIN, HIGH);
    } else {
      digitalWrite(LED_PIN, LOW);
    }
    lastTimeBlink = millis();
  }

  // Handle the OTA Functionality
  ArduinoOTA.handle();
}
