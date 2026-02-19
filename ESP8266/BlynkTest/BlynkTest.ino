
// Template ID, Device Name and Auth Token are provided by the Blynk.Cloud
// See the Device Info tab, or Template settings
#define BLYNK_TEMPLATE_ID "TMPLr81FC4TA"
#define BLYNK_DEVICE_NAME "WiFiSwitch"

#define BOARD_VENDOR "Alithis Technologies"
#define PRODUCT_WIFI_SSID "ALITHIS"

#define BLYNK_FIRMWARE_VERSION        "0.1.0"

#define USE_NODE_MCU_BOARD

#define DEFAULT_ON_DURATION   5000
#define DEFAULT_OFF_DURATION  2000


// Comment this out to disable prints and save space
#define BLYNK_PRINT Serial

#include <ArduinoOTA.h>
#include "BlynkEdgent.h"

int onDuration = DEFAULT_ON_DURATION;
int offDuration = DEFAULT_OFF_DURATION;
bool isBlinkOn = false;
long lastTimeBlink = 0;

// This function is called every time the Virtual Pin 0 state changes
BLYNK_WRITE(V0)
{
  if(param.asInt() == 0) {
    digitalWrite(0, HIGH);
    Blynk.setProperty(V0, "onLabel", "OFF");
    Blynk.setProperty(V0, "color", "#FF0000");
  } else {
    digitalWrite(0, LOW);
    Blynk.setProperty(V0, "offLabel", "ON");
    Blynk.setProperty(V0, "color", "#00FF00");
  }
}

BLYNK_WRITE(V1)
{
  if(param.asInt() == 0) {
    isBlinkOn = false;
     digitalWrite(0, HIGH);
     Blynk.virtualWrite(V0, 0);
  } else {
    isBlinkOn = true;
  }
}

BLYNK_WRITE(V2)
{
  onDuration = param.asInt() * 1000;
}

BLYNK_WRITE(V3)
{
  offDuration = param.asInt() * 1000;
}

// This function is called every time the device is connected to the Blynk.Cloud
BLYNK_CONNECTED()
{
}

void setup()
{
  pinMode(0, OUTPUT);

  BlynkEdgent.begin();

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname("AmplifierSwitch");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });

  ArduinoOTA.begin();

  Blynk.syncVirtual(V0);
  Blynk.syncVirtual(V1);
  Blynk.syncVirtual(V2);
  Blynk.syncVirtual(V3);
}

void loop()
{
  BlynkEdgent.run();
  
  // Handle the OTA Functionality
  ArduinoOTA.handle();

  if(isBlinkOn) {
    if(digitalRead(0) == LOW) {
      if(millis() - lastTimeBlink > onDuration) {
        digitalWrite(0, HIGH);
        Blynk.virtualWrite(V0, 0);
        lastTimeBlink = millis();
      }
    } else {
      if(millis() - lastTimeBlink > offDuration) {
        digitalWrite(0, LOW);
        Blynk.virtualWrite(V0, 1);
        lastTimeBlink = millis();
      }
    }
  }
}
