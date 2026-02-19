
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <WiFiClientSecure.h>
#include <Pinger.h>
extern "C"
{
  #include <lwip/icmp.h> // needed for icmp packet definitions
}

#define STASSID "QuickSync Lab"
#define STAPSK  "bg$786786"

//SHA1 finger print of certificate use web browser to view and copy
const char fingerprint[] PROGMEM = "B7 CB 1D 1B 02 72 1D 0E 89 A7 94 92 55 38 A7 37 9B 5D CD C4";

// Set global to avoid object removing after setup() routine
Pinger pinger;
bool lastPingResponse = true;

void setup() {

//  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(STASSID, STAPSK);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);                             // wait for 0.2 second
  }

  // Port defaults to 8266
  ArduinoOTA.setPort(8266);

  ArduinoOTA.setHostname("Barebones");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });

  ArduinoOTA.begin();
}

// the loop function runs over and over again forever
void loop() {
  // Handle the OTA Functionality
  ArduinoOTA.handle();

  if(pinger.Ping("arduino.com") == false) {
    // Internet is down
//    Serial.println("Internet is Down");
    lastPingResponse = false;
  } else {
    // Internet is up
    if(!lastPingResponse) {
      sendInternetOnlineMessage();
      lastPingResponse = true;
    }
  }

  delay(6000);
}

void sendInternetOnlineMessage() {
//  Serial.println("Internet Back Online");
  WiFiClientSecure client;    //Declare object of class WiFiClient
  client.setInsecure();//skip verification
  client.connect("maker.ifttt.com", 443);
  client.println("GET https://maker.ifttt.com/trigger/internet_online/json/with/key/gql4uOqAJGedovQODGGYHF1To0MyDTfwm2YjYjABgxl HTTP/1.0");
  client.println("Host: maker.ifttt.com");
  client.println("Connection: close");
  client.println();
  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
//      Serial.println("headers received");
      break;
    }
  }
  // if there are incoming bytes available
  // from the server, read them and print them:
  while (client.available()) {
    char c = client.read();
//    Serial.write(c);
  }

  client.stop();
}
